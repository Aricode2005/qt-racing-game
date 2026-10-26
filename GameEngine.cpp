#include "GameEngine.h"
#include <QtMath>
#include <QRandomGenerator>
#include <algorithm>

GameEngine::GameEngine(QObject *parent)
    : QObject(parent)
{
    setTrackTheme(THEME_SUNSET_COAST);
    resetPlayer();
}

void GameEngine::setTrackTheme(TrackTheme theme) {
    m_track.buildTrack(theme);
    m_traffic.clear();
    resetPlayer();
    spawnTraffic();
}

void GameEngine::setDifficulty(GameDifficulty diff) {
    m_difficulty = diff;
    m_traffic.clear();
    spawnTraffic();
    qreal fullTrackZ = qMax(240000.0, (qreal)m_track.trackLength() - 16000.0);
    qreal lenScale = (diff == DIFFICULTY_EASY) ? 0.65 : ((diff == DIFFICULTY_PRO) ? 1.0 : 0.82);
    m_player.finishLineZ = fullTrackZ * lenScale;
    spawnTraffic();
    QString diffName = (diff == DIFFICULTY_EASY) ? "LEVEL 1: EASY (6-CAR GRID)" : ((diff == DIFFICULTY_PRO) ? "LEVEL 3: PRO (16-CAR GRID • 2.5x)" : "LEVEL 2: NORMAL (10-CAR GRID • 1.5x)");
    m_player.lastAnnouncement = QString("DIFFICULTY: %1").arg(diffName);
    m_player.announcementTimer = 2.0;
}

void GameEngine::resetPlayer() {
    m_player.worldZ = 0;
    m_player.x = 0;
    m_player.speed = 0;
    m_player.steerAngle = 0;
    m_player.steerTarget = 0;
    m_player.gear = 1;
    m_player.rpm = 900;
    m_player.targetRpm = 900;
    m_player.boostBar = 0;
    m_player.pitchG = 0;
    m_player.rollG = 0;
    m_player.roadBanking = 0;
    m_player.vibration = 0;
    m_player.suspensionRebound = 0;
    m_player.lookingLeft = false;
    m_player.lookingRight = false;
    m_player.povPan = 0;
    m_player.wiperAngle = 0;
    m_player.wipersActive = true;
    m_player.wiperDirection = 1.0;
    m_player.glassReflectionOffset = 0;
    m_player.sunGlareX = 0;
    m_player.isBraking = false;
    m_player.isAccelerating = false;
    m_player.isHandbraking = false;
    m_player.isOffroad = false;
    m_player.lookingBack = false;

    // Reset health & damage
    m_player.health = 100;
    m_player.crashCount = 0;
    m_player.isWrecked = false;
    m_player.collisionCooldown = 0;
    m_player.damageFlashTimer = 0;

    m_player.score = 0;
    m_player.nearMissCombo = 1;
    m_player.comboTimer = 0;
    m_player.distanceTraveled = 0;
    m_player.currentSector = 1;
    m_player.lastSectorAnnounced = 1;
    m_player.x = 0.25;
    setSelectedCar(m_player.selectedCar);
    if (m_player.raceFinished && m_player.currentPosition <= 3) {
        if (m_difficulty == DIFFICULTY_EASY) {
            m_difficulty = DIFFICULTY_NORMAL;
        } else if (m_difficulty == DIFFICULTY_NORMAL) {
            m_difficulty = DIFFICULTY_PRO;
        }
    }
    m_player.raceTime = 0.0;
    m_player.finishTime = 0.0;
    m_player.raceFinished = false;
    qreal fullTrackZ = qMax(240000.0, (qreal)m_track.trackLength() - 16000.0);
    qreal lenScale = (m_difficulty == DIFFICULTY_EASY) ? 0.65 : ((m_difficulty == DIFFICULTY_PRO) ? 1.0 : 0.82);
    m_player.finishLineZ = fullTrackZ * lenScale;
    spawnTraffic();
    if (m_hasRemotePlayer) {
        m_remoteCar.worldZ = m_player.worldZ + 25.0; // Staggered beside player on starting grid
        m_remoteCar.x = -0.25;
        m_remoteCar.targetX = -0.25;
        m_remoteCar.speed = 0.0;
        m_remoteCar.hasFinished = false;
        m_remoteCar.finishTime = 0.0;
        m_traffic.append(m_remoteCar);
        m_player.totalRacers = m_traffic.size() + 1;
    }
    m_player.lastAnnouncement = QString("STARTING GRID READY (%1 CARS • %2) — FLOOR THE THROTTLE!")
        .arg(m_player.totalRacers)
        .arg(m_player.carModelName);
    m_player.announcementTimer = 2.0;
    m_player.crashTimer = 0;
}

void GameEngine::setSelectedCar(int carIdx) {
    m_player.selectedCar = qBound(0, carIdx, 3);
    switch (m_player.selectedCar) {
        case 0: // APEX GT-R (Balanced Twin-Turbo Coupe)
            m_player.carModelName = "APEX GT-R";
            m_player.maxSpeed = 200.0;
            m_player.accelRate = 96.0;
            m_player.armorMultiplier = 1.00;
            m_player.gripMultiplier = 1.00;
            break;
        case 1: // VORTEX V12 (Hypercar Interceptor)
            m_player.carModelName = "VORTEX V12";
            m_player.maxSpeed = 200.0;
            m_player.accelRate = 104.0;
            m_player.armorMultiplier = 0.85;
            m_player.gripMultiplier = 0.96;
            break;
        case 2: // DRIFT SPEC-R (Touge Track Special)
            m_player.carModelName = "DRIFT SPEC-R";
            m_player.maxSpeed = 196.0;
            m_player.accelRate = 94.0;
            m_player.armorMultiplier = 0.95;
            m_player.gripMultiplier = 1.28;
            break;
        case 3: // TITAN ENFORCER (Armored Endurance GT)
            m_player.carModelName = "TITAN ENFORCER";
            m_player.maxSpeed = 198.0;
            m_player.accelRate = 92.0;
            m_player.armorMultiplier = 1.55;
            m_player.gripMultiplier = 1.04;
            break;
    }
    m_player.lastAnnouncement = QString("🏎️ SUPERCAR SELECTED: %1 (MAX %2 KM/H • %3% ARMOR)")
        .arg(m_player.carModelName)
        .arg((int)m_player.maxSpeed)
        .arg(qRound(m_player.armorMultiplier * 100));
    m_player.announcementTimer = 2.4;
}

void GameEngine::toggleAdas() {
    m_player.adasEnabled = !m_player.adasEnabled;
    m_player.lastAnnouncement = m_player.adasEnabled
        ? "🛡️ ADAS STEERING ASSIST: ON (APEX COMPENSATION & LANE STABILITY)"
        : "⚙️ ADAS STEERING ASSIST: OFF (100% MANUAL CENTRIFUGAL REALISM)";
    m_player.announcementTimer = 2.4;
}

void GameEngine::toggleArGuides() {
    m_player.arGuidesEnabled = !m_player.arGuidesEnabled;
    m_player.lastAnnouncement = m_player.arGuidesEnabled
        ? "🎯 3D AR PASSING-GAP TRAJECTORY RAILS & TARGET BRACKETS: ON"
        : "🎯 3D AR PASSING-GAP TRAJECTORY RAILS: OFF";
    m_player.announcementTimer = 2.4;
}

void GameEngine::toggleZBuffer3DMode() {
    m_player.zBuffer3DMode = !m_player.zBuffer3DMode;
    m_player.lastAnnouncement = m_player.zBuffer3DMode
        ? "🧊 FULL-DEPTH 3D Z-BUFFER & LAMBERTIAN SHADING PIPELINE: ON"
        : "🧊 CLASSIC 2.5D SCANLINE RASTER PIPELINE: ON";
    m_player.announcementTimer = 2.4;
}

void GameEngine::setPixelSizePreset(int presetIdx) {
    static const int kPresets[5] = { 1, 2, 3, 4, 6 };
    int idx = qBound(0, presetIdx, 4);
    m_player.pixelSize = kPresets[idx];
    m_player.lastAnnouncement = QString("👾 RETRO PIXEL RASTERIZER: %1px (%2x%3 INTERNAL BUFFER)")
        .arg(m_player.pixelSize)
        .arg(1280 / m_player.pixelSize)
        .arg(720 / m_player.pixelSize);
    m_player.announcementTimer = 2.4;
}

