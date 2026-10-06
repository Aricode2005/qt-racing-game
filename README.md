# 🏎️ Apex Velocity — First-Person Qt 6 Racing Simulator

An arcade racing simulator engineered from the ground up in **C++17 and Qt 6**, featuring a **100% pure raster graphics pipeline** (zero vector graphics, zero SVGs), physically modeled vehicle dynamics, a dynamic first-person cockpit, intelligent NPC traffic, real-time multi-mirror backward reflection, responsive steering, and a panoramic sideways glance engine.

Inspired by arcade classics such as ***Traffic Rider*** and ***Hill Climb Racing***, Apex Velocity delivers an intense cockpit driving experience across four distinct tracks and multiple times of day.

---

## 🌟 Highlights & Key Features

### 1. 100% Pure Raster Graphics Pipeline
- **Zero Vector Graphics & Zero SVGs**: Every visual element in the game—from the asphalt road textures and sky backdrops to the carbon fiber dashboard, rotating GT steering wheel, instrument dials, and vehicle sprites—is procedurally generated and rendered directly into pixel buffers (`QImage::Format_ARGB32_Premultiplied`) using direct pixel scanline manipulation and raster painting.
- **60 FPS Hardware-Smooth Rendering**: High-performance raster engine optimized for zero memory allocations during the render loop.

### 2. Noticeable & Realistic Dynamic Scenery Progression
- **4 Distinct Thematic Sectors Per Track**: Instead of repeating identical trees or barriers, each track now transitions through 4 distinct visual zones with unique roadside architecture and scenery:
  - **Pacific Coast Highway**:
    - *Sector 1: Coastal Palm Boulevard* (Waving palms, beach billboards, guardrails)
    - *Sector 2: Ocean Cliffs & Sea Arches* (Towering coastal boulders, eroded natural sea arches `SCENERY_ROCK_ARCH`)
    - *Sector 3: Coastal Wind Ridge* (Giant aerodynamic wind turbines `SCENERY_WIND_TURBINE` lining the ocean ridges)
    - *Sector 4: Marina Bay Expressway* (Overhead streetlights, modern illuminated interstate billboards)
  - **Alpine Summit Ridge**:
    - *Sector 1: Pine Valley Approach* (Dense evergreen alpine pines and heavy timber guardrails)
    - *Sector 2: Granite Gorge & Crags* (Massive rocky bluffs, natural rock tunnels and arches, 12% grade climbs)
    - *Sector 3: High Pass Wind Ridge* (High-altitude wind turbine farm piercing the mountain mist)
    - *Sector 4: Summit Lookout Freeway* (Highway billboards, streetlights, runaway truck ramps)
  - **Neo City Expressway**:
    - *Sector 1: Downtown Cyber Core* (Double neon highway streetlights, electronic corporate billboards)
    - *Sector 2: Industrial Harbor & Docks* (Wind turbines on harbor docks, shipping cranes, cargo lights)
    - *Sector 3: Skyway Elevated Flyover* (Elevated bridge viaduct with continuous aerodynamic safety barriers)
    - *Sector 4: Outer Ring Expressway* (Fast multi-lane freeway with exit gantries and night lighting)
  - **Red Rock Canyon**:
    - *Sector 1: Saguaro Desert Plains* (Giant saguaro cacti, rolling desert sands, route billboards)
    - *Sector 2: Monument Valley Sandstone Arches* (Massive eroded red sandstone arches and monolithic boulders)
    - *Sector 3: Mojave Wind Basin* (Expansive wind turbine array spanning the desert valley)
    - *Sector 4: Red Rock Canyon Gorge* (Narrow sandstone canyon gorge with red rock walls and guardrails)
- **Overhead Highway Gantries (`SCENERY_HIGHWAY_GANTRY`)**:
  - Full-width steel truss bridges spanning across all four highway lanes every ~160 segments.
  - Features green interstate directional exit signs ("WEST 101 EXPRESS", "EXIT 28B - MARINA & BAY"), white shield route markers, and electronic amber LED advisory speed displays ("LIMIT 120 KM/H").
  - Zooms realistically overhead with true pseudo-3D scaling as you speed underneath.
