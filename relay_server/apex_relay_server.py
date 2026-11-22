#!/usr/bin/env python3
"""
APEX VELOCITY - Full Cloud Game Server, SQLite Account API & Multiplayer Relay
==============================================================================
When deployed on Render.com (or any cloud host):
  - GET /              -> Serves the full playable 60 FPS APEX VELOCITY game in the browser!
  - GET /status        -> JSON server health & active rooms check
  - GET /api/users     -> Lists registered driver usernames from SQLite (apex_accounts.db)
  - POST /api/register -> Creates a new driver account (Salted SHA-256 + 500 Starting Coins) in SQLite
  - POST /api/login    -> Authenticates driver username & passkey and loads Coins, Cars, Engines & Stats
  - POST /api/save_profile -> Saves updated Coins, Unlocked Cars/Engines, and Race History to SQLite
  - WS /ws             -> RFC-6455 Browser WebSocket Multiplayer Lobby & 60Hz Telemetry Server
  - WS /relay & TCP    -> Desktop Qt C++ Client Reverse-Tunnel Relay Server
"""

import asyncio
import base64
import datetime
import hashlib
import json
import os
import random
import secrets
import sqlite3
import struct
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
WEB_GAME_PATH = os.path.join(HERE, "web_game.html")
ASSETS_DIR = os.path.join(HERE, "assets")
DB_PATH = os.environ.get("APEX_DB_PATH", os.path.join(HERE, "apex_accounts.db"))

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

# ============================================================================
#  1. SQLITE DATABASE (apex_accounts.db) FOR USER ACCOUNTS & COINS
# ============================================================================
def get_db():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    conn.execute("""
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL COLLATE NOCASE,
            password_salt TEXT NOT NULL,
            password_hash TEXT NOT NULL,
            coins INTEGER NOT NULL DEFAULT 500,
            owned_cars INTEGER NOT NULL DEFAULT 1,
            owned_engines INTEGER NOT NULL DEFAULT 1,
            selected_car INTEGER NOT NULL DEFAULT 0,
            selected_engine INTEGER NOT NULL DEFAULT 0,
            races_completed INTEGER NOT NULL DEFAULT 0,
            wins INTEGER NOT NULL DEFAULT 0,
            podiums INTEGER NOT NULL DEFAULT 0,
            total_earned INTEGER NOT NULL DEFAULT 500,
            relay_server TEXT DEFAULT '',
            created_at TEXT NOT NULL,
            last_login_at TEXT NOT NULL
        );
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS race_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL COLLATE NOCASE,
            position INTEGER NOT NULL,
            coins_earned INTEGER NOT NULL,
            car_id INTEGER NOT NULL,
            engine_id INTEGER NOT NULL,
            played_at TEXT NOT NULL
        );
    """)
    conn.commit()
    return conn


def hash_password(salt: str, password: str) -> str:
    payload = f"{salt}:APEX_AUTH_v1:{password}".encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def db_list_users():
    conn = get_db()
    try:
        rows = conn.execute("SELECT username FROM users ORDER BY last_login_at DESC LIMIT 8").fetchall()
        return [r["username"] for r in rows]
    finally:
        conn.close()


def db_register_user(username: str, password: str):
    u = (username or "").strip()
    if len(u) < 2:
        return {"ok": False, "error": "Driver Callsign must be at least 2 characters."}
    if len(password or "") < 4:
        return {"ok": False, "error": "Security Passkey must be at least 4 characters."}

    conn = get_db()
    try:
        existing = conn.execute("SELECT id FROM users WHERE username = ? COLLATE NOCASE", (u,)).fetchone()
        if existing:
            return {"ok": False, "error": f"Driver '{u}' is already registered. Click Sign In!"}

        salt = secrets.token_hex(16)
        phash = hash_password(salt, password)
        now = datetime.datetime.now(datetime.timezone.utc).isoformat()
        conn.execute("""
            INSERT INTO users (
                username, password_salt, password_hash, coins, owned_cars, owned_engines,
                selected_car, selected_engine, races_completed, wins, podiums, total_earned,
                relay_server, created_at, last_login_at
            ) VALUES (?, ?, ?, 500, 1, 1, 0, 0, 0, 0, 0, 500, '', ?, ?)
        """, (u, salt, phash, now, now))
        conn.commit()
        return {
            "ok": True,
            "username": u,
            "coins": 500,
            "ownedCars": 1,
            "ownedEngines": 1,
            "selectedCar": 0,
            "selectedEngine": 0,
            "races": 0,
            "wins": 0,
            "podiums": 0
        }
    finally:
        conn.close()


