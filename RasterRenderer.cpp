#include "RasterRenderer.h"
#include <QtMath>
#include <QPainterPath>
#include <QPolygonF>
#include <QRandomGenerator>

struct RenderableSprite {
    qreal camZ = 0;
    qreal screenX = 0;
    qreal screenY = 0;
    qreal scale = 0;
    QImage sprite;
    int width = 0;
    int height = 0;
    qreal rotation = 0;
    bool drawSkidSmoke = false;
};

RasterRenderer::RasterRenderer() {
}

void RasterRenderer::render(QPainter& painter, const GameEngine& engine, int screenWidth, int screenHeight) {
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const PlayerCar& player = engine.getPlayer();

    // Smooth camera heading offset for backdrop parallax
    const RoadSegment& currentSeg = engine.getTrack().findSegment(player.worldZ);
    m_cameraHeading += currentSeg.curve * (player.speed / 180.0) * 0.016;
    m_rainAnimTime += 0.016;

    if (player.lookingBack) {
        // LOOK-BACK REAR VIEW
        renderBackdrop(painter, engine, screenWidth, screenHeight);
        renderRoad(painter, engine, screenWidth, screenHeight, true);
        renderSceneryAndCars(painter, engine, screenWidth, screenHeight, true);
        renderRearCockpitView(painter, engine, screenWidth, screenHeight);
    } else if (player.povPan < -0.25) {
        // FULL PANORAMIC SIDEWAYS POV: LOOKING LEFT OUT DRIVER'S SIDE WINDOW (Q key)
        renderSidewaysPOV(painter, engine, screenWidth, screenHeight, true);
    } else if (player.povPan > 0.25) {
        // FULL PANORAMIC SIDEWAYS POV: LOOKING RIGHT OUT PASSENGER'S SIDE WINDOW (E key)
        renderSidewaysPOV(painter, engine, screenWidth, screenHeight, false);
    } else {
        // FORWARD COCKPIT VIEW WITH VISIBLE SIDE GLASSES & CARS
        painter.save();

        // 1. Dynamic road banking & chassis tilt & impact shockwave
        qreal shakeWorldX = 0;
        qreal shakeWorldY = 0;
        if (player.impactCameraShake > 0) {
            qreal sMag = player.impactCameraShake * 15.0;
            shakeWorldX = (QRandomGenerator::global()->generateDouble() * 2.0 - 1.0) * sMag;
            shakeWorldY = (QRandomGenerator::global()->generateDouble() * 2.0 - 1.0) * sMag;
        }
        painter.translate(screenWidth / 2.0, screenHeight / 2.0);
        painter.rotate(player.roadBanking * 57.2958);
        qreal panOffsetX = player.povPan * (screenWidth * 0.35);
        painter.translate(-screenWidth / 2.0 + panOffsetX + shakeWorldX, -screenHeight / 2.0 + shakeWorldY);

        renderBackdrop(painter, engine, screenWidth, screenHeight);
        renderRoad(painter, engine, screenWidth, screenHeight, false);
        renderSceneryAndCars(painter, engine, screenWidth, screenHeight, false);

        if (engine.getTimeOfDay() == TIME_NIGHT) {
            renderNightHeadlights(painter, engine, screenWidth, screenHeight);
        }

        painter.restore(); // Restore frame for interior cockpit

        // Side window glass tint & diagonal reflection highlights
        painter.drawImage(QRect(0, 0, screenWidth, screenHeight), RasterAssets::instance().getSideGlassOverlay());

        // Adjacent side lane cars passing right outside the side windows!
        renderSideWindowsAndAdjacentCars(painter, engine, screenWidth, screenHeight);

        // 3 Real-time Mirrors (Left side mirror, Center rearview mirror, Right side mirror)
        renderRearviewMirror(painter, engine, screenWidth, screenHeight);
        renderSideMirrors(painter, engine, screenWidth, screenHeight);

        // Dynamic Front Windshield Glass Scene (Sun glare, reflections, HUD)
        renderDynamicGlassReflections(painter, engine, screenWidth, screenHeight);

        // Cockpit dashboard, instruments, infotainment
        renderCockpit(painter, engine, screenWidth, screenHeight);

        // Dynamic Animated Windshield Wipers sweeping across the front glass
        int pitchOffset = qRound(player.pitchG * 90.0);
        int rollOffset = qRound(player.rollG * 60.0);
        renderAnimatedWipers(painter, engine, screenWidth, screenHeight, rollOffset, pitchOffset);

        // Windshield Holographic Head-Up Display (HUD)
        renderWindshieldHolographicHUD(painter, engine, screenWidth, screenHeight, rollOffset, pitchOffset);

        renderSteeringWheel(painter, engine, screenWidth, screenHeight);
        renderWindshieldEffects(painter, engine, screenWidth, screenHeight);
    }

    renderHUD(painter, engine, screenWidth, screenHeight);
}

void RasterRenderer::renderBackdrop(QPainter& p, const GameEngine& engine, int w, int h) {
    const TrackTheme theme = engine.getCurrentTheme();
    const TimeOfDay tod = engine.getTimeOfDay();
    const PlayerCar& player = engine.getPlayer();

    qreal horizonY = (h * 0.42);
    qreal pitchOffset = player.pitchG * 140.0;
    qreal currentHorizon = horizonY + pitchOffset;

    // 1. Sky and distant celestial backdrop (Clouds, Sun/Moon, Stars)
    const QImage& bg = RasterAssets::instance().getBackdrop(theme, tod);
    int bgW = bg.width();
    int bgH = bg.height();

    int bgOffset = (int)(m_cameraHeading * 320.0) % bgW;
    if (bgOffset < 0) bgOffset += bgW;

    int destH = qRound(currentHorizon + 35);
    int destY = 0;

    p.drawImage(QRect(0, destY, w, destH), bg, QRect(bgOffset, 0, bgW - bgOffset, bgH));
    if (bgOffset > 0) {
        int drawnW = (int)(((qreal)(bgW - bgOffset) / bgW) * w);
        p.drawImage(QRect(drawnW, destY, w - drawnW, destH), bg, QRect(0, 0, bgOffset, bgH));
    }

    // 2. Dynamic drifting atmospheric cloud layers
    if (tod != TIME_NIGHT) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        qreal cloudShift = player.distanceTraveled * 120.0 + m_rainAnimTime * 35.0;
        QColor cloudCol = (tod == TIME_SUNSET) ? QColor(255, 190, 150, 75) : QColor(255, 255, 255, 85);
        p.setBrush(cloudCol);
        p.setPen(Qt::NoPen);

        for (int c = 0; c < 5; ++c) {
            qreal cx = fmod(c * 260.0 + cloudShift * (0.35 + c * 0.12) - (m_cameraHeading * 240.0), w + 300.0) - 150.0;
            qreal cy = 18 + (c % 3) * 22 + (int)(pitchOffset * 0.4);
            int cw = 150 + (c % 3) * 45;
            int ch = 34 + (c % 2) * 14;
            p.drawRoundedRect(QRectF(cx, cy, cw, ch), ch / 2.0, ch / 2.0);
            p.drawRoundedRect(QRectF(cx + 25, cy - 12, cw * 0.55, ch * 0.9), ch * 0.45, ch * 0.45);
        }
        p.restore();
    }

    // 3. The "Traffic Rider" Approaching Mountain/Scenery Illusion
    renderApproachingMountains(p, engine, w, h, currentHorizon);
}

void RasterRenderer::renderApproachingMountains(QPainter& p, const GameEngine& engine, int w, int h, qreal horizonY) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    const TrackTheme theme = engine.getCurrentTheme();
    const TimeOfDay tod = engine.getTimeOfDay();
    const PlayerCar& player = engine.getPlayer();
    qreal worldZ = player.worldZ;
    qreal speed = player.speed;

    // Traffic Rider Illusion:
    // Continuous approach progress scaling with distance traveled and forward velocity
    qreal approachCycle = fmod(worldZ * 0.00015, 1.0);
    qreal approachScale = 0.86 + approachCycle * 0.46 + (speed / 300.0) * 0.16;

    // Palette setup based on Theme & TimeOfDay
    QColor farLit, farShad, nearLit, nearShad, snowLit, snowShad, rimCol;
    if (theme == THEME_ALPINE_CLIMB) {
        if (tod == TIME_NIGHT) {
            farLit = QColor(28, 38, 56);
            farShad = QColor(14, 20, 32);
            nearLit = QColor(36, 48, 70);
            nearShad = QColor(18, 24, 38);
            snowLit = QColor(95, 115, 150);
            snowShad = QColor(48, 62, 85);
            rimCol = QColor(140, 165, 205);
        } else if (tod == TIME_SUNSET) {
            farLit = QColor(195, 105, 115);
            farShad = QColor(115, 52, 72);
            nearLit = QColor(170, 75, 92);
            nearShad = QColor(88, 32, 48);
            snowLit = QColor(255, 215, 190);
            snowShad = QColor(165, 85, 105);
            rimCol = QColor(255, 230, 180);
        } else {
            farLit = QColor(130, 160, 195);
            farShad = QColor(82, 105, 138);
            nearLit = QColor(105, 130, 165);
            nearShad = QColor(56, 75, 105);
            snowLit = QColor(255, 255, 255);
            snowShad = QColor(205, 222, 245);
            rimCol = QColor(255, 255, 255);
        }
    } else if (theme == THEME_RED_ROCK) {
        if (tod == TIME_NIGHT) {
            farLit = QColor(50, 26, 22);
            farShad = QColor(26, 13, 12);
            nearLit = QColor(42, 20, 16);
            nearShad = QColor(20, 10, 8);
            rimCol = QColor(85, 45, 35);
        } else if (tod == TIME_SUNSET) {
            farLit = QColor(240, 95, 45);
            farShad = QColor(135, 40, 25);
            nearLit = QColor(210, 65, 30);
            nearShad = QColor(95, 25, 18);
            rimCol = QColor(255, 180, 80);
        } else {
            farLit = QColor(215, 120, 60);
            farShad = QColor(145, 70, 35);
            nearLit = QColor(190, 95, 45);
            nearShad = QColor(120, 48, 25);
            rimCol = QColor(248, 165, 90);
        }
    } else if (theme == THEME_SUNSET_COAST) {
        if (tod == TIME_NIGHT) {
            farLit = QColor(24, 32, 46);
            farShad = QColor(12, 18, 26);
            nearLit = QColor(32, 42, 60);
            nearShad = QColor(16, 22, 34);
            rimCol = QColor(80, 110, 150);
        } else if (tod == TIME_SUNSET) {
            farLit = QColor(210, 100, 65);
            farShad = QColor(110, 42, 55);
            nearLit = QColor(165, 65, 60);
            nearShad = QColor(75, 25, 40);
            rimCol = QColor(255, 190, 90);
        } else {
            farLit = QColor(105, 160, 135);
            farShad = QColor(65, 110, 92);
            nearLit = QColor(82, 138, 108);
            nearShad = QColor(46, 82, 64);
            rimCol = QColor(160, 215, 180);
        }
    } else { // THEME_NEON_METROPOLIS
        farLit = QColor(32, 42, 65);
        farShad = QColor(16, 20, 34);
        nearLit = QColor(42, 54, 82);
        nearShad = QColor(22, 28, 44);
        rimCol = QColor(0, 240, 255);
    }

    // -------------------------------------------------------------
    // LAYER 1: FAR TOWERING MOUNTAIN MASSIF
    // -------------------------------------------------------------
    qreal farScroll = -(m_cameraHeading * 220.0) + fmod(worldZ * 0.006, 1200.0);
    qreal farH = 110.0 * (0.85 + approachCycle * 0.35);

    const int numFarPeaks = 8;
    const qreal farSpacing = 280.0;
    for (int i = -2; i < numFarPeaks + 2; ++i) {
        qreal peakBaseX = i * farSpacing + fmod(farScroll, farSpacing);
        qreal peakApexX = peakBaseX + 130.0;
        qreal peakApexY = horizonY - farH * (0.85 + (i % 3) * 0.22);
        qreal peakLeftX = peakBaseX - 60.0;
        qreal peakRightX = peakBaseX + 320.0;
        qreal peakRidgeX = peakBaseX + 135.0;

        // Lit Left Facet
        QPolygonF litPoly;
        litPoly << QPointF(peakLeftX, horizonY) << QPointF(peakApexX, peakApexY) << QPointF(peakRidgeX, horizonY);
        p.setBrush(farLit);
        p.setPen(Qt::NoPen);
        p.drawPolygon(litPoly);

        // Shadowed Right Facet
        QPolygonF shadPoly;
        shadPoly << QPointF(peakRidgeX, horizonY) << QPointF(peakApexX, peakApexY) << QPointF(peakRightX, horizonY);
        p.setBrush(farShad);
        p.drawPolygon(shadPoly);

        // Snow Cap / Mesa Top / Neon Spire
        if (theme == THEME_ALPINE_CLIMB) {
            qreal capH = (horizonY - peakApexY) * 0.32;
            qreal capY = peakApexY + capH;
            qreal capLeftX = peakApexX - 35.0;
            qreal capRightX = peakApexX + 45.0;

            QPolygonF snowLitPoly;
            snowLitPoly << QPointF(capLeftX, capY) << QPointF(peakApexX, peakApexY) << QPointF(peakRidgeX, capY);
            p.setBrush(snowLit);
            p.drawPolygon(snowLitPoly);

            QPolygonF snowShadPoly;
            snowShadPoly << QPointF(peakRidgeX, capY) << QPointF(peakApexX, peakApexY) << QPointF(capRightX, capY);
            p.setBrush(snowShad);
            p.drawPolygon(snowShadPoly);
        } else if (theme == THEME_RED_ROCK) {
            p.setPen(QPen(rimCol, 2));
            p.drawLine(QPointF(peakApexX - 25, peakApexY), QPointF(peakApexX + 25, peakApexY));
            p.setPen(Qt::NoPen);
        } else if (theme == THEME_NEON_METROPOLIS) {
            p.setPen(QPen(rimCol, 2));
            p.drawLine(QPointF(peakApexX, peakApexY), QPointF(peakApexX, peakApexY - 24));
            p.setPen(Qt::NoPen);
        }
    }

    // -------------------------------------------------------------
    // LAYER 2: NEAR APPROACHING MOUNTAIN PASS (The Traffic Rider Illusion)
    // Towering crags that visibly grow taller, approach, and reveal passes
    // -------------------------------------------------------------
    qreal nearScroll = -(m_cameraHeading * 390.0) + fmod(worldZ * 0.022, 960.0);
    qreal nearH = 165.0 * approachScale;

    const int numNearPeaks = 6;
    const qreal nearSpacing = 360.0;
    for (int i = -2; i < numNearPeaks + 2; ++i) {
        qreal peakBaseX = i * nearSpacing + fmod(nearScroll, nearSpacing);
        qreal peakApexX = peakBaseX + 160.0;
        qreal peakApexY = horizonY - nearH * (0.80 + ((i + 1) % 4) * 0.20);
        qreal peakLeftX = peakBaseX - 40.0;
        qreal peakRightX = peakBaseX + 380.0;
        qreal peakRidgeX = peakBaseX + 170.0;

        // Lit Left Slope
        QPolygonF litPoly;
        litPoly << QPointF(peakLeftX, horizonY) << QPointF(peakApexX, peakApexY) << QPointF(peakRidgeX, horizonY);
        p.setBrush(nearLit);
        p.setPen(Qt::NoPen);
        p.drawPolygon(litPoly);

        // Shadowed Right Slope
        QPolygonF shadPoly;
        shadPoly << QPointF(peakRidgeX, horizonY) << QPointF(peakApexX, peakApexY) << QPointF(peakRightX, horizonY);
        p.setBrush(nearShad);
        p.drawPolygon(shadPoly);

        // Dramatic Mountain Crest Highlights
        p.setPen(QPen(rimCol, 1.5));
        p.drawLine(QPointF(peakLeftX, horizonY), QPointF(peakApexX, peakApexY));
        p.setPen(Qt::NoPen);

        // Snow Cap / Mesa Strata / Neon Grid
        if (theme == THEME_ALPINE_CLIMB) {
            qreal capH = (horizonY - peakApexY) * 0.28;
            qreal capY = peakApexY + capH;
            qreal capLeftX = peakApexX - 42.0;
            qreal capRightX = peakApexX + 52.0;

            QPolygonF snowLitPoly;
            snowLitPoly << QPointF(capLeftX, capY) << QPointF(peakApexX, peakApexY) << QPointF(peakRidgeX, capY);
            p.setBrush(snowLit);
            p.drawPolygon(snowLitPoly);

            QPolygonF snowShadPoly;
            snowShadPoly << QPointF(peakRidgeX, capY) << QPointF(peakApexX, peakApexY) << QPointF(capRightX, capY);
            p.setBrush(snowShad);
            p.drawPolygon(snowShadPoly);
        } else if (theme == THEME_RED_ROCK) {
            p.setPen(QPen(rimCol, 2));
            qreal bandY1 = peakApexY + 28 * approachScale;
            qreal bandY2 = peakApexY + 65 * approachScale;
            p.drawLine(QPointF(peakApexX - 35, bandY1), QPointF(peakApexX + 45, bandY1));
            p.drawLine(QPointF(peakApexX - 65, bandY2), QPointF(peakApexX + 85, bandY2));
            p.setPen(Qt::NoPen);
        } else if (theme == THEME_NEON_METROPOLIS) {
            p.setPen(QColor(0, 240, 255, 180));
            for (int wy = (int)peakApexY + 20; wy < (int)horizonY - 10; wy += 14) {
                p.drawPoint(peakApexX - 10, wy);
                p.drawPoint(peakApexX + 10, wy);
                p.drawPoint(peakApexX - 25, wy + 6);
                p.drawPoint(peakApexX + 25, wy + 6);
            }
            p.setPen(Qt::NoPen);
        }
    }

    // -------------------------------------------------------------
    // LAYER 3: HORIZON FOOTHILLS & ATMOSPHERIC ROAD-BASE HAZE
    // Anchors the mountains cleanly right at the road vanishing point
    // -------------------------------------------------------------
    QPainterPath foothills;
    foothills.moveTo(0, horizonY);
    qreal footScroll = -(m_cameraHeading * 480.0) + fmod(worldZ * 0.04, 600.0);
    for (int fx = 0; fx <= w; fx += 50) {
        qreal fy = horizonY - 26.0 - 14.0 * qSin((fx + footScroll) * 0.015) - 8.0 * qCos((fx + footScroll) * 0.035);
        foothills.lineTo(fx, fy);
    }
    foothills.lineTo(w, horizonY + 2);
    foothills.lineTo(0, horizonY + 2);
    foothills.closeSubpath();
    QColor footColor = (theme == THEME_RED_ROCK) ? QColor(95, 42, 22) :
                       (theme == THEME_ALPINE_CLIMB ? QColor(36, 48, 38) :
                       (theme == THEME_SUNSET_COAST ? QColor(48, 62, 45) : QColor(18, 22, 35)));
    p.fillPath(foothills, footColor);

    // Soft atmospheric haze band across the mountain base
    QLinearGradient hazeGrad(0, horizonY - 45, 0, horizonY + 2);
    QColor hazeCol = (tod == TIME_NIGHT) ? QColor(10, 14, 22, 130) :
                     (tod == TIME_SUNSET ? QColor(245, 140, 75, 110) : QColor(210, 230, 255, 120));
    hazeGrad.setColorAt(0.0, QColor(hazeCol.red(), hazeCol.green(), hazeCol.blue(), 0));
    hazeGrad.setColorAt(1.0, hazeCol);
    p.fillRect(0, horizonY - 45, w, 47, hazeGrad);

    p.restore();
}

