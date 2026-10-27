# Unified Project Documentation — Sayanaditya Contribution & Feature Guide

This document details **what was changed/integrated** from Sayanaditya's repository (`https://github.com/Sayanaditya/racing-game`), **what was preserved (unchanged)** from the core **Apex Velocity** codebase, and **how to access and use every feature** during gameplay.

---

## 1. What Was Changed & Integrated (From Sayanaditya's Contribution)

All major gameplay, rendering, and driver-assist innovations from Sayanaditya's project have been integrated into the unified **Apex Velocity** engine (`GameEngine.h`, `GameEngine.cpp`, `RasterRenderer.cpp`, `GameWindow.cpp`, `Track.cpp`):

| # | Sayanaditya Feature | What Was Changed in the Codebase |
| :- | :--- | :--- |
| **1** | **Full-Depth 3D Mesh & Slope Lighting Pipeline** | Added toggleable 3D Volumetric Low-Poly Prism & Cyan Wireframe Cages around rival cars, 3D extruded rear aero wings, 3D perspective road depth grid lines, 3D road banking/camber (`roadBank`), and directional Lambertian `(N·L)` slope lighting on hills in `RasterRenderer.cpp`. |
| **2** | **Active ADAS Autopilot & Obstacle-Avoidance Assist** | Added forward ultrasonic lane radar (`adasRadarDistM`) scanning all 4 lanes ahead, automatic lane-change steering toward the clearest passing lane (`adasBestLaneX`), emergency collision-mitigation braking, shoulder lane-keeping, and a live windshield **ADAS Autopilot HUD Banner** in `GameEngine.cpp` and `RasterRenderer.cpp`. |
| **3** | **3D AR Passing-Gap Corridor & Trajectory Rails** | Added perspective-projected 3D neon bumper trajectory rails with forward chevrons (`n = 6..56`) plus a glowing green 3D **`▲ AR PASSING GAP ▲`** target gate and carpet in the safest overtaking lane ahead in `RasterRenderer.cpp`. |
| **4** | **Variable Retro Pixelation (`1px`–`6px`) & CRT Scanlines** | Added real-time nearest-neighbor software framebuffer downsampling (`1px`, `2px`, `3px`, `4px`, `6px`) in `GameWindow::paintEvent` and a retro CRT phosphor scanline filter in `RasterRenderer::render`. |
| **5** | **4 Selectable Supercars with Distinct Physics** | Added a 4-car roster (`APEX GT-R`, `VORTEX V12`, `DRIFT SPEC-R`, `TITAN ENFORCER`) with custom top speed, acceleration, cornering grip, armor durability, and 3rd-person exterior paint liveries in `GameEngine::setSelectedCar`. |
| **6** | **On-Spot Field Repair & Checkpoint Pit Repair** | Added instant on-spot field repair (`+50%` hull integrity restored via `GameEngine::fieldRepair`) and automatic `+25%` structural repair when crossing sector checkpoint archways. |
| **7** | **Smooth Uneven Road Hills, Camber & Crest Callouts** | Added `Track::addRollingHills`, continuous sub-segment elevation/curvature interpolation, and live **Rally Crest & Slope Callouts** (`⛰ CREST AHEAD! • SLOPE +4.2°`) in the top-center HUD. |

---

## 2. What Was NOT Changed (Preserved Core Apex Velocity Features)

To keep the rich visual polish and motorsport environment of **Apex Velocity**, the following core systems were **preserved intact** rather than overwritten by Sayanaditya's standalone low-res `ArgonRacer` framework:

1. **Dual Camera System (1st-Person Cockpit & 3rd-Person Chase View)**:
   - Full high-resolution `1280×720` 1st-person cockpit interior (rotating steering wheel, analog/digital instrument cluster, animated dual windshield wipers, foot pedals, and 3 real-time rear/side mirrors) **plus** the 3rd-person chase camera with dynamic chassis roll, glowing tail-lights, dual exhaust flames, and camera switch button.