void GameEngine::cyclePixelSize(int dir) {
    static const int kPresets[5] = { 1, 2, 3, 4, 6 };
    int curIdx = 0;
    for (int i = 0; i < 5; ++i) {
        if (kPresets[i] == m_player.pixelSize) {
            curIdx = i;
            break;
        }
    }
    int nextIdx = (curIdx + dir + 5) % 5;
    setPixelSizePreset(nextIdx);
}

void GameEngine::toggleCrtScanlines() {
    m_player.crtScanlines = !m_player.crtScanlines;
    m_player.lastAnnouncement = m_player.crtScanlines
        ? "📺 RETRO CRT PHOSPHOR SCANLINES: ON"
        : "📺 RETRO CRT PHOSPHOR SCANLINES: OFF";
    m_player.announcementTimer = 2.2;
}

void GameEngine::fieldRepair() {
    m_player.health = qMin(100, m_player.health + 50);
    m_player.isWrecked = false;
    m_player.crashTimer = 0;
    m_player.lastAnnouncement = QString("🔧 ON-SPOT FIELD REPAIR COMPLETE (+50% HEALTH → %1%)").arg(m_player.health);
    m_player.announcementTimer = 2.4;

    m_player.announcementTimer = 2.0;
    m_player.crashTimer = 0;
}

void GameEngine::setThrottle(bool active) {
    m_keyUp = active;
}

void GameEngine::setBrake(bool active) {
    m_keyDown = active;
}

void GameEngine::setSteerLeft(bool active) {
    m_keyLeft = active;
}

void GameEngine::setSteerRight(bool active) {
    m_keyRight = active;
}

void GameEngine::setHandbrake(bool active) {
    m_keySpace = active;
}

void GameEngine::setLookBack(bool active) {
    m_keyLookBack = active;
}

void GameEngine::setLookLeft(bool active) {
    m_keyLookLeft = active;
}

void GameEngine::setLookRight(bool active) {
    m_keyLookRight = active;
}

void GameEngine::setHorn(bool active) {
    m_keyHorn = active;
}

void GameEngine::update(qreal dt) {
    if (m_paused) return;

    if (dt > 0.05) dt = 0.05;

    m_player.lookingBack = m_keyLookBack;
    m_player.lookingLeft = m_keyLookLeft;
    m_player.lookingRight = m_keyLookRight;
    m_player.hornActive = m_keyHorn;

    // Smooth Sideways POV camera pan (-1.0 for look left, +1.0 for look right)
    qreal targetPan = 0;
    if (m_keyLookLeft) targetPan = -1.0;
    else if (m_keyLookRight) targetPan = 1.0;
    m_player.povPan += (targetPan - m_player.povPan) * 14.0 * dt;

    // Dynamic Windshield Wipers sweep animation
    if (m_player.wipersActive) {
        m_player.wiperAngle += m_player.wiperDirection * 160.0 * dt;
        if (m_player.wiperAngle >= 72.0) {
            m_player.wiperAngle = 72.0;
            m_player.wiperDirection = -1.0;
        } else if (m_player.wiperAngle <= 0.0) {
            m_player.wiperAngle = 0.0;
            m_player.wiperDirection = 1.0;
        }
    }

    updatePlayerPhysics(dt);
    updateGearAndRpm(dt);
    updateTraffic(dt);
    checkCollisionsAndNearMisses(dt);

    // Update active 3D crash sparks, glass shards, and smoke particles
    for (int s = m_sparks.size() - 1; s >= 0; --s) {
        CrashSpark& sp = m_sparks[s];
        sp.worldZ += sp.vz * dt * 14.0;
        sp.x += sp.vx * dt;
        sp.y += sp.vy * dt;
        sp.vy -= 420.0 * dt; // Gravity pulling downward to asphalt
        if (sp.type == 2) {
            sp.size += 24.0 * dt; // Smoke expands
        }
        sp.life -= dt * 1.8;
        if (sp.life <= 0 || sp.y < 0) {
            m_sparks.removeAt(s);
        }
    }

    // Dampen physical camera impact shake & lateral collision recoil
    if (m_player.impactCameraShake > 0) {
        m_player.impactCameraShake = qMax(0.0, m_player.impactCameraShake - dt * 2.4);
    }
    if (qAbs(m_player.lateralRecoil) > 0.001) {
        m_player.x += m_player.lateralRecoil * dt * 3.5;
        m_player.lateralRecoil -= m_player.lateralRecoil * dt * 5.5;
    }

    // Timers
    if (m_player.comboTimer > 0) {
        m_player.comboTimer -= dt;
        if (m_player.comboTimer <= 0) {
            m_player.nearMissCombo = 1;
        }
    }
    if (m_player.announcementTimer > 0) {
        m_player.announcementTimer -= dt;
        if (m_player.announcementTimer <= 0) {
            m_player.lastAnnouncement = "";
        }
    }
    if (m_player.crashTimer > 0) {
        m_player.crashTimer -= dt;
    }

    // Infotainment system dynamic updates
    m_infoState.webAnimTimer += dt;
    if (m_infoState.isPlaying) {
        m_infoState.trackElapsed += dt;
        if (m_infoState.trackElapsed >= m_infoState.trackDuration) {
            nextMusicTrack();
        }
    }
}

void GameEngine::setWebPage(int page) {
    m_infoState.currentWebPage = qBound(0, page, 3);
    static const char* urls[] = {
        "https://speedr-highway.io/live",
        "https://apex-records.net/leaderboard",
        "https://weather-sat.gov/doppler",
        "https://gt-telemetry.org/v8-spec"
    };
    m_infoState.currentUrl = urls[m_infoState.currentWebPage];
    m_infoState.webScrollY = 0;
}