void RasterRenderer::renderRoad(QPainter& p, const GameEngine& engine, int w, int h, bool isMirror, qreal mirrorOffsetAngle) {
    const Track& track = engine.getTrack();
    const PlayerCar& player = engine.getPlayer();
    int segLen = track.getInfo().segmentLength;
    qreal roadWidth = track.getInfo().roadWidth;
    int laneCount = 4;

    qreal horizonY = isMirror ? (h * 0.50) : (h * 0.42);
    qreal currentHorizon = horizonY + (player.pitchG * 140.0);

    qreal camDepth = 0.80;
    qreal camHeight = 950;
    qreal camX = player.x * roadWidth + mirrorOffsetAngle;

    int baseIndex = (int)(player.worldZ / segLen);
    qreal baseElevation = track.getSegment(baseIndex).elevation;
    qreal eyeHeight = baseElevation + camHeight;
    qreal camZ = player.worldZ;

    int drawDistance = isMirror ? 90 : 180;
    qreal maxy = h;

    qreal x = 0;
    qreal dx = 0;

    QVector<RoadSegment> segs;
    segs.reserve(drawDistance);

    for (int n = 0; n < drawDistance; ++n) {
        int segIdx = isMirror ? (baseIndex - n) : (baseIndex + n);
        RoadSegment seg = track.getSegment(segIdx);

        qreal segZ1 = (isMirror ? (baseIndex - n) : (baseIndex + n)) * segLen;
        qreal segZ2 = (isMirror ? (baseIndex - n - 1) : (baseIndex + n + 1)) * segLen;

        qreal curCurve = isMirror ? -seg.curve : seg.curve;
        x += dx;
        dx += curCurve;

        seg.p1.worldX = x;
        seg.p1.worldY = seg.elevation;
        seg.p1.worldZ = segZ1;

        seg.p2.worldX = x + dx;
        seg.p2.worldY = isMirror ? track.getSegment(segIdx - 1).elevation : track.getSegment(segIdx + 1).elevation;
        seg.p2.worldZ = segZ2;

        qreal p1CamZ = isMirror ? (camZ - seg.p1.worldZ) : (seg.p1.worldZ - camZ);
        qreal p2CamZ = isMirror ? (camZ - seg.p2.worldZ) : (seg.p2.worldZ - camZ);

        if (p1CamZ <= 1.0) p1CamZ = 1.0;
        if (p2CamZ <= 1.0) p2CamZ = 1.0;

        seg.p1.camZ = p1CamZ;
        seg.p2.camZ = p2CamZ;

        seg.p1.scale = camDepth / p1CamZ;
        seg.p2.scale = camDepth / p2CamZ;

        qreal p1CamX = seg.p1.worldX - (isMirror ? -camX : camX);
        qreal p2CamX = seg.p2.worldX - (isMirror ? -camX : camX);

        seg.p1.screenX = qRound((w / 2.0) + (seg.p1.scale * p1CamX * (w / 2.0)));
        seg.p1.screenY = qRound(currentHorizon - (seg.p1.scale * (seg.p1.worldY - eyeHeight) * (h / 2.0)));
        seg.p1.screenW = qRound(seg.p1.scale * roadWidth * (w / 2.0));

        seg.p2.screenX = qRound((w / 2.0) + (seg.p2.scale * p2CamX * (w / 2.0)));
        seg.p2.screenY = qRound(currentHorizon - (seg.p2.scale * (seg.p2.worldY - eyeHeight) * (h / 2.0)));
        seg.p2.screenW = qRound(seg.p2.scale * roadWidth * (w / 2.0));

        segs.append(seg);
    }

    TimeOfDay tod = engine.getTimeOfDay();

    for (int n = 0; n < segs.size(); ++n) {
        const RoadSegment& s = segs[n];

        if (s.p1.camZ <= 1.0 || s.p2.screenY >= s.p1.screenY || s.p2.screenY >= maxy) {
            continue;
        }

        // Terrain Color
        QColor terrainColor;
        if (tod == TIME_NIGHT) {
            terrainColor = (s.index % 2 == 0) ? QColor(14, 16, 24) : QColor(10, 12, 18);
        } else if (tod == TIME_DAY) {
            if (engine.getCurrentTheme() == THEME_RED_ROCK) {
                terrainColor = (s.index % 2 == 0) ? QColor(175, 105, 55) : QColor(165, 95, 48);
            } else {
                terrainColor = (s.index % 2 == 0) ? QColor(58, 88, 48) : QColor(52, 78, 42);
            }
        } else {
            if (engine.getCurrentTheme() == THEME_RED_ROCK) {
                terrainColor = (s.index % 2 == 0) ? QColor(145, 75, 40) : QColor(135, 65, 34);
            } else {
                terrainColor = (s.index % 2 == 0) ? QColor(48, 70, 42) : QColor(42, 62, 36);
            }
        }

        p.fillRect(0, s.p2.screenY, w, s.p1.screenY - s.p2.screenY + 1, terrainColor);

        // Rumble Strips
        qreal r1w = s.p1.screenW * 1.12;
        qreal r2w = s.p2.screenW * 1.12;
        QColor rumbleColor;
        if (tod == TIME_NIGHT) {
            rumbleColor = s.isRumbleRed ? QColor(160, 25, 25) : QColor(150, 150, 160);
        } else {
            rumbleColor = s.isRumbleRed ? QColor(220, 38, 38) : QColor(245, 245, 245);
        }

        QPolygonF rumblePoly;
        rumblePoly << QPointF(s.p1.screenX - r1w, s.p1.screenY)
                   << QPointF(s.p2.screenX - r2w, s.p2.screenY)
                   << QPointF(s.p2.screenX + r2w, s.p2.screenY)
                   << QPointF(s.p1.screenX + r1w, s.p1.screenY);
        p.setBrush(rumbleColor);
        p.setPen(Qt::NoPen);
        p.drawPolygon(rumblePoly);

        // Asphalt Road Surface
        QColor roadColor;
        if (tod == TIME_NIGHT) {
            roadColor = (s.index % 2 == 0) ? QColor(20, 22, 28) : QColor(16, 18, 24);
        } else if (tod == TIME_DAY) {
            roadColor = (s.index % 2 == 0) ? QColor(54, 56, 62) : QColor(48, 50, 56);
        } else {
            roadColor = (s.index % 2 == 0) ? QColor(44, 46, 52) : QColor(38, 40, 46);
        }

        QPolygonF roadPoly;
        roadPoly << QPointF(s.p1.screenX - s.p1.screenW, s.p1.screenY)
                 << QPointF(s.p2.screenX - s.p2.screenW, s.p2.screenY)
                 << QPointF(s.p2.screenX + s.p2.screenW, s.p2.screenY)
                 << QPointF(s.p1.screenX + s.p1.screenW, s.p1.screenY);
        p.setBrush(roadColor);
        p.drawPolygon(roadPoly);

        // Lane Dividers
        for (int l = 1; l < laneCount; ++l) {
            qreal laneRatio = -1.0 + (2.0 * l) / (qreal)laneCount;
            qreal p1lx = s.p1.screenX + s.p1.screenW * laneRatio;
            qreal p2lx = s.p2.screenX + s.p2.screenW * laneRatio;
            qreal lw1 = qMax(1.0, s.p1.screenW * 0.018);
            qreal lw2 = qMax(1.0, s.p2.screenW * 0.018);

            if (l == 1) {
                // Solid Double Yellow Line separating Lane 0 (Oncoming) from Lanes 1-3 (Same-Direction)
                QColor yellowColor = (tod == TIME_NIGHT) ? QColor(210, 160, 20) : QColor(255, 205, 30);
                p.setBrush(yellowColor);

                qreal off1 = lw1 * 0.9;
                qreal off2 = lw2 * 0.9;
                QPolygonF yPoly1;
                yPoly1 << QPointF(p1lx - off1 - lw1 * 0.4, s.p1.screenY)
                       << QPointF(p2lx - off2 - lw2 * 0.4, s.p2.screenY)
                       << QPointF(p2lx - off2 + lw2 * 0.4, s.p2.screenY)
                       << QPointF(p1lx - off1 + lw1 * 0.4, s.p1.screenY);
                p.drawPolygon(yPoly1);

                QPolygonF yPoly2;
                yPoly2 << QPointF(p1lx + off1 - lw1 * 0.4, s.p1.screenY)
                       << QPointF(p2lx + off2 - lw2 * 0.4, s.p2.screenY)
                       << QPointF(p2lx + off2 + lw2 * 0.4, s.p2.screenY)
                       << QPointF(p1lx + off1 + lw1 * 0.4, s.p1.screenY);
                p.drawPolygon(yPoly2);
            } else {
                // Dashed White dividing lines between same-direction lanes
                if ((s.index / 2) % 2 == 0) {
                    p.setBrush((tod == TIME_NIGHT) ? QColor(180, 185, 200) : QColor(245, 245, 250));
                    QPolygonF dashPoly;
                    dashPoly << QPointF(p1lx - lw1, s.p1.screenY)
                             << QPointF(p2lx - lw2, s.p2.screenY)
                             << QPointF(p2lx + lw2, s.p2.screenY)
                             << QPointF(p1lx + lw1, s.p1.screenY);
                    p.drawPolygon(dashPoly);
                }
            }
        }

        maxy = s.p2.screenY;
    }
}