- **Dynamic Drifting Clouds & Parallax Sky**:
  - Multi-layer procedural clouds drifting dynamically across the sky, responsive to vehicle velocity and steering heading.
- **GPS Zone Announcements**:
  - Crossing into each new sector triggers a sleek GPS navigation banner on the cockpit infotainment display and HUD, rewarding bonus sector progress score.

### 3. Physically Correct NPC Orientation & Highway Layout
- **Dedicated Oncoming Traffic Lane (Far-Left Lane 0)**:
  - Lane 0 is separated from Lanes 1–3 by a **solid double-yellow highway line**, mimicking authentic highway engineering.
  - Oncoming vehicles travel in the opposite world direction toward the player at 110–145 km/h, creating intense *Traffic Rider*-style high-speed encounters.
- **100% Accurate Optical & Physical Orientation**:
  - **Forward Windshield View**:
    - Oncoming cars face *towards* the player, displaying bright halogen/LED projector headlights, front radiator grilles, and front bumper air dams.
    - Same-direction cars face *away* from the player, displaying full-width LED taillights, exhaust tips, diffusers, and brake lights.
  - **Rearview & Side Mirrors**:
    - Oncoming cars that have passed the player now drive away behind the car, showing their red taillights receding into the distance.
    - Following cars approaching behind the player show their headlights approaching in the mirror glass.
  - **Sideways POV Glance (<kbd>Q</kbd> / <kbd>E</kbd>)**:
    - Same-direction traffic travels forward across the window (facing forward toward the A-pillar).
    - Oncoming traffic flashes past in reverse direction across the window (facing backwards toward the B-pillar).
  - **Eliminated Crab-Walking Artifact**: Removed the 90° perpendicular side sprite from forward quarter windows so adjacent cars appear naturally in 3D road projection.

### 4. Realistic Crash Dynamics, Particle Explosions & Kinematics
- **3-Tier Impact Classification & Physical Kinematics**:
  - **Head-On Catastrophe with Oncoming Car ($\Delta v > 200$ km/h)**:
    - Severe structural damage (**-48% to -62% Health**), instantaneous velocity dump (speed clamped to $\le 30$ km/h).
    - Extreme cockpit shockwave: chassis pitch dive (`pitchG = -0.85`), violent roll jolt (`rollG = ±0.95`), steering wheel kickback kick ($\pm 35^\circ$), and lateral recoil impulse (`lateralRecoil = ±0.35`).
    - Impacted oncoming vehicle is knocked backwards and deflected across lanes (`lateralVelocity = ±2.6`), fishtailing with oscillating yaw skid angle (`skidAngle`), smoking tires, and automatic 4-way emergency hazard flashers.
    - Massive 48-particle explosive eruption: glowing golden friction sparks, sharp crystalline glass shards, and expanding smoke billows.
  - **High-Speed Rear-End Crash ($\Delta v = 110–200$ km/h)**:
    - Heavy damage (**-28% to -38% Health**), speed cut by ~58%.
    - Momentum transfer scaled by vehicle mass ratios:
      - Striking a massive **Semi-Truck (`VEHICLE_TRUCK_SEMI`, Mass = 2.8x)**: Severe rebound recoil against the player, minimal truck displacement.
      - Striking a light **Compact Car (`VEHICLE_COMPACT_BLUE`, Mass = 0.75x)**: Shoves the compact vehicle forward (+38 km/h boost) and knocks it sideways across lanes.
    - 36-particle spark and debris spray with steering wheel kickback.
  - **Glancing Sideswipe Scrape ($\Delta v \le 110$ km/h)**:
    - Light friction damage (**-9% to -15% Health**), speed retained at ~84%.
    - Both vehicles experience lateral deflection and yaw fishtailing before recovering.
    - 24 friction sparks and smoke puffs generated at the contact seam.
- **NPC Post-Crash Reactions & Tire Smoke**:
  - Impacted NPC vehicles don't merely slide; they exhibit authentic loss-of-control physics:
    - **Lateral Knockback Drift**: Velocity decays naturally across the road surface.
    - **Fishtailing Yaw Skid (`skidAngle`)**: Damped sinusoidal yaw oscillations simulate losing and regaining tire grip.
    - **Tire Skid Smoke Puffs**: Twin billows of raster tire smoke erupt from underneath the tires during high-angle yaw skids.
    - **4-Way Emergency Hazard Flashers**: Impacted cars immediately illuminate amber hazard blinkers until full physical recovery.