def db_login_user(username: str, password: str):
    u = (username or "").strip()
    conn = get_db()
    try:
        row = conn.execute("SELECT * FROM users WHERE username = ? COLLATE NOCASE", (u,)).fetchone()
        if not row:
            return {"ok": False, "error": f"Driver '{u}' not found. Click REGISTER (+500 COINS) to create it!"}
        actual = hash_password(row["password_salt"], password or "")
        if actual != row["password_hash"]:
            return {"ok": False, "error": f"Invalid passkey for driver '{row['username']}'."}

        now = datetime.datetime.now(datetime.timezone.utc).isoformat()
        conn.execute("UPDATE users SET last_login_at = ? WHERE id = ?", (now, row["id"]))
        conn.commit()
        return {
            "ok": True,
            "username": row["username"],
            "coins": row["coins"],
            "ownedCars": row["owned_cars"] | 1,
            "ownedEngines": row["owned_engines"] | 1,
            "selectedCar": row["selected_car"],
            "selectedEngine": row["selected_engine"],
            "races": row["races_completed"],
            "wins": row["wins"],
            "podiums": row["podiums"]
        }
    finally:
        conn.close()


def db_save_profile(data: dict):
    u = (data.get("username") or "").strip()
    p = data.get("password") or ""
    if not u:
        return {"ok": False}
    conn = get_db()
    try:
        row = conn.execute("SELECT * FROM users WHERE username = ? COLLATE NOCASE", (u,)).fetchone()
        if not row:
            return {"ok": False}
        if hash_password(row["password_salt"], p) != row["password_hash"]:
            return {"ok": False, "error": "Unauthorized"}

        coins = int(data.get("coins", row["coins"]))
        oc = int(data.get("ownedCars", row["owned_cars"])) | 1
        oe = int(data.get("ownedEngines", row["owned_engines"])) | 1
        sc = int(data.get("selectedCar", row["selected_car"]))
        se = int(data.get("selectedEngine", row["selected_engine"]))
        wins = int(data.get("wins", row["wins"]))
        podiums = int(data.get("podiums", row["podiums"]))
        races = int(data.get("races", row["races_completed"]))

        conn.execute("""
            UPDATE users SET
                coins = ?, owned_cars = ?, owned_engines = ?,
                selected_car = ?, selected_engine = ?,
                wins = ?, podiums = ?, races_completed = ?
            WHERE id = ?
        """, (coins, oc, oe, sc, se, wins, podiums, races, row["id"]))

        rr = data.get("raceRecord")
        if isinstance(rr, dict):
            now = datetime.datetime.now(datetime.timezone.utc).isoformat()
            conn.execute("""
                INSERT INTO race_history (username, position, coins_earned, car_id, engine_id, played_at)
                VALUES (?, ?, ?, ?, ?, ?)
            """, (row["username"], int(rr.get("position", 1)), int(rr.get("coinsEarned", 0)),
                  int(rr.get("carId", sc)), int(rr.get("engineId", se)), now))
        conn.commit()
        return {"ok": True}
    finally:
        conn.close()


# ============================================================================
#  2. RFC-6455 BROWSER WEBSOCKET FRAME HELPERS & MULTIPLAYER ROOM MANAGER
# ============================================================================
WS_ROOMS = {}        # roomId -> {"password": str, "theme": int, "tod": int, "members": {peerId: {"writer": w, "name": str, "car": int, "ready": bool}}}
WS_CLIENTS = set()   # all connected browser WS writers
WS_CLIENT_ROOM = {}  # writer -> (roomId, peerId)


def ws_encode_text(text: str) -> bytes:
    payload = text.encode("utf-8")
    n = len(payload)
    header = bytearray([0x81])
    if n < 126:
        header.append(n)
    elif n < 65536:
        header.append(126)
        header.extend(struct.pack("!H", n))
    else:
        header.append(127)
        header.extend(struct.pack("!Q", n))
    return bytes(header) + payload


async def ws_send_json(writer: asyncio.StreamWriter, obj: dict):
    try:
        frame = ws_encode_text(json.dumps(obj, separators=(",", ":")))
        writer.write(frame)
        await writer.drain()
    except Exception:
        pass