void RasterRenderer::renderSceneryAndCars(QPainter& p, const GameEngine& engine, int w, int h, bool isMirror, qreal mirrorOffsetAngle) {
    const Track& track = engine.getTrack();
    const PlayerCar& player = engine.getPlayer();
    int segLen = track.getInfo().segmentLength;
    qreal roadWidth = track.getInfo().roadWidth;
    qreal camDepth = 0.80;
    qreal camHeight = 950;
    qreal camX = player.x * roadWidth + mirrorOffsetAngle;

    int baseIndex = (int)(player.worldZ / segLen);
    qreal baseElevation = track.getSegment(baseIndex).elevation;
    qreal eyeHeight = baseElevation + camHeight;
    qreal camZ = player.worldZ;

    qreal horizonY = isMirror ? (h * 0.50) : (h * 0.42);
    qreal currentHorizon = horizonY + (player.pitchG * 140.0);

    int drawDist = isMirror ? 90 : 180;
    QVector<RenderableSprite> spritesToDraw;

    for (int n = drawDist; n >= 0; --n) {
        int segIdx = isMirror ? (baseIndex - n) : (baseIndex + n);
        const RoadSegment& seg = track.getSegment(segIdx);
        qreal segZ = isMirror ? (camZ - n * segLen) : (camZ + n * segLen);

        qreal relCamZ = isMirror ? (camZ - segZ) : (segZ - camZ);
        if (relCamZ <= 80) continue;

        qreal scale = camDepth / relCamZ;
        qreal screenY = currentHorizon - (scale * (seg.elevation - eyeHeight) * (h / 2.0));
        qreal projScale = scale * (w / 2.0);

        auto addScenery = [&](SceneryType st, qreal lateralOffset) {
            const QImage& sImg = RasterAssets::instance().getScenery(st);
            qreal screenX = (w / 2.0) + (scale * (lateralOffset * roadWidth - (isMirror ? -camX : camX)) * (w / 2.0));

            int sw = 0;
            int sh = 0;
            if (st == SCENERY_HIGHWAY_GANTRY) {
                sw = qRound(projScale * 2500.0);
                sh = qRound(sw * ((qreal)sImg.height() / sImg.width()));
            } else if (st == SCENERY_WIND_TURBINE) {
                sh = qRound(projScale * 2400.0);
                sw = qRound(sh * ((qreal)sImg.width() / sImg.height()));
            } else if (st == SCENERY_ROCK_ARCH) {
                sw = qRound(projScale * 1900.0);
                sh = qRound(sw * ((qreal)sImg.height() / sImg.width()));
            } else if (st == SCENERY_PALM_TREE || st == SCENERY_PINE_TREE) {
                sh = qRound(projScale * 1250.0);
                sw = qRound(sh * ((qreal)sImg.width() / sImg.height()));
            } else if (st == SCENERY_STREETLIGHT) {
                sh = qRound(projScale * 1150.0);
                sw = qRound(sh * ((qreal)sImg.width() / sImg.height()));
            } else if (st == SCENERY_BILLBOARD_1 || st == SCENERY_BILLBOARD_2) {
                sw = qRound(projScale * 2550.0);
                sh = qRound(sw * ((qreal)sImg.height() / sImg.width()));
            } else if (st == SCENERY_HOARDING_NEON) {
                sw = qRound(projScale * 2750.0);
                sh = qRound(sw * ((qreal)sImg.height() / sImg.width()));
            } else if (st == SCENERY_UTILITY_POLE) {
                sh = qRound(projScale * 1450.0);
                sw = qRound(sh * ((qreal)sImg.width() / sImg.height()));
            } else if (st == SCENERY_BOULDER || st == SCENERY_CACTUS) {
                sw = qRound(projScale * 800.0);
                sh = qRound(sw * ((qreal)sImg.height() / sImg.width()));
            } else {
                sh = qRound(projScale * 950.0);
                sw = qRound(sh * ((qreal)sImg.width() / sImg.height()));
            }

            if (sw >= 3 && screenX + sw / 2 > -80 && screenX - sw / 2 < w + 80 && screenY > 30 && screenY < h + 250) {
                RenderableSprite rs;
                rs.camZ = relCamZ;
                rs.screenX = screenX;
                rs.screenY = screenY;
                rs.scale = scale;
                rs.sprite = sImg;
                rs.width = sw;
                rs.height = sh;
                spritesToDraw.append(rs);
            }
        };

        // Highway Overhead Gantry (Centered across road lanes)
        if (seg.hasGantry) {
            addScenery(SCENERY_HIGHWAY_GANTRY, 0.0);
        }
        if (seg.hasSceneryLeft) {
            addScenery(seg.sceneryLeft, seg.sceneryLeftX);
        }
        if (seg.hasSceneryRight) {
            addScenery(seg.sceneryRight, seg.sceneryRightX);
        }
    }

    const QVector<NPCCar>& traffic = engine.getTraffic();
    int totalTrackLen = track.trackLength();

    for (const NPCCar& npc : traffic) {
        qreal relZ = npc.worldZ - camZ;
        if (relZ < -totalTrackLen / 2) relZ += totalTrackLen;
        if (relZ > totalTrackLen / 2) relZ -= totalTrackLen;

        // Front POV traffic is visible up to 8,000 units ahead; mirror traffic up to 2,800 units behind
        bool visibleInThisPass = isMirror ? (relZ < 0 && relZ > -2800) : (relZ > 0 && relZ < 8000);
        if (!visibleInThisPass) continue;

        qreal relCamZ = isMirror ? -relZ : relZ;
        if (relCamZ <= 50) continue;

        qreal scale = camDepth / relCamZ;
        const RoadSegment& npcSeg = track.findSegment(npc.worldZ);
        qreal screenY = currentHorizon - (scale * (npcSeg.elevation - eyeHeight) * (h / 2.0));
        qreal screenX = (w / 2.0) + (scale * (npc.x * roadWidth - (isMirror ? -camX : camX)) * (w / 2.0));

        // Orientation logic:
        // Oncoming cars face player (show headlights/front view) in forward view, and face away (rear view) in mirror.
        // Same-direction cars face away (rear view) in forward view, and face player (front view) in mirror.
        bool showRear = isMirror ? npc.isOncoming : !npc.isOncoming;
        bool flashLeft = (npc.turnLeft || npc.hazardLights) && npc.blinkerFlash;
        bool flashRight = (npc.turnRight || npc.hazardLights) && npc.blinkerFlash;
        const QImage& carImg = RasterAssets::instance().getVehicle(npc.type, showRear, npc.isBraking, flashLeft, flashRight);

        // Perspective car projection (a car is ~470 world units wide, semi-truck ~580)
        qreal projScale = scale * (w / 2.0);
        qreal worldCarW = (npc.type == VEHICLE_TRUCK_SEMI) ? 580.0 : 470.0;
        int cw = qRound(projScale * worldCarW);
        int ch = qRound(cw * ((qreal)carImg.height() / carImg.width()));

        if (cw >= 3 && screenX + cw / 2 > -80 && screenX - cw / 2 < w + 80 && screenY > 30 && screenY < h + 250) {
            RenderableSprite rs;
            rs.camZ = relCamZ;
            rs.screenX = screenX;
            rs.screenY = screenY;
            rs.scale = scale;
            rs.sprite = carImg;
            rs.width = cw;
            rs.height = ch;
            rs.rotation = isMirror ? -npc.skidAngle : npc.skidAngle;
            rs.drawSkidSmoke = npc.isCrashing && (qAbs(npc.skidAngle) > 0.06);
            spritesToDraw.append(rs);
        }
    }

    std::sort(spritesToDraw.begin(), spritesToDraw.end(), [](const RenderableSprite& a, const RenderableSprite& b) {
        return a.camZ > b.camZ;
    });

    for (const RenderableSprite& s : spritesToDraw) {
        p.save();
        p.translate(s.screenX, s.screenY - s.height / 2.0);
        if (qAbs(s.rotation) > 0.001) {
            p.rotate(s.rotation * 57.2958);
        }
        QRect destRect(-s.width / 2, -s.height / 2, s.width, s.height);
        p.drawImage(destRect, s.sprite);

        // Realistic tire skid smoke puffs billowed across asphalt during crashes
        if (s.drawSkidSmoke) {
            p.setPen(Qt::NoPen);
            int smokeRadius = qMax(4, qRound(s.width * 0.16));
            QRadialGradient smokeGrad(0, s.height * 0.42, smokeRadius);
            smokeGrad.setColorAt(0.0, QColor(220, 220, 225, 170));
            smokeGrad.setColorAt(0.6, QColor(190, 195, 205, 90));
            smokeGrad.setColorAt(1.0, QColor(170, 175, 185, 0));
            p.setBrush(smokeGrad);
            p.drawEllipse(QPointF(-s.width * 0.30, s.height * 0.42), smokeRadius, smokeRadius * 0.7);
            p.drawEllipse(QPointF(s.width * 0.30, s.height * 0.42), smokeRadius, smokeRadius * 0.7);
        }
        p.restore();
    }

    // 3D Crash Particles (Glowing friction sparks, crystalline glass shards, expanding smoke puffs)
    const QVector<CrashSpark>& sparks = engine.getSparks();
    for (const CrashSpark& spk : sparks) {
        if (spk.life <= 0) continue;
        qreal relZ = spk.worldZ - camZ;
        if (relZ < -totalTrackLen / 2) relZ += totalTrackLen;
        if (relZ > totalTrackLen / 2) relZ -= totalTrackLen;

        bool visibleInThisPass = isMirror ? (relZ < 0 && relZ > -2500) : (relZ > 0 && relZ < 5000);
        if (!visibleInThisPass) continue;

        qreal relCamZ = isMirror ? -relZ : relZ;
        if (relCamZ < 450) continue; // Close-range collision sparks rendered on windshield layer

        qreal scale = camDepth / relCamZ;
        const RoadSegment& spkSeg = track.findSegment(spk.worldZ);
        qreal screenY = currentHorizon - (scale * (spkSeg.elevation + spk.y - eyeHeight) * (h / 2.0));
        qreal screenX = (w / 2.0) + (scale * (spk.x * roadWidth - (isMirror ? -camX : camX)) * (w / 2.0));

        qreal alphaRatio = spk.life / spk.maxLife;
        int alpha = qBound(0, (int)(alphaRatio * 255), 255);
        qreal drawSize = qMax(3.0, spk.size * scale * (w / 2.0) * 0.45);

        if (screenX >= -50 && screenX <= w + 50 && screenY >= -50 && screenY <= h + 50) {
            p.save();
            p.setRenderHint(QPainter::Antialiasing, true);
            if (spk.type == 0) {
                // Radiant golden metal friction spark
                QRadialGradient spkGrad(screenX, screenY, drawSize * 1.5);
                spkGrad.setColorAt(0.0, QColor(255, 255, 220, alpha));
                spkGrad.setColorAt(0.4, QColor(255, 190, 40, (int)(alpha * 0.85)));
                spkGrad.setColorAt(1.0, QColor(255, 90, 10, 0));
                p.setBrush(spkGrad);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(screenX, screenY), drawSize * 1.5, drawSize * 1.5);
            } else if (spk.type == 1) {
                // Crystalline shattered glass shard
                p.setPen(QColor(230, 245, 255, alpha));
                p.setBrush(QColor(200, 235, 255, (int)(alpha * 0.75)));
                QPolygonF shard;
                shard << QPointF(screenX - drawSize, screenY)
                      << QPointF(screenX, screenY - drawSize * 1.4)
                      << QPointF(screenX + drawSize * 0.8, screenY + drawSize * 0.6)
                      << QPointF(screenX - drawSize * 0.4, screenY + drawSize * 0.8);
                p.drawPolygon(shard);
            } else {
                // Expanding smoke / steam puff
                QRadialGradient smkGrad(screenX, screenY, drawSize * 2.2);
                smkGrad.setColorAt(0.0, QColor(210, 215, 225, (int)(alpha * 0.55)));
                smkGrad.setColorAt(0.6, QColor(160, 165, 175, (int)(alpha * 0.28)));
                smkGrad.setColorAt(1.0, QColor(130, 135, 145, 0));
                p.setBrush(smkGrad);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(screenX, screenY), drawSize * 2.2, drawSize * 1.8);
            }
            p.restore();
        }
    }
}

void RasterRenderer::renderSideWindowsAndAdjacentCars(QPainter& p, const GameEngine& engine, int w, int h) {
    const PlayerCar& player = engine.getPlayer();

    // Quarter window geometry for clipping
    QPolygon leftWin;
    leftWin << QPoint(0, 34) << QPoint(140, 34) << QPoint(185, 480) << QPoint(0, 465);

    QPolygon rightWin;
    rightWin << QPoint(w - 140, 34) << QPoint(w, 34) << QPoint(w, 465) << QPoint(w - 185, 480);

    // Dynamic tinted quarter glass reflections with highway motion streaks
    p.save();
    QPainterPath leftPath;
    leftPath.addPolygon(leftWin);
    p.setClipPath(leftPath);
    QLinearGradient lg(0, 34, 185, 480);
    lg.setColorAt(0.0, QColor(20, 25, 35, 120));
    lg.setColorAt(0.5, QColor(40, 50, 70, 70));
    lg.setColorAt(1.0, QColor(10, 15, 22, 140));
    p.fillPath(leftPath, lg);
    // Road speed reflection line
    int sRefY = (int)(player.worldZ * 0.4) % 300 + 80;
    p.setPen(QColor(255, 255, 255, 45));
    p.drawLine(0, sRefY, 160, sRefY + 80);
    p.restore();

    p.save();
    QPainterPath rightPath;
    rightPath.addPolygon(rightWin);
    p.setClipPath(rightPath);
    QLinearGradient rg(w - 140, 34, w, 480);
    rg.setColorAt(0.0, QColor(20, 25, 35, 120));
    rg.setColorAt(0.5, QColor(40, 50, 70, 70));
    rg.setColorAt(1.0, QColor(10, 15, 22, 140));
    p.fillPath(rightPath, rg);
    p.setPen(QColor(255, 255, 255, 45));
    p.drawLine(w - 160, sRefY + 80, w, sRefY);
    p.restore();
}