void GameEngine::updatePlayerPhysics(qreal dt) {
    // 1. Steering dynamics (easier, highly responsive, smooth, and natural)
    m_player.steerTarget = 0;
    if (m_keyLeft) m_player.steerTarget -= 1.0;
    if (m_keyRight) m_player.steerTarget += 1.0;

    // Fast turn-in response with smooth, confident auto-centering
    qreal steerSpeedFactor = qBound(0.60, 1.0 - (m_player.speed / 480.0), 1.0);
    qreal steerInterpolationSpeed = (m_player.steerTarget == 0) ? 18.0 : (15.0 * steerSpeedFactor);
    m_player.steerAngle += (m_player.steerTarget - m_player.steerAngle) * steerInterpolationSpeed * dt;

    // Agile and effortless lateral maneuverability for dodging traffic smoothly
    qreal lateralSpeed = m_player.steerAngle * m_player.gripMultiplier * (qMax(45.0, m_player.speed) / 125.0) * 4.0 * dt;
    m_player.x += lateralSpeed;

    // Gentle centrifugal curve push so taking corners feels effortless and intuitive
    const RoadSegment& currentSeg = m_track.findSegment(m_player.worldZ);
    const RoadSegment& nextSeg = m_track.findSegment(m_player.worldZ + m_track.getInfo().segmentLength);
    qreal adasFactor = m_player.adasEnabled ? 0.18 : 0.48;
    qreal curveForce = currentSeg.curve * (m_player.speed / 300.0) * adasFactor * dt;
    m_player.x -= curveForce;

    // Forward Ultrasonic Radar & ADAS Passing-Gap / Obstacle Avoidance Assist (Sayanaditya Feature)
    m_player.adasRadarDistM = 99.0;
    m_player.adasInterventionActive = false;
    static const qreal kLaneCenters[4] = { -0.75, -0.25, 0.25, 0.75 };
    qreal laneClearDist[4] = { 2600.0, 2600.0, 2600.0, 2600.0 };
    for (const NPCCar& npc : m_traffic) {
        qreal dz = npc.worldZ - m_player.worldZ;
        if (dz > 20.0 && dz < 2400.0) {
            if (qAbs(npc.x - m_player.x) < 0.23) {
                m_player.adasRadarDistM = qMin(m_player.adasRadarDistM, dz / 20.0);
            }
            for (int l = 0; l < 4; ++l) {
                if (qAbs(npc.x - kLaneCenters[l]) < 0.23) {
                    laneClearDist[l] = qMin(laneClearDist[l], dz);
                }
            }
        }
    }
    int bestLane = 1;
    qreal bestScore = -1e9;
    for (int l = 0; l < 4; ++l) {
        qreal score = laneClearDist[l] - qAbs(kLaneCenters[l] - m_player.x) * 320.0;
        if (score > bestScore) {
            bestScore = score;
            bestLane = l;
        }
    }
    m_player.adasBestLaneX = kLaneCenters[bestLane];

    if (m_player.adasEnabled) {
        // Active lane-keep shoulder recovery (prevents accidental off-road drift)
        if (qAbs(m_player.x) > 0.88 && m_player.steerTarget == 0) {
            m_player.x += (m_player.x > 0 ? -1.0 : 1.0) * 0.55 * dt;
            m_player.adasInterventionActive = true;
        }
        // Gentle obstacle-avoidance nudge only after clearing starting grid and when very close to a car ahead
        if (m_player.worldZ > 4200.0 && m_player.adasRadarDistM < 26.0 && m_player.speed > 65.0 && m_player.steerTarget == 0) {
            qreal laneDelta = m_player.adasBestLaneX - m_player.x;
            if (qAbs(laneDelta) > 0.04) {
                qreal autoSteer = qBound(-1.0, laneDelta * 2.2, 1.0);
                m_player.x += autoSteer * 0.45 * dt;
                m_player.adasInterventionActive = true;
            }
        }
    }

    // Soft asphalt edge stabilization to prevent sudden spins
    if (qAbs(m_player.x) > 0.98) {
        qreal assist = (m_player.x > 0 ? -1.0 : 1.0) * 0.6 * dt;
        m_player.x += assist;
    }

    m_player.isOffroad = (qAbs(m_player.x) > 1.02);

    // 2. Acceleration & Braking
    m_player.isAccelerating = m_keyUp && (m_player.crashTimer <= 0) && !m_player.isWrecked;
    m_player.isBraking = m_keyDown;
    m_player.isHandbraking = m_keySpace;

    qreal accel = 0;
    if (m_player.isWrecked) {
        accel = -120.0; // Rapid braking when wrecked
        m_player.boostBar = 0.0;
    } else if (m_player.isAccelerating) {
        qreal powerFactor = qBound(0.35, 1.0 - (m_player.speed / 215.0), 1.0);
        m_player.slipstreamActive = false;
        for (const NPCCar& npc : m_traffic) {
            qreal dz = npc.worldZ - m_player.worldZ;
            if (dz > 120.0 && dz < 950.0 && qAbs(npc.x - m_player.x) < 0.22 && m_player.speed > 75.0) {
                m_player.slipstreamActive = true;
                break;
            }
        }
        accel = m_player.accelRate * powerFactor + (m_player.slipstreamActive ? 22.0 : 0.0);
        m_player.boostBar = qMin(1.0, m_player.boostBar + 1.2 * dt);
    } else {
        m_player.boostBar = qMax(0.0, m_player.boostBar - 2.5 * dt);
    }

    // Hill elevation slope effect (Gravity & Uneven Road Telemetry)
    qreal segLen = (qreal)m_track.getInfo().segmentLength;
    qreal slope = (nextSeg.elevation - currentSeg.elevation) / segLen;
    m_player.roadSlopeDeg = qAtan(slope) * 57.2958;
    m_player.roadBankDeg  = currentSeg.roadBank * 42.0;

    // Scan ahead 18 segments for steep hill crests (Rally Crest Warning)
    m_player.crestWarning = false;
    for (int d = 3; d <= 18; ++d) {
        const RoadSegment& sA = m_track.findSegment(m_player.worldZ + d * segLen);
        const RoadSegment& sB = m_track.findSegment(m_player.worldZ + (d + 2) * segLen);
        qreal slA = (sA.p2.worldY - sA.p1.worldY);
        qreal slB = (sB.p2.worldY - sB.p1.worldY);
        if (slA > 35.0 && slB < -15.0) {
            m_player.crestWarning = true;
            break;
        }
    }

    qreal gravityForce = -slope * 35.0;
    accel += gravityForce;
    if (m_player.adasEnabled && m_player.adasRadarDistM < 18.0 && m_player.speed > 110.0) {
        accel -= 55.0; // ADAS Emergency Collision-Mitigation Braking
        m_player.adasInterventionActive = true;
    }

    if (m_player.isBraking) {
        if (m_player.speed > 5) {
            accel -= m_player.brakeRate;
        } else {
            accel -= 25.0;
        }
    }

    if (m_player.isHandbraking) {
        accel -= m_player.brakeRate * 1.6;
    }

    qreal drag = 0.00018 * m_player.speed * m_player.speed;
    qreal rollingResistance = 4.0;
    accel -= (drag + rollingResistance);

    if (m_player.isOffroad) {
        accel -= 65.0;
        if (m_player.speed > 110) {
            m_player.speed -= 85.0 * dt;
        }
    }

    m_player.speed += accel * dt;

    if (m_player.isBraking && m_player.speed < 0) {
        m_player.speed = qMax(-45.0, m_player.speed);
    } else {
        m_player.speed = qBound(0.0, m_player.speed, qMin(200.0, m_player.maxSpeed));
    }

    qreal speedBoost = 1.0 + qBound(0.0, (m_player.speed - 60.0) / 200.0, 0.25);
    qreal absSpd = qAbs(m_player.speed);
    qreal signSpd = (m_player.speed >= 0.0) ? 1.0 : -1.0;
    qreal worldSpeed = (absSpd > 0.2)
        ? signSpd * ((absSpd / 3.6) * 62.0 + 240.0 * qMin(1.0, absSpd / 22.0)) * dt
        : 0.0;
    if ((m_player.isAccelerating || absSpd > 0.5 || m_player.worldZ > 15.0) && !m_player.raceFinished) {
        m_player.raceTime += dt;
    }
    m_player.worldZ += worldSpeed;
    m_player.distanceTraveled += (m_player.speed / 3600.0) * dt;

    // Sector Transition Detection & Notification
    m_player.currentSector = currentSeg.sector;
    if (m_player.currentSector != m_player.lastSectorAnnounced && !currentSeg.sectorName.isEmpty()) {
        m_player.lastSectorAnnounced = m_player.currentSector;
        m_player.health = qMin(100, m_player.health + 25);
        m_player.lastAnnouncement = QString("🔧 CHECKPOINT ARCH: SECTOR %1 - %2 (+25% HEALTH)").arg(m_player.currentSector).arg(currentSeg.sectorName);
        m_player.announcementTimer = 3.5;
        m_player.score += 500;
    }

    // Continuous distance & high-speed scoring
    m_player.score += (int)((m_player.speed / 3600.0) * dt * 250);
    if (m_player.speed > 180.0) {
        m_player.score += (int)(25 * dt);
    }
    if (m_player.speed > 250.0) {
        m_player.score += (int)(50 * dt);
    }

    int totalTrackLength = m_track.trackLength();
    if (m_player.worldZ >= totalTrackLength) {
        m_player.worldZ -= totalTrackLength;
    } else if (m_player.worldZ < 0) {
        m_player.worldZ += totalTrackLength;
    }

    // 3. Dynamic Suspension, G-Forces & Track Momentum
    qreal targetPitch = 0;
    if (m_player.isBraking) {
        targetPitch = -0.06;
    } else if (m_player.isAccelerating) {
        targetPitch = 0.035;
    }
    targetPitch += (slope * 0.08);
    m_player.pitchG += (targetPitch - m_player.pitchG) * 6.0 * dt;

    qreal targetRoll = -m_player.steerAngle * (m_player.speed / 200.0) * 0.05;
    m_player.rollG += (targetRoll - m_player.rollG) * 7.0 * dt;

    // Dynamic Road Banking on curves
    qreal targetBanking = -currentSeg.curve * (m_player.speed / 180.0) * 0.035;
    m_player.roadBanking += (targetBanking - m_player.roadBanking) * 5.0 * dt;

    // Dynamic front windshield reflections & sun glare sweep
    m_player.glassReflectionOffset += worldSpeed * 0.2;
    qreal targetGlareX = currentSeg.curve * 140.0;
    m_player.sunGlareX += (targetGlareX - m_player.sunGlareX) * 4.0 * dt;

    // Vibration
    qreal baseVib = 0.001 * (m_player.speed / 100.0);
    if (m_player.isOffroad) {
        baseVib += 0.008;
    }
    if (qAbs(m_player.x) > 0.92 && qAbs(m_player.x) < 1.08) {
        baseVib += 0.006;
    }
    m_player.vibration = baseVib;
}

