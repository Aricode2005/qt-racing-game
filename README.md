# 🏎️ Apex Velocity — First-Person & Third-Person Qt 6 Motorsport Racing Simulator

An arcade and circuit motorsport racing simulator engineered from the ground up in **C++17 and Qt 6**, featuring a **100% pure software raster graphics pipeline** (`QPainter` / `QImage`, zero OpenGL/Vulkan, zero external 3D engines), switchable **First-Person Luxury GT Cockpit** and **Third-Person Supercar Chase** views, a **One-Way 4-Lane Motorsport Circuit** with **Level-Scaled Starting Grids (`6`, `10`, and `16` Participating Cars)**, **3D Start & Finish Line Archway Gantries**, a live **Side 2D Winding Circuit Track Map**, a **Live Race Standings Broadcast Tower**, and a **Finish-Time Final Standings Leaderboard**.

---

## 🌟 Highlights & Key Features

### 1. 100% Pure Software Raster Graphics Pipeline (60 FPS)
- **Zero External 3D Engines or Vector Assets**: Every visual element—from the multi-lane asphalt road surface, specular road sheen, wheel-rut wear tracks, 3D galvanized steel W-beam guardrails, and cat's-eye reflector studs to the luxury cockpit, 3D chase-cam supercar, and rival race cars—is procedurally generated and rendered directly into `QImage::Format_ARGB32_Premultiplied` pixel buffers.
- **60 FPS Precision Game Loop**: Driven by `Qt::PreciseTimer` (~16 ms frame pacing) with cached post-processing color grading, cinematic vignette, speed-based FOV boost, and atmospheric horizon distance fog.

---

### 2. Dual Camera System: First-Person Cockpit & Third-Person Chase View (<kbd>C</kbd> / <kbd>V</kbd>)
- **Interactive Top-HUD Camera Switch Button**: Click the **`🎥 3RD PERSON VIEW [C]` / `🎥 1ST PERSON VIEW [C]`** button in the top broadcast bar or press <kbd>C</kbd> / <kbd>V</kbd> at any time to switch seamlessly between camera perspectives.
- **First-Person Luxury GT Cockpit View**:
  - **Two-Tone Bespoke Interior**: Matte Obsidian Charcoal Alcantara upper anti-glare cowl paired with Warm Espresso / Saddle-Tan Nappa Leather lower console panels, French champagne-gold contrast stitching, and a brushed satin champagne-titanium trim spear.
  - **Sculpted GT Flat-Bottom Steering Wheel**: Perforated Saddle-Tan leather side grips (`9` and `3 o'clock`), forged-carbon upper/lower rim, 12-o'clock champagne-gold centering stripe, recessed **5-LED shift-light array**, CNC-machined titanium paddle shifters (`-` / `+`), and an obsidian/gold `GT` center crest.
  - **Liquid Gunmetal Exterior Hood**: Metallic gunmetal hood visible through the windshield with power-dome creases and twin carbon-fiber NACA heat-extractor louvers.
- **Third-Person Supercar Chase View (`renderThirdPersonPlayerCar` & `renderThirdPersonHUD`)**:
  - **Widebody APEX GT Rear Exterior**: Dynamic cornering body roll, suspension pitch/bounce, rotating 335-width rear racing slicks, multi-channel carbon-fiber underbody diffuser with F1 rain/brake strobe light, and quad blue-tipped titanium exhaust pipes that spit blue/orange afterburner flames at high RPM.
  - **Active Swan-Neck Rear Wing**: Automatically rises at high speed (`> 80 km/h`) and tilts forward as an aerodynamic airbrake under braking.
  - **Supercar Telemetry Pod**: Bottom-left glassmorphic cluster featuring a circular neon RPM tachometer ring, central gear indicator (`R` / `N` / `D1–D6`), digital `KM/H` readout, 10-stage LED shift-light bar, and turbo boost gauge (`BAR`).

---

### 3. Motorsport Circuit Environment, Starting Grid & 3D Start/Finish Gantries
- **One-Way 4-Lane Motorsport Race Track**: All participating cars race in the same direction across 4 high-speed lanes separated by one-way dashed white racing lane lines and flanked by red-and-white checkered FIA kerbs and steel W-beam guardrails.
- **Stationary Starting Grid Launch**: All participating rival race cars start stationary (`speed = 0`) alongside `[YOU] APEX GT` on painted white starting grid launch boxes and accelerate together when the race begins.
- **Checkered Start & Finish Lines + 3D Overhead Archway Gantries**:
  - 16-column black-and-white checkered Start Line and Finish Line asphalt strips spanning all 4 lanes.
  - 3D overhead **Start Line (`🏁 START LINE — X-CAR GRID 🏁`)** and **Finish Line (`🏁 FINISH LINE — GRAND PRIX 🏁`)** carbon-and-gold archway gantries with checkered support pylons and 5 FIA start/finish lights.