- **3D Particle Explosion System**:
  - **Golden Friction Sparks**: Radiant multi-stop radial gradient sparks bursting upward in parabolic arcs with gravity and friction decay.
  - **Crystalline Glass Shards**: Sharp multi-point faceted polygons that tumble and scatter across the field of view.
  - **Expanding Smoke Billows**: Multi-layered translucent smoke and radiator steam billows that expand outwards as they dissipate.
  - **Windshield Spray Projection**: Near-field crash debris erupts directly across the windshield glass and dashboard, providing intense first-person visceral immersion.
- **Total Wreck State**:
  - When Health reaches **0%**, the engine fails: throttle is disabled, hazard lights activate, and the vehicle coasts to a stop.
  - Centered flashing emergency banner: `"⚠️ TOTAL WRECK - VEHICLE DESTROYED ⚠️ PRESS [R] TO TOW & REPAIR"`.
  - Pressing <kbd>R</kbd> instantly tows the vehicle, repairs health back to 100%, and clears collision cooldowns.
- **Visual Damage & Radiator Steam Effects**:
  - **Full-Screen Red Damage Flash**: Pulsing red impact vignette on impacts.
  - **Radiator Steam Puffs**: When vehicle health falls below **40%**, white radiator steam billows realistically out from under the hood and rises across the windshield glass.
  - **Windshield Fractures**: High-impact collisions generate realistic spiderweb glass cracks.

### 5. Surrounding Bubble & Platooned NPC Spawning Engine
- **Surrounding Rear Traffic Bubble (Mirrors Populated from Frame 1)**:
  - 5 vehicles are spawned *behind* the player (-140 to -700 m) upon level start:
    - Oncoming cars receding into the distance in Lane 0.
    - High-speed sports cars approaching to overtake in Lane 1.
    - Cruising SUVs and commercial semi-trucks in Lanes 2 and 3.
  - Guarantees that the rearview mirror and left/right side mirrors are vibrant, active, and populated immediately from the first frame.
- **Adjacent Cruising Traffic**:
  - 2 vehicles spawned alongside the player (-30 to +45 m) pacing adjacent lanes for immediate immersion and highway presence.
- **Platooned Forward Traffic**:
  - Rather than spawning traffic on a rigid, predictable grid, vehicles spawn in **natural clusters / platoons** (groups of 2–3 cars traveling together, separated by 150–210 m gaps between clusters).
- **Anti-Block Corridor Guarantee (Zero Dead-Ends)**:
  - Traffic spawning algorithm enforces an anti-block rule: at any given longitudinal position, at least one forward lane is guaranteed clear and open, ensuring skill-based navigability is always maintained.
- **Speed & Lane Hierarchy**:
  - **Lane 0 (Oncoming)**: 110–145 km/h oncoming rush.
  - **Lane 1 (Fast / Passing)**: 140–170 km/h sports cars and high-speed sedans.
  - **Lane 2 (Cruising)**: 110–130 km/h SUVs and standard cruisers.
  - **Lane 3 (Slow / Heavy)**: 85–105 km/h semi-trucks and compacts.
- **Dynamic Lane Overtaking AI with Pre-Turn Blinkers**:
  - Faster NPC cars detect slower vehicles ahead in their lane using forward radar.
  - If an adjacent lane is clear, they engage their amber turn blinker for **0.6 seconds** as an advance warning before executing a smooth, continuous lateral lane change.
- **Seamless Continuous Recycling**:
  - When traffic falls 680 m behind the player, it is recycled smoothly ahead into distant platoons (950–2400 m) with randomized properties, eliminating popping artifacts.

### 5. Advanced Scoring & Near-Miss Mechanics
- **Distance Traveled**: Points accumulate continuously with speed (+250 pts / km).
- **High-Speed Bonus**: Cruising above 180 km/h awards +25 pts/sec; driving over 250 km/h awards +50 pts/sec.
- **Same-Direction Near Miss**: Overtaking within inches of a car at > 95 km/h awards **+180 pts × Combo**.
- **Oncoming Near Miss (Bravery Bonus)**: Passing an oncoming car in Lane 0 at high speed awards **+450 pts × Combo**!
- **Combo Multipliers**: Chaining consecutive close calls within 3.8 seconds builds up to **8x COMBO**!
- **Sector Progress Bonus**: Completing each zone awards +500 bonus points.
- **Crash Penalty**: Each collision resets the combo back to 1x and incurs a -300 point penalty.