void RasterRenderer::renderSidewaysPOV(QPainter& p, const GameEngine& engine, int w, int h, bool lookLeft) {
    const PlayerCar& player = engine.getPlayer();
    const QVector<NPCCar>& traffic = engine.getTraffic();
    int totalTrackLen = engine.getTrack().trackLength();

    // 1. Panoramic Sky & Horizon Backdrop
    const QImage& bg = RasterAssets::instance().getBackdrop(engine.getCurrentTheme(), engine.getTimeOfDay());
    int bgW = bg.width();
    int bgOffset = (int)(m_cameraHeading * 420.0 + (lookLeft ? -w * 0.55 : w * 0.55)) % bgW;
    p.drawImage(QRect(-bgOffset, 0, bgW, h * 0.65), bg);
    p.drawImage(QRect(-bgOffset + bgW, 0, bgW, h * 0.65), bg);
    if (bgOffset > 0) p.drawImage(QRect(-bgOffset - bgW, 0, bgW, h * 0.65), bg);

    // 2. Dynamic Sideways Moving Ground & Asphalt
    int dir = lookLeft ? -1 : 1;
    QLinearGradient aspGrad(0, 250, 0, h);
    aspGrad.setColorAt(0.0, QColor(54, 58, 65));
    aspGrad.setColorAt(0.35, QColor(42, 45, 52));
    aspGrad.setColorAt(1.0, QColor(28, 30, 35));
    p.fillRect(0, 250, w, h - 250, aspGrad);

    // High-speed sideways motion lines & lane dashes
    int scrollX = (int)(player.worldZ * 8.0 * dir) % 180;
    for (int lx = -180 + scrollX; lx < w + 180; lx += 180) {
        p.fillRect(lx, 385, 110, 6, QColor(240, 240, 245, 140)); // Lane dividing dash
        p.fillRect(lx + 45, 425, 80, 4, QColor(240, 240, 245, 85));  // Asphalt speed line
    }
    // Solid highway edge line
    p.fillRect(0, 320, w, 8, QColor(245, 245, 250, 190));

    // Roadside Guardrail
    QLinearGradient railGrad(0, 288, 0, 315);
    railGrad.setColorAt(0.0, QColor(215, 220, 230));
    railGrad.setColorAt(0.5, QColor(140, 145, 155));
    railGrad.setColorAt(1.0, QColor(185, 190, 200));
    p.fillRect(0, 288, w, 27, railGrad);
    p.setPen(QColor(80, 85, 95));
    p.drawLine(0, 288, w, 288);
    p.drawLine(0, 315, w, 315);

    // Repeating vertical posts
    int postSpacing = 85;
    int postScroll = (int)(player.worldZ * 4.2 * dir) % postSpacing;
    for (int px = -postSpacing + postScroll; px < w + postSpacing; px += postSpacing) {
        p.fillRect(px, 305, 8, 38, QColor(100, 105, 115));
    }

    // 1. Towering Elevated Highway Hoardings & Neon Billboards passing by
    int hoardSpacing = 680;
    int hoardScroll = (int)(player.worldZ * 2.6 * dir) % hoardSpacing;
    for (int hx = -hoardSpacing + hoardScroll; hx < w + hoardSpacing; hx += hoardSpacing) {
        int boardIndex = (int)((player.worldZ * 2.6 * dir - hx) / hoardSpacing);
        SceneryType btype = (qAbs(boardIndex) % 2 == 0) ? SCENERY_HOARDING_NEON : SCENERY_BILLBOARD_1;
        const QImage& bImg = RasterAssets::instance().getScenery(btype);
        int bw = qRound(bImg.width() * 0.72);
        int bh = qRound(bImg.height() * 0.72);
        p.drawImage(QRect(hx, 320 - bh, bw, bh), bImg);
    }

    // 2. Roadside Scenery (Trees / Streetlights passing by sideways)
    int scenSpacing = 440;
    int scenScroll = (int)(player.worldZ * 3.4 * dir) % scenSpacing;
    for (int sx = -scenSpacing + scenScroll; sx < w + scenSpacing; sx += scenSpacing) {
        SceneryType stype = (engine.getCurrentTheme() == THEME_SUNSET_COAST) ? SCENERY_PALM_TREE : SCENERY_STREETLIGHT;
        const QImage& treeImg = RasterAssets::instance().getScenery(stype);
        p.drawImage(QRect(sx, 120, treeImg.width() * 0.9, treeImg.height() * 0.9), treeImg);
    }

    // 3. Roadside Utility / Telegraph Poles with Overhead Catenary Wires
    int poleSpacing = 280;
    int poleScroll = (int)(player.worldZ * 4.2 * dir) % poleSpacing;
    int prevPoleX = -9999;
    int prevPoleY = 0;
    for (int px = -poleSpacing + poleScroll; px < w + poleSpacing; px += poleSpacing) {
        const QImage& poleImg = RasterAssets::instance().getScenery(SCENERY_UTILITY_POLE);
        int pw = qRound(poleImg.width() * 0.75);
        int ph = qRound(poleImg.height() * 0.75);
        int py = 325 - ph;
        p.drawImage(QRect(px - pw / 2, py, pw, ph), poleImg);

        // Sagging overhead power lines between adjacent utility poles
        if (prevPoleX != -9999) {
            p.setPen(QPen(QColor(22, 26, 32, 175), 1.8));
            QPainterPath wirePath1;
            wirePath1.moveTo(prevPoleX, prevPoleY + 45);
            wirePath1.quadTo((prevPoleX + px) / 2, prevPoleY + 68, px, py + 45);
            p.drawPath(wirePath1);

            QPainterPath wirePath2;
            wirePath2.moveTo(prevPoleX, prevPoleY + 85);
            wirePath2.quadTo((prevPoleX + px) / 2, prevPoleY + 104, px, py + 85);
            p.drawPath(wirePath2);
        }
        prevPoleX = px;
        prevPoleY = py;
    }

    // 3. Adjacent NPC Cars Passing / Cruising Alongside
    for (const NPCCar& npc : traffic) {
        qreal relZ = npc.worldZ - player.worldZ;
        if (relZ < -totalTrackLen / 2) relZ += totalTrackLen;
        if (relZ > totalTrackLen / 2) relZ -= totalTrackLen;

        qreal lateralDist = npc.x - player.x;
        bool inTargetLane = lookLeft ? (lateralDist < -0.15) : (lateralDist > 0.15);

        if (inTargetLane && qAbs(relZ) < 180) {
            // Center of the panoramic window opening
            int winCenterX = lookLeft ? qRound(w * 0.45) : qRound(w * 0.58);

            // Longitudinal position: car ahead moves towards front A-pillar, car behind moves towards rear B-pillar
            int carX = lookLeft ? qRound(winCenterX + relZ * 3.8) : qRound(winCenterX - relZ * 3.8);
            int carY = qRound(355 + player.pitchG * 50.0);

            // Distance scaling (adjacent lane vs two lanes over)
            qreal distFactor = (qAbs(lateralDist) < 0.6) ? 1.05 : 0.82;
            int carW = qRound(380 * distFactor);
            int carH = qRound(160 * distFactor);

            // Orientation logic:
            // When looking left:
            //   Same-direction cars travel forward -> facing right on screen (facingLeft = false)
            //   Oncoming cars travel opposite -> facing left on screen (facingLeft = true)
            // When looking right:
            //   Same-direction cars travel forward -> facing left on screen (facingLeft = true)
            //   Oncoming cars travel opposite -> facing right on screen (facingLeft = false)
            bool spriteFacingLeft = lookLeft ? npc.isOncoming : !npc.isOncoming;
            const QImage& sideSprite = RasterAssets::instance().getVehicleSide(npc.type, spriteFacingLeft);

            // Soft contact drop shadow
            p.setBrush(QColor(8, 10, 14, 190));
            p.setPen(Qt::NoPen);
            p.drawEllipse(carX - carW / 2 + 10, carY + carH / 2 - 14, carW - 20, 24);

            // Render crisp, high-detail side profile of the NPC car
            QRect carRect(carX - carW / 2, carY - carH / 2, carW, carH);
            p.drawImage(carRect, sideSprite);

            // Volumetric glowing brake light if braking!
            if (npc.isBraking) {
                int brakeX = spriteFacingLeft ? carX + carW / 2 - 15 : carX - carW / 2 + 15;
                int brakeY = carY + 8;
                QRadialGradient bGlow(brakeX, brakeY, 65);
                bGlow.setColorAt(0.0, QColor(255, 30, 30, 200));
                bGlow.setColorAt(0.4, QColor(255, 30, 30, 90));
                bGlow.setColorAt(1.0, QColor(255, 30, 30, 0));
                p.setBrush(bGlow);
                p.drawEllipse(brakeX - 45, brakeY - 35, 90, 70);
            }

            // Flashing amber turn signal repeater if signaling
            if ((npc.turnLeft || npc.turnRight) && npc.blinkerFlash) {
                int blinkX = spriteFacingLeft ? carX - carW / 4 : carX + carW / 4;
                int blinkY = carY + 6;
                QRadialGradient tGlow(blinkX, blinkY, 40);
                tGlow.setColorAt(0.0, QColor(255, 180, 20, 220));
                tGlow.setColorAt(0.5, QColor(255, 180, 20, 90));
                tGlow.setColorAt(1.0, QColor(255, 180, 20, 0));
                p.setBrush(tGlow);
                p.drawEllipse(blinkX - 25, blinkY - 20, 50, 40);
            }
        }
    }

    // 4. Interior Cockpit Frame Overlay (Frames the Window Naturally)
    if (lookLeft) {
        p.drawImage(QRect(0, 0, w, h), RasterAssets::instance().getLeftCockpitPOV());
    } else {
        p.drawImage(QRect(0, 0, w, h), RasterAssets::instance().getRightCockpitPOV());
    }

    // 5. Real-Time Exterior Side Mirror on Door Frame
    const int mw = 145;
    const int mh = 90;
    int mx = lookLeft ? qRound(w * 0.65 + 8) : qRound(w * 0.28 + 8);
    int my = 338;
    {
        QImage mirBuffer(mw, mh, QImage::Format_ARGB32_Premultiplied);
        mirBuffer.fill(QColor(14, 18, 26));
        QPainter mp(&mirBuffer);
        mp.setRenderHint(QPainter::Antialiasing, true);
        const QImage& bgImg = RasterAssets::instance().getBackdrop(engine.getCurrentTheme(), engine.getTimeOfDay());
        mp.drawImage(QRect(0, 0, mw, mh * 0.6), bgImg);
        qreal mirAngle = lookLeft ? -engine.getTrack().getInfo().roadWidth * 0.45 : engine.getTrack().getInfo().roadWidth * 0.45;
        renderRoad(mp, engine, mw, mh, true, mirAngle);
        renderSceneryAndCars(mp, engine, mw, mh, true, mirAngle);
        mp.fillRect(0, 0, mw, mh, QColor(20, 50, 90, 40));
        mp.end();

        QPainterPath mirClip;
        mirClip.addRoundedRect(mx, my, mw - 25, mh - 18, 8, 8);
        p.save();
        p.setClipPath(mirClip);
        p.drawImage(mx, my, mirBuffer);
        p.restore();
    }

    // 6. Tempered Automotive Glass Refraction Sheen
    p.save();
    QLinearGradient winSheen(200, 35, 550, 460);
    winSheen.setColorAt(0.0, QColor(255, 255, 255, 0));
    winSheen.setColorAt(0.45, QColor(240, 248, 255, 18));
    winSheen.setColorAt(0.55, QColor(240, 248, 255, 26));
    winSheen.setColorAt(0.70, QColor(255, 255, 255, 0));
    p.setBrush(winSheen);
    p.setPen(Qt::NoPen);
    p.drawRect(lookLeft ? 60 : 240, 35, (int)(w * 0.65), 440);
    p.restore();

    // 7. Sleek Glancing HUD in Bottom Corner
    int hudX = lookLeft ? w - 240 : 30;
    int hudY = h - 120;
    p.setBrush(QColor(12, 14, 18, 210));
    p.setPen(QPen(QColor(0, 210, 255, 180), 1));
    p.drawRoundedRect(hudX, hudY, 210, 85, 8, 8);

    p.setFont(QFont("Arial", 9, QFont::Bold));
    p.setPen(QColor(0, 220, 255));
    p.drawText(hudX + 12, hudY + 22, lookLeft ? "SIDE POV: LEFT [Q]" : "SIDE POV: RIGHT [E]");

    p.setFont(QFont("Arial", 20, QFont::Bold));
    p.setPen(Qt::white);
    p.drawText(hudX + 12, hudY + 54, QString::number(qRound(player.speed)));
    p.setFont(QFont("Arial", 10, QFont::Bold));
    p.setPen(QColor(180, 185, 195));
    p.drawText(hudX + 75, hudY + 54, "KM/H");

    p.setPen(QColor(255, 210, 30));
    p.drawText(hudX + 130, hudY + 54, QString("GEAR %1").arg(player.gear));

    // Mini RPM Bar
    p.fillRect(hudX + 12, hudY + 66, 186, 6, QColor(25, 28, 35));
    qreal rpmRatio = qBound(0.0, (player.rpm - 900.0) / 7100.0, 1.0);
    p.fillRect(hudX + 12, hudY + 66, qRound(186 * rpmRatio), 6, QColor(0, 230, 255));
}

void RasterRenderer::renderDynamicGlassReflections(QPainter& p, const GameEngine& engine, int w, int h) {
    const PlayerCar& player = engine.getPlayer();

    // 1. Dynamic Sun Glare Flare sweep on curves
    if (engine.getTimeOfDay() != TIME_NIGHT) {
        const QImage& glare = RasterAssets::instance().getSunGlare();
        int glareX = qRound(w * 0.45 + player.sunGlareX);
        int glareY = qRound(h * 0.18 + player.pitchG * 40.0);

        p.save();
        p.setOpacity(0.65);
        p.drawImage(glareX - glare.width() / 2, glareY - glare.height() / 2, glare);
        p.restore();
    }

    // 2. Translucent Ambient Scenery Reflection rolling down windshield
    int scrollY = (int)player.glassReflectionOffset % 180;
    QLinearGradient glassReflect(0, 120 + scrollY, 0, 320 + scrollY);
    glassReflect.setColorAt(0.0, QColor(255, 255, 255, 0));
    glassReflect.setColorAt(0.5, QColor(230, 240, 255, 18));
    glassReflect.setColorAt(1.0, QColor(255, 255, 255, 0));

    QPolygonF glassZone;
    glassZone << QPointF(200, 130) << QPointF(w - 200, 130) << QPointF(w - 270, 460) << QPointF(270, 460);
    p.setBrush(glassReflect);
    p.setPen(Qt::NoPen);
    p.drawPolygon(glassZone);
}