- **4 Extended Multi-Sector Grand Prix Circuits (Up to ~28.9 KM)**:
  1. **Pacific Coast Highway**: *Coastal Palm Boulevard* ➔ *Ocean Cliffs & Sea Arches* ➔ *Coastal Wind Ridge* ➔ *Marina Bay Expressway*
  2. **Alpine Summit Ridge**: *Pine Valley Approach* ➔ *Granite Gorge & Crags* ➔ *High Pass Wind Ridge* ➔ *Summit Lookout Freeway*
  3. **Neo City Expressway**: *Downtown Cyber Core* ➔ *Industrial Harbor & Docks* ➔ *Skyway Elevated Flyover* ➔ *Outer Ring Expressway*
  4. **Red Rock Canyon**: *Saguaro Desert Plains* ➔ *Monument Valley Sandstone Arches* ➔ *Mojave Wind Basin* ➔ *Red Rock Canyon Gorge*

---

### 4. Level-Scaled Participating Car Grids (`6`, `10`, and `16` Cars) & Overtaking Physics
- **Progressive Car Count & Difficulty by Level**:
  - **🟢 Level 1 (`DIFFICULTY_EASY` — Rookie)**: **6 Participating Cars** (`5` AI rivals + `[YOU] APEX GT`), starting at `P4 / 6`, `65%` circuit distance, relaxed rival cruise speeds (`106–160 km/h`).
  - **🟡 Level 2 (`DIFFICULTY_NORMAL` — Circuit)**: **10 Participating Cars** (`9` AI rivals + `[YOU] APEX GT`), starting at `P6 / 10`, `82%` circuit distance, competitive rival speeds (`128–176 km/h`), 1.5x score multiplier.
  - **🔴 Level 3 (`DIFFICULTY_PRO` — Championship)**: **16 Participating Cars** (`15` AI rivals + `[YOU] APEX GT`), starting at `P9 / 16`, `100%` full endurance circuit distance (`~28.9 KM`), fast & aggressive AI (`134–197 km/h` scaled further by track difficulty), 2.5x score multiplier.
- **Automatic Top-3 Level-Up Promotion**: Finishing a race in the **Top 3 (`P1–P3`)** and pressing <kbd>R</kbd> automatically promotes you to the next level (`6 Cars → 10 Cars → 16 Cars`).
- **Calibrated Visual Speed & Realistic Overtaking**:
  - Unified world-space speed formula (`((absSpd / 3.6) * 62.0 + 240.0 * qMin(1.0, absSpd / 22.0)) * dt`) used identically for both the player and AI rivals so driving at **40 km/h** visibly flows at **~4.65 road segments/sec** and relative speeds match **1:1**.
  - Tuned player twin-turbo V8 acceleration (`accelRate = 95.0`, `maxSpeed = 295.0 km/h`) so skilled driving lets you draft and overtake every rival car to fight for **P1 (1st Place)**, awarding **`+1,000 PTS`** per position gained.

---

### 5. Broadcast Motorsport UI/UX: Side 2D Circuit Track Map & Live Standings Tower
- **Right Side Panel — `🗺️ CIRCUIT TRACK MAP`**:
  - Samples actual track segment curvature (`seg.curve`) from `Start (Z = 0)` to `Finish (finishLineZ)` to render a **2D Winding Circuit Spline**.
  - Displays the completed route in glowing cyan, the `🏁 FIN` marker, **all rival car dots** (color-coded by rank), a pulsing **Player beacon (`YOU`)**, and live `DIST` / `REM TO FINISH` telemetry.
- **Left Side Panel — `🏆 LIVE STANDINGS`**:
  - Real-time motorsport broadcast standings tower showing position badges (`P1`–`P16`), driver names (`VORTEX GT`, `NITRO RS`, `COBRA V8`, `YOU • APEX GT`, etc.), and live time gaps to the leader (`LEADER`, `+0.3s`).
  - Automatically windows around `P1 (LEADER)` and the 7 positions surrounding `YOU • APEX GT` on 10-car and 16-car grids.
- **Floating Rival Position & Driver Tags**:
  - Every rival race car on the track displays a floating **`P1 • VORTEX GT`** badge perched above its roof in both 1st-person and 3rd-person views.
- **Finish-Time Final Race Standings Overlay**:
  - Records each car's exact `finishTime` (`MM:SS.ms`) upon crossing `finishLineZ` and presents a full ranked **Final Race Standings Leaderboard** when the race finishes.

---