### 6. Agile, Smooth & Easy Steering Dynamics
- **Instantaneous Turn-In**: Tuned steering interpolation speed (`15.0–18.0`) provides immediate, sharp turn-in response with zero input lag.
- **Effortless Lane Dodging**: Lateral maneuvering multiplier tuned to `4.0`, enabling rapid and nimble lane changes to dodge traffic with light taps of <kbd>A</kbd>/<kbd>D</kbd> or <kbd>←</kbd>/<kbd>→</kbd>.
- **Balanced Centrifugal Curve Push**: Curve drift force reduced by **55%** (to `0.32`), so carving through high-speed mountain bends and sharp curves feels natural, intuitive, and manageable without sliding off the track.
- **Soft Edge Stabilization Assist**: Gentle stabilization resistance near the road boundary prevents sudden spin-outs when grazing the curb.
- **Confident Auto-Centering**: High-speed auto-centering returns the car straight the moment steering keys are released.

### 7. Panoramic Sideways POV Glance Engine
- **Expansive Window Openings**: Lowered interior door sills and streamlined A-pillars provide an unobstructed panoramic window opening over 900 pixels wide.
- **Dedicated Sideways POV Engine (<kbd>Q</kbd> / <kbd>E</kbd>)**:
  - **<kbd>Q</kbd> (Look Left)**: Turns 75° out the driver's side window.
  - **<kbd>E</kbd> (Look Right)**: Turns 75° out the passenger's side window.
  - **Dynamic Sideways World**: Panoramic sky backdrop with horizontal parallax; asphalt road surface with speed lines and lane dashes rushing horizontally across the view; roadside guardrails with repeating vertical posts and passing scenery.
  - **Prominently Centered Adjacent Traffic**: Adjacent vehicles traveling alongside or being overtaken are rendered directly in the center of the window opening, large (380 px wide), crisp, with soft drop shadows, glowing volumetric brake halos when braking, and flashing amber repeaters when signaling.
  - **Natural Cockpit Framing**: Stitched leather door panels, power window switches, and GT wheel rim frame the window.
  - **Real-Time Exterior Mirrors**: Door-mounted exterior mirrors render live rearward road view.

### 8. Interactive Cyber-Infotainment Center Console & Procedural Synthwave Audio
- **Pixel-Accurate Touchscreen Bezel Integration**:
  - The center console touchscreen (`355 × 175` px) is mathematically anchored inside the dashboard's carbon-fiber console bezel between the dual air-conditioning vents across all window sizes and display aspect ratios.
- **Top Navigation App Tabs**:
  - <kbd>1</kbd> **`🌐 WEB` (Cyber-Net Web Browser)**
  - <kbd>2</kbd> **`🎵 MUSIC` (Synthwave Media Player)**
  - <kbd>3</kbd> **`🧭 GPS` (Full-Screen Navigation & Traffic Radar)**
  - <kbd>4</kbd> **`🚗 CAR` (Diagnostics, G-Force Dynamics & Health)**
- **Cyber-Net Web Browser Capabilities**:
  - **Live URL Address Bar**: Displays secure lock `🔒`, current URL (e.g., `https://speedr-highway.io/live`), and interactive refresh button `⟳`.
  - **Quick Bookmark Pills**: Clickable categories with custom neon badges:
    - `💬 FEED`: **Speedr Highway Driver Social Feed** featuring live driver chatter from `@HighwayPatrol`, `@TurboGT_Driver`, and `@TruckerDave`.
    - `📰 NEWS`: **World Speedway Pursuit Leaderboards & News** with live rankings, high scores, and real-time player telemetry.
    - `🌦️ WX`: **Highway Doppler Satellite Weather Radar** with rotating radar sweep beam, surface temperature, grip rating (98%), wind speed, and precipitation readouts.
    - `🏎️ SPEC`: **Supercar Specs Wiki** detailing the twin-turbo 4.0L V8, live boost pressure (PSI), coolant/oil temperatures, and tire pressures.
