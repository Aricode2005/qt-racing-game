#include "GameEngine.h"
#include <QtMath>
#include <QRandomGenerator>

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
    QString diffName = (diff == DIFFICULTY_EASY) ? "EASY" : ((diff == DIFFICULTY_PRO) ? "PRO (2.5x SCORE)" : "NORMAL");
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
    m_player.lastAnnouncement = "VEHICLE REPAIRED & READY!";
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
    qreal lateralSpeed = m_player.steerAngle * (qMax(45.0, m_player.speed) / 125.0) * 4.0 * dt;
    m_player.x += lateralSpeed;

    // Gentle centrifugal curve push so taking corners feels effortless and intuitive
    const RoadSegment& currentSeg = m_track.findSegment(m_player.worldZ);
    const RoadSegment& nextSeg = m_track.findSegment(m_player.worldZ + m_track.getInfo().segmentLength);
    qreal curveForce = currentSeg.curve * (m_player.speed / 300.0) * 0.32 * dt;
    m_player.x -= curveForce;

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
        qreal powerFactor = qBound(0.18, 1.0 - (m_player.speed / m_player.maxSpeed), 1.0);
        accel = m_player.accelRate * powerFactor;
        m_player.boostBar = qMin(1.0, m_player.boostBar + 1.2 * dt);
    } else {
        m_player.boostBar = qMax(0.0, m_player.boostBar - 2.5 * dt);
    }

    // Hill elevation slope effect (Gravity)
    qreal slope = (nextSeg.elevation - currentSeg.elevation) / (qreal)m_track.getInfo().segmentLength;
    qreal gravityForce = -slope * 35.0;
    accel += gravityForce;

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

    qreal drag = 0.0006 * m_player.speed * m_player.speed;
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
        m_player.speed = qBound(0.0, m_player.speed, m_player.maxSpeed);
    }

    qreal worldSpeed = (m_player.speed / 3.6) * 14.0 * dt;
    m_player.worldZ += worldSpeed;
    m_player.distanceTraveled += (m_player.speed / 3600.0) * dt;

    // Sector Transition Detection & Notification
    m_player.currentSector = currentSeg.sector;
    if (m_player.currentSector != m_player.lastSectorAnnounced && !currentSeg.sectorName.isEmpty()) {
        m_player.lastSectorAnnounced = m_player.currentSector;
        m_player.lastAnnouncement = QString("ZONE: SECTOR %1 - %2").arg(m_player.currentSector).arg(currentSeg.sectorName);
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

    if (s < 55) {
        m_player.gear = 1;
        m_player.rpm = 900 + (s / 55.0) * 6300.0;
    } else if (s < 105) {
        m_player.gear = 2;
        m_player.rpm = 3400 + ((s - 55.0) / 50.0) * 3800.0;
    } else if (s < 155) {
        m_player.gear = 3;
        m_player.rpm = 3900 + ((s - 105.0) / 50.0) * 3400.0;
    } else if (s < 205) {
        m_player.gear = 4;
        m_player.rpm = 4300 + ((s - 155.0) / 50.0) * 3100.0;
    } else if (s < 250) {
        m_player.gear = 5;
        m_player.rpm = 4800 + ((s - 205.0) / 45.0) * 2700.0;
    } else {
        m_player.gear = 6;
        m_player.rpm = 5300 + ((s - 250.0) / 50.0) * 2400.0;
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
        adj1.isSideLanePacer = true;
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

    // 3. Forward Traffic Platoons (+140 to +2400 units)
    // Distributed in realistic clusters of 2-3 cars with guaranteed open passing lines
    int remainingCount = totalTrafficCount - m_traffic.size();
    qreal curZ = m_player.worldZ + 160.0;

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
            curZ += 120.0 + (QRandomGenerator::global()->generate() % 90);
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

            // Cluster spacing: small gap within platoon (65-85m), larger gap between platoons (150-210m)
            if (i % 2 == 0) {
                curZ += 75.0 + (QRandomGenerator::global()->generate() % 30);
            } else {
                curZ += 165.0 + (QRandomGenerator::global()->generate() % 70);
            }
        }

        npc.worldZ = fmod(curZ, totalTrackLen);
        npc.maxSpeed = npc.speed;
        m_traffic.append(npc);
    }
}

void GameEngine::updateTraffic(qreal dt) {
    const qreal laneCoords[4] = { -0.75, -0.25, 0.25, 0.75 };
    int totalTrackLen = m_track.trackLength();

    for (int i = 0; i < m_traffic.size(); ++i) {
        NPCCar& npc = m_traffic[i];

        qreal worldSpeed = (npc.speed / 3.6) * 14.0 * dt;

        if (npc.isOncoming) {
            // Oncoming traffic moves towards player (decreasing Z)
            npc.worldZ -= worldSpeed;
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

            // Once oncoming car passes behind player (-220m), recycle ahead in oncoming Lane 0
            if (relDist < -220) {
                npc.worldZ = m_player.worldZ + 1100 + (QRandomGenerator::global()->generate() % 1600);
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

            // Continuous recycling: when car falls too far behind (-680m), recycle ahead
            if (relDist < -680) {
                npc.worldZ = m_player.worldZ + 950 + (QRandomGenerator::global()->generate() % 1450);
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
        if (m_player.collisionCooldown <= 0 && qAbs(relZ) < 88 && lateralDist < 0.28) {
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
                npc.crashRecoveryTimer = 1.8;
                npc.lateralVelocity = (lateralOffset >= 0 ? -1.2 : 1.2);
                npc.skidAngle = (lateralOffset >= 0 ? -0.22 : 0.22);

                spawnCrashSparks((m_player.worldZ + npc.worldZ) / 2.0, (m_player.x + npc.x) / 2.0, 24, false, 0.7);
            }

            // Difficulty damage scaling
            qreal damageScale = (m_difficulty == DIFFICULTY_EASY) ? 0.45 : ((m_difficulty == DIFFICULTY_PRO) ? 1.6 : 1.0);
            damageTaken = qMax(3, qRound(damageTaken * damageScale));

            m_player.health = qMax(0, m_player.health - damageTaken);
            m_player.crashCount++;
            m_player.collisionCooldown = 1.4;
            m_player.crashTimer = 2.5;
            m_player.damageFlashTimer = 0.65;
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
        if (!npc.nearMissAwarded && relZ < 0 && relZ > -110 && lateralDist < 0.42 && m_player.speed > 95) {
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