void GameEngine::updateGearAndRpm(qreal dt) {
    if (m_player.speed < 0) {
        m_player.gear = -1;
        m_player.rpm = 900 + qAbs(m_player.speed) * 90;
        return;
    }

    int oldGear = m_player.gear;
    qreal s = m_player.speed;

    if (s < 38) {
        m_player.gear = 1;
        m_player.rpm = 900 + (s / 38.0) * 6200.0;
    } else if (s < 72) {
        m_player.gear = 2;
        m_player.rpm = 3400 + ((s - 38.0) / 34.0) * 3700.0;
    } else if (s < 108) {
        m_player.gear = 3;
        m_player.rpm = 3800 + ((s - 72.0) / 36.0) * 3400.0;
    } else if (s < 144) {
        m_player.gear = 4;
        m_player.rpm = 4200 + ((s - 108.0) / 36.0) * 3100.0;
    } else if (s < 176) {
        m_player.gear = 5;
        m_player.rpm = 4600 + ((s - 144.0) / 32.0) * 2700.0;
    } else {
        m_player.gear = 6;
        m_player.rpm = 5100 + ((s - 176.0) / 24.0) * 2300.0;
    }

    if (m_player.isAccelerating && m_player.rpm > 7400) {
        m_player.rpm = 7400 + (QRandomGenerator::global()->generate() % 200) - 100;
    }

    if (m_player.gear != oldGear) {
        emit gearChanged(m_player.gear);
    }
}

void GameEngine::spawnCrashSparks(qreal z, qreal x, int count, bool isHeadOn, qreal severity) {
    for (int k = 0; k < count; ++k) {
        CrashSpark spark;
        spark.worldZ = z + ((QRandomGenerator::global()->generateDouble() - 0.5) * 35.0);
        spark.x = x + ((QRandomGenerator::global()->generateDouble() - 0.5) * 0.16);
        spark.y = 80.0 + (QRandomGenerator::global()->generateDouble() * 110.0);

        qreal angle = QRandomGenerator::global()->generateDouble() * 2.0 * M_PI;
        qreal speed = (160.0 + QRandomGenerator::global()->generateDouble() * 340.0) * severity;

        spark.vx = cos(angle) * speed * 0.003;
        spark.vy = 90.0 + QRandomGenerator::global()->generateDouble() * 170.0; // Upward explosive arc
        spark.vz = sin(angle) * speed * (isHeadOn ? -1.3 : 0.9);
        spark.life = 0.45 + QRandomGenerator::global()->generateDouble() * 0.55;
        spark.maxLife = spark.life;
        spark.type = (k % 4 == 0) ? 1 : ((k % 7 == 0) ? 2 : 0); // 0: spark, 1: glass shard, 2: smoke
        spark.size = (spark.type == 2) ? 18.0 : (2.5 + QRandomGenerator::global()->generateDouble() * 3.5);

        m_sparks.append(spark);
    }
}

void GameEngine::spawnTraffic() {
    const qreal laneCoords[4] = { -0.75, -0.25, 0.25, 0.75 };
    m_traffic.clear();
    m_sparks.clear();
    m_player.impactCameraShake = 0;
    m_player.lateralRecoil = 0;

    int rivalCount = (m_difficulty == DIFFICULTY_EASY) ? 5 : ((m_difficulty == DIFFICULTY_PRO) ? 15 : 9);
    int aheadCount = (rivalCount + 1) / 2; // Player starts in the middle of the pack!
    qreal trackDiffBonus = (m_track.getInfo().difficulty - 2) * 2.0;
    qreal diffBoost = ((m_difficulty == DIFFICULTY_EASY) ? -8.0 : ((m_difficulty == DIFFICULTY_PRO) ? 4.0 : 0.0)) + trackDiffBonus;
    qreal accelBoost = (m_difficulty == DIFFICULTY_EASY) ? -3.0 : ((m_difficulty == DIFFICULTY_PRO) ? 5.0 : 0.0);

    struct RacerTemplate {
        const char* name;
        VehicleType type;
        int lane;
        qreal baseSpd;
        qreal baseAccel;
    } roster[15] = {
        { "VORTEX GT",   VEHICLE_SPORTS_RED,   0, 198.0, 48.0 },
        { "NITRO RS",    VEHICLE_SEDAN_SILVER, 2, 196.0, 46.5 },
        { "COBRA V8",    VEHICLE_COMPACT_BLUE, 1, 194.0, 45.0 },
        { "TITAN GTR",   VEHICLE_SUV_BLACK,    3, 192.0, 44.0 },
        { "PHANTOM X",   VEHICLE_SPORTS_RED,   0, 190.0, 43.0 },
        { "FALCON R",    VEHICLE_COMPACT_BLUE, 3, 188.0, 42.0 },
        { "BLITZ EVO",   VEHICLE_SEDAN_SILVER, 1, 186.0, 41.0 },
        { "APEX FURY",   VEHICLE_SPORTS_RED,   2, 184.0, 40.0 },
        { "HYPERION Z",  VEHICLE_SUV_BLACK,    0, 182.0, 39.0 },
        { "SHADOW M8",   VEHICLE_SEDAN_SILVER, 3, 180.0, 38.0 },
        { "VALKYRIE S",  VEHICLE_SPORTS_RED,   1, 178.0, 37.0 },
        { "TEMPEST RS",  VEHICLE_COMPACT_BLUE, 2, 176.0, 36.0 },
        { "MONARCH V12", VEHICLE_SUV_BLACK,    0, 175.0, 35.0 },
        { "SPECTRE GT",  VEHICLE_SEDAN_SILVER, 3, 174.0, 34.0 },
        { "KRAKEN R",    VEHICLE_COMPACT_BLUE, 1, 172.0, 33.0 }
    };

    for (int i = 0; i < rivalCount; ++i) {
        NPCCar npc;
        npc.id = m_nextNpcId++;
        npc.racerName = QString::fromLatin1(roster[i].name);
        npc.type = roster[i].type;
        npc.assignedLane = roster[i].lane;
        npc.isOncoming = false; // 100% One-Way Motorsport Race Track
        npc.x = laneCoords[roster[i].lane];
        npc.targetX = npc.x;

        qreal startZ = 0.0;
        if (i < aheadCount) {
            // Staggered rows ahead of the player (furthest is P1 at ~3400)
            int stepsAhead = aheadCount - i;
            startZ = 1120.0 + stepsAhead * 290.0;
            npc.currentPosition = i + 1;
        } else {
            // Staggered rows behind the player
            int stepsBehind = (i - aheadCount) + 1;
            startZ = -140.0 - stepsBehind * 210.0;
            npc.currentPosition = i + 2;
        }

        npc.worldZ = m_player.worldZ + startZ;
        npc.speed = 0.0;        // Starts stationary on the Starting Grid with the player!
        npc.maxSpeed = qBound(168.0, roster[i].baseSpd + diffBoost, 200.0);
        npc.accelRate = roster[i].baseAccel + accelBoost;
        npc.hasFinished = false;
        npc.finishTime = 0.0;
        m_traffic.append(npc);
    }
    m_player.totalRacers = m_traffic.size() + 1;
    m_player.currentPosition = aheadCount + 1;
    if (true) return;
    m_player.lateralRecoil = 0;

    int totalTrafficCount = (m_difficulty == DIFFICULTY_EASY) ? 18 : ((m_difficulty == DIFFICULTY_PRO) ? 28 : 24);
    qreal speedDelta = (m_difficulty == DIFFICULTY_EASY) ? -16.0 : ((m_difficulty == DIFFICULTY_PRO) ? 24.0 : 0.0);
    int totalTrackLen = m_track.trackLength();

    // 1. Surrounding Rear Traffic (Populates mirrors right from frame 1!)
    // 5 vehicles situated behind the player (-140 to -700 units)
    for (int r = 0; r < 5; ++r) {
        NPCCar npc;
        npc.id = m_nextNpcId++;

        if (r == 0) {
            // Oncoming car in Lane 0 receding into the rear distance
            npc.assignedLane = 0;
            npc.isOncoming = true;
            npc.x = laneCoords[0];
            npc.targetX = npc.x;
            npc.type = VEHICLE_SEDAN_SILVER;
            npc.speed = 120.0 + speedDelta;
            npc.worldZ = m_player.worldZ - 240.0;
        } else {
            // Same-direction cars trailing or approaching in Lanes 1, 2, 3
            npc.assignedLane = 1 + ((r - 1) % 3);
            npc.isOncoming = false;
            npc.x = laneCoords[npc.assignedLane];
            npc.targetX = npc.x;

            if (npc.assignedLane == 1) {
                npc.type = VEHICLE_SPORTS_RED;
                npc.speed = 155.0 + speedDelta; // Fast car overtaking from behind
            } else if (npc.assignedLane == 3) {
                npc.type = VEHICLE_TRUCK_SEMI;
                npc.speed = 88.0 + speedDelta;
            } else {
                npc.type = VEHICLE_SUV_BLACK;
                npc.speed = 118.0 + speedDelta;
            }
            npc.worldZ = m_player.worldZ - (140.0 + r * 125.0);
        }

        if (npc.worldZ < 0) npc.worldZ += totalTrackLen;
        npc.maxSpeed = npc.speed;
        m_traffic.append(npc);
    }

    // 2. Adjacent Traffic (Cruising alongside player for instant immersion)
    {
        NPCCar adj1;
        adj1.id = m_nextNpcId++;
        adj1.assignedLane = 2;
        adj1.isOncoming = false;
        adj1.x = laneCoords[2];
        adj1.targetX = adj1.x;
        adj1.type = VEHICLE_SEDAN_SILVER;
        adj1.speed = 112.0 + speedDelta;
        adj1.maxSpeed = adj1.speed;
        adj1.worldZ = m_player.worldZ - 30.0;
        adj1.isSideLanePacer = false;
        if (adj1.worldZ < 0) adj1.worldZ += totalTrackLen;
        m_traffic.append(adj1);

        NPCCar adj2;
        adj2.id = m_nextNpcId++;
        adj2.assignedLane = 3;
        adj2.isOncoming = false;
        adj2.x = laneCoords[3];
        adj2.targetX = adj2.x;
        adj2.type = VEHICLE_COMPACT_BLUE;
        adj2.speed = 98.0 + speedDelta;
        adj2.maxSpeed = adj2.speed;
        adj2.worldZ = m_player.worldZ + 45.0;
        if (adj2.worldZ >= totalTrackLen) adj2.worldZ -= totalTrackLen;
        m_traffic.append(adj2);
    }

    // 3. Forward Traffic Corridor (+350 to +18,000 units)
    // Distributed across highway horizon with realistic spacing so cars appear far in distance
    int remainingCount = totalTrafficCount - m_traffic.size();
    qreal curZ = m_player.worldZ + 1250.0;

    for (int i = 0; i < remainingCount; ++i) {
        NPCCar npc;
        npc.id = m_nextNpcId++;

        // Every 3rd vehicle is an oncoming rush car in Lane 0
        if (i % 3 == 0) {
            npc.assignedLane = 0;
            npc.isOncoming = true;
            npc.x = laneCoords[0];
            npc.targetX = npc.x;
            npc.type = (i % 2 == 0) ? VEHICLE_SPORTS_RED : VEHICLE_SEDAN_SILVER;
            npc.speed = 115.0 + speedDelta + (QRandomGenerator::global()->generate() % 30);
            curZ += 450.0 + (QRandomGenerator::global()->generate() % 350);
        } else {
            // Forward lane with anti-block guarantee: pick a lane that avoids 3-wide blocks
            int lane = 1 + (i % 3);
            npc.assignedLane = lane;
            npc.isOncoming = false;
            npc.x = laneCoords[lane];
            npc.targetX = npc.x;

            if (lane == 1) {
                npc.type = (i % 2 == 0) ? VEHICLE_SPORTS_RED : VEHICLE_SEDAN_SILVER;
                npc.speed = 148.0 + speedDelta + (QRandomGenerator::global()->generate() % 25);
            } else if (lane == 3) {
                npc.type = (i % 2 == 0) ? VEHICLE_TRUCK_SEMI : VEHICLE_COMPACT_BLUE;
                npc.speed = 88.0 + speedDelta + (QRandomGenerator::global()->generate() % 18);
            } else {
                npc.type = (i % 2 == 0) ? VEHICLE_SUV_BLACK : VEHICLE_SEDAN_SILVER;
                npc.speed = 118.0 + speedDelta + (QRandomGenerator::global()->generate() % 22);
            }

            // Varied distance spreading across the full forward road
            if (i % 2 == 0) {
                curZ += 320.0 + (QRandomGenerator::global()->generate() % 180);
            } else {
                curZ += 680.0 + (QRandomGenerator::global()->generate() % 350);
            }
        }

        npc.worldZ = fmod(curZ, totalTrackLen);
        npc.maxSpeed = npc.speed;
        m_traffic.append(npc);
    }
}