- **Procedural Synthwave Music Player**:
  - **4 Authentic 80s Synthwave Tracks**:
    1. *"MIDNIGHT HIGHWAY 101"* - Apex Synth Collective (Neon Horizon)
    2. *"TURBO OUTRUN 84"* - Kavinsky Soundworks (Redline Velocity)
    3. *"PACIFIC SUNSET CHILL"* - Sunset Dreamwave (Golden Memories)
    4. *"NEO MATRIX OVERDRIVE"* - Tokyo Cyber Knights (Expressway Run)
  - **Procedural PCM Synthesis**:
    - Synthesizes authentic analog synth sounds in real-time: pumping 16th-note sawtooth basslines, harmonic polyphonic lead synths, punchy kick drums, and crisp hi-hat percussion.
  - **Interactive Player UI**:
    - Procedural retro album art (neon sun, wireframe horizon grid).
    - Status badge (`▶ PLAYING` / `⏸ PAUSED`).
    - **16-Band Dynamic Equalizer Spectrum**: Jumping real-time audio visualizer frequency bars rendered with smooth multi-color gradients responding live to the beat.
    - **Scrubber Bar & Timecode**: Shows elapsed time and total duration; click anywhere on the bar to seek!
    - **Media Control Buttons**: Clickable `|◀ PREV`, `▶ PLAY / ⏸ PAUSE`, and `NEXT ▶|` buttons.
- **Full Touchscreen Mouse Interaction**:
  - Click any tab to switch apps immediately.
  - Click URL bar or bookmark pills to navigate web pages.
  - Click media control buttons or album art to toggle play/pause or switch tracks.
  - Click scrubber bar to jump to any point in the song.

### 9. Colossal Sky-Towering Billboards & Hoardings
- **High-Altitude Soaring Billboards (`SCENERY_BILLBOARD_1` & `SCENERY_BILLBOARD_2`)**:
  - Sprite height extended up to **1,150 pixels** with high-altitude roadway projection scaling (`sw = projScale * 2550.0`, reaching over **4,800 world units** of vertical elevation).
  - Soars dramatically into the sky, towering high above overhead highway gantries, palm trees, mountain slopes, and passing traffic.
  - **Structural Engineering**:
    - Poured reinforced concrete base footings with anchor bolts.
    - Massive dual structural steel I-beam columns (Billboard 1) and giant cylindrical monopole pylon with multi-tiered diagonal cantilever outriggers (Billboard 2).
    - Extended multi-tier lattice cross-truss X-bracing and maintenance ladder climbing rungs reaching the sky.
    - Continuous service catwalk with safety handrails and 4 overhead curved gooseneck spotlight fixtures.
  - **Dynamic Electronic Displays**:
    - Digital LED matrix screen with illuminated phosphor scanlines.
    - Glowing amber and cyan neon border frames with hazard chevron trims.
    - Electronic advisory speed camera radar box: `"SPEED RADAR: 295 KM/H • PHOTO ENFORCED"`.
    - `"★ APEX VELOCITY GT ★"`, `"⚡ TURBO CHARGED NITRO ⚡"`, `"COASTAL SUPER EXPRESSWAY"`.

- **Wide Panoramic Highway Advertising Hoardings (`SCENERY_HOARDING_NEON`)**:
  - Large widescreen highway advertising hoardings (`460 × 1000` px) spanning alongside the road.
  - Dual A-frame structural steel truss towers and 5 high-output floodlight fixtures.
  - High-impact commercial motorsport displays:
    - `"🏁 NITRO RACING RADIALS 🏁"`.
    - `"ULTIMATE 300+ KM/H ROAD GRIP • CASTROL EDGE APEX PARTNER"`.
    - Glowing cyan and magenta neon tube frames with animated hazard light blocks.