void RasterRenderer::renderAnimatedWipers(QPainter& p, const GameEngine& engine, int w, int h, int dashX, int dashY) {
    Q_UNUSED(w);
    Q_UNUSED(h);
    const PlayerCar& player = engine.getPlayer();
    const QImage& wiper = RasterAssets::instance().getWiperBlade();

    // Left Driver Wiper Arm
    p.save();
    p.translate(dashX + 380, dashY + 465);
    p.rotate(-80.0 + player.wiperAngle);
    p.drawImage(-wiper.width() / 2, -wiper.height() + 10, wiper);
    p.restore();

    // Right Passenger Wiper Arm
    p.save();
    p.translate(dashX + 740, dashY + 465);
    p.rotate(-80.0 + player.wiperAngle);
    p.drawImage(-wiper.width() / 2, -wiper.height() + 10, wiper);
    p.restore();
}

void RasterRenderer::renderWindshieldHolographicHUD(QPainter& p, const GameEngine& engine, int w, int h, int dashX, int dashY) {
    Q_UNUSED(w);
    Q_UNUSED(h);
    const PlayerCar& player = engine.getPlayer();

    // Modern Fighter Jet / Corvette holographic HUD projected on windshield
    int hudX = dashX + 370;
    int hudY = dashY + 395;

    p.save();
    p.setOpacity(0.85);

    // Glowing Cyan holographic speed
    QFont hudFont("Arial", 16, QFont::Bold);
    p.setFont(hudFont);
    p.setPen(QColor(0, 240, 255, 220));
    p.drawText(hudX - 60, hudY, QString("%1").arg((int)player.speed));

    QFont unitFont("Arial", 8, QFont::Bold);
    p.setFont(unitFont);
    p.setPen(QColor(0, 210, 230, 180));
    p.drawText(hudX - 25, hudY, "KM/H");

    // Gear
    QString gearStr;
    if (player.gear == -1) gearStr = "R";
    else if (player.gear == 0) gearStr = "N";
    else gearStr = QString("D%1").arg(player.gear);

    p.drawText(hudX + 25, hudY, gearStr);

    // Mini HUD RPM bar
    QRectF hudRpmBar(hudX - 60, hudY + 6, 110, 4);
    p.fillRect(hudRpmBar, QColor(0, 120, 160, 100));
    qreal rpmRatio = qBound(0.0, (player.rpm - 800) / 7200.0, 1.0);
    p.fillRect(QRectF(hudX - 60, hudY + 6, 110 * rpmRatio, 4), (rpmRatio > 0.85) ? QColor(255, 50, 50, 220) : QColor(0, 240, 255, 220));

    p.restore();
}

void RasterRenderer::renderNightHeadlights(QPainter& p, const GameEngine& engine, int w, int h) {
    Q_UNUSED(engine);
    const QImage& beam = RasterAssets::instance().getHeadlightBeam();
    QRect beamRect(w / 2 - 380, h / 2 + 10, 760, h / 2 - 10);
    p.drawImage(beamRect, beam);
}