2. **Interactive 4-App Touchscreen Infotainment Console**:
   - The clickable dashboard touchscreen with **Web Browser (`WEB`)**, **4-Track Synthwave Music Player & 16-Band Visualizer (`MUSIC`)**, **Doppler Weather Radar (`GPS`)**, and **V8 G-Force Telemetry (`CAR`)** remains completely intact.
3. **Motorsport Starting Grid, 3D Start/Finish Archways & Level-Scaled Grids (`6`, `10`, `16` Cars)**:
   - One-way 4-lane Grand Prix racing environment, painted starting grid launch boxes, 3D overhead Start & Finish Line gantries, and level-scaled AI grids (**Level 1**: `6` cars, **Level 2**: `10` cars, **Level 3**: `16` cars) with automatic Top-3 level promotion.
4. **Professional Side Panels (Left Live Standings Tower & Right 2D Circuit Track Map)**:
   - Real-time motorsport leaderboard tower (`P1`–`P16` with live time gaps) on the left and the 2D winding GPS circuit map with live rival/player dots and distance telemetry on the right.
5. **4 Multi-Sector Scenic Circuits & 3 Times of Day**:
   - **Pacific Coast Highway**, **Alpine Summit Ridge**, **Neo City Expressway**, and **Red Rock Canyon** across **Daylight ☀️**, **Sunset Glow 🌅**, and **Midnight Run 🌙** with procedural 44.1 kHz engine/music audio synthesis (`AudioSynthesizer.cpp`).

---

## 3. How to Avail & Use All Features In-Game

### A. Top-Center Interactive Feature Bar (`1`, `2`, `3`, `4`)
During a race, look at the **4 glowing buttons right below the top bar** (`y = 50`). You can **click any of the 4 buttons with your mouse** OR press keys **<kbd>1</kbd>, <kbd>2</kbd>, <kbd>3</kbd>, <kbd>4</kbd>**:

| Feature # | How to Trigger | What Happens & Where to Look |
| :---: | :--- | :--- |
| **Feature 1: 3D Mesh & Depth Pipeline** | Press **<kbd>1</kbd>** (or **<kbd>Z</kbd>** / **<kbd>L</kbd>**) or click **`[1] 3D MESH`** on the top HUD | • Draws **3D Cyan Volumetric Wireframe & Shaded Prism Cages** around all rival cars (3D cabin prism, 3D lower chassis box, 3D rear wing).<br>• Draws **3D Perspective Depth Grid Lines** across the road segments and enables 3D road banking & Lambertian slope lighting. |
| **Feature 2: ADAS Autopilot Assist** | Press **<kbd>2</kbd>** (or **<kbd>G</kbd>**) or click **`[2] ADAS`** on the top HUD | • Shows the **Windshield ADAS Autopilot Banner** (`🛡️ ADAS RADAR: 24m • ◀◀ AUTO-STEERING LEFT TO CLEAR GAP`).<br>• Hold **<kbd>W</kbd>** (gas) without touching steering: when approaching slower cars ahead, ADAS **automatically turns your steering wheel and changes lanes** into the clearest gap and prevents off-road drift. |
| **Feature 3: 3D AR Passing-Gap & Rails** | Press **<kbd>3</kbd>** (or **<kbd>F</kbd>**) or click **`[3] AR GAP`** on the top HUD | • Projects bright **3D Neon Trajectory Rails & Forward Chevrons** far up the road (`n = 6..56`, visible above the dashboard in 1st-person and in 3rd-person).<br>• Projects a glowing green **`▲ AR PASSING GAP ▲`** 3D gate and lane carpet in the safest overtaking lane ahead. |
| **Feature 4: Variable Pixel Size (`1px`–`6px`)** | Press **<kbd>4</kbd>** (or **<kbd>J</kbd>**) or click **`[4] PIXEL`** on the top HUD *(or press <kbd>5</kbd>–<kbd>9</kbd>)* | • Cycles the internal software rasterizer block size: **`1px` (1280×720 HD) → `2px` (640×360) → `3px` (426×240) → `4px` (320×180) → `6px` (213×120 Retro Pixel)**.<br>• Press **<kbd>K</kbd>** to toggle **Retro CRT Phosphor Scanlines** on/off. |

---