- **Roadside Utility & Telegraph Poles with Overhead Catenary Wires (`SCENERY_UTILITY_POLE`)**:
  - Weathered dark timber utility poles (`160 × 580` px) lining the highway shoulders across all tracks.
  - Upper and lower wooden crossarms with diagonal steel angle braces.
  - 6 glass/porcelain high-voltage insulators.
  - Pole-mounted cylindrical electrical distribution transformer can with cooling fins and high-voltage caution plate.
  - Galvanized steel guy wire with bright yellow reflective ground safety guard.
  - **Sideways POV (<kbd>Q</kbd> / <kbd>E</kbd>) Integration**: When glancing out the side windows, utility poles and high-altitude hoardings flash past behind the guardrail, complete with realistic sagging overhead transmission cables connecting adjacent poles across the panoramic view!

### 10. Photorealistic Multi-Mirror Real-Time Reflections
- **Center Rear-View Mirror ("Back Glass Mirror")**:
  - Top-center cockpit mounting stalk and slim chamfered titanium bezel.
  - Authentic interior vehicle framing: roof headliner trim, rear C-pillars, sports bucket seat headrests, and fine horizontal rear-window defroster wire lines.
  - Auto-dimming electrochromic glass tint with a soft emerald sensor LED dot.
  - True 3D perspective backward road projection showing receding traffic cars, guardrails, roadside poles, and panoramic sky.
- **Side Exterior Mirrors (Driver & Passenger)**:
  - Sculpted aerodynamic exterior mirror housings mounted outside the cockpit A-pillars.
  - **Player Car Flank Reflections**: Inner 18–20% of each mirror reflects the player car's own metallic red sports body panel, curved rear wheel arch flare, and door handle.
  - **Aspherical Blind-Spot Division Line**: Fine vertical dotted line separating standard curvature from the outer wide-angle blind-spot zone.
  - Real-time backward traffic reflections showing vehicles trailing in adjacent highway lanes.
  - Road edge lines, rumble strips, and mountain silhouettes reflected in real time.

### 11. Interactive Main Menu & Difficulty Dashboard (<kbd>M</kbd>)
- **Instant Menu Toggle (<kbd>M</kbd> / <kbd>T</kbd> / <kbd>Esc</kbd>)**:
  - Opens a sleek glassmorphic dashboard overlay while gracefully pausing track simulation and setting engine audio to a gentle idle purr.
- **3-Tier Racing Difficulty Modes**:
  - **`EASY / CASUAL`**: 18 ambient traffic cars, 0.45x collision damage, high tire grip, 1.0x score multiplier.
  - **`NORMAL / HIGHWAY`**: 24 cars, standard physical damage, realistic highway traffic flow, 1.5x score multiplier.
  - **`PRO / INSANE`**: 28 dense cars, aggressive speeds, 1.6x heavy collision damage, **2.5x Bravery Score multiplier**!
- **Environment & Time of Day Selector**:
  - Live buttons for `DAYLIGHT ☀️`, `SUNSET GLOW 🌅`, and `MIDNIGHT RUN 🌙`.
- **4 Track Cards with Previews & Star Ratings**:
  - Pacific Coast Highway, Alpine Summit Ridge, Neo City Expressway, and Red Rock Canyon.
  - Displays thumbnail previews, sector themes, Hills rating, Traffic rating, and Difficulty rating.
- **Quick Action Buttons**:
  - `RESUME RACE` (returns instantly to high-speed cockpit view).
  - `REPAIR VEHICLE` (restores 100% car health and clears crash counter).
  - `CYCLE LIGHTS` (toggles headlights and illuminated dashboard cluster).

---

## 🎮 Complete Controls Reference