void RasterRenderer::renderRearviewMirror(QPainter& p, const GameEngine& engine, int w, int h) {
    const int mw = 320;
    const int mh = 90;
    int mx = (w - mw) / 2;
    int my = 18;

    // Windshield Header Mounting Stalk
    p.fillRect(w / 2 - 12, 0, 24, 22, QColor(20, 22, 26));
    p.fillRect(w / 2 - 7, 18, 14, 10, QColor(48, 52, 60)); // Ball joint pivot

    QImage mirrorBuffer(mw, mh, QImage::Format_ARGB32_Premultiplied);
    mirrorBuffer.fill(QColor(12, 16, 24));

    QPainter mp(&mirrorBuffer);
    mp.setRenderHint(QPainter::Antialiasing, true);
    mp.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 1. Reflected Sky and Horizon Behind
    const QImage& bg = RasterAssets::instance().getBackdrop(engine.getCurrentTheme(), engine.getTimeOfDay());
    int bgW = bg.width();
    int bgH = bg.height();
    int bgOffset = (int)(-m_cameraHeading * 280.0 + bgW * 0.5) % bgW;
    if (bgOffset < 0) bgOffset += bgW;
    mp.drawImage(QRect(0, 0, mw, (int)(mh * 0.52)), bg, QRect(bgOffset, 0, bgW - bgOffset, bgH));
    if (bgOffset > 0) {
        int drawnW = (int)(((qreal)(bgW - bgOffset) / bgW) * mw);
        mp.drawImage(QRect(drawnW, 0, mw - drawnW, (int)(mh * 0.52)), bg, QRect(0, 0, bgOffset, bgH));
    }

    // 2. Real-time Rear Road & Receding/Approaching Traffic
    renderRoad(mp, engine, mw, mh, true, 0);
    renderSceneryAndCars(mp, engine, mw, mh, true, 0);

    // 3. Realistic Sports Car Interior Frame Reflections
    // Rear headliner arch
    p.setPen(Qt::NoPen);
    QPainterPath headliner;
    headliner.moveTo(0, 0);
    headliner.lineTo(mw, 0);
    headliner.lineTo(mw, 10);
    headliner.quadTo(mw / 2, 16, 0, 10);
    headliner.closeSubpath();
    mp.fillPath(headliner, QColor(16, 18, 22));

    // Left & Right Rear C-pillars (framing the rear glass)
    QPolygonF leftPillar;
    leftPillar << QPointF(0, 0) << QPointF(24, 0) << QPointF(0, mh * 0.7);
    mp.setBrush(QColor(18, 20, 25));
    mp.drawPolygon(leftPillar);

    QPolygonF rightPillar;
    rightPillar << QPointF(mw, 0) << QPointF(mw - 24, 0) << QPointF(mw, mh * 0.7);
    mp.drawPolygon(rightPillar);

    // Twin Sports Bucket Seat Headrests silhouetted in bottom corners
    mp.setBrush(QColor(22, 25, 30));
    mp.setPen(QPen(QColor(36, 40, 48), 1));
    mp.drawRoundedRect(QRectF(14, mh - 26, 46, 26), 8, 8);
    mp.drawRoundedRect(QRectF(mw - 60, mh - 26, 46, 26), 8, 8);

    // Rear window heating defroster lines (3 subtle horizontal grid wires)
    mp.setPen(QColor(215, 95, 35, 30));
    mp.drawLine(0, (int)(mh * 0.35), mw, (int)(mh * 0.35));
    mp.drawLine(0, (int)(mh * 0.50), mw, (int)(mh * 0.50));
    mp.drawLine(0, (int)(mh * 0.65), mw, (int)(mh * 0.65));

    // 4. Electrochromic Auto-Dimming Glass Coating
    TimeOfDay tod = engine.getTimeOfDay();
    QColor dimCol = (tod == TIME_NIGHT) ? QColor(8, 42, 32, 75) : ((tod == TIME_SUNSET) ? QColor(38, 18, 48, 40) : QColor(14, 55, 110, 35));
    mp.fillRect(0, 0, mw, mh, dimCol);

    // 5. Diagonal High-Gloss Glass Sheen
    QLinearGradient sheen(0, 0, mw, mh);
    sheen.setColorAt(0.0, QColor(255, 255, 255, 65));
    sheen.setColorAt(0.25, QColor(255, 255, 255, 20));
    sheen.setColorAt(0.55, QColor(255, 255, 255, 0));
    sheen.setColorAt(0.85, QColor(255, 255, 255, 15));
    mp.fillRect(0, 0, mw, mh, sheen);
    mp.end();

    // Clip mirror to aerodynamic beveled shape
    QPainterPath mirrorClip;
    mirrorClip.addRoundedRect(mx + 6, my + 6, mw - 12, mh - 12, 14, 14);
    p.save();
    p.setClipPath(mirrorClip);
    p.drawImage(mx, my, mirrorBuffer);
    p.restore();

    // Outer titanium/chrome bevel frame
    p.setPen(QPen(QColor(85, 95, 112), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(mx + 6, my + 6, mw - 12, mh - 12, 14, 14);
    p.setPen(QPen(QColor(22, 25, 32), 3));
    p.drawRoundedRect(mx + 4, my + 4, mw - 8, mh - 8, 16, 16);

    // Auto-dimming sensor green LED dot on bottom chin
    p.setBrush(QColor(0, 240, 120));
    p.setPen(Qt::NoPen);
    p.drawEllipse(mx + mw / 2 - 2, my + mh - 7, 4, 4);
}

void RasterRenderer::renderSideMirrors(QPainter& p, const GameEngine& engine, int w, int h) {
    const int mw = 160;
    const int mh = 100;
    qreal roadWidth = engine.getTrack().getInfo().roadWidth;
    TimeOfDay tod = engine.getTimeOfDay();

    // =========================================================
    // 1. LEFT EXTERIOR SIDE MIRROR (Driver Side)
    // =========================================================
    int lx = 20;
    int ly = 320;
    {
        // Aerodynamic exterior mirror casing mounting arm to door
        QPolygonF mountArm;
        mountArm << QPointF(lx + mw - 8, ly + mh * 0.45) << QPointF(lx + mw + 18, ly + mh * 0.38)
                 << QPointF(lx + mw + 22, ly + mh * 0.65) << QPointF(lx + mw - 8, ly + mh * 0.60);
        p.setBrush(QColor(20, 22, 26));
        p.setPen(QPen(QColor(42, 46, 54), 1));
        p.drawPolygon(mountArm);

        // Exterior aerodynamic carbon mirror shell
        QPainterPath shellPath;
        shellPath.addRoundedRect(lx, ly, mw, mh, 16, 16);
        p.setBrush(QColor(16, 18, 22));
        p.setPen(QPen(QColor(36, 40, 48), 2));
        p.drawPath(shellPath);

        // Outer Amber LED turn repeater indicator strip
        QColor indCol = (engine.getPlayer().steerAngle < -0.15) ? QColor(255, 175, 20) : QColor(220, 160, 40, 110);
        p.setBrush(indCol);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(lx + 3, ly + mh * 0.35, 5, mh * 0.32), 2, 2);

        // Mirror reflection buffer
        QImage leftBuffer(mw, mh, QImage::Format_ARGB32_Premultiplied);
        leftBuffer.fill(QColor(12, 16, 24));

        QPainter lmp(&leftBuffer);
        lmp.setRenderHint(QPainter::Antialiasing, true);
        lmp.setRenderHint(QPainter::SmoothPixmapTransform, true);

        // Background sky reflected behind
        const QImage& bg = RasterAssets::instance().getBackdrop(engine.getCurrentTheme(), tod);
        int bgW = bg.width();
        int bgH = bg.height();
        int bgOffset = (int)(-m_cameraHeading * 260.0 + bgW * 0.45) % bgW;
        if (bgOffset < 0) bgOffset += bgW;
        lmp.drawImage(QRect(0, 0, mw, (int)(mh * 0.55)), bg, QRect(bgOffset, 0, bgW - bgOffset, bgH));

        // Real-time Road and Receding Traffic in Left Lanes
        renderRoad(lmp, engine, mw, mh, true, -roadWidth * 0.42);
        renderSceneryAndCars(lmp, engine, mw, mh, true, -roadWidth * 0.42);

        // REALISTIC CAR FLANK REFLECTION:
        // The rightmost 20% reflects your own sports car's red door flank and rear fender!
        QPolygonF carFlank;
        carFlank << QPointF(mw * 0.82, 0) << QPointF(mw, 0) << QPointF(mw, mh) << QPointF(mw * 0.74, mh);
        QLinearGradient flankGrad(mw * 0.74, 0, mw, 0);
        flankGrad.setColorAt(0.0, QColor(215, 35, 35));
        flankGrad.setColorAt(0.65, QColor(155, 20, 20));
        flankGrad.setColorAt(1.0, QColor(85, 10, 10));
        lmp.setBrush(flankGrad);
        lmp.setPen(QColor(45, 10, 10));
        lmp.drawPolygon(carFlank);
        // Door handle & air intake strake
        lmp.fillRect(QRectF(mw * 0.83, mh * 0.48, 16, 3), QColor(30, 10, 10));

        // Aspherical wide-angle blind spot dividing line
        lmp.setPen(QPen(QColor(220, 230, 255, 130), 1.5, Qt::DashLine));
        lmp.drawLine(mw * 0.28, 6, mw * 0.28, mh - 6);

        // Electrochromic anti-glare glass tint
        QColor tintCol = (tod == TIME_NIGHT) ? QColor(8, 42, 32, 70) : QColor(14, 55, 110, 35);
        lmp.fillRect(0, 0, mw, mh, tintCol);

        // Glass sheen
        QLinearGradient sheen(0, 0, mw, mh);
        sheen.setColorAt(0.0, QColor(255, 255, 255, 50));
        sheen.setColorAt(0.35, QColor(255, 255, 255, 15));
        sheen.setColorAt(0.7, QColor(255, 255, 255, 0));
        lmp.fillRect(0, 0, mw, mh, sheen);
        lmp.end();

        // Clip and draw mirror glass
        QPainterPath leftClip;
        leftClip.addRoundedRect(lx + 8, ly + 8, mw - 16, mh - 16, 12, 12);
        p.save();
        p.setClipPath(leftClip);
        p.drawImage(lx, ly, leftBuffer);
        p.restore();

        // Glass bevel rim
        p.setPen(QPen(QColor(75, 85, 100), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(lx + 8, ly + 8, mw - 16, mh - 16, 12, 12);
    }

    // =========================================================
    // 2. RIGHT EXTERIOR SIDE MIRROR (Passenger Side)
    // =========================================================
    int rx = w - mw - 20;
    int ry = 320;
    {
        // Aerodynamic mounting arm to passenger door
        QPolygonF mountArm;
        mountArm << QPointF(rx + 8, ly + mh * 0.45) << QPointF(rx - 18, ly + mh * 0.38)
                 << QPointF(rx - 22, ly + mh * 0.65) << QPointF(rx + 8, ly + mh * 0.60);
        p.setBrush(QColor(20, 22, 26));
        p.setPen(QPen(QColor(42, 46, 54), 1));
        p.drawPolygon(mountArm);

        // Exterior aerodynamic mirror shell
        QPainterPath shellPath;
        shellPath.addRoundedRect(rx, ry, mw, mh, 16, 16);
        p.setBrush(QColor(16, 18, 22));
        p.setPen(QPen(QColor(36, 40, 48), 2));
        p.drawPath(shellPath);

        // Outer Amber LED turn repeater indicator strip
        QColor indCol = (engine.getPlayer().steerAngle > 0.15) ? QColor(255, 175, 20) : QColor(220, 160, 40, 110);
        p.setBrush(indCol);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(rx + mw - 8, ry + mh * 0.35, 5, mh * 0.32), 2, 2);

        // Mirror reflection buffer
        QImage rightBuffer(mw, mh, QImage::Format_ARGB32_Premultiplied);
        rightBuffer.fill(QColor(12, 16, 24));

        QPainter rmp(&rightBuffer);
        rmp.setRenderHint(QPainter::Antialiasing, true);
        rmp.setRenderHint(QPainter::SmoothPixmapTransform, true);

        // Background sky reflected behind
        const QImage& bg = RasterAssets::instance().getBackdrop(engine.getCurrentTheme(), tod);
        int bgW = bg.width();
        int bgH = bg.height();
        int bgOffset = (int)(-m_cameraHeading * 260.0 + bgW * 0.55) % bgW;
        if (bgOffset < 0) bgOffset += bgW;
        rmp.drawImage(QRect(0, 0, mw, (int)(mh * 0.55)), bg, QRect(bgOffset, 0, bgW - bgOffset, bgH));

        // Real-time Road and Receding Traffic in Right Lanes
        renderRoad(rmp, engine, mw, mh, true, roadWidth * 0.42);
        renderSceneryAndCars(rmp, engine, mw, mh, true, roadWidth * 0.42);

        // REALISTIC CAR FLANK REFLECTION:
        // The leftmost 20% reflects your own sports car's right door flank and rear fender!
        QPolygonF carFlank;
        carFlank << QPointF(0, 0) << QPointF(mw * 0.18, 0) << QPointF(mw * 0.26, mh) << QPointF(0, mh);
        QLinearGradient flankGrad(0, 0, mw * 0.26, 0);
        flankGrad.setColorAt(0.0, QColor(85, 10, 10));
        flankGrad.setColorAt(0.35, QColor(155, 20, 20));
        flankGrad.setColorAt(1.0, QColor(215, 35, 35));
        rmp.setBrush(flankGrad);
        rmp.setPen(QColor(45, 10, 10));
        rmp.drawPolygon(carFlank);
        rmp.fillRect(QRectF(mw * 0.08, mh * 0.48, 16, 3), QColor(30, 10, 10));

        // Aspherical wide-angle blind spot dividing line
        rmp.setPen(QPen(QColor(220, 230, 255, 130), 1.5, Qt::DashLine));
        rmp.drawLine(mw * 0.72, 6, mw * 0.72, mh - 6);

        // Laser-Etched Safety Notice at bottom
        QFont ef("Arial", 5, QFont::Bold);
        rmp.setFont(ef);
        rmp.setPen(QColor(220, 230, 245, 150));
        rmp.drawText(QRect(24, mh - 12, mw - 48, 9), Qt::AlignCenter, "OBJECTS IN MIRROR ARE CLOSER THAN THEY APPEAR");

        // Electrochromic anti-glare glass tint & sheen
        QColor tintCol = (tod == TIME_NIGHT) ? QColor(8, 42, 32, 70) : QColor(14, 55, 110, 35);
        rmp.fillRect(0, 0, mw, mh, tintCol);

        QLinearGradient sheen(0, 0, mw, mh);
        sheen.setColorAt(0.0, QColor(255, 255, 255, 50));
        sheen.setColorAt(0.35, QColor(255, 255, 255, 15));
        sheen.setColorAt(0.7, QColor(255, 255, 255, 0));
        rmp.fillRect(0, 0, mw, mh, sheen);
        rmp.end();

        // Clip and draw mirror glass
        QPainterPath rightClip;
        rightClip.addRoundedRect(rx + 8, ry + 8, mw - 16, mh - 16, 12, 12);
        p.save();
        p.setClipPath(rightClip);
        p.drawImage(rx, ry, rightBuffer);
        p.restore();

        // Glass bevel rim
        p.setPen(QPen(QColor(75, 85, 100), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rx + 8, ry + 8, mw - 16, mh - 16, 12, 12);
    }
}

void RasterRenderer::renderCockpit(QPainter& p, const GameEngine& engine, int w, int h) {
    const PlayerCar& player = engine.getPlayer();

    int pitchOffset = qRound(player.pitchG * 90.0);
    int rollOffset = qRound(player.rollG * 60.0);
    int vibX = (player.vibration > 0) ? (QRandomGenerator::global()->generate() % 5 - 2) : 0;
    int vibY = (player.vibration > 0) ? (QRandomGenerator::global()->generate() % 5 - 2) : 0;

    int dashX = rollOffset + vibX;
    int dashY = pitchOffset + vibY;

    const QImage& dash = RasterAssets::instance().getDashboard();
    p.drawImage(QRect(dashX, dashY, w, h), dash);

    renderDashboardInstruments(p, engine, dashX, dashY, w, h);

    qreal scaleX = w / 1280.0;
    qreal scaleY = h / 720.0;
    int sx = dashX + qRound(615 * scaleX);
    int sy = dashY + qRound(505 * scaleY);
    int sw = qRound(355 * scaleX);
    int sh = qRound(175 * scaleY);
    QRect screenRect(sx, sy, sw, sh);
    renderInfotainmentScreen(p, engine, screenRect);
}

void RasterRenderer::renderRearCockpitView(QPainter& p, const GameEngine& engine, int w, int h) {
    const PlayerCar& player = engine.getPlayer();
    int vibY = (player.vibration > 0) ? (QRandomGenerator::global()->generate() % 4 - 2) : 0;
    const QImage& rearDash = RasterAssets::instance().getRearCockpitView();
    p.drawImage(QRect(0, vibY, w, h), rearDash);
}

void RasterRenderer::renderDashboardInstruments(QPainter& p, const GameEngine& engine, int dashX, int dashY, int w, int h) {
    const PlayerCar& player = engine.getPlayer();
    const RasterAssets& assets = RasterAssets::instance();
    bool isNight = (engine.getTimeOfDay() == TIME_NIGHT);

    qreal scaleX = w / 1280.0;
    qreal scaleY = h / 720.0;
    qreal dialScale = qMin(scaleX, scaleY);

    // 1. SPEEDOMETER (Left Dial)
    int dialSize = qRound(180 * dialScale);
    int speedX = dashX + qRound(175 * scaleX);
    int speedY = dashY + qRound(465 * scaleY);
    p.drawImage(QRect(speedX, speedY, dialSize, dialSize), assets.getGaugeDialSpeed(isNight));

    qreal speedAngle = 135.0 + (qBound(0.0, player.speed, 300.0) / 300.0) * 270.0;
    p.save();
    p.translate(speedX + dialSize / 2.0, speedY + dialSize / 2.0);
    p.rotate(speedAngle - 270.0);
    const QImage& needle = assets.getNeedle();
    int needleW = qRound(needle.width() * dialScale);
    int needleH = qRound(needle.height() * dialScale);
    p.drawImage(-needleW / 2, -needleH + qRound(16 * dialScale), needle);
    p.restore();

    QFont numFont("Arial", qMax(9, qRound(18 * dialScale)), QFont::Bold);
    p.setFont(numFont);
    p.setPen(isNight ? QColor(0, 240, 255) : QColor(255, 230, 80));
    p.drawText(QRect(speedX, speedY + qRound(115 * dialScale), dialSize, qRound(30 * dialScale)), Qt::AlignCenter, QString::number((int)player.speed));

    // 2. TACHOMETER (Right Dial)
    int rpmX = dashX + qRound(375 * scaleX);
    int rpmY = dashY + qRound(465 * scaleY);
    p.drawImage(QRect(rpmX, rpmY, dialSize, dialSize), assets.getGaugeDialRpm(isNight));

    qreal rpmAngle = 135.0 + (qBound(800.0, player.rpm, 8000.0) / 8000.0) * 270.0;
    p.save();
    p.translate(rpmX + dialSize / 2.0, rpmY + dialSize / 2.0);
    p.rotate(rpmAngle - 270.0);
    p.drawImage(-needleW / 2, -needleH + qRound(16 * dialScale), needle);
    p.restore();

    QString gearStr;
    if (player.gear == -1) gearStr = "R";
    else if (player.gear == 0) gearStr = "N";
    else gearStr = QString("D%1").arg(player.gear);

    p.setFont(numFont);
    p.setPen(isNight ? QColor(255, 160, 40) : QColor(0, 240, 255));
    p.drawText(QRect(rpmX, rpmY + qRound(115 * dialScale), dialSize, qRound(30 * dialScale)), Qt::AlignCenter, gearStr);
}

void RasterRenderer::renderInfotainmentScreen(QPainter& p, const GameEngine& engine, const QRect& screenRect) {
    p.save();
    p.setClipRect(screenRect);

    // Dark sleek high-contrast OLED black background with subtle blue tint
    p.fillRect(screenRect, QColor(8, 12, 18));

    const InfotainmentState& info = engine.getInfotainment();
    const PlayerCar& player = engine.getPlayer();

    int sx = screenRect.left();
    int sy = screenRect.top();
    int sw = screenRect.width();
    int sh = screenRect.height();

    // -------------------------------------------------------------
    // TOP NAVIGATION TAB BAR (Height: 22 px)
    // -------------------------------------------------------------
    QRect tabStrip(sx, sy, sw, 22);
    p.fillRect(tabStrip, QColor(14, 18, 26));
    p.setPen(QColor(35, 45, 60));
    p.drawLine(sx, sy + 22, sx + sw, sy + 22);

    struct TabInfo {
        InfotainmentMode mode;
        QString title;
    } tabs[4] = {
        { INFO_MODE_WEB, "🌐 WEB" },
        { INFO_MODE_MUSIC, "🎵 MUSIC" },
        { INFO_MODE_RADAR, "🧭 GPS" },
        { INFO_MODE_CAR, "🚗 CAR" }
    };

    int tabW = sw / 4;
    QFont tabFont("Arial", 7, QFont::Bold);
    p.setFont(tabFont);

    for (int t = 0; t < 4; ++t) {
        int tx = sx + t * tabW;
        QRect tRect(tx, sy, tabW, 22);
        bool isActive = (info.mode == tabs[t].mode);

        if (isActive) {
            p.fillRect(tRect, QColor(22, 30, 44));
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 240, 255));
            p.drawRect(tx + 4, sy + 20, tabW - 8, 2); // Neon active underline
            p.setPen(QColor(0, 240, 255));
        } else {
            p.setPen(QColor(130, 145, 170));
        }
        p.drawText(tRect, Qt::AlignCenter, tabs[t].title);
    }

    int contentY = sy + 24;

    // -------------------------------------------------------------
    // 1. MODE: WEB BROWSER
    // -------------------------------------------------------------
    if (info.mode == INFO_MODE_WEB) {
        // Browser URL & Navigation Bar (Height: 20 px)
        QRect urlBar(sx + 4, contentY + 2, sw - 8, 19);
        p.setBrush(QColor(18, 24, 34));
        p.setPen(QPen(QColor(42, 54, 72), 1));
        p.drawRoundedRect(urlBar, 4, 4);

        // Security Lock Icon & URL
        p.setPen(QColor(0, 220, 120));
        QFont urlFont("Arial", 6, QFont::Bold);
        p.setFont(urlFont);
        p.drawText(urlBar.left() + 6, urlBar.top() + 13, "🔒");

        p.setPen(QColor(190, 215, 245));
        p.drawText(QRect(urlBar.left() + 22, urlBar.top(), urlBar.width() - 55, 19), Qt::AlignVCenter | Qt::AlignLeft, info.currentUrl);

        // Refresh icon
        p.setPen(QColor(110, 130, 160));
        p.drawText(urlBar.right() - 20, urlBar.top() + 13, "⟳");

        // Bookmark Category Pills (Height: 17 px)
        int catY = contentY + 24;
        struct CatPill {
            int page;
            QString label;
            QColor col;
        } pills[4] = {
            { 0, "💬 FEED", QColor(0, 220, 255) },
            { 1, "📰 NEWS", QColor(255, 180, 40) },
            { 2, "🌦️ WX", QColor(80, 220, 120) },
            { 3, "🏎️ SPEC", QColor(255, 70, 70) }
        };

        int pillW = (sw - 18) / 4;
        for (int c = 0; c < 4; ++c) {
            int px = sx + 4 + c * (pillW + 3);
            QRect pRect(px, catY, pillW, 16);
            bool isCurPage = (info.currentWebPage == pills[c].page);
            p.setBrush(isCurPage ? QColor(pills[c].col.red(), pills[c].col.green(), pills[c].col.blue(), 55) : QColor(16, 20, 28));
            p.setPen(QPen(isCurPage ? pills[c].col : QColor(36, 44, 58), 1));
            p.drawRoundedRect(pRect, 3, 3);

            QFont pf("Arial", 6, QFont::Bold);
            p.setFont(pf);
            p.setPen(isCurPage ? Qt::white : QColor(140, 155, 175));
            p.drawText(pRect, Qt::AlignCenter, pills[c].label);
        }

        // Web Page Body Content
        int pageY = catY + 19;
        int pageH = sh - (pageY - sy) - 4;
        QRect pageRect(sx + 4, pageY, sw - 8, pageH);
        p.fillRect(pageRect, QColor(12, 16, 24));
        p.setPen(QPen(QColor(28, 36, 48), 1));
        p.drawRect(pageRect);

        if (info.currentWebPage == 0) {
            // PAGE 0: "Speedr" Live Highway Driver Social Feed
            QFont postF("Arial", 6, QFont::Normal);
            p.setFont(postF);

            struct Post {
                QString user;
                QString msg;
                QString time;
                QColor tagCol;
            } posts[3] = {
                { "@HighwayPatrol", "⚠️ Speed enforcement active Sec 2. Limit 120.", "1m", QColor(255, 190, 40) },
                { "@TurboGT_Driver", "Clocked 294 km/h through coastal sweep! 🔥", "2m", QColor(0, 240, 255) },
                { "@TruckerDave", "Heavy freight in lane 3. Pass on left lanes.", "4m", QColor(80, 220, 120) }
            };

            int postH = pageH / 3;
            for (int i = 0; i < 3; ++i) {
                int py = pageY + i * postH;
                if (i > 0) {
                    p.setPen(QColor(24, 30, 42));
                    p.drawLine(pageRect.left() + 4, py, pageRect.right() - 4, py);
                }
                // Avatar badge
                p.setBrush(posts[i].tagCol);
                p.setPen(Qt::NoPen);
                p.drawEllipse(pageRect.left() + 5, py + 3, 10, 10);

                QFont uf("Arial", 6, QFont::Bold);
                p.setFont(uf);
                p.setPen(posts[i].tagCol);
                p.drawText(pageRect.left() + 19, py + 11, posts[i].user);

                p.setPen(QColor(110, 125, 145));
                p.setFont(postF);
                p.drawText(pageRect.right() - 22, py + 11, posts[i].time);

                p.setPen(QColor(210, 225, 240));
                p.drawText(QRect(pageRect.left() + 5, py + 13, pageRect.width() - 10, 13), Qt::AlignLeft | Qt::AlignVCenter, posts[i].msg);
            }
        } else if (info.currentWebPage == 1) {
            // PAGE 1: Global Leaderboards & Racing News
            QFont nhf("Arial", 6, QFont::Bold);
            p.setFont(nhf);
            p.setPen(QColor(255, 210, 50));
            p.drawText(pageRect.left() + 6, pageY + 12, "★ WORLD SPEEDWAY PURSUIT ★");

            QFont tf("Arial", 6, QFont::Normal);
            p.setFont(tf);
            p.setPen(QColor(180, 195, 215));

            p.drawText(pageRect.left() + 6, pageY + 26, QString("1. GhostRider (PRO) - 312 KM/H • 148K PTS"));
            p.drawText(pageRect.left() + 6, pageY + 39, QString("2. CyberDrifter - 298 KM/H • 112K PTS"));

            // Current player's live ranking
            p.setPen(QColor(0, 240, 255));
            QFont yf("Arial", 6, QFont::Bold);
            p.setFont(yf);
            p.drawText(pageRect.left() + 6, pageY + 53, QString("3. [YOU] Apex - %1 KM/H • %2 PTS").arg((int)player.speed).arg(player.score));

            p.setPen(QColor(130, 145, 165));
            p.setFont(tf);
            p.drawText(pageRect.left() + 6, pageY + 66, "Breaking: Clear skies on Pacific Coast Highway.");
        } else if (info.currentWebPage == 2) {
            // PAGE 2: Highway Satellite Doppler Weather Radar
            int radarRadius = qMin(pageH / 2 - 4, 28);
            int rcx = pageRect.left() + radarRadius + 8;
            int rcy = pageY + pageH / 2;

            // Doppler rings
            p.setBrush(QColor(10, 24, 18));
            p.setPen(QPen(QColor(30, 90, 60), 1));
            p.drawEllipse(QPoint(rcx, rcy), radarRadius, radarRadius);
            p.drawEllipse(QPoint(rcx, rcy), radarRadius / 2, radarRadius / 2);
            p.drawLine(rcx - radarRadius, rcy, rcx + radarRadius, rcy);
            p.drawLine(rcx, rcy - radarRadius, rcx, rcy + radarRadius);

            // Rotating sweep line
            qreal sweepAngle = info.webAnimTimer * 2.8;
            p.setPen(QPen(QColor(40, 255, 120), 1.5));
            p.drawLine(rcx, rcy, rcx + qCos(sweepAngle) * radarRadius, rcy + qSin(sweepAngle) * radarRadius);

            // Telemetry readouts on right
            int wxX = rcx + radarRadius + 10;
            QFont wf("Arial", 6, QFont::Bold);
            p.setFont(wf);
            p.setPen(QColor(80, 240, 140));
            p.drawText(wxX, pageY + 13, "DOPPLER SATELLITE: CLEAR");

            QFont wsf("Arial", 6, QFont::Normal);
            p.setFont(wsf);
            p.setPen(QColor(185, 205, 225));
            p.drawText(wxX, pageY + 26, "Surface Temp: 24°C • Grip: 98%");
            p.drawText(wxX, pageY + 39, "Wind: 14 KM/H WNW • Gusts: 20");
            p.drawText(wxX, pageY + 52, "Visibility: 10+ KM • Precip: 0%");
            p.drawText(wxX, pageY + 65, "Status: OPTIMAL TRACK COND.");
        } else {
            // PAGE 3: Supercar Wiki & Engine Specs
            QFont shf("Arial", 6, QFont::Bold);
            p.setFont(shf);
            p.setPen(QColor(255, 75, 75));
            p.drawText(pageRect.left() + 6, pageY + 13, "APEX GT-R // 4.0L TWIN-TURBO V8");

            QFont sf("Arial", 6, QFont::Normal);
            p.setFont(sf);
            p.setPen(QColor(190, 205, 225));
            p.drawText(pageRect.left() + 6, pageY + 26, "Power: 740 HP @ 7,800 RPM • 850 Nm");
            p.drawText(pageRect.left() + 6, pageY + 39, QString("Live Boost: %1 PSI • RPM: %2").arg(QString::number(player.boostBar * 22.0, 'f', 1)).arg((int)player.rpm));
            p.drawText(pageRect.left() + 6, pageY + 52, "Oil Temp: 94°C • Coolant: 89°C");
            p.drawText(pageRect.left() + 6, pageY + 65, "Tire Press: 34 PSI (FL/FR/RL/RR)");
        }
    }
    // -------------------------------------------------------------
    // 2. MODE: MUSIC PLAYER
    // -------------------------------------------------------------
    else if (info.mode == INFO_MODE_MUSIC) {
        struct TrackMeta {
            QString title;
            QString artist;
            QString album;
            QColor accent;
        } songList[4] = {
            { "MIDNIGHT HIGHWAY 101", "Apex Synth Collective", "Neon Horizon", QColor(0, 240, 255) },
            { "TURBO OUTRUN 84", "Kavinsky Soundworks", "Redline Velocity", QColor(255, 60, 160) },
            { "PACIFIC SUNSET CHILL", "Sunset Dreamwave", "Golden Memories", QColor(255, 180, 40) },
            { "NEO MATRIX OVERDRIVE", "Tokyo Cyber Knights", "Expressway Run", QColor(80, 255, 120) }
        };

        const TrackMeta& curSong = songList[info.currentTrack];

        // Album Art Thumbnail (Left: 52x52 px)
        QRect artRect(sx + 10, contentY + 8, 52, 52);
        p.fillRect(artRect, QColor(14, 18, 28));
        p.setPen(QPen(curSong.accent, 1.5));
        p.drawRoundedRect(artRect, 4, 4);

        // Procedural retro album art (neon sun & wireframe grid)
        p.save();
        p.setClipRect(artRect);
        p.setBrush(curSong.accent);
        p.setPen(Qt::NoPen);
        p.drawEllipse(artRect.center().x(), artRect.center().y() - 5, 16, 16); // Sun
        p.setPen(QColor(255, 255, 255, 120));
        for (int gy = artRect.center().y(); gy < artRect.bottom(); gy += 6) {
            p.drawLine(artRect.left(), gy, artRect.right(), gy); // Grid
        }
        p.restore();

        // Play status badge below album art
        QRect playBadge(sx + 10, contentY + 64, 52, 14);
        p.setBrush(info.isPlaying ? QColor(20, 80, 45) : QColor(80, 50, 20));
        p.setPen(QPen(info.isPlaying ? QColor(40, 230, 90) : QColor(240, 160, 40), 1));
        p.drawRoundedRect(playBadge, 3, 3);
        QFont pbf("Arial", 5, QFont::Bold);
        p.setFont(pbf);
        p.setPen(Qt::white);
        p.drawText(playBadge, Qt::AlignCenter, info.isPlaying ? "▶ PLAYING" : "⏸ PAUSED");

        // Song Title & Artist (Right of album art)
        int infoX = sx + 70;
        QFont titleFont("Arial", 8, QFont::Bold);
        p.setFont(titleFont);
        p.setPen(Qt::white);
        p.drawText(infoX, contentY + 16, curSong.title);

        QFont artFont("Arial", 6, QFont::Normal);
        p.setFont(artFont);
        p.setPen(QColor(165, 185, 210));
        p.drawText(infoX, contentY + 28, QString("%1 • %2").arg(curSong.artist).arg(curSong.album));

        // 16-Band Animated Visualizer Spectrum (Jumping bars to the beat!)
        int vizX = infoX;
        int vizY = contentY + 34;
        int vizW = sw - (vizX - sx) - 12;
        int vizH = 24;
        int barW = (vizW - 15 * 2) / 16;

        for (int b = 0; b < 16; ++b) {
            int bx = vizX + b * (barW + 2);
            qreal lvl = info.isPlaying ? qBound(0.08, (qreal)info.visualizer[b], 1.0) : 0.05;
            int bh = qRound(lvl * vizH);
            int by = vizY + vizH - bh;

            QLinearGradient barGrad(bx, by, bx, vizY + vizH);
            barGrad.setColorAt(0.0, (b < 4) ? QColor(0, 240, 255) : ((b < 11) ? QColor(255, 220, 40) : QColor(255, 60, 140)));
            barGrad.setColorAt(1.0, QColor(20, 30, 50));
            p.fillRect(bx, by, barW, bh, barGrad);
        }

        // Scrubber Progress Bar & Timestamps
        int scrubY = contentY + 84;
        int scrubW = sw - 24;
        QRect scrubRect(sx + 12, scrubY, scrubW, 5);
        p.fillRect(scrubRect, QColor(24, 30, 42));

        qreal progress = qBound(0.0, info.trackElapsed / info.trackDuration, 1.0);
        int fillW = qRound(scrubW * progress);
        p.fillRect(sx + 12, scrubY, fillW, 5, curSong.accent);

        QFont timeFont("Arial", 5, QFont::Normal);
        p.setFont(timeFont);
        p.setPen(QColor(135, 150, 175));
        int elMin = (int)info.trackElapsed / 60;
        int elSec = (int)info.trackElapsed % 60;
        int durMin = (int)info.trackDuration / 60;
        int durSec = (int)info.trackDuration % 60;
        p.drawText(sx + 12, scrubY + 14, QString("%1:%2").arg(elMin).arg(elSec, 2, 10, QChar('0')));
        p.drawText(sx + scrubW - 18, scrubY + 14, QString("%1:%2").arg(durMin).arg(durSec, 2, 10, QChar('0')));

        // Interactive Media Control Buttons: [ |◀ PREV ] [ ▶ PLAY/PAUSE ] [ NEXT ▶| ]
        int btnW = 55;
        int btnH = 18;
        int btnY = scrubY + 18;
        int btnStartX = (sw - (3 * btnW + 2 * 10)) / 2 + sx;

        // Prev button
        QRect prevBtn(btnStartX, btnY, btnW, btnH);
        p.setBrush(QColor(22, 28, 40));
        p.setPen(QPen(QColor(42, 54, 72), 1));
        p.drawRoundedRect(prevBtn, 3, 3);
        QFont ctlFont("Arial", 6, QFont::Bold);
        p.setFont(ctlFont);
        p.setPen(Qt::white);
        p.drawText(prevBtn, Qt::AlignCenter, "|◀ PREV");

        // Play/Pause button
        QRect playBtn(btnStartX + btnW + 10, btnY, btnW, btnH);
        p.setBrush(curSong.accent);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(playBtn, 3, 3);
        p.setPen(QColor(10, 14, 22));
        p.drawText(playBtn, Qt::AlignCenter, info.isPlaying ? "⏸ PAUSE" : "▶ PLAY");

        // Next button
        QRect nextBtn(btnStartX + 2 * (btnW + 10), btnY, btnW, btnH);
        p.setBrush(QColor(22, 28, 40));
        p.setPen(QPen(QColor(42, 54, 72), 1));
        p.drawRoundedRect(nextBtn, 3, 3);
        p.setPen(Qt::white);
        p.drawText(nextBtn, Qt::AlignCenter, "NEXT ▶|");

        // Volume Indicator
        p.setPen(QColor(120, 140, 165));
        p.drawText(sx + sw - 60, btnY + 12, QString("VOL %1%").arg((int)(info.volume * 100)));
    }
    // -------------------------------------------------------------
    // 3. MODE: GPS RADAR
    // -------------------------------------------------------------
    else if (info.mode == INFO_MODE_RADAR) {
        QRect radarRect(sx + 8, contentY + 4, sw - 16, sh - 32);
        p.setBrush(QColor(14, 20, 30));
        p.setPen(QPen(QColor(35, 48, 68), 1));
        p.drawRoundedRect(radarRect, 6, 6);

        p.setPen(QPen(QColor(0, 220, 255, 180), 3));
        int currentSegIdx = (int)(player.worldZ / engine.getTrack().getInfo().segmentLength);

        qreal rx = radarRect.center().x();
        qreal ry = radarRect.bottom() - 15;
        QPainterPath radarRoad;
        radarRoad.moveTo(rx, ry);
        qreal curX = rx;
        for (int step = 0; step < 24; ++step) {
            const RoadSegment& s = engine.getTrack().getSegment(currentSegIdx + step * 2);
            curX += s.curve * 2.2;
            ry -= (radarRect.height() - 25) / 24.0;
            radarRoad.lineTo(curX, ry);
        }
        p.drawPath(radarRoad);

        p.setBrush(QColor(50, 255, 120));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(rx + player.x * 24.0, radarRect.bottom() - 15), 5, 5);

        for (const NPCCar& npc : engine.getTraffic()) {
            qreal relZ = npc.worldZ - player.worldZ;
            if (relZ > 0 && relZ < 2400) {
                qreal dotY = (radarRect.bottom() - 15) - (relZ / 2400.0) * (radarRect.height() - 30);
                qreal dotX = rx + npc.x * 28.0;
                p.setBrush(QColor(255, 60, 60));
                p.drawEllipse(QPointF(dotX, dotY), 4, 4);
            }
        }

        const RoadSegment& cseg = engine.getTrack().findSegment(player.worldZ);
        p.setPen(QColor(0, 240, 255));
        QFont rf("Arial", 7, QFont::Bold);
        p.setFont(rf);
        p.drawText(radarRect.left() + 8, radarRect.top() + 14, QString("GPS LIVE • SEC %1: %2").arg(player.currentSector).arg(cseg.sectorName));
    }
    // -------------------------------------------------------------
    // 4. MODE: CAR DIAGNOSTICS & TELEMETRY
    // -------------------------------------------------------------
    else {
        int tx = sx + 12;
        int ty = contentY + 6;

        // Health Bar
        p.setPen(QPen(QColor(170, 185, 205)));
        QFont sf("Arial", 7, QFont::Bold);
        p.setFont(sf);
        p.drawText(tx, ty + 10, QString("CAR HEALTH: %1%").arg(player.health));

        QRect healthBarRect(tx, ty + 15, sw - 24, 10);
        p.fillRect(healthBarRect, QColor(25, 30, 40));
        int hW = qRound(healthBarRect.width() * (player.health / 100.0));
        QColor hCol = (player.health > 60) ? QColor(40, 220, 80) : ((player.health > 30) ? QColor(245, 180, 30) : QColor(240, 35, 35));
        p.fillRect(tx, ty + 15, hW, 10, hCol);

        // Crashes & Near-Miss Combo
        p.setPen(QColor(230, 235, 245));
        QFont cf("Arial", 8, QFont::Bold);
        p.setFont(cf);
        p.drawText(tx, ty + 42, QString("💥 CRASHES: %1   🔥 COMBO: x%2").arg(player.crashCount).arg(player.nearMissCombo));

        // Boost bar
        p.setPen(QColor(150, 165, 185));
        p.setFont(sf);
        p.drawText(tx, ty + 58, "TURBO CHARGER");
        QRect boostBarRect(tx, ty + 63, sw - 24, 8);
        p.fillRect(boostBarRect, QColor(25, 30, 40));
        int fillW = qRound(boostBarRect.width() * player.boostBar);
        QLinearGradient bGrad(tx, 0, tx + sw - 24, 0);
        bGrad.setColorAt(0.0, QColor(0, 200, 255));
        bGrad.setColorAt(0.7, QColor(255, 200, 40));
        bGrad.setColorAt(1.0, QColor(255, 40, 40));
        p.fillRect(tx, ty + 63, fillW, 8, bGrad);

        // Score & Distance
        p.setPen(QColor(0, 240, 255));
        QFont mf("Arial", 8, QFont::Bold);
        p.setFont(mf);
        p.drawText(tx, ty + 88, QString("SCORE: %1  |  DISTANCE: %2 KM").arg(player.score).arg(QString::number(player.distanceTraveled, 'f', 1)));

        // G-force meter & alert banner
        if (!player.lastAnnouncement.isEmpty()) {
            QRect alertRect(tx, ty + 96, sw - 24, 22);
            p.setBrush(QColor(210, 35, 35, 220));
            p.setPen(QPen(QColor(255, 220, 60), 2));
            p.drawRoundedRect(alertRect, 4, 4);
            QFont af("Arial", 7, QFont::Bold);
            p.setFont(af);
            p.setPen(Qt::white);
            p.drawText(alertRect, Qt::AlignCenter, player.lastAnnouncement);
        } else {
            p.setPen(QColor(120, 140, 165));
            p.setFont(sf);
            p.drawText(tx, ty + 108, QString("G-FORCE DYNAMICS: PITCH %1G • ROLL %2G").arg(QString::number(player.pitchG, 'f', 2)).arg(QString::number(player.rollG, 'f', 2)));
        }
    }

    p.restore();
}