async def ws_read_frame(reader: asyncio.StreamReader):
    hdr = await reader.readexactly(2)
    b1, b2 = hdr[0], hdr[1]
    opcode = b1 & 0x0F
    masked = (b2 & 0x80) != 0
    length = b2 & 0x7F
    if length == 126:
        length = struct.unpack("!H", await reader.readexactly(2))[0]
    elif length == 127:
        length = struct.unpack("!Q", await reader.readexactly(8))[0]
    mask_key = await reader.readexactly(4) if masked else None
    data = bytearray(await reader.readexactly(length))
    if masked and mask_key:
        for i in range(length):
            data[i] ^= mask_key[i & 3]
    if opcode == 0x8:  # Close
        return None
    if opcode == 0x9:  # Ping
        return ("PING", bytes(data))
    if opcode == 0x1:  # Text
        return ("TEXT", data.decode("utf-8", errors="ignore"))
    return ("OTHER", bytes(data))


async def broadcast_ws_room_list():
    room_list = []
    for rid, r in list(WS_ROOMS.items()):
        room_list.append({
            "roomId": rid,
            "players": len(r["members"]),
            "max": 8,
            "protected": bool(r["password"])
        })
    # Also include any desktop TCP rooms
    for rid, r in list(ROOMS.items()):
        if rid not in WS_ROOMS:
            room_list.append({
                "roomId": rid,
                "players": r.get("beacon", {}).get("players", 1),
                "max": 8,
                "protected": bool(r.get("beacon", {}).get("protected", False))
            })
    msg = {"type": "WS_ROOM_LIST", "rooms": room_list}
    for w in list(WS_CLIENTS):
        await ws_send_json(w, msg)


async def cleanup_ws_client(writer: asyncio.StreamWriter):
    WS_CLIENTS.discard(writer)
    info = WS_CLIENT_ROOM.pop(writer, None)
    if not info:
        return
    rid, pid = info
    room = WS_ROOMS.get(rid)
    if not room:
        return
    member = room["members"].pop(pid, None)
    if not room["members"] or pid == 0:
        WS_ROOMS.pop(rid, None)
    else:
        for m in list(room["members"].values()):
            await ws_send_json(m["writer"], {"type": "WS_PEER_LEFT", "id": pid, "name": member["name"] if member else ""})
    await broadcast_ws_room_list()


