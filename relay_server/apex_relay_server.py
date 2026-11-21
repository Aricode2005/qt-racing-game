#!/usr/bin/env python3
"""
APEX VELOCITY - Cloud (Render / Railway / VPS) & Cross-LAN Multiplayer Relay Server
===================================================================================
Supports BOTH:
  1. Render.com / Cloudflare / Nginx HTTP Health Checks (GET / -> 200 OK status page)
     & HTTP/1.1 WebSocket Upgrade (101 Switching Protocols -> raw TCP JSON stream)
     so you can deploy directly as a free Render Web Service (https://your-app.onrender.com)!
  2. Direct Raw TCP connections (e.g. 127.0.0.1:7779 or VPS IP:7779).
"""

import asyncio
import base64
import hashlib
import json
import os
import random
import sys
import time

ROOMS = {}           # roomId -> {"writer": StreamWriter, "beacon": dict}
WRITER_TO_ROOM = {}  # StreamWriter -> roomId
SUBSCRIBERS = set()  # set of StreamWriter
PENDING_TUNNELS = {} # tunnelId -> dict

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


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


async def handle_http_or_ws_upgrade(first_line: bytes, reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> bool:
    """
    Handles an incoming HTTP request (e.g. from Render health check or game client WS upgrade).
    Returns True if upgraded to a persistent game stream (101 Switching Protocols),
    or False if it was a normal HTTP GET request (200 OK sent and connection closed).
    """
    headers = {}
    while True:
        hline = await reader.readline()
        if not hline or hline in (b"\r\n", b"\n"):
            break
        decoded = hline.decode("utf-8", errors="ignore").strip()
        if ":" in decoded:
            k, v = decoded.split(":", 1)
            headers[k.strip().lower()] = v.strip()

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
        return True

    # Normal HTTP request (Render Health Check or Browser Visit)
    active_rooms = list(ROOMS.keys())
    body = json.dumps({
        "service": "Apex Velocity Global Multiplayer Relay Server",
        "status": "ONLINE",
        "activeRoomsCount": len(active_rooms),
        "activeRooms": active_rooms,
    }, indent=2).encode("utf-8")

    http_resp = (
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        f"Content-Length: {len(body)}\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).encode("utf-8") + body
    writer.write(http_resp)
    await writer.drain()
    writer.close()
    return False


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
                if trimmed.startswith((b"GET ", b"HEAD ", b"POST ")):
                    upgraded = await handle_http_or_ws_upgrade(trimmed, reader, writer)
                    if not upgraded:
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
        try:
            writer.close()
        except Exception:
            pass


async def main():
    port = int(os.environ.get("PORT", sys.argv[1] if len(sys.argv) > 1 else "7779"))
    server = await asyncio.start_server(handle_client, "0.0.0.0", port)
    print("=================================================================", flush=True)
    print("  APEX VELOCITY - GLOBAL ONLINE MULTIPLAYER RELAY SERVER", flush=True)
    print(f"  Listening on 0.0.0.0:{port} (HTTP/WS Upgrade + Raw TCP ready)", flush=True)
    print("=================================================================", flush=True)
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