### 6. Elevation-Aware Screen-Space Bumper-Touch Collision Detection & 3D Crash Physics
- **True Visual Bumper-Touch Hitbox (`checkCollisionsAndNearMisses`)**:
  - Projects each nearby rival car's exact screen-space tire contact point (`npcScreenY`, accounting for hill elevation differences `npcElev - baseElevation`) and only triggers a collision when the rival car visibly touches the player's front bumper contact zone on screen (`screenY ∈ [502, 572]` in 1st-person, `[558, 648]` in 3rd-person) and painted body widths overlap (`lateralDist < 0.165`).
  - Impacted cars are pushed forward/sideways in front of the player's hood (`player.worldZ + 210.0`) so the colliding car remains clearly visible during impact and fishtail recovery.
- **3D Particle Explosion & Post-Crash Kinematics**:
  - Generates golden friction sparks, tumbling glass shards, expanding tire smoke puffs, chassis pitch/roll shockwaves, and emergency 4-way hazard flashers.

---

### 7. 4 Selectable Supercars, ADAS Assist, 3D AR Trajectory Rails & On-Spot Repair (Unified Contributor Features)
- **4 Selectable Supercar Models (<kbd>F1</kbd>–<kbd>F4</kbd> or Main Menu <kbd>M</kbd>)**:
  1. **`APEX GT-R`** (*Balanced Twin-Turbo Coupe* — `300 km/h`, `100%` Armor, `100%` Grip, Liquid Gunmetal Livery)
  2. **`VORTEX V12`** (*Hypercar Interceptor* — `345 km/h`, `124%` Accel, `85%` Armor, Rosso Corsa Crimson Livery)
  3. **`DRIFT SPEC-R`** (*Touge Track Special* — `288 km/h`, `128%` Grip, `95%` Armor, Solar Amber-Gold Livery)
  4. **`TITAN ENFORCER`** (*Armored Endurance GT* — `294 km/h`, `155%` Armor, `104%` Grip, Tactical Emerald Livery)
- **Toggleable ADAS Apex & Lane-Keeping Assist (<kbd>G</kbd>)**:
  - **`ADAS: ON`**: Compensates `55%` of centrifugal cornering force and stabilizes lane position.
  - **`ADAS: OFF`**: `100%` manual centrifugal cornering realism.
- **3D AR Bumper Trajectory Rails & Slipstream Drafting (<kbd>F</kbd>)**:
  - Projects the player car's exact bumper width onto the 3D road ahead (`Green` = Clear Corridor, `Cyan` = Slipstream Drafting `+22 km/h/s`, `Amber` = Tight Squeeze, `Red` = Obstacle in Path).
- **Full-Depth 3D Z-Buffer & Polygon Shading Toggle (<kbd>Z</kbd> / <kbd>L</kbd>)** and **On-Spot Field & Checkpoint Repair (<kbd>Y</kbd> for `+50%` Health, `+25%` on Sector Checkpoints)**.

---

### 8. Multi-Mirror Reflections, Sideways POV (<kbd>Q</kbd>/<kbd>E</kbd>), Infotainment & Synthwave Audio
- **3 Real-Time Mirrors**: Top-center rearview mirror (visible in both 1st and 3rd person) plus left and right aerodynamic side wing mirrors reflecting trailing race cars behind you.
- **75° Sideways Window Glance (<kbd>Q</kbd> / <kbd>E</kbd>)**: Look out the driver or passenger window to inspect rival cars racing door-to-door alongside you.
- **Interactive Touchscreen Infotainment Console (<kbd>1</kbd>–<kbd>4</kbd> / Mouse Click)**:
  - `🌐 WEB`: Cyber-Net Browser (`FEED`, `NEWS`, `WX` Doppler Radar, `SPEC` V8 Telemetry).
  - `🎵 MUSIC`: Real-time 44.1 kHz procedural PCM Synthwave Radio with 4 stations and a 16-band live spectrum analyzer.
  - `🧭 GPS`: In-dash track sector radar.
  - `🚗 CAR`: Live G-force meter, tire temperatures, and chassis integrity diagnostics.
- **Interactive On-Screen Pedals & Mouse Driving**: Hold **Left Mouse Button** (or click the on-screen **GAS** pedal) to accelerate and **Right Mouse Button** (or **BRAKE** pedal) to brake.

---

## 🎮 Complete Controls Reference