### B. Additional Integrated Controls (Supercars, Repair, Camera & Menu)

| Action | Keyboard / Mouse Control | Description |
| :--- | :--- | :--- |
| **Select Supercar (`1` of `4`)** | Press **<kbd>F1</kbd>, <kbd>F2</kbd>, <kbd>F3</kbd>, <kbd>F4</kbd>** *(or click the 4 car cards in the Main Menu <kbd>M</kbd>)* | • **<kbd>F1</kbd> `APEX GT-R`**: `300 km/h` Balanced Track Coupé (Crimson)<br>• **<kbd>F2</kbd> `VORTEX V12`**: `345 km/h` Top-Speed Hypercar (Hyper Cyan)<br>• **<kbd>F3</kbd> `DRIFT SPEC-R`**: `288 km/h` High-Grip Cornering Tuner (`1.28×` agility, Gold)<br>• **<kbd>F4</kbd> `TITAN ENFORCER`**: `294 km/h` Heavy Armored GT (`155%` armor durability, Emerald) |
| **On-Spot Field Repair** | Press **<kbd>Y</kbd>** | Immediately restores **`+50%` Vehicle Health** on the spot during a race without resetting your position. |
| **Switch 1st / 3rd Person View** | Press **<kbd>C</kbd>** (or **<kbd>V</kbd>**) or click **`🎥 3RD PERSON`** on the top bar | Toggles between **1st-Person Cockpit View** and **3rd-Person Exterior Chase Camera**. |
| **Drive & Steer** | **<kbd>W</kbd> / <kbd>S</kbd>** (or **<kbd>↑</kbd> / <kbd>↓</kbd>**) & **<kbd>A</kbd> / <kbd>D</kbd>** (or **<kbd>←</kbd> / <kbd>→</kbd>**) | Accelerate, brake/reverse, and steer across the 4 highway lanes (or click the on-screen Gas/Brake pedals). |
| **Select Level (`6`, `10`, `16` Cars)** | Press **<kbd>M</kbd>** (or **<kbd>Esc</kbd>**) → Click **Level 1 / Level 2 / Level 3** | • **Level 1 (Rookie)**: `6` Participating Cars<br>• **Level 2 (Pro Racer)**: `10` Participating Cars<br>• **Level 3 (Hyper Speed)**: `16` Participating Cars |
| **Restart / Auto-Level Promotion** | Press **<kbd>R</kbd>** | Resets to the starting grid (automatically promotes you to the next level if you finished in **P1–P3**). |

---

### C. Custom Rooms Multiplayer (ID & Password Private Lobbies)

Apex Velocity includes a complete **Custom Rooms Multiplayer Architecture** (`NetworkManager.h`, `NetworkManager.cpp`, `GameWindow.cpp`) that replaces unauthenticated LAN broadcast matchmaking with private rooms:

1. **Host Flow**:
   - Press **<kbd>O</kbd>** (or click `🌐 CUSTOM ROOMS` in the Main Menu).
   - Enter a custom **Room ID** (e.g. `APEX-101`) or click **`[🎲 RANDOM]`** to generate a code.
   - Enter an optional **Room Password** (leave blank for open access).
   - Click **`[🚀 CREATE & HOST CUSTOM ROOM]`**.
   - Your local LAN IP (`net.getLocalIpAddress()`) and Room ID are displayed to share with friends.
2. **Client Flow**:
   - Enter the Host's IP address (or click **`[💻 127.0.0.1]`** for local two-instance testing).
   - Enter the matching **Room ID** (case-insensitive) and **Password**.
   - Click **`[🏁 ENTER & JOIN ROOM]`**.
   - If Room ID or Password is invalid, the server sends `AUTH_FAILED` with the exact reason and displays a red on-screen warning banner.
3. **8-Racer Room Lobby**:
   - Once connected, both Host and Clients transition to the active Room Lobby.
   - Click **`[👍 TOGGLE READY]`** to signal readiness.
   - Host clicks **`[🏁 LAUNCH RACE]`** to trigger synchronized countdown and start the Grand Prix with 60 Hz peer telemetry interpolation.