void RasterRenderer::renderSteeringWheel(QPainter& p, const GameEngine& engine, int w, int h) {
    const PlayerCar& player = engine.getPlayer();
    const QImage& wheel = RasterAssets::instance().getSteeringWheel();

    qreal wheelKick = 0;
    if (player.impactCameraShake > 0) {
        wheelKick = (QRandomGenerator::global()->generateDouble() * 2.0 - 1.0) * player.impactCameraShake * 35.0;
    }
    qreal wheelCenterX = 285.0 + player.rollG * 40.0;
    qreal wheelCenterY = h - 145.0 + player.pitchG * 60.0;
    qreal wheelRot = player.steerAngle * 85.0 + wheelKick;

    p.save();
    p.translate(wheelCenterX, wheelCenterY);
    p.rotate(wheelRot);
    p.drawImage(-wheel.width() / 2, -wheel.height() / 2, wheel);
    p.restore();
}

void RasterRenderer::renderWindshieldEffects(QPainter& p, const GameEngine& engine, int w, int h) {
    const PlayerCar& player = engine.getPlayer();

    // High speed blur
    if (player.speed > 200) {
        qreal intensity = (player.speed - 200) / 100.0;
        QRadialGradient vignette(w / 2.0, h / 2.0, w * 0.7);
        vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
        vignette.setColorAt(0.7, QColor(0, 0, 0, 0));
        vignette.setColorAt(1.0, QColor(255, 255, 255, (int)(40 * intensity)));
        p.fillRect(0, 0, w, h, vignette);
    }

    // Red Damage Impact Vignette Flash
    if (player.damageFlashTimer > 0) {
        qreal intensity = qMin(1.0, player.damageFlashTimer / 0.65);
        QRadialGradient damVig(w / 2.0, h / 2.0, w * 0.65);
        damVig.setColorAt(0.0, QColor(0, 0, 0, 0));
        damVig.setColorAt(0.65, QColor(220, 20, 20, 0));
        damVig.setColorAt(1.0, QColor(240, 25, 25, (int)(170 * intensity)));
        p.fillRect(0, 0, w, h, damVig);
    }

    // Radiator Steam / Smoke Billows on Low Health (< 40%)
    if (player.health < 40) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);
        qreal steamPhase = m_rainAnimTime * 3.5;
        for (int s = 0; s < 10; ++s) {
            qreal sx = w * 0.42 + (s * 39) % (int)(w * 0.24) + qSin(steamPhase + s) * 16.0;
            qreal sy = h * 0.64 - fmod(steamPhase * 75.0 + s * 35.0, h * 0.38);
            int rad = 18 + (s % 3) * 9;
            QRadialGradient sGrad(sx, sy, rad);
            sGrad.setColorAt(0.0, QColor(245, 245, 255, 100));
            sGrad.setColorAt(0.5, QColor(215, 225, 240, 40));
            sGrad.setColorAt(1.0, QColor(190, 205, 220, 0));
            p.setBrush(sGrad);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(sx, sy), rad, rad);
        }
        p.restore();
    }

    // Windshield Crack on Collision
    if (player.crashTimer > 0 || player.health < 50) {
        const QImage& crack = RasterAssets::instance().getWindshieldCrack();
        qreal crackAlpha = (player.health < 50) ? 0.85 : qMin(1.0, player.crashTimer / 1.0);
        p.setOpacity(crackAlpha);
        p.drawImage(w / 2 - crack.width() / 2, h * 0.25, crack);
        p.setOpacity(1.0);
    }

    // Dynamic Crash Sparks, Glass Shards & Smoke billows spraying across front windshield
    const QVector<CrashSpark>& sparks = engine.getSparks();
    for (const CrashSpark& spk : sparks) {
        if (spk.life <= 0) continue;
        qreal relZ = spk.worldZ - player.worldZ;
        if (relZ < -150 || relZ > 450) continue; // Close-range collision impact particles

        qreal progress = 1.0 - (spk.life / spk.maxLife);
        qreal screenX = (w * 0.5) + (spk.x - player.x) * (w * 0.65) + (spk.vx * progress * 600.0);
        qreal screenY = (h * 0.58) - (spk.y * 1.5) - (spk.vy * progress * 1.6);

        qreal alphaRatio = spk.life / spk.maxLife;
        int alpha = qBound(0, (int)(alphaRatio * 255), 255);
        qreal drawSize = spk.size * 2.4;

        if (screenX >= -40 && screenX <= w + 40 && screenY >= -40 && screenY <= h) {
            p.save();
            p.setRenderHint(QPainter::Antialiasing, true);
            if (spk.type == 0) {
                // Radiant golden metal friction spark
                QRadialGradient spkGrad(screenX, screenY, drawSize * 2.2);
                spkGrad.setColorAt(0.0, QColor(255, 255, 230, alpha));
                spkGrad.setColorAt(0.4, QColor(255, 200, 50, (int)(alpha * 0.85)));
                spkGrad.setColorAt(1.0, QColor(255, 90, 10, 0));
                p.setBrush(spkGrad);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(screenX, screenY), drawSize * 2.2, drawSize * 2.2);
            } else if (spk.type == 1) {
                // Shattered glass shard
                p.setPen(QPen(QColor(240, 250, 255, alpha), 1.5));
                p.setBrush(QColor(210, 240, 255, (int)(alpha * 0.75)));
                QPolygonF shard;
                shard << QPointF(screenX - drawSize, screenY)
                      << QPointF(screenX, screenY - drawSize * 1.4)
                      << QPointF(screenX + drawSize * 0.8, screenY + drawSize * 0.6)
                      << QPointF(screenX - drawSize * 0.4, screenY + drawSize * 0.8);
                p.drawPolygon(shard);
            } else {
                // Expanding smoke / steam puff
                QRadialGradient smkGrad(screenX, screenY, drawSize * 3.0);
                smkGrad.setColorAt(0.0, QColor(220, 225, 235, (int)(alpha * 0.60)));
                smkGrad.setColorAt(0.6, QColor(170, 175, 185, (int)(alpha * 0.30)));
                smkGrad.setColorAt(1.0, QColor(140, 145, 155, 0));
                p.setBrush(smkGrad);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(screenX, screenY), drawSize * 3.0, drawSize * 2.4);
            }
            p.restore();
        }
    }

    // Dynamic Raindrops streaming across the front glass
    if (engine.getCurrentTheme() == THEME_NEON_METROPOLIS || engine.getTimeOfDay() == TIME_NIGHT) {
        p.setPen(QColor(190, 225, 255, 140));
        qreal speedStretch = qMax(1.0, player.speed / 40.0);
        for (int i = 0; i < 45; ++i) {
            int rx = (i * 97 + (int)(m_rainAnimTime * 800)) % (w - 240) + 120;
            int ry = (i * 73 + (int)(m_rainAnimTime * 1400)) % (int)(h * 0.58) + 40;
            p.drawLine(rx, ry, rx + qRound(speedStretch * 2), ry + qRound(speedStretch * 7));
        }
    }

    // Total Wreck Center Screen Warning Banner
    if (player.isWrecked) {
        QRect wreckBox(w / 2 - 260, h * 0.32, 520, 84);
        p.setBrush(QColor(180, 15, 15, 225));
        p.setPen(QPen(QColor(255, 220, 50), 3));
        p.drawRoundedRect(wreckBox, 10, 10);

        QFont wf("Arial", 16, QFont::Bold);
        p.setFont(wf);
        p.setPen(Qt::white);
        p.drawText(QRect(wreckBox.left(), wreckBox.top() + 10, wreckBox.width(), 32), Qt::AlignCenter, "⚠️ TOTAL WRECK - VEHICLE DESTROYED ⚠️");
        QFont subWf("Arial", 12, QFont::Bold);
        p.setFont(subWf);
        p.setPen(QColor(255, 230, 80));
        p.drawText(QRect(wreckBox.left(), wreckBox.top() + 45, wreckBox.width(), 26), Qt::AlignCenter, "PRESS [R] TO TOW & REPAIR");
    }
}