QVector<RaceStandingEntry> GameEngine::getRaceStandings() const {
    QVector<RaceStandingEntry> list;
    list.reserve(m_traffic.size() + 1);

    RaceStandingEntry playerEntry;
    playerEntry.name = QString("[YOU] %1").arg(m_player.carModelName);
    playerEntry.isPlayer = true;
    playerEntry.hasFinished = m_player.raceFinished;
    playerEntry.finishTime = m_player.finishTime;
    playerEntry.progressZ = m_player.worldZ;
    playerEntry.speed = m_player.speed;
    list.append(playerEntry);

    for (const NPCCar& npc : m_traffic) {
        RaceStandingEntry e;
        e.name = npc.isRemotePlayer ? QString("👑 [P2] %1").arg(npc.racerName) : (npc.racerName.isEmpty() ? QString("RACER #%1").arg(npc.id) : npc.racerName);
        e.isPlayer = false;
        e.hasFinished = npc.hasFinished;
        e.finishTime = npc.finishTime;
        e.progressZ = npc.worldZ;
        e.speed = npc.speed;
        list.append(e);
    }

    std::sort(list.begin(), list.end(), [](const RaceStandingEntry& a, const RaceStandingEntry& b) {
        if (a.hasFinished && b.hasFinished) {
            return a.finishTime < b.finishTime;
        }
        if (a.hasFinished != b.hasFinished) {
            return a.hasFinished;
        }
        return a.progressZ > b.progressZ;
    });

    for (int i = 0; i < list.size(); ++i) {
        list[i].position = i + 1;
    }
    return list;
}

void GameEngine::setRemotePlayerCar(const NPCCar& remoteCar) {
    m_hasRemotePlayer = true;
    m_remoteCar = remoteCar;

    bool found = false;
    for (int i = 0; i < m_traffic.size(); ++i) {
        if (m_traffic[i].id == 9999 || m_traffic[i].isRemotePlayer) {
            m_traffic[i].worldZ += (remoteCar.worldZ - m_traffic[i].worldZ) * 0.45;
            m_traffic[i].x += (remoteCar.x - m_traffic[i].x) * 0.50;
            m_traffic[i].speed = remoteCar.speed;
            m_traffic[i].type = remoteCar.type;
            m_traffic[i].racerName = remoteCar.racerName;
            m_traffic[i].isRemotePlayer = true;
            m_traffic[i].isBraking = remoteCar.isBraking;
            m_traffic[i].isCrashing = remoteCar.isCrashing;
            m_traffic[i].hazardLights = remoteCar.hazardLights;
            m_traffic[i].hasFinished = remoteCar.hasFinished;
            m_traffic[i].finishTime = remoteCar.finishTime;
            found = true;
            break;
        }
    }
    if (!found) {
        m_traffic.append(remoteCar);
        m_player.totalRacers = m_traffic.size() + 1;
    }
}