| Key | Action | Description |
| :--- | :--- | :--- |
| <kbd>W</kbd> or <kbd>↑</kbd> | **Accelerate** | Opens throttle; builds RPM and turbo boost |
| <kbd>S</kbd> or <kbd>↓</kbd> | **Brake / Reverse** | Hydraulic braking; activates rear brake lights; reverses from standstill |
| <kbd>A</kbd> or <kbd>←</kbd> | **Steer Left** | Responsive turn-in; rotates steering wheel left |
| <kbd>D</kbd> or <kbd>→</kbd> | **Steer Right** | Responsive turn-in; rotates steering wheel right |
| <kbd>1</kbd> | **Dashboard: Web Browser** | Switches center console screen to Cyber-Net Web Browser |
| <kbd>2</kbd> | **Dashboard: Music Player** | Switches center console screen to Synthwave Music Player |
| <kbd>3</kbd> | **Dashboard: GPS Radar** | Switches center console screen to GPS Track Navigation & Traffic Radar |
| <kbd>4</kbd> | **Dashboard: Car Diagnostics** | Switches center console screen to Vehicle Health & G-Force Telemetry |
| <kbd>Tab</kbd> | **Cycle Dashboard App** | Cycles between Web Browser ➔ Music Player ➔ GPS Radar ➔ Car Diagnostics |
| <kbd>P</kbd> | **Play / Pause Music** | Toggles procedural synthwave music playback on the radio |
| <kbd>[</kbd> / <kbd>]</kbd> | **Prev / Next Song** | Skips between the 4 synthwave soundtrack stations |
| <kbd>I</kbd> | **Next Web Page** | Cycles browseable web pages (Social Feed, News, Weather, Specs) |
| <kbd>Mouse Click</kbd> | **Touchscreen Console** | Tap tabs, bookmarks, music player buttons, or seek bar on the dashboard |
| <kbd>M</kbd> or <kbd>T</kbd> | **Main Menu / Settings** | Opens full Dashboard: Select Difficulty (Easy/Normal/Pro), Track, Time of Day |
| <kbd>Q</kbd> | **Look Left** | Glances 75° out driver's window (panoramic side POV) |
| <kbd>E</kbd> | **Look Right** | Glances 75° out passenger's window (panoramic side POV) |
| <kbd>B</kbd> | **Look Back** | Rear camera perspective looking out rear windshield |
| <kbd>N</kbd> | **Cycle Time of Day** | Instant switch between Day ☀️, Sunset 🌅, and Night 🌙 |
| <kbd>X</kbd> | **Toggle Wipers** | Starts / stops dual animated windshield wipers |
| <kbd>Spacebar</kbd> | **Handbrake** | Locks rear tires for sharp slides and emergency stops |
| <kbd>H</kbd> | **Horn** | Dual-tone automotive horn |
| <kbd>R</kbd> | **Repair & Tow / Reset** | Fully restores 100% car health and resets vehicle position |
| <kbd>Esc</kbd> | **Pause / Main Menu** | Pauses simulation and opens the Main Menu |

---

## 🏗️ Architecture & Codebase Structure

```
qt-racing-game/
├── CMakeLists.txt           # Modern CMake configuration with Qt6 dependencies
├── run.sh                   # One-click launch shell script
├── main.cpp                 # Application entry point & Qt event loop
├── GameWindow.h / .cpp      # Main window, keyboard input handling, 60 FPS QTimer loop
├── GameEngine.h / .cpp      # Physics engine, steering dynamics, NPC AI radar, gear simulation
├── RasterRenderer.h / .cpp  # Pure raster rendering engine, 3-mirror buffers, sideways POV, HUD
├── RasterAssets.h / .cpp    # Procedural raster asset generator (vehicles, cockpit, textures)
├── Track.h / .cpp           # Procedural road segment generator, curves, hills, theme definitions
└── AudioSynthesizer.h / .cpp# Real-time PCM procedural audio synthesizer (engine, turbo, screech)
```