void RasterRenderer::renderHUD(QPainter& p, const GameEngine& engine, int w, int h) {
    const PlayerCar& player = engine.getPlayer();

    p.fillRect(0, 0, w, 28, QColor(10, 12, 16, 225));

    QFont hf("Arial", 9, QFont::Bold);
    p.setFont(hf);
    p.setPen(QColor(220, 230, 245));

    QString todName = "SUNSET";
    if (engine.getTimeOfDay() == TIME_DAY) todName = "DAY ☀️";
    else if (engine.getTimeOfDay() == TIME_NIGHT) todName = "NIGHT 🌙";
    else todName = "SUNSET 🌅";

    const RoadSegment& cseg = engine.getTrack().findSegment(player.worldZ);
    QString diffTag = "[NORMAL]";
    if (engine.getDifficulty() == DIFFICULTY_EASY) diffTag = "[EASY]";
    else if (engine.getDifficulty() == DIFFICULTY_PRO) diffTag = "[PRO 2.5x]";

    QString trackTag = QString("%1 %2 [SEC %3] %4").arg(engine.getTrack().getInfo().name.toUpper()).arg(diffTag).arg(player.currentSector).arg(todName);
    p.drawText(16, 19, trackTag);

    // Controls Help in center
    p.setPen(QColor(150, 165, 185));
    QFont cf("Arial", 8, QFont::Normal);
    p.setFont(cf);
    p.drawText(w / 2 - 235, 19, "WASD: Drive | M: MENU | Q/E: Look | B: Rear | N: Light | R: Fix");

    // Right-side Telemetry: Health & Crashes & Score
    int rx = w - 380;

    // Health
    QColor hCol = (player.health > 60) ? QColor(40, 225, 90) : ((player.health > 30) ? QColor(250, 180, 20) : QColor(255, 45, 45));
    p.setPen(hCol);
    p.setFont(hf);
    p.drawText(rx, 19, QString("❤️ %1%").arg(player.health));

    // Crashes
    p.setPen(player.crashCount > 0 ? QColor(255, 100, 100) : QColor(160, 220, 255));
    p.drawText(rx + 75, 19, QString("💥 %1").arg(player.crashCount));

    // Combo
    if (player.nearMissCombo > 1) {
        p.setPen(QColor(255, 210, 50));
        p.drawText(rx + 155, 19, QString("🔥 x%1").arg(player.nearMissCombo));
    }

    // Score
    p.setPen(QColor(0, 240, 255));
    p.drawText(w - 135, 19, QString("SCORE: %1").arg(player.score));
}