void GameEngine::updateTraffic(qreal dt) {
  {
    const qreal laneCoords[4] = { -0.75, -0.25, 0.25, 0.75 };
    int totalTrackLen = m_track.trackLength();
    bool raceStarted = m_player.isAccelerating || m_player.speed > 0.5 || m_player.raceTime > 0.0 || m_player.worldZ > 15.0;

    for (int i = 0; i < m_traffic.size(); ++i) {
        NPCCar& npc = m_traffic[i];

        if (npc.isRemotePlayer) {
            if (!npc.hasFinished && npc.worldZ >= m_player.finishLineZ) {
                npc.hasFinished = true;
                if (npc.finishTime <= 0) npc.finishTime = m_player.raceTime;
            }
            continue; // Movement controlled by network packets
        }
        npc.isOncoming = false; // Strictly one-sided race cars

        // Post-crash skid, fishtailing & recovery
        if (npc.isCrashing) {
            npc.crashRecoveryTimer -= dt;
            npc.x += npc.lateralVelocity * dt;
            npc.x = qBound(-1.10, npc.x, 1.10);
            npc.lateralVelocity *= (1.0 - 2.8 * dt);
            npc.skidAngle = sin(npc.crashRecoveryTimer * 15.0) * 0.35 * qMax(0.0, npc.crashRecoveryTimer / 2.5);
            npc.hazardLights = true;
            npc.isBraking = true;
            if (npc.crashRecoveryTimer <= 0) {
                npc.isCrashing = false;
                npc.hazardLights = false;
                npc.skidAngle = 0;
                npc.isBraking = false;
                npc.targetX = laneCoords[qBound(0, npc.assignedLane, 3)];
            }
        } else if (raceStarted) {
            // Detect lead car in same lane
            qreal minLeadDist = 99999.0;
            qreal leadSpeed = npc.maxSpeed;
            for (int j = 0; j < m_traffic.size(); ++j) {
                if (i == j) continue;
                const NPCCar& other = m_traffic[j];
                if (qAbs(other.x - npc.x) < 0.22) {
                    qreal dist = other.worldZ - npc.worldZ;
                    if (dist > 0 && dist < minLeadDist) {
                        minLeadDist = dist;
                        leadSpeed = other.speed;
                    }
                }
            }

            // Competitive Motorsport Pack Pacing — rivals race aggressively at up to 172-198 km/h!
            qreal relDist = npc.worldZ - m_player.worldZ;
            qreal targetRaceSpeed = npc.maxSpeed;
            if (relDist < -250.0 && !npc.hasFinished) {
                // Trailing rivals slipstream and attempt to counter-attack at near top speed
                targetRaceSpeed = qBound(165.0, m_player.speed * 0.98, npc.maxSpeed);
            }

            if (minLeadDist < 220.0 && npc.laneChangeTimer <= 1.2) {
                npc.isBraking = (npc.speed > leadSpeed + 8.0);
                qreal followSpd = qMax(140.0, leadSpeed - 3.0);
                npc.speed += (followSpd - npc.speed) * 3.5 * dt;
            } else {
                npc.isBraking = false;
                if (npc.speed < targetRaceSpeed) {
                    npc.speed += npc.accelRate * dt;
                    if (npc.speed > targetRaceSpeed) npc.speed = targetRaceSpeed;
                } else {
                    npc.speed -= 28.0 * dt;
                }
            }

            // Overtaking AI across all 4 one-way racing lanes (0, 1, 2, 3)
            npc.laneChangeTimer += dt;
            if (minLeadDist < 320.0 && npc.laneChangeTimer > 1.2) {
                npc.laneChangeTimer = 0;
                int curLane = qBound(0, npc.assignedLane, 3);
                int bestLane = curLane;
                for (int cand = 0; cand < 4; ++cand) {
                    if (cand == curLane || qAbs(cand - curLane) > 1) continue;
                    bool clear = true;
                    for (int k = 0; k < m_traffic.size(); ++k) {
                        if (k == i) continue;
                        if (qAbs(m_traffic[k].x - laneCoords[cand]) < 0.22 &&
                            qAbs(m_traffic[k].worldZ - npc.worldZ) < 260.0) {
                            clear = false;
                            break;
                        }
                    }
                    if (clear) {
                        bestLane = cand;
                        break;
                    }
                }
                npc.assignedLane = bestLane;
                npc.targetX = laneCoords[bestLane];
            }
        }

        // Exact same road-speed formula as the player so relative speeds match 1:1!
        qreal absSpd = qAbs(npc.speed);
        qreal worldSpeed = (absSpd > 0.2)
            ? ((absSpd / 3.6) * 62.0 + 240.0 * qMin(1.0, absSpd / 22.0)) * dt
            : 0.0;
        npc.worldZ += worldSpeed;

        // Check if this AI racer crossed the Finish Line
        if (!npc.hasFinished && npc.worldZ >= m_player.finishLineZ) {
            npc.hasFinished = true;
            npc.finishTime = qMax(0.1, m_player.raceTime);
        }

        // Blinker & lateral lane-change interpolation
        npc.blinkerTimer += dt;
        npc.blinkerFlash = (fmod(npc.blinkerTimer, 0.28) < 0.14);
        if (qAbs(npc.x - npc.targetX) > 0.02) {
            npc.x += (npc.targetX - npc.x) * 2.8 * dt;
            if (!npc.hazardLights) {
                npc.turnLeft = (npc.targetX < npc.x);
                npc.turnRight = (npc.targetX > npc.x);
            }
        } else {
            npc.x = npc.targetX;
            if (!npc.hazardLights) {
                npc.turnLeft = false;
                npc.turnRight = false;
            }
        }
    }

    // Check if the Player crossed the Finish Line!
    if (!m_player.raceFinished && m_player.worldZ >= m_player.finishLineZ) {
        m_player.raceFinished = true;
        m_player.finishTime = qMax(0.1, m_player.raceTime);

        // Compute projected finish times for any AI racers still behind the finish line
        for (int i = 0; i < m_traffic.size(); ++i) {
            NPCCar& npc = m_traffic[i];
            if (!npc.hasFinished) {
                qreal remZ = qMax(40.0, m_player.finishLineZ - npc.worldZ);
                qreal estSpd = qMax(165.0, npc.speed);
                qreal unitsPerSec = (estSpd / 3.6) * 62.0 + 240.0;
                npc.finishTime = m_player.finishTime + (remZ / unitsPerSec) + (i * 0.18);
                npc.hasFinished = true;
            }
        }
    }

    // Update live race positions (1st to 8th) for player and all AI racers
    int prevPlayerPos = m_player.currentPosition;
    QVector<RaceStandingEntry> standings = getRaceStandings();
    for (const RaceStandingEntry& e : standings) {
        if (e.isPlayer) {
            m_player.currentPosition = e.position;
        } else {
            for (int i = 0; i < m_traffic.size(); ++i) {
                if (m_traffic[i].racerName == e.name) {
                    m_traffic[i].currentPosition = e.position;
                }
            }
        }
    }
    if (!m_player.raceFinished && m_player.raceTime > 0.3 && m_player.currentPosition < prevPlayerPos) {
        int gained = prevPlayerPos - m_player.currentPosition;
        m_player.score += gained * 1000;
        m_player.lastAnnouncement = (m_player.currentPosition == 1)
            ? QString("🏆 RACE LEADER! OVERTAKE FOR P1 / %1 (+%2 PTS)").arg(m_player.totalRacers).arg(gained * 1000)
            : QString("🔥 OVERTAKE! ADVANCED TO P%1 / %2 (+%3 PTS)").arg(m_player.currentPosition).arg(m_player.totalRacers).arg(gained * 1000);
        m_player.announcementTimer = 2.4;
    }

    if (m_player.raceFinished && m_player.announcementTimer < 100.0) {
        m_player.lastAnnouncement = QString("🏁 FINISH LINE! YOU FINISHED P%1 / %2 (%3s) — PRESS [R] TO RACE AGAIN")
            .arg(m_player.currentPosition)
            .arg(m_player.totalRacers)
            .arg(QString::number(m_player.finishTime, 'f', 2));
        m_player.announcementTimer = 999.0;
    }

    if (true) return;
  }
    const qreal laneCoords[4] = { -0.75, -0.25, 0.25, 0.75 };
    int totalTrackLen = m_track.trackLength();

    for (int i = 0; i < m_traffic.size(); ++i) {
        NPCCar& npc = m_traffic[i];

        qreal worldSpeed = (npc.speed / 3.6) * 24.0 * dt;

        if (npc.isOncoming) {
            // Oncoming traffic moves toward player (decreasing Z)
            if (npc.isCrashing) {
                npc.worldZ += ((m_player.speed * 0.85) / 3.6) * 24.0 * dt;
                if (npc.worldZ >= totalTrackLen) npc.worldZ -= totalTrackLen;
            } else {
                npc.worldZ -= worldSpeed;
            }
            if (npc.worldZ < 0) {
                npc.worldZ += totalTrackLen;
            }

            // Post-crash skid & drift handling for oncoming cars
            if (npc.isCrashing) {
                npc.crashRecoveryTimer -= dt;
                npc.x += npc.lateralVelocity * dt;
                npc.x = qBound(-1.18, npc.x, 1.18);
                npc.lateralVelocity *= (1.0 - 2.5 * dt);
                npc.skidAngle = sin(npc.crashRecoveryTimer * 16.0) * 0.40 * qMax(0.0, npc.crashRecoveryTimer / 3.0);
                npc.hazardLights = true;

                if (npc.crashRecoveryTimer <= 0) {
                    npc.isCrashing = false;
                    npc.hazardLights = false;
                    npc.skidAngle = 0;
                    npc.targetX = laneCoords[0];
                }
            }

            // Relative distance to player
            qreal relDist = npc.worldZ - m_player.worldZ;
            if (relDist < -totalTrackLen / 2) relDist += totalTrackLen;
            if (relDist > totalTrackLen / 2) relDist -= totalTrackLen;

            // Once oncoming car passes behind player (-3000m, well past mirror range), recycle ahead far on horizon
            if (relDist < -3000) {
                npc.worldZ = m_player.worldZ + 16000 + (QRandomGenerator::global()->generate() % 10000);
                if (npc.worldZ >= totalTrackLen) npc.worldZ -= totalTrackLen;
                npc.assignedLane = 0;
                npc.x = laneCoords[0];
                npc.targetX = npc.x;
                npc.isBraking = false;
                npc.isCrashing = false;
                npc.hazardLights = false;
                npc.skidAngle = 0;
                npc.turnLeft = false;
                npc.turnRight = false;
                npc.nearMissAwarded = false;
            }
        } else {
            // Same-direction traffic moves with player (increasing Z)
            npc.worldZ += worldSpeed;
            if (npc.worldZ >= totalTrackLen) {
                npc.worldZ -= totalTrackLen;
            }

            // Post-crash skid, fishtailing & recovery
            if (npc.isCrashing) {
                npc.crashRecoveryTimer -= dt;
                npc.x += npc.lateralVelocity * dt;
                npc.x = qBound(-1.18, npc.x, 1.18);
                npc.lateralVelocity *= (1.0 - 2.8 * dt);
                npc.skidAngle = sin(npc.crashRecoveryTimer * 15.0) * 0.35 * qMax(0.0, npc.crashRecoveryTimer / 2.5);
                npc.hazardLights = true;
                npc.isBraking = true;

                if (npc.crashRecoveryTimer <= 0) {
                    npc.isCrashing = false;
                    npc.hazardLights = false;
                    npc.skidAngle = 0;
                    npc.isBraking = false;
                    npc.targetX = laneCoords[npc.assignedLane];
                }
            }

            // Relative distance to player
            qreal relDist = npc.worldZ - m_player.worldZ;
            if (relDist < -totalTrackLen / 2) relDist += totalTrackLen;
            if (relDist > totalTrackLen / 2) relDist -= totalTrackLen;

            // Continuous recycling: when car falls far behind (-4500m, receding completely out of mirrors), recycle far ahead
            if (relDist < -4500) {
                npc.worldZ = m_player.worldZ + 14000 + (QRandomGenerator::global()->generate() % 9000);
                if (npc.worldZ >= totalTrackLen) npc.worldZ -= totalTrackLen;

                int lane = 1 + (QRandomGenerator::global()->generate() % 3);
                npc.assignedLane = lane;
                npc.x = laneCoords[lane];
                npc.targetX = npc.x;
                npc.turnLeft = false;
                npc.turnRight = false;
                npc.isBraking = false;
                npc.isCrashing = false;
                npc.hazardLights = false;
                npc.skidAngle = 0;
                npc.passedByPlayer = false;
                npc.nearMissAwarded = false;
            }

            // Radar: detect lead vehicle ahead in current or target lane
            qreal minLeadDist = 9999.0;
            qreal leadSpeed = npc.maxSpeed;
            for (int j = 0; j < m_traffic.size(); ++j) {
                if (i == j) continue;
                const NPCCar& other = m_traffic[j];
                if (other.isOncoming) continue;
                if (other.assignedLane == npc.assignedLane || qAbs(other.x - npc.x) < 0.26) {
                    qreal dist = other.worldZ - npc.worldZ;
                    if (dist < -totalTrackLen / 2) dist += totalTrackLen;
                    if (dist > totalTrackLen / 2) dist -= totalTrackLen;
                    if (dist > 0 && dist < minLeadDist) {
                        minLeadDist = dist;
                        leadSpeed = other.speed;
                    }
                }
            }

            // Following distance and braking AI
            if (!npc.isCrashing) {
                if (minLeadDist < 160.0) {
                    npc.isBraking = true;
                    qreal targetFollowSpeed = qMax(60.0, leadSpeed - 5.0);
                    npc.speed += (targetFollowSpeed - npc.speed) * 3.4 * dt;
                } else if (npc.isSideLanePacer && qAbs(relDist) < 220.0) {
                    qreal targetPaceSpeed = qBound(85.0, m_player.speed + ((i % 2 == 0) ? 6.0 : -6.0), 230.0);
                    npc.speed += (targetPaceSpeed - npc.speed) * 1.8 * dt;
                    npc.isBraking = (npc.speed > targetPaceSpeed + 14.0);
                } else {
                    npc.isBraking = false;
                    if (npc.speed < npc.maxSpeed) {
                        npc.speed += 22.0 * dt;
                        if (npc.speed > npc.maxSpeed) npc.speed = npc.maxSpeed;
                    }
                }
            }

            // Blinker flashing animation
            npc.blinkerTimer += dt;
            npc.blinkerFlash = (fmod(npc.blinkerTimer, 0.28) < 0.14);

            // Lateral lane changing physics with turn signals
            if (qAbs(npc.x - npc.targetX) > 0.02) {
                npc.x += (npc.targetX - npc.x) * 2.4 * dt;
                if (!npc.hazardLights) {
                    if (npc.targetX < npc.x) {
                        npc.turnLeft = true;
                        npc.turnRight = false;
                    } else {
                        npc.turnRight = true;
                        npc.turnLeft = false;
                    }
                }
            } else {
                npc.x = npc.targetX;
                if (!npc.hazardLights) {
                    npc.turnLeft = false;
                    npc.turnRight = false;
                }
            }

            // Intelligent Overtaking AI with Pre-Turn Signal Warning
            npc.laneChangeTimer += dt;
            if (!npc.isCrashing && minLeadDist < 135.0 && npc.laneChangeTimer > 1.8) {
                npc.laneChangeTimer = 0;
                int curLane = npc.assignedLane;
                int newLane = curLane;

                if (curLane == 1) newLane = 2;
                else if (curLane == 3) newLane = 2;
                else newLane = (QRandomGenerator::global()->generate() % 2 == 0) ? 1 : 3;

                npc.assignedLane = newLane;
                npc.targetX = laneCoords[newLane];
            }
        }
    }
}