### Module Descriptions
- **[`GameEngine`](file:///Users/mukutchatterjee/Downloads/qt-racing-game/GameEngine.h)**:
  Manages the vehicle state (speed, gear, RPM, steer angle, road position `x`, `worldZ`), centrifugal curve physics, slope gravity forces, traffic spawning, forward radar detection, following distance, lane changes, near-miss scoring, and collision detection.
- **[`RasterRenderer`](file:///Users/mukutchatterjee/Downloads/qt-racing-game/RasterRenderer.h)**:
  Contains the pseudo-3D raster projection pipeline (`renderRoad`, `renderSceneryAndCars`). Manages mirror rendering passes for rearview and side mirrors, the dedicated `renderSidewaysPOV` for left/right window glances, the windshield HUD, animated wipers, and gauge instrument rendering.
- **[`RasterAssets`](file:///Users/mukutchatterjee/Downloads/qt-racing-game/RasterAssets.h)**:
  Generates and caches all procedural raster bitmaps:
  - `createVehicleSprite()`: High-detail front and rear views with LED lights, diffusers, exhausts, and license plates.
  - `createSideVehicleSprite()`: High-detail side profiles with multi-spoke alloy wheels, ventilated brake rotors, Brembo calipers, and character lines.
  - `generateCockpitAssets()`: Carbon fiber dashboard, A-pillars, door sills, and windshield tint.
  - `generateSidePOVs()`: Stitched leather door panels, power window switches, steering wheel rim, and exterior mirror mounts.
  - `generateSteeringWheel()`: 420×420 perforated leather GT wheel with red center stripe.
- **[`Track`](file:///Users/mukutchatterjee/Downloads/qt-racing-game/Track.h)**:
  Generates track geometries with smooth S-curves, hill crests, elevation dips, road width parameters, and roadside scenery types.
- **[`AudioSynthesizer`](file:///Users/mukutchatterjee/Downloads/qt-racing-game/AudioSynthesizer.h)**:
  Pure software audio synthesizer generating 44.1 kHz 16-bit PCM stereo audio streams on the fly.

---

## ⚙️ Requirements & Building

### Prerequisites
- **macOS** (Apple Silicon or Intel) or **Linux**
- **Qt 6.5+** (Core, Gui, Widgets, Multimedia, Network)
- **CMake 3.20+**
- **Ninja** or Make
- Modern C++17 compiler (AppleClang or GCC / Clang)

### Build Instructions
```bash
# Set Qt and Ninja in PATH (adjust paths if needed for your Qt installation):
export PATH="/Users/mukutchatterjee/Qt/Tools/Ninja:/Users/mukutchatterjee/Qt/Tools/CMake/CMake.app/Contents/bin:$PATH"

# Navigate to project folder:
cd ~/Downloads/qt-racing-game

# Configure with CMake:
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH="/Users/mukutchatterjee/Qt/6.11.1/macos"

# Build executable:
ninja -C build
```

### Running the Game
```bash
cd ~/Downloads/qt-racing-game
./run.sh
```
Or run the binary directly:
```bash
./build/ApexVelocity
```

---

## 🏆 Scoring & Near-Miss Mechanics
- **Distance Traveled**: Points accumulate continuously with speed.
- **Near Misses**: Passing within inches of an NPC car at speeds above 100 km/h triggers a **NEAR MISS** bonus (+100 pts) with an on-screen banner and audio cue.
- **Combos**: Chaining consecutive near misses within 4 seconds activates a multiplier (up to **5x COMBO**).
- **High-Speed Cruising**: Maintaining speeds above 220 km/h awards continuous bonus score multipliers.

---

## 🌄 Traffic Rider Inspired Scenery & Traffic System
1. **Front POV Road Visibility**:
   - Lowered horizon (`horizonY = 0.42 * H`) and camera elevation (`camHeight = 950`) provide an unobstructed forward windshield view spanning over 200 vertical pixels of asphalt, rumble strips, double-yellow center dividing lines, and dashed lane markers.
   - Front-to-back scanline clipping with continuous hill elevation gradients prevents any road disappearing artifacts.
2. **Long-Distance Front Traffic Visibility**:
   - Perspective projection (`projScale = scale * (W / 2.0)`) correctly scales oncoming cars (headlights facing forward in Lane 0) and same-direction vehicles (taillights in Lanes 1–3) from up to 8,000 units away.
   - 24 active traffic vehicles maintain a continuous stream of cars, trucks, SUVs, and sports cars ahead of the player.
3. **Approaching Mountain & Horizon Illusion**:
   - **Continuous Approach Scaling**: As forward distance is traveled and speed increases, procedural mountain ranges actively grow in height and width (`approachScale` expands up to +62%), creating the authentic sensation of racing toward towering mountain passes.
   - **Faceted 3D Peaks**: Multi-ridge peaks feature lit facets, shadowed facets, snow caps (*Alpine*), horizontal canyon strata (*Red Rock*), coastal promontories (*Sunset Coast*), and neon spires (*Neon Metropolis*).
   - **Atmospheric Road-Base Haze**: Soft haze gradients seamlessly anchor the mountain bases right at the road vanishing point.

---

## 📜 License
Developed with Qt 6 Open Source Edition. Free for educational, evaluation, and arcade racing demonstration purposes.