| Key / Input | Action | Description |
| :--- | :--- | :--- |
| <kbd>W</kbd> / <kbd>↑</kbd> / **Left-Click** | **Accelerate (Gas)** | Opens throttle; launches from starting grid; builds RPM & boost |
| <kbd>S</kbd> / <kbd>↓</kbd> / **Right-Click** | **Brake / Reverse** | Hydraulic braking; tilts active rear wing airbrake |
| <kbd>A</kbd> / <kbd>←</kbd> | **Steer Left** | Responsive turn-in left across the 4 racing lanes |
| <kbd>D</kbd> / <kbd>→</kbd> | **Steer Right** | Responsive turn-in right across the 4 racing lanes |
| <kbd>C</kbd> / <kbd>V</kbd> / **HUD Button** | **Switch Camera View** | Toggles between **1st-Person Cockpit** and **3rd-Person Chase** view |
| <kbd>F1</kbd> / <kbd>F2</kbd> / <kbd>F3</kbd> / <kbd>F4</kbd> | **Select Supercar** | Switches between `APEX GT-R`, `VORTEX V12`, `DRIFT SPEC-R`, and `TITAN ENFORCER` |
| <kbd>G</kbd> | **Toggle ADAS Assist** | Switches between `ADAS ON` (Apex/Lane Assist) and `ADAS OFF` (Manual Realism) |
| <kbd>F</kbd> | **Toggle 3D AR Rails** | Toggles projected 3D AR bumper trajectory rails on the road |
| <kbd>Z</kbd> / <kbd>L</kbd> | **Toggle 3D Z-Buffer** | Toggles Full-Depth 3D Z-Buffer & Polygon Shading mode |
| <kbd>Y</kbd> | **On-Spot Field Repair** | Restores `+50%` Car Health on the spot without losing race progress |
| <kbd>Spacebar</kbd> | **Handbrake / Drift** | Initiates rear-wheel drift with billowing tire smoke |
| <kbd>R</kbd> | **Restart Race / Level Up** | Resets to Starting Grid (auto-promotes to next level after Top-3 finish) |
| <kbd>M</kbd> / <kbd>T</kbd> / <kbd>Esc</kbd> | **Main Menu** | Opens Command Center: Select Level (6/10/16 Cars), Circuit, Time of Day |
| <kbd>Q</kbd> / <kbd>E</kbd> | **Look Left / Right** | Glances 75° out driver or passenger side window |
| <kbd>B</kbd> | **Look Back** | Rear camera perspective looking out the back |
| <kbd>N</kbd> | **Cycle Time of Day** | Switches between **Daylight ☀️**, **Sunset Glow 🌅**, and **Midnight Run 🌙** |
| <kbd>X</kbd> | **Toggle Wipers** | Starts / stops dual animated windshield wipers |
| <kbd>H</kbd> | **Horn** | Dual-tone automotive horn |
| <kbd>1</kbd> / <kbd>2</kbd> / <kbd>3</kbd> / <kbd>4</kbd> / <kbd>Tab</kbd> | **Infotainment Apps** | Switches between `WEB`, `MUSIC`, `GPS`, and `CAR` console tabs |
| <kbd>P</kbd> / <kbd>[</kbd> / <kbd>]</kbd> | **Music Controls** | Play/Pause and skip between the 4 procedural synthwave tracks |

---

## 🏗️ Architecture & Codebase Structure

```text
qt-racing-game/
├── CMakeLists.txt            # CMake build configuration (Qt6 Core, Gui, Widgets, Multimedia, Network)
├── main.cpp                  # Application entry point
├── GameWindow.h / .cpp       # Main window, keyboard/mouse input, 60 FPS PreciseTimer, Main Menu overlay
├── GameEngine.h / .cpp       # Physics, 6/10/16-car starting grid, AI pack pacing, standings & collisions
├── RasterRenderer.h / .cpp   # Pure software raster renderer (1st/3rd person, 3D gantries, side track map, HUD)
├── RasterAssets.h / .cpp     # Procedural raster bitmap generator (cockpit, steering wheel, vehicles, scenery)
├── Track.h / .cpp            # Procedural multi-sector circuit generator (curves, hills, 4 track themes)
├── AudioSynthesizer.h / .cpp # Real-time 44.1 kHz PCM procedural engine & synthwave music synthesizer
└── capture_screenshots.cpp   # Automated offscreen/windowed verification screenshot generator
```

---

## ⚙️ Requirements & Building

### Prerequisites
- **Windows** (MinGW 64-bit / MSVC), **macOS**, or **Linux**
- **Qt 6.5+** (`Core`, `Gui`, `Widgets`, `Multimedia`, `Network`)
- **CMake 3.16+**
- **C++17** compatible compiler

### Build & Run (Windows PowerShell — Qt 6 MinGW)
```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build build --target ApexVelocity --parallel 4
.\build\ApexVelocity.exe
```

### Build & Run (macOS / Linux)
```bash
cmake -B build -G Ninja
cmake --build build --target ApexVelocity --parallel 4
./build/ApexVelocity
```

---

## 📜 License
Developed with Qt 6 Open Source Edition. Free for educational, evaluation, and motorsport simulation demonstration purposes.