void GameEngine::checkCollisionsAndNearMisses(qreal dt) {
    if (m_player.collisionCooldown > 0) {
        m_player.collisionCooldown -= dt;
    }
    if (m_player.damageFlashTimer > 0) {
        m_player.damageFlashTimer -= dt;
    }

    int totalTrackLen = m_track.trackLength();

    for (int i = 0; i < m_traffic.size(); ++i) {
        NPCCar& npc = m_traffic[i];

        qreal relZ = npc.worldZ - m_player.worldZ;
        if (relZ < -totalTrackLen / 2) relZ += totalTrackLen;
        if (relZ > totalTrackLen / 2) relZ -= totalTrackLen;

        qreal lateralDist = qAbs(m_player.x - npc.x);
        qreal lateralOffset = m_player.x - npc.x; // Positive: player is to right of NPC

        // Realistic Multi-Phase Physical Collision
        // Exact Screen-Space Bumper-Touch Collision Detection:
        // Project the NPC car's actual screen Y position (accounting for hill elevation & camera pitch)
        // so a collision ONLY triggers when the two cars visibly touch on screen!
        bool carsTouchingOnScreen = false;
        qreal hitLateralWidth = (npc.type == VEHICLE_TRUCK_SEMI) ? 0.185 : 0.165;
        if (relZ > 60.0 && relZ < 1400.0 && lateralDist < hitLateralWidth) {
            int segLen = m_track.getInfo().segmentLength;
            int baseIdx = (int)(m_player.worldZ / segLen);
            qreal baseElev = m_track.getSegment(baseIdx).elevation;
            qreal eyeHeight = baseElev + 950.0;

            int npcSegIdx = (int)(npc.worldZ / segLen);
            qreal npcFrac = qBound(0.0, (npc.worldZ - npcSegIdx * (qreal)segLen) / (qreal)segLen, 1.0);
            qreal e1 = m_track.getSegment(npcSegIdx).elevation;
            qreal e2 = m_track.getSegment(npcSegIdx + 1).elevation;
            qreal npcElev = e1 + (e2 - e1) * npcFrac;

            qreal speedFovBoost = 1.0 + (m_player.speed / 400.0) * 0.15;
            qreal scale = (0.80 * speedFovBoost) / relZ;
            qreal horizon720 = 720.0 * 0.42 + m_player.pitchG * 140.0;
            qreal npcScreenY = horizon720 - (scale * (npcElev - eyeHeight) * 360.0);

            // In 3rd-person view, the player car's front nose is at y = 560..645 (on 720p reference).
            // In 1st-person view, the player car's front hood edge is at y = 500..570 (on 720p reference).
            qreal minTouchY = m_player.thirdPersonView ? 558.0 : 502.0;
            qreal maxTouchY = m_player.thirdPersonView ? 648.0 : 572.0;
            carsTouchingOnScreen = (npcScreenY >= minTouchY && npcScreenY <= maxTouchY);
        }

        if (m_player.collisionCooldown <= 0 && carsTouchingOnScreen) {
            // Rebound the collided vehicle slightly forward from the exact bumper contact point so both cars remain clearly visible!
            npc.worldZ = fmod(npc.worldZ + 45.0, (qreal)totalTrackLen);
            qreal deltaV = npc.isOncoming ? (m_player.speed + npc.speed) : qAbs(m_player.speed - npc.speed);
            qreal mass = (npc.type == VEHICLE_TRUCK_SEMI) ? 2.8 : ((npc.type == VEHICLE_COMPACT_BLUE) ? 0.75 : 1.0);
            int damageTaken = 0;

            if (npc.isOncoming) {
                // 1. Violent Head-On Crash (Delta V > 200 km/h)
                damageTaken = 48 + (QRandomGenerator::global()->generate() % 14);
                m_player.speed = qMin(30.0, m_player.speed * 0.20);
                m_player.impactCameraShake = 1.0;
                m_player.pitchG = -0.85;
                m_player.rollG = (lateralOffset >= 0 ? 0.95 : -0.95);
                m_player.steerAngle += (lateralOffset >= 0 ? 0.45 : -0.45);
                m_player.lateralRecoil = (lateralOffset >= 0 ? 0.35 : -0.35);

                // NPC knocked backwards and deflected towards shoulder
                npc.isCrashing = true;
                npc.speed = qMax(npc.speed, m_player.speed + 28.0);
                npc.crashRecoveryTimer = 3.2;
                npc.lateralVelocity = (lateralOffset >= 0 ? -2.6 : 2.6);
                npc.skidAngle = (lateralOffset >= 0 ? -0.55 : 0.55);
                npc.speed *= 0.35;

                spawnCrashSparks((m_player.worldZ + npc.worldZ) / 2.0, (m_player.x + npc.x) / 2.0, 48, true, 1.2);
            } else if (deltaV > 110) {
                // 2. High-Speed Rear-End Collision
                damageTaken = 28 + (QRandomGenerator::global()->generate() % 10);
                m_player.speed = qMax(35.0, m_player.speed * 0.42);
                m_player.impactCameraShake = 0.80;
                m_player.pitchG = -0.65;
                m_player.rollG = (lateralOffset >= 0 ? 0.60 : -0.60);
                m_player.steerAngle += (lateralOffset >= 0 ? 0.35 : -0.35);
                m_player.lateralRecoil = (lateralOffset >= 0 ? 0.25 : -0.25) / mass;

                // NPC shoved forward by impact momentum
                npc.isCrashing = true;
                npc.speed = qMax(npc.speed, m_player.speed + 28.0);
                npc.crashRecoveryTimer = 2.6;
                npc.speed = qMin(185.0, npc.speed + 38.0 / mass);
                npc.lateralVelocity = (lateralOffset >= 0 ? -1.8 : 1.8) / mass;
                npc.skidAngle = (lateralOffset >= 0 ? -0.35 : 0.35);

                spawnCrashSparks((m_player.worldZ + npc.worldZ) / 2.0, (m_player.x + npc.x) / 2.0, 36, false, 1.0);
            } else {
                // 3. Glancing Sideswipe Scrape
                damageTaken = 9 + (QRandomGenerator::global()->generate() % 6);
                m_player.speed = qMax(25.0, m_player.speed * 0.84);
                m_player.impactCameraShake = 0.45;
                m_player.rollG = (lateralOffset >= 0 ? 0.35 : -0.35);
                m_player.steerAngle += (lateralOffset >= 0 ? 0.22 : -0.22);
                m_player.lateralRecoil = (lateralOffset >= 0 ? 0.18 : -0.18);

                // NPC fishtails from lateral glance
                npc.isCrashing = true;
                npc.speed = qMax(npc.speed, m_player.speed + 28.0);
                npc.crashRecoveryTimer = 1.8;
                npc.lateralVelocity = (lateralOffset >= 0 ? -1.2 : 1.2);
                npc.skidAngle = (lateralOffset >= 0 ? -0.22 : 0.22);

                spawnCrashSparks((m_player.worldZ + npc.worldZ) / 2.0, (m_player.x + npc.x) / 2.0, 24, false, 0.7);
            }

            // Difficulty damage scaling
            qreal damageScale = ((m_difficulty == DIFFICULTY_EASY) ? 0.45 : ((m_difficulty == DIFFICULTY_PRO) ? 1.6 : 1.0)) / qMax(0.5, m_player.armorMultiplier);
            damageTaken = qMax(3, qRound(damageTaken * damageScale));

            m_player.health = qMax(0, m_player.health - damageTaken);
            m_player.crashCount++;
            m_player.collisionCooldown = 0.8;
            m_player.crashTimer = 3.0;
            m_player.damageFlashTimer = 1.2;
            m_player.nearMissCombo = 1;
            m_player.score = qMax(0, m_player.score - 300);

            if (m_player.health <= 0) {
                m_player.isWrecked = true;
                m_player.speed = 0;
                m_player.lastAnnouncement = "TOTAL WRECK! PRESS [R] TO TOW & REPAIR";
                m_player.announcementTimer = 999.0;
            } else {
                m_player.lastAnnouncement = QString("CRASH IMPACT! -%1% HEALTH (%2 HITS)").arg(damageTaken).arg(m_player.crashCount);
                m_player.announcementTimer = 2.2;
            }
            emit collisionOccurred();
        }

        // Near Miss Check
        if (!npc.nearMissAwarded && relZ < 680 && relZ > 380 && lateralDist < 0.36 && lateralDist >= hitLateralWidth && m_player.speed > 75) {
            npc.nearMissAwarded = true;
            int basePoints = npc.isOncoming ? 450 : 180;
            qreal scoreScale = (m_difficulty == DIFFICULTY_EASY) ? 1.0 : ((m_difficulty == DIFFICULTY_PRO) ? 2.5 : 1.5);
            int points = qRound(basePoints * m_player.nearMissCombo * scoreScale);
            m_player.score += points;
            m_player.nearMissCombo = qMin(8, m_player.nearMissCombo + 1);
            m_player.comboTimer = 3.8;

            if (npc.isOncoming) {
                m_player.lastAnnouncement = QString("ONCOMING CLOSE CALL! +%1 PTS (x%2)").arg(points).arg(m_player.nearMissCombo);
            } else {
                m_player.lastAnnouncement = QString("NEAR MISS! +%1 PTS (x%2)").arg(points).arg(m_player.nearMissCombo);
            }
            m_player.announcementTimer = 1.8;
            emit nearMissOccurred(points, m_player.nearMissCombo);
        }
    }
}