async def handle_browser_websocket(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    WS_CLIENTS.add(writer)
    await broadcast_ws_room_list()
    try:
        while True:
            frame = await ws_read_frame(reader)
            if frame is None:
                break
            ftype, payload = frame
            if ftype == "PING":
                # Send Pong
                pong = bytearray([0x8A, len(payload)]) + payload
                writer.write(bytes(pong))
                await writer.drain()
                continue
            if ftype != "TEXT":
                continue

            try:
                obj = json.loads(payload)
            except Exception:
                continue

            mtype = obj.get("type", "")
            if mtype == "WS_SUBSCRIBE_LOBBY":
                await broadcast_ws_room_list()

            elif mtype == "WS_HOST_ROOM":
                await cleanup_ws_client(writer)
                WS_CLIENTS.add(writer)
                rid = str(obj.get("roomId", "APEX-101")).strip().upper()
                pwd = str(obj.get("password", "")).strip()
                name = str(obj.get("name", "HostDriver")).strip()
                car = int(obj.get("car", 0))
                WS_ROOMS[rid] = {
                    "password": pwd,
                    "theme": int(obj.get("theme", 0)),
                    "tod": int(obj.get("tod", 1)),
                    "members": {
                        0: {"writer": writer, "name": name, "car": car, "ready": True}
                    }
                }
                WS_CLIENT_ROOM[writer] = (rid, 0)
                await ws_send_json(writer, {"type": "WS_HOST_OK", "roomId": rid})
                await broadcast_ws_room_list()

            elif mtype == "WS_JOIN_ROOM":
                rid = str(obj.get("roomId", "APEX-101")).strip().upper()
                pwd = str(obj.get("password", "")).strip()
                name = str(obj.get("name", "Driver")).strip()
                car = int(obj.get("car", 0))
                room = WS_ROOMS.get(rid)
                if not room:
                    await ws_send_json(writer, {"type": "WS_AUTH_FAILED", "reason": f"Room '{rid}' does not exist"})
                    continue
                if room["password"] and room["password"] != pwd:
                    await ws_send_json(writer, {"type": "WS_AUTH_FAILED", "reason": "Incorrect Room Passkey"})
                    continue
                if len(room["members"]) >= 8:
                    await ws_send_json(writer, {"type": "WS_AUTH_FAILED", "reason": "Room is full (8/8 drivers)"})
                    continue

                pid = 1
                while pid in room["members"]:
                    pid += 1
                room["members"][pid] = {"writer": writer, "name": name, "car": car, "ready": False}
                WS_CLIENT_ROOM[writer] = (rid, pid)

                roster = [{"id": k, "name": v["name"], "car": v["car"], "ready": v["ready"]} for k, v in room["members"].items()]
                await ws_send_json(writer, {
                    "type": "WS_WELCOME",
                    "roomId": rid,
                    "assignedId": pid,
                    "theme": room["theme"],
                    "tod": room["tod"],
                    "roster": roster
                })
                for k, v in list(room["members"].items()):
                    if k != pid:
                        await ws_send_json(v["writer"], {"type": "WS_PEER_JOINED", "id": pid, "name": name, "car": car})
                await broadcast_ws_room_list()

            elif mtype == "WS_LEAVE_ROOM":
                await cleanup_ws_client(writer)
                WS_CLIENTS.add(writer)

            elif mtype in ("WS_READY", "WS_CAR_SYNC", "WS_TRACK_SYNC", "WS_START_RACE", "WS_TELEM"):
                info = WS_CLIENT_ROOM.get(writer)
                if not info:
                    continue
                rid, pid = info
                room = WS_ROOMS.get(rid)
                if not room:
                    continue
                if mtype == "WS_READY" and pid in room["members"]:
                    room["members"][pid]["ready"] = bool(obj.get("ready", False))
                elif mtype == "WS_CAR_SYNC" and pid in room["members"]:
                    room["members"][pid]["car"] = int(obj.get("car", 0))
                elif mtype == "WS_START_RACE" and pid == 0:
                    obj = {"type": "WS_COUNTDOWN", "duration": obj.get("duration", 3.5)}

                for k, v in list(room["members"].items()):
                    if mtype == "WS_TELEM" and k == pid:
                        continue
                    await ws_send_json(v["writer"], obj)
    except Exception:
        pass
    finally:
        await cleanup_ws_client(writer)


# ============================================================================
#  3. DESKTOP QT CLIENT RELAY & HTTP WEB SERVER ROUTER
# ============================================================================
ROOMS = {}           # roomId -> {"writer": StreamWriter, "beacon": dict}
WRITER_TO_ROOM = {}  # StreamWriter -> roomId
SUBSCRIBERS = set()  # set of StreamWriter
PENDING_TUNNELS = {} # tunnelId -> dict


def send_json(writer: asyncio.StreamWriter, obj: dict):
    try:
        data = (json.dumps(obj, separators=(",", ":")) + "\n").encode("utf-8")
        writer.write(data)
    except Exception:
        pass


async def pipe_stream(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    try:
        while True:
            chunk = await reader.read(8192)
            if not chunk:
                break
            writer.write(chunk)
            await writer.drain()
    except Exception:
        pass
    finally:
        try:
            writer.close()
        except Exception:
            pass


async def send_http_response(writer: asyncio.StreamWriter, status: str, content_type: str, body: bytes):
    hdr = (
        f"HTTP/1.1 {status}\r\n"
        f"Content-Type: {content_type}\r\n"
        f"Content-Length: {len(body)}\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).encode("utf-8")
    writer.write(hdr + body)
    await writer.drain()
    writer.close()


async def handle_http_or_ws_upgrade(first_line: bytes, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> str:
    """
    Returns:
      - "RAW_RELAY" if upgraded for Desktop Qt client (/relay)
      - "BROWSER_WS" if upgraded for Browser WebSocket (/ws)
      - "CLOSED" if normal HTTP request was answered and closed
    """
    parts = first_line.decode("utf-8", errors="ignore").split()
    method = parts[0].upper() if len(parts) > 0 else "GET"
    path = parts[1].split("?")[0] if len(parts) > 1 else "/"

    headers = {}
    while True:
        hline = await reader.readline()
        if not hline or hline in (b"\r\n", b"\n"):
            break
        decoded = hline.decode("utf-8", errors="ignore").strip()
        if ":" in decoded:
            k, v = decoded.split(":", 1)
            headers[k.strip().lower()] = v.strip()

    content_len = int(headers.get("content-length", "0") or "0")
    body_bytes = await reader.readexactly(content_len) if content_len > 0 else b""

    if headers.get("upgrade", "").lower() == "websocket":
        ws_key = headers.get("sec-websocket-key", "dGhlIHNhbXBsZSBub25jZQ==")
        accept_raw = hashlib.sha1((ws_key + WS_GUID).encode("utf-8")).digest()
        accept_b64 = base64.b64encode(accept_raw).decode("utf-8")
        resp = (
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Accept: {accept_b64}\r\n"
            "\r\n"
        )
        writer.write(resp.encode("utf-8"))
        await writer.drain()
        if path.startswith("/ws"):
            return "BROWSER_WS"
        return "RAW_RELAY"

    # REST API Endpoints for User Account SQLite DB
    if path == "/api/users" and method == "GET":
        res = {"ok": True, "users": db_list_users()}
        await send_http_response(writer, "200 OK", "application/json; charset=utf-8", json.dumps(res).encode("utf-8"))
        return "CLOSED"

    if path == "/api/register" and method == "POST":
        try:
            payload = json.loads(body_bytes.decode("utf-8", errors="ignore"))
        except Exception:
            payload = {}
        res = db_register_user(payload.get("username", ""), payload.get("password", ""))
        await send_http_response(writer, "200 OK", "application/json; charset=utf-8", json.dumps(res).encode("utf-8"))
        return "CLOSED"

    if path == "/api/login" and method == "POST":
        try:
            payload = json.loads(body_bytes.decode("utf-8", errors="ignore"))
        except Exception:
            payload = {}
        res = db_login_user(payload.get("username", ""), payload.get("password", ""))
        await send_http_response(writer, "200 OK", "application/json; charset=utf-8", json.dumps(res).encode("utf-8"))
        return "CLOSED"

    if path == "/api/save_profile" and method == "POST":
        try:
            payload = json.loads(body_bytes.decode("utf-8", errors="ignore"))
        except Exception:
            payload = {}
        res = db_save_profile(payload)
        await send_http_response(writer, "200 OK", "application/json; charset=utf-8", json.dumps(res).encode("utf-8"))
        return "CLOSED"

    if path.startswith("/assets/"):
        fname = os.path.basename(path[len("/assets/"):])
        if fname.endswith(".png"):
            fpath = os.path.join(ASSETS_DIR, fname)
            if os.path.isfile(fpath):
                with open(fpath, "rb") as f:
                    img_bytes = f.read()
                resp = (
                    f"HTTP/1.1 200 OK\r\n"
                    f"Content-Type: image/png\r\n"
                    f"Content-Length: {len(img_bytes)}\r\n"
                    f"Cache-Control: public, max-age=86400\r\n"
                    f"Connection: close\r\n\r\n"
                ).encode("utf-8") + img_bytes
                writer.write(resp)
                await writer.drain()
                writer.close()
                return "CLOSED"
        await send_http_response(writer, "404 Not Found", "text/plain; charset=utf-8", b"Asset Not Found")
        return "CLOSED"

    if path == "/status":
        active_rooms = list(set(list(ROOMS.keys()) + list(WS_ROOMS.keys())))
        body = json.dumps({
            "service": "Apex Velocity Global Multiplayer Relay Server",
            "status": "ONLINE",
            "activeRoomsCount": len(active_rooms),
            "activeRooms": active_rooms,
        }, indent=2).encode("utf-8")
        await send_http_response(writer, "200 OK", "application/json; charset=utf-8", body)
        return "CLOSED"

    # Default: GET / -> Serve the full playable 60 FPS APEX VELOCITY game in the browser!
    if os.path.exists(WEB_GAME_PATH):
        with open(WEB_GAME_PATH, "rb") as f:
            html_bytes = f.read()
        await send_http_response(writer, "200 OK", "text/html; charset=utf-8", html_bytes)
        return "CLOSED"

    await send_http_response(writer, "404 Not Found", "text/plain; charset=utf-8", b"Not Found")
    return "CLOSED"


async def handle_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    peer = writer.get_extra_info("peername")
    first_check_done = False
    try:
        while True:
            raw_line = await reader.readline()
            if not raw_line:
                break
            trimmed = raw_line.strip()
            if not trimmed:
                continue

            if not first_check_done:
                first_check_done = True
                if trimmed.startswith((b"GET ", b"HEAD ", b"POST ", b"OPTIONS ")):
                    mode = await handle_http_or_ws_upgrade(trimmed, reader, writer)
                    if mode == "CLOSED":
                        return
                    if mode == "BROWSER_WS":
                        await handle_browser_websocket(reader, writer)
                        return
                    continue

            try:
                obj = json.loads(trimmed.decode("utf-8", errors="ignore"))
            except Exception:
                continue

            msg_type = obj.get("type", "")

            if msg_type == "RELAY_SUBSCRIBE_LOBBY":
                SUBSCRIBERS.add(writer)
                for room in list(ROOMS.values()):
                    if room.get("beacon"):
                        send_json(writer, room["beacon"])
                await writer.drain()

            elif msg_type == "RELAY_REGISTER_HOST":
                room_id = str(obj.get("roomId", "APEX-101")).strip().upper()
                beacon = dict(obj)
                beacon["type"] = "APEX_BEACON"
                ROOMS[room_id] = {"writer": writer, "beacon": beacon}
                WRITER_TO_ROOM[writer] = room_id
                print(f"[RELAY] Host registered Room [{room_id}] from {peer}", flush=True)
                for sub in list(SUBSCRIBERS):
                    send_json(sub, beacon)
                await broadcast_ws_room_list()

            elif msg_type == "APEX_BEACON":
                room_id = str(obj.get("roomId", "")).strip().upper()
                if room_id in ROOMS and ROOMS[room_id]["writer"] is writer:
                    ROOMS[room_id]["beacon"] = obj
                    for sub in list(SUBSCRIBERS):
                        send_json(sub, obj)

            elif msg_type in ("AUTH", "HELLO"):
                room_id = str(obj.get("roomId", "APEX-101")).strip().upper()
                room = ROOMS.get(room_id)
                if not room or room["writer"].is_closing():
                    send_json(writer, {
                        "type": "AUTH_FAILED",
                        "code": "WRONG_ROOM",
                        "reason": f"Room '{room_id}' is not currently hosted on this server"
                    })
                    await writer.drain()
                    writer.close()
                    return

                tunnel_id = f"T-{int(time.time()*1000)}-{random.randint(1000,9999)}"
                evt = asyncio.Event()
                PENDING_TUNNELS[tunnel_id] = {
                    "roomId": room_id,
                    "client_reader": reader,
                    "client_writer": writer,
                    "initial_data": raw_line,
                    "ready_event": evt,
                }
                send_json(room["writer"], {
                    "type": "RELAY_NEW_CLIENT",
                    "roomId": room_id,
                    "tunnelId": tunnel_id,
                })
                await room["writer"].drain()
                try:
                    await asyncio.wait_for(evt.wait(), timeout=12.0)
                    while not writer.is_closing():
                        await asyncio.sleep(1.0)
                except asyncio.TimeoutError:
                    PENDING_TUNNELS.pop(tunnel_id, None)
                    writer.close()
                return

            elif msg_type == "RELAY_BIND_HOST":
                tunnel_id = str(obj.get("tunnelId", ""))
                pt = PENDING_TUNNELS.pop(tunnel_id, None)
                if not pt:
                    writer.close()
                    return
                c_reader = pt["client_reader"]
                c_writer = pt["client_writer"]
                pt["ready_event"].set()

                writer.write(pt["initial_data"])
                await writer.drain()
                print(f"[RELAY] Bridged client into Room [{pt['roomId']}] ({tunnel_id})", flush=True)

                await asyncio.gather(
                    pipe_stream(c_reader, writer),
                    pipe_stream(reader, c_writer),
                )
                return
    except Exception:
        pass
    finally:
        SUBSCRIBERS.discard(writer)
        if writer in WRITER_TO_ROOM:
            rid = WRITER_TO_ROOM.pop(writer)
            ROOMS.pop(rid, None)
            print(f"[RELAY] Room [{rid}] closed", flush=True)
            await broadcast_ws_room_list()
        try:
            writer.close()
        except Exception:
            pass


async def main():
    get_db().close()
    port = int(os.environ.get("PORT", sys.argv[1] if len(sys.argv) > 1 else "7779"))
    server = await asyncio.start_server(handle_client, "0.0.0.0", port)
    print("=================================================================", flush=True)
    print("  APEX VELOCITY - PLAYABLE CLOUD GAME + MULTIPLAYER RELAY SERVER", flush=True)
    print(f"  Listening on http://0.0.0.0:{port}  (Open in Browser to Play!)", flush=True)
    print("=================================================================", flush=True)
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
