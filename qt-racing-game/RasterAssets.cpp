#include "RasterAssets.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QColor>
#include <QFont>
#include <QtMath>
#include <QRandomGenerator>

RasterAssets& RasterAssets::instance() {
    static RasterAssets s_instance;
    return s_instance;
}

RasterAssets::RasterAssets() {
}

void RasterAssets::initialize() {
    generateCockpitAssets();
    generateSteeringWheel();
    generateSideMirrors();
    generateSideGlass();
    generateSidePOVs();
    generateRearCockpit();
    generateGauges();
    generateBackdrops();
    generateSceneryAssets();
    generateVehicles();
    generateSideVehicles();
    generateTextures();
    generateCrackTexture();
    generateHeadlightBeam();
    generateWiperAndGlare();
}

const QImage& RasterAssets::getBackdrop(TrackTheme theme, TimeOfDay tod) const {
    int key = (int)theme * 10 + (int)tod;
    auto it = m_backdrops.constFind(key);
    if (it != m_backdrops.constEnd()) {
        return it.value();
    }
    int fallbackKey = (int)theme * 10 + (int)TIME_SUNSET;
    auto itFb = m_backdrops.constFind(fallbackKey);
    if (itFb != m_backdrops.constEnd()) {
        return itFb.value();
    }
    return m_backdrops.constBegin().value();
}

const QImage& RasterAssets::getScenery(SceneryType type) const {
    auto it = m_scenery.constFind(type);
    if (it != m_scenery.constEnd()) {
        return it.value();
    }
    return m_scenery.constFind(SCENERY_PALM_TREE).value();
}

const QImage& RasterAssets::getGrassTexture(TrackTheme theme) const {
    switch (theme) {
        case THEME_ALPINE_CLIMB: return m_grassAlpineTex;
        case THEME_NEON_METROPOLIS: return m_grassNeonTex;
        case THEME_RED_ROCK: return m_grassDesertTex;
        case THEME_SUNSET_COAST:
        default: return m_grassSunsetTex;
    }
}

const QImage& RasterAssets::getVehicle(VehicleType type, bool rearView, bool brakeLights, bool turnLeft, bool turnRight) const {
    int key = (int)type * 1000 + (rearView ? 1 : 0) * 100 + (brakeLights ? 1 : 0) * 10 + (turnLeft ? 1 : (turnRight ? 2 : 0));
    auto it = m_vehicleSprites.constFind(key);
    if (it != m_vehicleSprites.constEnd()) {
        return it.value();
    }
    int fallbackKey = (int)type * 1000 + (rearView ? 1 : 0) * 100 + (brakeLights ? 1 : 0) * 10;
    auto itFallback = m_vehicleSprites.constFind(fallbackKey);
    if (itFallback != m_vehicleSprites.constEnd()) {
        return itFallback.value();
    }
    return m_vehicleSprites.constBegin().value();
}

const QImage& RasterAssets::getVehicleSide(VehicleType type, bool facingLeft) const {
    int key = (int)type * 10 + (facingLeft ? 1 : 0);
    auto it = m_vehicleSideSprites.constFind(key);
    if (it != m_vehicleSideSprites.constEnd()) {
        return it.value();
    }
    return m_vehicleSideSprites.constBegin().value();
}

void RasterAssets::generateCockpitAssets() {
    const int W = 1280;
    const int H = 720;
    m_dashboard = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
    m_dashboard.fill(Qt::transparent);

    QPainter p(&m_dashboard);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Windshield top tint gradient
    QLinearGradient topTint(0, 0, 0, 120);
    topTint.setColorAt(0.0, QColor(10, 18, 30, 170));
    topTint.setColorAt(0.6, QColor(10, 18, 30, 60));
    topTint.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.fillRect(0, 0, W, 120, topTint);

    // Left A-Pillar (slanted sports car pillar framing windshield and side window)
    QPolygon leftPillar;
    leftPillar << QPoint(140, 0) << QPoint(195, 0) << QPoint(270, 480) << QPoint(205, 560) << QPoint(155, 480);
    QLinearGradient leftPillarGrad(140, 0, 260, 480);
    leftPillarGrad.setColorAt(0.0, QColor(22, 24, 28));
    leftPillarGrad.setColorAt(0.5, QColor(36, 40, 48));
    leftPillarGrad.setColorAt(0.85, QColor(48, 54, 64));
    leftPillarGrad.setColorAt(1.0, QColor(18, 20, 24));
    p.setBrush(leftPillarGrad);
    p.setPen(QColor(10, 10, 12));
    p.drawPolygon(leftPillar);

    // Left Door Sill & Quarter Window Frame (forming spacious driver's side window)
    QPolygon leftDoorSill;
    leftDoorSill << QPoint(0, 465) << QPoint(185, 485) << QPoint(205, 560) << QPoint(0, 560);
    p.setBrush(QColor(24, 27, 32));
    p.drawPolygon(leftDoorSill);
    p.fillRect(0, 0, 150, 34, QColor(18, 20, 24));

    // Right A-Pillar (framing windshield and passenger side window)
    QPolygon rightPillar;
    rightPillar << QPoint(W - 195, 0) << QPoint(W - 140, 0) << QPoint(W - 155, 480) << QPoint(W - 205, 560) << QPoint(W - 270, 480);
    QLinearGradient rightPillarGrad(W - 140, 0, W - 260, 480);
    rightPillarGrad.setColorAt(0.0, QColor(22, 24, 28));
    rightPillarGrad.setColorAt(0.5, QColor(36, 40, 48));
    rightPillarGrad.setColorAt(0.85, QColor(48, 54, 64));
    rightPillarGrad.setColorAt(1.0, QColor(18, 20, 24));
    p.setBrush(rightPillarGrad);
    p.setPen(QColor(10, 10, 12));
    p.drawPolygon(rightPillar);

    // Right Door Sill & Quarter Window Frame
    QPolygon rightDoorSill;
    rightDoorSill << QPoint(W - 185, 485) << QPoint(W, 465) << QPoint(W, 560) << QPoint(W - 205, 560);
    p.setBrush(QColor(24, 27, 32));
    p.drawPolygon(rightDoorSill);
    p.fillRect(W - 150, 0, 150, 34, QColor(18, 20, 24));

    // Roof center liner
    QLinearGradient roofGrad(0, 0, 0, 45);
    roofGrad.setColorAt(0.0, QColor(15, 16, 18));
    roofGrad.setColorAt(1.0, QColor(26, 28, 33));
    p.fillRect(110, 0, W - 220, 42, roofGrad);

    // Dashboard main leather body
    // Sculpted Supercar Front Hood / Bonnet visible outside lower windshield (Liquid Gunmetal Metallic)
    {
        QPainterPath hoodPath;
        hoodPath.moveTo(235, 478);
        hoodPath.cubicTo(360, 442, 520, 434, 650, 436);
        hoodPath.cubicTo(780, 438, 920, 448, 1045, 476);
        hoodPath.lineTo(1065, 510);
        hoodPath.lineTo(220, 510);
        hoodPath.closeSubpath();

        QLinearGradient hoodGrad(640, 432, 640, 500);
        hoodGrad.setColorAt(0.0, QColor(74, 82, 94));
        hoodGrad.setColorAt(0.35, QColor(48, 54, 64));
        hoodGrad.setColorAt(0.75, QColor(28, 32, 38));
        hoodGrad.setColorAt(1.0, QColor(16, 18, 22));
        p.setBrush(hoodGrad);
        p.setPen(QPen(QColor(95, 105, 120), 1.5));
        p.drawPath(hoodPath);

        // Specular sky reflection across center power-bulge & twin aero hood creases
        p.setPen(QPen(QColor(185, 200, 220, 140), 2.0));
        p.drawLine(455, 444, 385, 474);
        p.drawLine(825, 446, 895, 474);
        p.setPen(QPen(QColor(14, 16, 20, 190), 1.5));
        p.drawLine(458, 445, 388, 475);
        p.drawLine(822, 447, 892, 475);

        // Twin recessed carbon-fiber hood heat-extractor vents
        p.setBrush(QColor(16, 18, 22));
        p.setPen(QPen(QColor(62, 68, 78), 1));
        QPolygon leftVent, rightVent;
        leftVent  << QPoint(470, 448) << QPoint(535, 446) << QPoint(525, 458) << QPoint(458, 460);
        rightVent << QPoint(745, 446) << QPoint(810, 448) << QPoint(822, 460) << QPoint(755, 458);
        p.drawPolygon(leftVent);
        p.drawPolygon(rightVent);
    }

    const int dashY = 460;
    QPainterPath dashPath;
    dashPath.moveTo(0, dashY + 40);
    dashPath.cubicTo(120, dashY + 30, 200, dashY - 45, 420, dashY - 45);
    dashPath.cubicTo(560, dashY - 45, 620, dashY + 15, 750, dashY + 20);
    dashPath.cubicTo(850, dashY + 22, 1050, dashY + 25, W, dashY + 35);
    dashPath.lineTo(W, H);
    dashPath.lineTo(0, H);
    dashPath.closeSubpath();

    QLinearGradient dashGrad(0, dashY - 45, 0, H);
    dashGrad.setColorAt(0.0, QColor(32, 36, 42));
    dashGrad.setColorAt(0.2, QColor(24, 27, 32));
    dashGrad.setColorAt(0.5, QColor(16, 18, 22));
    dashGrad.setColorAt(1.0, QColor(10, 11, 14));
    p.setBrush(dashGrad);
    p.setPen(QColor(48, 52, 60));
    p.drawPath(dashPath);

    // Lower Dashboard Two-Tone Luxury Bolster (Warm Espresso / Saddle-Tan Nappa Leather Band)
    {
        QPainterPath lowerTrim;
        lowerTrim.moveTo(0, dashY + 94);
        lowerTrim.cubicTo(150, dashY + 84, 520, dashY + 74, 615, dashY + 78);
        lowerTrim.lineTo(970, dashY + 78);
        lowerTrim.cubicTo(1080, dashY + 78, 1180, dashY + 82, W, dashY + 90);
        lowerTrim.lineTo(W, H);
        lowerTrim.lineTo(0, H);
        lowerTrim.closeSubpath();

        QLinearGradient trimGrad(0, dashY + 74, 0, H);
        trimGrad.setColorAt(0.0, QColor(74, 52, 38));
        trimGrad.setColorAt(0.14, QColor(56, 39, 28));
        trimGrad.setColorAt(0.45, QColor(34, 24, 18));
        trimGrad.setColorAt(1.0, QColor(16, 12, 10));
        p.setBrush(trimGrad);
        p.setPen(QPen(QColor(150, 118, 84, 190), 1.5));
        p.drawPath(lowerTrim);

        // Brushed Satin Champagne-Titanium Trim Spear across dashboard
        QLinearGradient spearGrad(0, dashY + 74, W, dashY + 84);
        spearGrad.setColorAt(0.0, QColor(125, 120, 110));
        spearGrad.setColorAt(0.5, QColor(190, 182, 166));
        spearGrad.setColorAt(1.0, QColor(115, 110, 100));
        p.setPen(QPen(QBrush(spearGrad), 3.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(972, dashY + 78, W - 12, dashY + 86);
        p.drawLine(12, dashY + 92, 165, dashY + 84);
    }

    // Recessed Dark Forged-Carbon Instrument Binnacle Well
    {
        QPainterPath binnacleWell;
        binnacleWell.moveTo(155, dashY + 42);
        binnacleWell.cubicTo(205, dashY - 34, 395, dashY - 34, 540, dashY + 25);
        binnacleWell.lineTo(540, dashY + 195);
        binnacleWell.lineTo(155, dashY + 195);
        binnacleWell.closeSubpath();
        QLinearGradient wellGrad(350, dashY - 30, 350, dashY + 195);
        wellGrad.setColorAt(0.0, QColor(11, 12, 15));
        wellGrad.setColorAt(0.5, QColor(18, 20, 24));
        wellGrad.setColorAt(1.0, QColor(9, 10, 12));
        p.setBrush(wellGrad);
        p.setPen(QPen(QColor(68, 72, 82), 1.5));
        p.drawPath(binnacleWell);
    }

    p.setPen(QPen(QColor(225, 178, 98, 210), 2, Qt::DashLine));
    QPainterPath stitchPath;
    stitchPath.moveTo(160, dashY + 18);
    stitchPath.cubicTo(220, dashY - 38, 380, dashY - 38, 480, dashY - 30);
    p.drawPath(stitchPath);

    QRect centerConsoleRect(615, dashY + 30, 355, 225);
    QLinearGradient carbonGrad(centerConsoleRect.topLeft(), centerConsoleRect.bottomRight());
    carbonGrad.setColorAt(0.0, QColor(30, 32, 36));
    carbonGrad.setColorAt(0.5, QColor(20, 22, 25));
    carbonGrad.setColorAt(1.0, QColor(35, 38, 42));
    p.setBrush(carbonGrad);
    p.setPen(QPen(QColor(60, 65, 75), 2));
    p.drawRoundedRect(centerConsoleRect, 10, 10);

    QRect screenRect(630, dashY + 45, 325, 170);
    p.setBrush(QColor(12, 16, 22));
    p.setPen(QPen(QColor(42, 48, 58), 2));
    p.drawRoundedRect(screenRect, 6, 6);

    auto drawAirVent = [&](int vx, int vy, int vw, int vh) {
        p.setBrush(QColor(18, 20, 24));
        p.setPen(QPen(QColor(80, 85, 95), 2));
        p.drawRoundedRect(vx, vy, vw, vh, 4, 4);
        p.setPen(QColor(45, 50, 58));
        for (int i = vy + 6; i < vy + vh - 4; i += 6) {
            p.drawLine(vx + 4, i, vx + vw - 4, i);
        }
    };
    drawAirVent(160, dashY + 50, 70, 45);
    drawAirVent(550, dashY + 50, 55, 45);
    drawAirVent(980, dashY + 55, 75, 45);

    // Sculpted 3D Instrument Binnacle Cowl & Warm Champagne-Amber Ambient Interior Light Pipe
    {
        QPainterPath cowlRim;
        cowlRim.moveTo(152, dashY + 22);
        cowlRim.cubicTo(215, dashY - 42, 390, dashY - 42, 505, dashY - 26);
        p.setPen(QPen(QColor(115, 110, 102, 190), 3.5, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        p.drawPath(cowlRim);

        // Warm Champagne-Gold / Ice-Amber Ambient LED light pipe across passenger fascia
        p.setPen(QPen(QColor(245, 185, 85, 70), 5.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(982, dashY + 42, W - 18, dashY + 48);
        p.setPen(QPen(QColor(255, 228, 170, 215), 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(982, dashY + 42, W - 18, dashY + 48);

        // Brushed Titanium & Satin Amber "ENGINE START/STOP" Push Button on Console Left Bezel
        QPointF startBtn(576, dashY + 132);
        p.setBrush(QColor(32, 35, 40));
        p.setPen(QPen(QColor(175, 168, 155), 2));
        p.drawEllipse(startBtn, 18, 18);
        QRadialGradient btnGrad(startBtn.x() - 3, startBtn.y() - 4, 15);
        btnGrad.setColorAt(0.0, QColor(78, 82, 92));
        btnGrad.setColorAt(0.7, QColor(38, 40, 46));
        btnGrad.setColorAt(1.0, QColor(20, 22, 26));
        p.setBrush(btnGrad);
        p.setPen(QPen(QColor(235, 182, 88), 1.5));
        p.drawEllipse(startBtn, 13, 13);
        p.setFont(QFont("Arial", 4, QFont::Bold));
        p.setPen(QColor(255, 225, 160));
        p.drawText(QRectF(startBtn.x() - 13, startBtn.y() - 7, 26, 14), Qt::AlignCenter, "START\nENGINE");

        // Knurled Titanium "DRIVE MODE" Rotary Selector below Start Button
        QPointF modeDial(576, dashY + 182);
        p.setBrush(QColor(22, 24, 28));
        p.setPen(QPen(QColor(180, 172, 158, 210), 1.8));
        p.drawEllipse(modeDial, 15, 15);
        p.setBrush(QColor(54, 58, 66));
        p.drawEllipse(modeDial, 9, 9);
        p.setPen(QPen(QColor(245, 188, 80), 2));
        p.drawLine(modeDial, QPointF(modeDial.x() + 7, modeDial.y() - 7));
    }

    p.end();

    // Procedural leather micro-noise
    for (int y = dashY - 40; y < H; ++y) {
        QRgb* scanLine = (QRgb*)m_dashboard.scanLine(y);
        for (int x = 0; x < W; ++x) {
            QRgb pixel = scanLine[x];
            if (qAlpha(pixel) > 200) {
                int noise = (QRandomGenerator::global()->generate() % 9) - 4;
                int r = qBound(0, qRed(pixel) + noise, 255);
                int g = qBound(0, qGreen(pixel) + noise, 255);
                int b = qBound(0, qBlue(pixel) + noise, 255);
                scanLine[x] = qRgba(r, g, b, qAlpha(pixel));
            }
        }
    }

    // Rearview mirror frame
    const int mw = 320;
    const int mh = 90;
    m_mirrorFrame = QImage(mw, mh, QImage::Format_ARGB32_Premultiplied);
    m_mirrorFrame.fill(Qt::transparent);
    QPainter mp(&m_mirrorFrame);
    mp.setRenderHint(QPainter::Antialiasing, true);

    mp.setBrush(QColor(25, 28, 32));
    mp.setPen(Qt::NoPen);
    mp.drawRect(mw / 2 - 10, 0, 20, 20);

    QLinearGradient mGrad(0, 15, 0, mh);
    mGrad.setColorAt(0.0, QColor(45, 50, 58));
    mGrad.setColorAt(0.5, QColor(25, 28, 32));
    mGrad.setColorAt(1.0, QColor(15, 16, 18));
    mp.setBrush(mGrad);
    mp.setPen(QPen(QColor(80, 85, 95), 2));
    mp.drawRoundedRect(6, 16, mw - 12, mh - 22, 16, 16);

    mp.setPen(QPen(QColor(10, 12, 14), 2));
    mp.drawRoundedRect(10, 20, mw - 20, mh - 30, 12, 12);
    mp.end();
}

void RasterAssets::generateSideMirrors() {
    const int mw = 140;
    const int mh = 95;

    // Left Exterior Side Mirror
    m_sideMirrorLeft = QImage(mw, mh, QImage::Format_ARGB32_Premultiplied);
    m_sideMirrorLeft.fill(Qt::transparent);
    {
        QPainter p(&m_sideMirrorLeft);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.setBrush(QColor(25, 28, 32));
        p.setPen(QPen(QColor(15, 16, 18), 1));
        QPolygon stalk;
        stalk << QPoint(mw - 15, 55) << QPoint(mw, 58) << QPoint(mw, 75) << QPoint(mw - 25, 70);
        p.drawPolygon(stalk);

        QLinearGradient shellGrad(0, 0, mw, mh);
        shellGrad.setColorAt(0.0, QColor(55, 60, 70));
        shellGrad.setColorAt(0.5, QColor(28, 30, 35));
        shellGrad.setColorAt(1.0, QColor(15, 16, 18));
        p.setBrush(shellGrad);
        p.setPen(QPen(QColor(70, 75, 85), 2));
        p.drawRoundedRect(4, 8, mw - 24, mh - 16, 14, 14);

        p.setPen(QPen(QColor(10, 12, 15), 2));
        p.drawRoundedRect(8, 12, mw - 32, mh - 24, 10, 10);
    }

    // Right Exterior Side Mirror
    m_sideMirrorRight = QImage(mw, mh, QImage::Format_ARGB32_Premultiplied);
    m_sideMirrorRight.fill(Qt::transparent);
    {
        QPainter p(&m_sideMirrorRight);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.setBrush(QColor(25, 28, 32));
        p.setPen(QPen(QColor(15, 16, 18), 1));
        QPolygon stalk;
        stalk << QPoint(0, 58) << QPoint(15, 55) << QPoint(25, 70) << QPoint(0, 75);
        p.drawPolygon(stalk);

        QLinearGradient shellGrad(mw, 0, 0, mh);
        shellGrad.setColorAt(0.0, QColor(55, 60, 70));
        shellGrad.setColorAt(0.5, QColor(28, 30, 35));
        shellGrad.setColorAt(1.0, QColor(15, 16, 18));
        p.setBrush(shellGrad);
        p.setPen(QPen(QColor(70, 75, 85), 2));
        p.drawRoundedRect(20, 8, mw - 24, mh - 16, 14, 14);

        p.setPen(QPen(QColor(10, 12, 15), 2));
        p.drawRoundedRect(24, 12, mw - 32, mh - 24, 10, 10);
    }
}

void RasterAssets::generateSideGlass() {
    const int W = 1280;
    const int H = 720;
    m_sideGlassOverlay = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
    m_sideGlassOverlay.fill(Qt::transparent);

    QPainter p(&m_sideGlassOverlay);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Left quarter window glass (ultra-clear tempered automotive tint)
    QPolygon leftGlass;
    leftGlass << QPoint(0, 34) << QPoint(140, 34) << QPoint(185, 480) << QPoint(0, 465);
    p.setBrush(QColor(25, 45, 75, 20));
    p.setPen(Qt::NoPen);
    p.drawPolygon(leftGlass);

    QLinearGradient leftGlare(0, 90, 140, 320);
    leftGlare.setColorAt(0.0, QColor(255, 255, 255, 0));
    leftGlare.setColorAt(0.5, QColor(255, 255, 255, 22));
    leftGlare.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.setBrush(leftGlare);
    QPolygon leftStreak;
    leftStreak << QPoint(20, 60) << QPoint(50, 60) << QPoint(115, 400) << QPoint(85, 400);
    p.drawPolygon(leftStreak);

    // Right quarter window glass
    QPolygon rightGlass;
    rightGlass << QPoint(W - 140, 34) << QPoint(W, 34) << QPoint(W, 465) << QPoint(W - 185, 480);
    p.setBrush(QColor(25, 45, 75, 20));
    p.drawPolygon(rightGlass);

    QLinearGradient rightGlare(W, 90, W - 140, 320);
    rightGlare.setColorAt(0.0, QColor(255, 255, 255, 0));
    rightGlare.setColorAt(0.5, QColor(255, 255, 255, 22));
    rightGlare.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.setBrush(rightGlare);
    QPolygon rightStreak;
    rightStreak << QPoint(W - 50, 60) << QPoint(W - 20, 60) << QPoint(W - 85, 400) << QPoint(W - 115, 400);
    p.drawPolygon(rightStreak);

    p.end();
}

void RasterAssets::generateSidePOVs() {
    const int W = 1280;
    const int H = 720;

    // 1. LEFT SIDE POV (Looking 75 degrees out driver's side window)
    m_leftCockpitPOV = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
    m_leftCockpitPOV.fill(Qt::transparent);
    {
        QPainter p(&m_leftCockpitPOV);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Driver door sill, armrest & interior panel (lowered for panoramic window view)
        QPolygon doorSill;
        doorSill << QPoint(0, 480) << QPoint(W * 0.72, 515) << QPoint(W * 0.88, 550) << QPoint(W, 580) << QPoint(W, H) << QPoint(0, H);
        QLinearGradient dGrad(0, 480, 0, H);
        dGrad.setColorAt(0.0, QColor(36, 40, 48));
        dGrad.setColorAt(0.3, QColor(26, 29, 35));
        dGrad.setColorAt(0.7, QColor(18, 20, 24));
        dGrad.setColorAt(1.0, QColor(10, 11, 14));
        p.setBrush(dGrad);
        p.setPen(QColor(50, 55, 65));
        p.drawPolygon(doorSill);

        // Red leather accent stitching along the door armrest
        p.setPen(QPen(QColor(225, 178, 98, 210), 2, Qt::DashLine));
        p.drawLine(10, 495, (int)(W * 0.70), 530);

        // Carbon fiber trim inlay
        QRect carbonTrim(60, 520, 360, 22);
        p.fillRect(carbonTrim, QColor(30, 33, 38));
        p.setPen(QColor(60, 65, 75));
        p.drawRect(carbonTrim);

        // Brushed metal interior door pull handle
        p.setBrush(QColor(165, 172, 185));
        p.setPen(QPen(QColor(60, 65, 75), 1));
        p.drawRoundedRect(W * 0.38, 518, 90, 24, 6, 6);

        // Power window switch cluster with illuminated cyan LEDs
        p.setBrush(QColor(18, 20, 24));
        p.drawRoundedRect(W * 0.54, 532, 95, 30, 4, 4);
        p.setBrush(QColor(0, 210, 255));
        p.setPen(Qt::NoPen);
        p.drawRect(W * 0.54 + 10, 540, 12, 14);
        p.drawRect(W * 0.54 + 30, 540, 12, 14);
        p.drawRect(W * 0.54 + 52, 540, 12, 14);
        p.drawRect(W * 0.54 + 72, 540, 12, 14);

        // Slim B-Pillar on the left edge (rear of the window)
        QLinearGradient bPillarGrad(0, 0, 55, 0);
        bPillarGrad.setColorAt(0.0, QColor(14, 16, 20));
        bPillarGrad.setColorAt(1.0, QColor(28, 32, 38));
        p.fillRect(0, 0, 55, 480, bPillarGrad);

        // Roof rail at top with sun visor contour
        QLinearGradient roofGrad(0, 0, 0, 32);
        roofGrad.setColorAt(0.0, QColor(12, 14, 16));
        roofGrad.setColorAt(1.0, QColor(26, 29, 34));
        p.fillRect(0, 0, W, 32, roofGrad);
        p.setBrush(QColor(32, 36, 44));
        p.drawRoundedRect(180, 6, 280, 20, 4, 4);

        // Forward A-Pillar (slender sports rake)
        QPolygon apillar;
        apillar << QPoint(W * 0.78, 0) << QPoint(W * 0.86, 0) << QPoint(W, 460) << QPoint(W * 0.90, 520);
        QLinearGradient aGrad(W * 0.78, 0, W, 460);
        aGrad.setColorAt(0.0, QColor(24, 27, 32));
        aGrad.setColorAt(0.6, QColor(36, 40, 48));
        aGrad.setColorAt(1.0, QColor(18, 20, 24));
        p.setBrush(aGrad);
        p.setPen(QColor(15, 17, 20));
        p.drawPolygon(apillar);

        // Left exterior side mirror clearly visible mounted outside the door
        p.drawImage(QRect(W * 0.65, 330, 160, 110), m_sideMirrorLeft);

        // Right side of view: Steering wheel rim & instrument cowl in perspective
        QPainterPath dashCowl;
        dashCowl.moveTo(W * 0.88, 440);
        dashCowl.cubicTo(W * 0.92, 380, W * 0.96, 380, W, 400);
        dashCowl.lineTo(W, H);
        dashCowl.lineTo(W * 0.88, H);
        dashCowl.closeSubpath();
        p.setBrush(QColor(22, 24, 28));
        p.setPen(QColor(45, 50, 60));
        p.drawPath(dashCowl);

        // Steering wheel rim arc visible on right edge
        p.setPen(QPen(QColor(35, 38, 44), 28));
        p.drawArc(W - 90, 400, 220, 220, 70 * 16, 140 * 16);
        p.setPen(QPen(QColor(238, 188, 78), 4));
        p.drawArc(W - 90, 400, 220, 220, 135 * 16, 12 * 16); // Center red stripe
    }

    // 2. RIGHT SIDE POV (Looking 75 degrees out passenger side window)
    m_rightCockpitPOV = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
    m_rightCockpitPOV.fill(Qt::transparent);
    {
        QPainter p(&m_rightCockpitPOV);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Passenger door sill & lower interior (lowered for panoramic window view)
        QPolygon doorSill;
        doorSill << QPoint(0, 580) << QPoint(W * 0.12, 550) << QPoint(W * 0.28, 515) << QPoint(W, 480) << QPoint(W, H) << QPoint(0, H);
        QLinearGradient dGrad(0, 480, 0, H);
        dGrad.setColorAt(0.0, QColor(36, 40, 48));
        dGrad.setColorAt(0.3, QColor(26, 29, 35));
        dGrad.setColorAt(0.7, QColor(18, 20, 24));
        dGrad.setColorAt(1.0, QColor(10, 11, 14));
        p.setBrush(dGrad);
        p.setPen(QColor(50, 55, 65));
        p.drawPolygon(doorSill);

        // Red leather accent stitching along passenger door
        p.setPen(QPen(QColor(225, 178, 98, 210), 2, Qt::DashLine));
        p.drawLine((int)(W * 0.30), 530, W - 10, 495);

        // Passenger sports bucket seat on the left (ergonomic racing contour)
        QPainterPath seatPath;
        seatPath.moveTo(0, 650);
        seatPath.lineTo(190, 650);
        seatPath.cubicTo(210, 500, 180, 360, 150, 240);
        seatPath.cubicTo(140, 190, 60, 190, 0, 210);
        seatPath.closeSubpath();
        QLinearGradient seatGrad(0, 200, 200, 600);
        seatGrad.setColorAt(0.0, QColor(34, 37, 44));
        seatGrad.setColorAt(0.6, QColor(22, 24, 28));
        seatGrad.setColorAt(1.0, QColor(14, 15, 18));
        p.setBrush(seatGrad);
        p.setPen(QPen(QColor(55, 60, 70), 2));
        p.drawPath(seatPath);

        // Racing harness slot pass-throughs
        p.setBrush(QColor(12, 14, 16));
        p.setPen(QPen(QColor(210, 40, 40), 2));
        p.drawRoundedRect(35, 270, 75, 24, 6, 6);

        // Passenger A-pillar
        QPolygon apillar;
        apillar << QPoint(W * 0.14, 0) << QPoint(W * 0.22, 0) << QPoint(W * 0.10, 520) << QPoint(0, 460);
        QLinearGradient aGrad(W * 0.22, 0, 0, 460);
        aGrad.setColorAt(0.0, QColor(24, 27, 32));
        aGrad.setColorAt(0.6, QColor(36, 40, 48));
        aGrad.setColorAt(1.0, QColor(18, 20, 24));
        p.setBrush(aGrad);
        p.setPen(QColor(15, 17, 20));
        p.drawPolygon(apillar);

        // Slim B-pillar on right edge (rear of passenger window)
        QLinearGradient bPillarGrad(W - 55, 0, W, 0);
        bPillarGrad.setColorAt(0.0, QColor(28, 32, 38));
        bPillarGrad.setColorAt(1.0, QColor(14, 16, 20));
        p.fillRect(W - 55, 0, 55, 480, bPillarGrad);

        // Roof rail
        QLinearGradient roofGrad(0, 0, 0, 32);
        roofGrad.setColorAt(0.0, QColor(12, 14, 16));
        roofGrad.setColorAt(1.0, QColor(26, 29, 34));
        p.fillRect(0, 0, W, 32, roofGrad);
        p.setBrush(QColor(32, 36, 44));
        p.drawRoundedRect(W - 460, 6, 280, 20, 4, 4);

        // Right exterior mirror visible outside passenger window
        p.drawImage(QRect(W * 0.28, 330, 160, 110), m_sideMirrorRight);
    }
}

void RasterAssets::generateSteeringWheel() {
    const int S = 420;
    m_steeringWheel = QImage(S, S, QImage::Format_ARGB32_Premultiplied);
    m_steeringWheel.fill(Qt::transparent);

    QPainter p(&m_steeringWheel);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QPointF center(S / 2.0, S / 2.0);
    const qreal outerR = 195.0;
    const qreal innerR = 150.0;
    const qreal spokeW = 34.0;

    // Brushed Satin Champagne-Titanium Gearshift Paddles (- Downshift Left / + Upshift Right) behind spokes
    {
        QLinearGradient padGrad(0, center.y() - 95, 0, center.y() + 25);
        padGrad.setColorAt(0.0, QColor(195, 188, 172));
        padGrad.setColorAt(0.5, QColor(128, 122, 112));
        padGrad.setColorAt(1.0, QColor(68, 65, 60));
        p.setBrush(padGrad);
        p.setPen(QPen(QColor(225, 218, 202), 1.5));

        QPolygonF leftPaddle, rightPaddle;
        leftPaddle  << QPointF(center.x() - 138, center.y() - 88)
                    << QPointF(center.x() - 112, center.y() - 98)
                    << QPointF(center.x() - 98,  center.y() + 18)
                    << QPointF(center.x() - 132, center.y() + 8);
        rightPaddle << QPointF(center.x() + 138, center.y() - 88)
                    << QPointF(center.x() + 112, center.y() - 98)
                    << QPointF(center.x() + 98,  center.y() + 18)
                    << QPointF(center.x() + 132, center.y() + 8);
        p.drawPolygon(leftPaddle);
        p.drawPolygon(rightPaddle);

        p.setFont(QFont("Arial", 13, QFont::Bold));
        p.setPen(QColor(245, 192, 85));
        p.drawText(QRectF(center.x() - 136, center.y() - 78, 28, 28), Qt::AlignCenter, "-");
        p.drawText(QRectF(center.x() + 108, center.y() - 78, 28, 28), Qt::AlignCenter, "+");
    }

    QPainterPath rimPath;
    rimPath.addEllipse(center, outerR, outerR);
    QPainterPath innerHole;
    innerHole.addEllipse(center, innerR, innerR);
    rimPath = rimPath.subtracted(innerHole);

    QRadialGradient rimGrad(center.x() - 40, center.y() - 60, outerR + 20);
    rimGrad.setColorAt(0.0, QColor(58, 62, 70));
    rimGrad.setColorAt(0.5, QColor(28, 30, 35));
    rimGrad.setColorAt(0.85, QColor(18, 20, 24));
    rimGrad.setColorAt(1.0, QColor(12, 13, 16));

    p.setBrush(rimGrad);
    p.setPen(QPen(QColor(15, 16, 18), 2));
    p.drawPath(rimPath);

    // Two-Tone Warm Espresso / Saddle-Tan Nappa Leather Side Grips (9 o'clock & 3 o'clock)
    {
        auto drawLeatherGripArc = [&](qreal startDeg, qreal spanDeg) {
            QPainterPath grip;
            grip.arcMoveTo(center.x() - outerR, center.y() - outerR, outerR * 2, outerR * 2, startDeg);
            grip.arcTo(center.x() - outerR, center.y() - outerR, outerR * 2, outerR * 2, startDeg, spanDeg);
            qreal endRad = qDegreesToRadians(startDeg + spanDeg);
            grip.lineTo(center.x() + innerR * qCos(endRad), center.y() - innerR * qSin(endRad));
            grip.arcTo(center.x() - innerR, center.y() - innerR, innerR * 2, innerR * 2, startDeg + spanDeg, -spanDeg);
            grip.closeSubpath();

            QRadialGradient leatherGrad(center.x() - 30, center.y() - 40, outerR);
            leatherGrad.setColorAt(0.0, QColor(96, 66, 46));
            leatherGrad.setColorAt(0.55, QColor(68, 46, 32));
            leatherGrad.setColorAt(0.88, QColor(44, 30, 22));
            leatherGrad.setColorAt(1.0, QColor(28, 19, 14));
            p.setBrush(leatherGrad);
            p.setPen(QPen(QColor(175, 145, 105, 160), 1.5));
            p.drawPath(grip);
        };
        drawLeatherGripArc(-46.0, 92.0);   // Right grip (3 o'clock)
        drawLeatherGripArc(134.0, 92.0);   // Left grip (9 o'clock)

        // Upper carbon-fiber rim gloss clearcoat highlight arc & 5-LED shift-light array
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(180, 192, 210, 85), 3.0, Qt::SolidLine, Qt::RoundCap));
        qreal midR = (outerR + innerR) * 0.5 + 8.0;
        p.drawArc(QRectF(center.x() - midR, center.y() - midR, midR * 2, midR * 2), 52 * 16, 76 * 16);

        // Recessed 5-LED shift-light array on top carbon rim
        for (int i = -2; i <= 2; ++i) {
            if (i == 0) continue; // Leave 12-o'clock center for gold stripe
            qreal ang = qDegreesToRadians(90.0 - i * 7.5);
            qreal lr = (outerR + innerR) * 0.5;
            QPointF lp(center.x() + lr * qCos(ang), center.y() - lr * qSin(ang));
            p.setBrush(qAbs(i) == 1 ? QColor(45, 225, 130) : QColor(245, 185, 55));
            p.setPen(QPen(QColor(20, 22, 26), 1));
            p.drawEllipse(lp, 3.5, 3.5);
        }
    }

    p.setPen(QColor(10, 12, 15, 140));
    for (int angleDeg = -50; angleDeg <= 50; angleDeg += 8) {
        qreal radL = qDegreesToRadians((qreal)(180 + angleDeg));
        qreal radR = qDegreesToRadians((qreal)angleDeg);
        for (qreal r = innerR + 8; r <= outerR - 8; r += 7) {
            p.drawPoint(QPointF(center.x() + r * qCos(radL), center.y() + r * qSin(radL)));
            p.drawPoint(QPointF(center.x() + r * qCos(radR), center.y() + r * qSin(radR)));
        }
    }

    QPainterPath redStripe;
    redStripe.arcMoveTo(center.x() - outerR, center.y() - outerR, outerR * 2, outerR * 2, 85);
    redStripe.arcTo(center.x() - outerR, center.y() - outerR, outerR * 2, outerR * 2, 85, 10);
    redStripe.lineTo(center.x() + innerR * qCos(qDegreesToRadians(95.0)), center.y() - innerR * qSin(qDegreesToRadians(95.0)));
    redStripe.arcTo(center.x() - innerR, center.y() - innerR, innerR * 2, innerR * 2, 95, -10);
    redStripe.closeSubpath();
    p.setBrush(QColor(238, 188, 78));
    p.setPen(Qt::NoPen);
    p.drawPath(redStripe);

    QLinearGradient spokeGrad(center.x() - 100, center.y(), center.x() + 100, center.y());
    spokeGrad.setColorAt(0.0, QColor(60, 65, 75));
    spokeGrad.setColorAt(0.5, QColor(90, 95, 108));
    spokeGrad.setColorAt(1.0, QColor(50, 54, 62));
    p.setBrush(spokeGrad);
    p.setPen(QPen(QColor(30, 32, 38), 2));

    QPolygonF leftSpoke;
    leftSpoke << QPointF(center.x() - innerR - 2, center.y() - spokeW / 2)
              << QPointF(center.x() - 50, center.y() - spokeW / 2 + 5)
              << QPointF(center.x() - 50, center.y() + spokeW / 2 - 5)
              << QPointF(center.x() - innerR - 2, center.y() + spokeW / 2);
    p.drawPolygon(leftSpoke);

    QPolygonF rightSpoke;
    rightSpoke << QPointF(center.x() + innerR + 2, center.y() - spokeW / 2)
               << QPointF(center.x() + 50, center.y() - spokeW / 2 + 5)
               << QPointF(center.x() + 50, center.y() + spokeW / 2 - 5)
               << QPointF(center.x() + innerR + 2, center.y() + spokeW / 2);
    p.drawPolygon(rightSpoke);

    QPolygonF bottomSpoke;
    bottomSpoke << QPointF(center.x() - spokeW / 2, center.y() + 50)
                << QPointF(center.x() - spokeW / 2 + 3, center.y() + innerR + 2)
                << QPointF(center.x() + spokeW / 2 - 3, center.y() + innerR + 2)
                << QPointF(center.x() + spokeW / 2, center.y() + 50);
    p.drawPolygon(bottomSpoke);

    QRadialGradient hubGrad(center.x() - 10, center.y() - 15, 65);
    hubGrad.setColorAt(0.0, QColor(45, 48, 55));
    hubGrad.setColorAt(0.8, QColor(22, 24, 28));
    hubGrad.setColorAt(1.0, QColor(12, 14, 16));
    p.setBrush(hubGrad);
    p.setPen(QPen(QColor(85, 90, 105), 3));
    p.drawEllipse(center, 58, 58);

    p.setBrush(QColor(210, 40, 40));
    p.setPen(QPen(QColor(240, 240, 245), 2));
    p.drawEllipse(center, 26, 26);
    QFont badgeFont("Arial", 12, QFont::Bold);
    p.setFont(badgeFont);
    p.setPen(Qt::white);
    // Spoke thumb-encoders, traction dials & hollowed bottom-spoke trim
    p.setBrush(QColor(18, 20, 24));
    p.setPen(QPen(QColor(185, 175, 150), 1.5));
    p.drawRoundedRect(QRectF(center.x() - 7, center.y() + 68, 14, 62), 4, 4); // Hollowed lower spoke slot

    // Left & right spoke rotary thumb dials + illuminated tactile buttons
    p.setBrush(QColor(28, 30, 36));
    p.setPen(QPen(QColor(215, 180, 110), 1.5));
    p.drawEllipse(QPointF(center.x() - 96, center.y()), 9, 9);
    p.drawEllipse(QPointF(center.x() + 96, center.y()), 9, 9);
    p.setBrush(QColor(240, 188, 78));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(center.x() - 122, center.y() - 5), 3.5, 3.5);
    p.setBrush(QColor(80, 215, 245));
    p.drawEllipse(QPointF(center.x() + 122, center.y() - 5), 3.5, 3.5);

    // Luxury Center Airbag Hub with Champagne-Gold Bezel & Obsidian Crest
    QRadialGradient luxHub(center.x() - 12, center.y() - 16, 62);
    luxHub.setColorAt(0.0, QColor(52, 55, 62));
    luxHub.setColorAt(0.75, QColor(24, 26, 30));
    luxHub.setColorAt(1.0, QColor(14, 15, 18));
    p.setBrush(luxHub);
    p.setPen(QPen(QColor(185, 168, 132), 2.5));
    p.drawEllipse(center, 58, 58);

    // Inner stitched leather ring on hub
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(220, 178, 98, 175), 1.5, Qt::DashLine));
    p.drawEllipse(center, 46, 46);

    // Center GT Medallion (Deep Obsidian Enamel + Satin Champagne-Gold Rim)
    QRadialGradient crestGrad(center.x() - 6, center.y() - 8, 28);
    crestGrad.setColorAt(0.0, QColor(42, 36, 28));
    crestGrad.setColorAt(0.7, QColor(20, 18, 16));
    crestGrad.setColorAt(1.0, QColor(10, 10, 12));
    p.setBrush(crestGrad);
    p.setPen(QPen(QColor(235, 192, 105), 2.2));
    p.drawEllipse(center, 27, 27);
    p.setFont(badgeFont);
    p.setPen(QColor(245, 210, 135));
    p.drawText(QRectF(center.x() - 25, center.y() - 25, 50, 50), Qt::AlignCenter, "GT");

    p.end();
}

void RasterAssets::generateRearCockpit() {
    const int W = 1280;
    const int H = 720;
    m_rearCockpitView = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
    m_rearCockpitView.fill(Qt::transparent);

    QPainter p(&m_rearCockpitView);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath rearRoof;
    rearRoof.moveTo(0, 0);
    rearRoof.lineTo(W, 0);
    rearRoof.lineTo(W, 160);
    rearRoof.cubicTo(W - 220, 140, 220, 140, 0, 160);
    rearRoof.closeSubpath();

    QLinearGradient roofGrad(0, 0, 0, 160);
    roofGrad.setColorAt(0.0, QColor(15, 16, 20));
    roofGrad.setColorAt(1.0, QColor(25, 28, 35));
    p.fillPath(rearRoof, roofGrad);

    QPolygon leftC;
    leftC << QPoint(0, 160) << QPoint(200, 145) << QPoint(240, 520) << QPoint(0, 560);
    p.setBrush(QColor(22, 24, 28));
    p.setPen(QColor(10, 12, 14));
    p.drawPolygon(leftC);

    QPolygon rightC;
    rightC << QPoint(W, 160) << QPoint(W - 200, 145) << QPoint(W - 240, 520) << QPoint(W, 560);
    p.setBrush(QColor(22, 24, 28));
    p.setPen(QColor(10, 12, 14));
    p.drawPolygon(rightC);

    const int shelfY = 500;
    p.fillRect(0, shelfY, W, H - shelfY, QColor(18, 20, 24));

    p.setBrush(QColor(35, 38, 45));
    p.setPen(QColor(15, 16, 18));
    p.drawRoundedRect(W / 2 - 80, 142, 160, 20, 6, 6);

    auto drawHeadrest = [&](int hx, int hy) {
        QPainterPath hr;
        hr.moveTo(hx, hy + 120);
        hr.cubicTo(hx - 20, hy + 20, hx + 10, hy - 40, hx + 70, hy - 40);
        hr.cubicTo(hx + 130, hy - 40, hx + 160, hy + 20, hx + 140, hy + 120);
        hr.closeSubpath();
        QLinearGradient hGrad(hx, hy - 40, hx + 70, hy + 120);
        hGrad.setColorAt(0.0, QColor(42, 45, 52));
        hGrad.setColorAt(0.7, QColor(25, 27, 32));
        hGrad.setColorAt(1.0, QColor(15, 16, 20));
        p.fillPath(hr, hGrad);
        p.setPen(QPen(QColor(210, 40, 40), 2, Qt::DashLine));
        p.drawPath(hr);
    };

    drawHeadrest(300, 430);
    drawHeadrest(840, 430);

    p.end();
}

void RasterAssets::generateGauges() {
    const int S = 220;
    const QPointF c(S / 2.0, S / 2.0);

    // Speedometer Day
    m_gaugeDialSpeed = QImage(S, S, QImage::Format_ARGB32_Premultiplied);
    m_gaugeDialSpeed.fill(Qt::transparent);
    {
        QPainter p(&m_gaugeDialSpeed);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRadialGradient bg(c, S / 2.0);
        bg.setColorAt(0.0, QColor(14, 16, 20));
        bg.setColorAt(0.85, QColor(8, 10, 12));
        bg.setColorAt(0.95, QColor(35, 40, 48));
        bg.setColorAt(1.0, QColor(10, 12, 15));
        p.setBrush(bg);
        p.setPen(QPen(QColor(55, 60, 72), 2));
        p.drawEllipse(c, S / 2.0 - 4, S / 2.0 - 4);

        QFont f("Arial", 9, QFont::Bold);
        p.setFont(f);
        for (int speed = 0; speed <= 300; speed += 20) {
            qreal angle = 135.0 + (speed / 300.0) * 270.0;
            qreal rad = qDegreesToRadians(angle);
            qreal cosA = qCos(rad);
            qreal sinA = qSin(rad);

            qreal r1 = S / 2.0 - 12;
            qreal r2 = (speed % 40 == 0) ? (S / 2.0 - 24) : (S / 2.0 - 18);
            p.setPen(QPen((speed >= 240) ? QColor(240, 60, 60) : QColor(220, 225, 235), (speed % 40 == 0) ? 2 : 1));
            p.drawLine(QPointF(c.x() + r1 * cosA, c.y() + r1 * sinA),
                       QPointF(c.x() + r2 * cosA, c.y() + r2 * sinA));

            if (speed % 40 == 0) {
                qreal rText = S / 2.0 - 36;
                QRectF tr(c.x() + rText * cosA - 18, c.y() + rText * sinA - 10, 36, 20);
                p.drawText(tr, Qt::AlignCenter, QString::number(speed));
            }
        }
        QFont sf("Arial", 8, QFont::DemiBold);
        p.setFont(sf);
        p.setPen(QColor(140, 150, 170));
        p.drawText(QRectF(c.x() - 40, c.y() + 25, 80, 20), Qt::AlignCenter, "KM / H");
    }

    // Speedometer Night
    m_gaugeDialSpeedNight = QImage(S, S, QImage::Format_ARGB32_Premultiplied);
    m_gaugeDialSpeedNight.fill(Qt::transparent);
    {
        QPainter p(&m_gaugeDialSpeedNight);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRadialGradient bg(c, S / 2.0);
        bg.setColorAt(0.0, QColor(4, 16, 26));
        bg.setColorAt(0.85, QColor(2, 8, 16));
        bg.setColorAt(0.95, QColor(0, 180, 255, 80));
        bg.setColorAt(1.0, QColor(5, 10, 18));
        p.setBrush(bg);
        p.setPen(QPen(QColor(0, 220, 255), 2));
        p.drawEllipse(c, S / 2.0 - 4, S / 2.0 - 4);

        QFont f("Arial", 9, QFont::Bold);
        p.setFont(f);
        for (int speed = 0; speed <= 300; speed += 20) {
            qreal angle = 135.0 + (speed / 300.0) * 270.0;
            qreal rad = qDegreesToRadians(angle);
            qreal cosA = qCos(rad);
            qreal sinA = qSin(rad);

            qreal r1 = S / 2.0 - 12;
            qreal r2 = (speed % 40 == 0) ? (S / 2.0 - 24) : (S / 2.0 - 18);
            p.setPen(QPen((speed >= 240) ? QColor(255, 60, 60) : QColor(0, 240, 255), (speed % 40 == 0) ? 2 : 1));
            p.drawLine(QPointF(c.x() + r1 * cosA, c.y() + r1 * sinA),
                       QPointF(c.x() + r2 * cosA, c.y() + r2 * sinA));

            if (speed % 40 == 0) {
                qreal rText = S / 2.0 - 36;
                QRectF tr(c.x() + rText * cosA - 18, c.y() + rText * sinA - 10, 36, 20);
                p.drawText(tr, Qt::AlignCenter, QString::number(speed));
            }
        }
        QFont sf("Arial", 8, QFont::Bold);
        p.setFont(sf);
        p.setPen(QColor(0, 240, 255));
        p.drawText(QRectF(c.x() - 40, c.y() + 25, 80, 20), Qt::AlignCenter, "KM / H");
    }

    // Tachometer Day
    m_gaugeDialRpm = QImage(S, S, QImage::Format_ARGB32_Premultiplied);
    m_gaugeDialRpm.fill(Qt::transparent);
    {
        QPainter p(&m_gaugeDialRpm);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRadialGradient bg(c, S / 2.0);
        bg.setColorAt(0.0, QColor(14, 16, 20));
        bg.setColorAt(0.85, QColor(8, 10, 12));
        bg.setColorAt(0.95, QColor(35, 40, 48));
        bg.setColorAt(1.0, QColor(10, 12, 15));
        p.setBrush(bg);
        p.setPen(QPen(QColor(55, 60, 72), 2));
        p.drawEllipse(c, S / 2.0 - 4, S / 2.0 - 4);

        qreal redlineStart = 135.0 + (6.5 / 8.0) * 270.0;
        qreal redlineEnd = 405.0;
        p.setPen(QPen(QColor(230, 40, 40, 160), 6));
        QRectF arcRect(16, 16, S - 32, S - 32);
        p.drawArc(arcRect, (int)((360 - redlineEnd) * 16), (int)((redlineEnd - redlineStart) * 16));

        QFont f("Arial", 10, QFont::Bold);
        p.setFont(f);
        for (int rpm = 0; rpm <= 8; ++rpm) {
            qreal angle = 135.0 + (rpm / 8.0) * 270.0;
            qreal rad = qDegreesToRadians(angle);
            qreal cosA = qCos(rad);
            qreal sinA = qSin(rad);

            qreal r1 = S / 2.0 - 12;
            qreal r2 = S / 2.0 - 24;
            p.setPen(QPen((rpm >= 7) ? QColor(240, 60, 60) : QColor(220, 225, 235), 2));
            p.drawLine(QPointF(c.x() + r1 * cosA, c.y() + r1 * sinA),
                       QPointF(c.x() + r2 * cosA, c.y() + r2 * sinA));

            qreal rText = S / 2.0 - 36;
            QRectF tr(c.x() + rText * cosA - 15, c.y() + rText * sinA - 10, 30, 20);
            p.drawText(tr, Qt::AlignCenter, QString::number(rpm));
        }
        QFont sf("Arial", 8, QFont::DemiBold);
        p.setFont(sf);
        p.setPen(QColor(140, 150, 170));
        p.drawText(QRectF(c.x() - 50, c.y() + 25, 100, 20), Qt::AlignCenter, "RPM x 1000");
    }

    // Tachometer Night
    m_gaugeDialRpmNight = QImage(S, S, QImage::Format_ARGB32_Premultiplied);
    m_gaugeDialRpmNight.fill(Qt::transparent);
    {
        QPainter p(&m_gaugeDialRpmNight);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRadialGradient bg(c, S / 2.0);
        bg.setColorAt(0.0, QColor(26, 12, 4));
        bg.setColorAt(0.85, QColor(14, 6, 2));
        bg.setColorAt(0.95, QColor(255, 140, 0, 80));
        bg.setColorAt(1.0, QColor(18, 8, 4));
        p.setBrush(bg);
        p.setPen(QPen(QColor(255, 150, 30), 2));
        p.drawEllipse(c, S / 2.0 - 4, S / 2.0 - 4);

        qreal redlineStart = 135.0 + (6.5 / 8.0) * 270.0;
        qreal redlineEnd = 405.0;
        p.setPen(QPen(QColor(255, 30, 30, 230), 6));
        QRectF arcRect(16, 16, S - 32, S - 32);
        p.drawArc(arcRect, (int)((360 - redlineEnd) * 16), (int)((redlineEnd - redlineStart) * 16));

        QFont f("Arial", 10, QFont::Bold);
        p.setFont(f);
        for (int rpm = 0; rpm <= 8; ++rpm) {
            qreal angle = 135.0 + (rpm / 8.0) * 270.0;
            qreal rad = qDegreesToRadians(angle);
            qreal cosA = qCos(rad);
            qreal sinA = qSin(rad);

            qreal r1 = S / 2.0 - 12;
            qreal r2 = S / 2.0 - 24;
            p.setPen(QPen((rpm >= 7) ? QColor(255, 50, 50) : QColor(255, 180, 50), 2));
            p.drawLine(QPointF(c.x() + r1 * cosA, c.y() + r1 * sinA),
                       QPointF(c.x() + r2 * cosA, c.y() + r2 * sinA));

            qreal rText = S / 2.0 - 36;
            QRectF tr(c.x() + rText * cosA - 15, c.y() + rText * sinA - 10, 30, 20);
            p.drawText(tr, Qt::AlignCenter, QString::number(rpm));
        }
        QFont sf("Arial", 8, QFont::Bold);
        p.setFont(sf);
        p.setPen(QColor(255, 160, 40));
        p.drawText(QRectF(c.x() - 50, c.y() + 25, 100, 20), Qt::AlignCenter, "RPM x 1000");
    }

    // Needle
    const int NW = 14;
    const int NH = 100;
    m_needle = QImage(NW, NH, QImage::Format_ARGB32_Premultiplied);
    m_needle.fill(Qt::transparent);
    {
        QPainter p(&m_needle);
        p.setRenderHint(QPainter::Antialiasing, true);

        QPolygonF needlePoly;
        needlePoly << QPointF(NW / 2.0, 4)
                   << QPointF(NW / 2.0 + 3, NH - 20)
                   << QPointF(NW / 2.0 - 3, NH - 20);

        p.setBrush(QColor(255, 40, 40));
        p.setPen(QPen(QColor(255, 140, 140), 1));
        p.drawPolygon(needlePoly);

        p.setBrush(QColor(25, 28, 32));
        p.setPen(QPen(QColor(80, 85, 95), 1));
        p.drawEllipse(QPointF(NW / 2.0, NH - 16), 8, 8);
        p.setBrush(QColor(255, 40, 40));
        p.drawEllipse(QPointF(NW / 2.0, NH - 16), 3, 3);
    }
}

void RasterAssets::generateBackdrops() {
    m_backdrops[(int)THEME_SUNSET_COAST * 10 + (int)TIME_SUNSET] = createBackdropSunset();
    m_backdrops[(int)THEME_SUNSET_COAST * 10 + (int)TIME_DAY] = createBackdropDay();
    m_backdrops[(int)THEME_SUNSET_COAST * 10 + (int)TIME_NIGHT] = createBackdropNight();

    m_backdrops[(int)THEME_ALPINE_CLIMB * 10 + (int)TIME_SUNSET] = createBackdropAlpine(TIME_SUNSET);
    m_backdrops[(int)THEME_ALPINE_CLIMB * 10 + (int)TIME_DAY] = createBackdropAlpine(TIME_DAY);
    m_backdrops[(int)THEME_ALPINE_CLIMB * 10 + (int)TIME_NIGHT] = createBackdropAlpine(TIME_NIGHT);

    m_backdrops[(int)THEME_NEON_METROPOLIS * 10 + (int)TIME_NIGHT] = createBackdropNeon();
    m_backdrops[(int)THEME_NEON_METROPOLIS * 10 + (int)TIME_SUNSET] = createBackdropNeon();
    m_backdrops[(int)THEME_NEON_METROPOLIS * 10 + (int)TIME_DAY] = createBackdropDay();

    m_backdrops[(int)THEME_RED_ROCK * 10 + (int)TIME_SUNSET] = createBackdropRedRock(TIME_SUNSET);
    m_backdrops[(int)THEME_RED_ROCK * 10 + (int)TIME_DAY] = createBackdropRedRock(TIME_DAY);
    m_backdrops[(int)THEME_RED_ROCK * 10 + (int)TIME_NIGHT] = createBackdropRedRock(TIME_NIGHT);
}

QImage RasterAssets::createBackdropSunset() {
    const int W = 1920;
    const int H = 500;
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient sky(0, 0, 0, H);
    sky.setColorAt(0.0, QColor(25, 20, 55));
    sky.setColorAt(0.35, QColor(90, 35, 80));
    sky.setColorAt(0.65, QColor(215, 75, 45));
    sky.setColorAt(0.85, QColor(245, 155, 55));
    sky.setColorAt(1.0, QColor(255, 210, 110));
    p.fillRect(0, 0, W, H, sky);

    QRadialGradient sun(W * 0.45, H * 0.78, 180);
    sun.setColorAt(0.0, QColor(255, 255, 220, 255));
    sun.setColorAt(0.3, QColor(255, 210, 80, 210));
    sun.setColorAt(0.7, QColor(245, 100, 40, 120));
    sun.setColorAt(1.0, QColor(215, 60, 40, 0));
    p.setBrush(sun);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(W * 0.45, H * 0.78), 160, 160);

    p.fillRect(0, H * 0.82, W, H * 0.18, QColor(40, 45, 75));
    QLinearGradient waterReflect(W * 0.45 - 150, 0, W * 0.45 + 150, 0);
    waterReflect.setColorAt(0.0, QColor(245, 120, 50, 0));
    waterReflect.setColorAt(0.5, QColor(255, 200, 100, 180));
    waterReflect.setColorAt(1.0, QColor(245, 120, 50, 0));
    p.fillRect(W * 0.45 - 150, H * 0.82, 300, H * 0.18, waterReflect);

    QPainterPath ridge;
    ridge.moveTo(0, H * 0.84);
    for (int x = 0; x <= W; x += 80) {
        qreal h = H * 0.72 + 25.0 * qSin(x * 0.008) + 15.0 * qCos(x * 0.02);
        ridge.lineTo(x, h);
    }
    ridge.lineTo(W, H);
    ridge.lineTo(0, H);
    ridge.closeSubpath();
    p.fillPath(ridge, QColor(32, 22, 45));

    p.end();
    return img;
}

QImage RasterAssets::createBackdropDay() {
    const int W = 1920;
    const int H = 500;
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient sky(0, 0, 0, H);
    sky.setColorAt(0.0, QColor(40, 120, 220));
    sky.setColorAt(0.55, QColor(110, 185, 250));
    sky.setColorAt(0.85, QColor(190, 225, 255));
    sky.setColorAt(1.0, QColor(230, 245, 255));
    p.fillRect(0, 0, W, H, sky);

    QRadialGradient sun(W * 0.7, H * 0.25, 130);
    sun.setColorAt(0.0, QColor(255, 255, 255, 255));
    sun.setColorAt(0.25, QColor(255, 245, 190, 220));
    sun.setColorAt(0.65, QColor(255, 220, 130, 90));
    sun.setColorAt(1.0, QColor(255, 200, 100, 0));
    p.setBrush(sun);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(W * 0.7, H * 0.25), 130, 130);

    auto drawCloud = [&](int cx, int cy, int cw) {
        p.setBrush(QColor(255, 255, 255, 210));
        p.drawEllipse(cx, cy, cw, 40);
        p.drawEllipse(cx + 25, cy - 18, cw - 30, 48);
        p.drawEllipse(cx + 55, cy - 8, cw - 40, 42);
    };
    drawCloud(150, 90, 140);
    drawCloud(550, 130, 180);
    drawCloud(1050, 80, 160);
    drawCloud(1520, 120, 150);

    QPainterPath hills;
    hills.moveTo(0, H * 0.85);
    for (int x = 0; x <= W; x += 100) {
        qreal h = H * 0.75 + 30.0 * qSin(x * 0.005) + 18.0 * qCos(x * 0.015);
        hills.lineTo(x, h);
    }
    hills.lineTo(W, H);
    hills.lineTo(0, H);
    hills.closeSubpath();
    p.fillPath(hills, QColor(70, 135, 80));

    p.end();
    return img;
}

QImage RasterAssets::createBackdropNight() {
    const int W = 1920;
    const int H = 500;
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient sky(0, 0, 0, H);
    sky.setColorAt(0.0, QColor(4, 6, 14));
    sky.setColorAt(0.65, QColor(10, 16, 28));
    sky.setColorAt(1.0, QColor(20, 28, 48));
    p.fillRect(0, 0, W, H, sky);

    p.setPen(Qt::white);
    for (int i = 0; i < 220; ++i) {
        int sx = (i * 127 + 43) % W;
        int sy = (i * 89 + 17) % (int)(H * 0.72);
        int brightness = (i % 3 == 0) ? 255 : ((i % 3 == 1) ? 180 : 120);
        p.setPen(QColor(brightness, brightness, brightness));
        p.drawPoint(sx, sy);
    }

    p.setBrush(QColor(245, 245, 230));
    p.setPen(Qt::NoPen);
    p.drawEllipse(W * 0.35, H * 0.22, 50, 50);
    p.setBrush(QColor(5, 8, 18));
    p.drawEllipse(W * 0.35 + 14, H * 0.22 - 4, 46, 46);

    QPainterPath ridge;
    ridge.moveTo(0, H * 0.82);
    for (int x = 0; x <= W; x += 90) {
        qreal h = H * 0.74 + 20.0 * qSin(x * 0.007);
        ridge.lineTo(x, h);
    }
    ridge.lineTo(W, H);
    ridge.lineTo(0, H);
    ridge.closeSubpath();
    p.fillPath(ridge, QColor(8, 12, 18));

    p.end();
    return img;
}

QImage RasterAssets::createBackdropAlpine(TimeOfDay tod) {
    const int W = 1920;
    const int H = 500;
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor skyTop, skyMid, skyBot, peakCol, snowCol;
    if (tod == TIME_NIGHT) {
        skyTop = QColor(6, 10, 22);
        skyMid = QColor(14, 22, 38);
        skyBot = QColor(25, 36, 56);
        peakCol = QColor(22, 30, 44);
        snowCol = QColor(120, 140, 165);
    } else if (tod == TIME_SUNSET) {
        skyTop = QColor(45, 35, 75);
        skyMid = QColor(160, 65, 80);
        skyBot = QColor(235, 140, 85);
        peakCol = QColor(80, 45, 60);
        snowCol = QColor(255, 180, 170);
    } else {
        skyTop = QColor(35, 75, 130);
        skyMid = QColor(95, 150, 205);
        skyBot = QColor(185, 215, 235);
        peakCol = QColor(70, 95, 120);
        snowCol = QColor(245, 250, 255);
    }

    QLinearGradient sky(0, 0, 0, H);
    sky.setColorAt(0.0, skyTop);
    sky.setColorAt(0.6, skyMid);
    sky.setColorAt(1.0, skyBot);
    p.fillRect(0, 0, W, H, sky);

    if (tod == TIME_NIGHT) {
        p.setPen(Qt::white);
        for (int i = 0; i < 160; ++i) {
            p.drawPoint((i * 137) % W, (i * 83) % (int)(H * 0.6));
        }
    }

    QPolygonF snowPeaks;
    snowPeaks << QPointF(0, H * 0.75);
    snowPeaks << QPointF(180, H * 0.42) << QPointF(400, H * 0.7)
              << QPointF(750, H * 0.38) << QPointF(1100, H * 0.72)
              << QPointF(1450, H * 0.40) << QPointF(1800, H * 0.68)
              << QPointF(W, H * 0.72) << QPointF(W, H) << QPointF(0, H);
    p.setBrush(peakCol);
    p.setPen(Qt::NoPen);
    p.drawPolygon(snowPeaks);

    p.setBrush(snowCol);
    auto drawSnowCap = [&](qreal px, qreal py) {
        QPolygonF cap;
        cap << QPointF(px, py) << QPointF(px + 45, py + 55) << QPointF(px - 45, py + 55);
        p.drawPolygon(cap);
    };
    drawSnowCap(180, H * 0.42);
    drawSnowCap(750, H * 0.38);
    drawSnowCap(1450, H * 0.40);

    p.end();
    return img;
}

QImage RasterAssets::createBackdropNeon() {
    const int W = 1920;
    const int H = 500;
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient sky(0, 0, 0, H);
    sky.setColorAt(0.0, QColor(8, 8, 16));
    sky.setColorAt(0.5, QColor(16, 12, 28));
    sky.setColorAt(0.85, QColor(36, 18, 52));
    sky.setColorAt(1.0, QColor(50, 22, 65));
    p.fillRect(0, 0, W, H, sky);

    for (int x = 0; x < W; x += 45) {
        int bWidth = 35 + (x * 17) % 30;
        int bHeight = 120 + (x * 37) % 220;
        int by = H - bHeight - 40;

        p.fillRect(x, by, bWidth, bHeight + 40, QColor(12, 14, 22));

        if ((x / 45) % 3 == 0) {
            p.setPen(QColor(0, 220, 255, 180));
            p.drawLine(x, by, x + bWidth, by);
            p.drawLine(x + bWidth / 2, by, x + bWidth / 2, by - 25);
        } else if ((x / 45) % 3 == 1) {
            p.setPen(QColor(255, 40, 160, 180));
            p.drawLine(x, by, x + bWidth, by);
        }

        p.setPen(Qt::NoPen);
        for (int wy = by + 15; wy < H - 50; wy += 14) {
            for (int wx = x + 6; wx < x + bWidth - 6; wx += 8) {
                if ((wx * 13 + wy * 7) % 4 == 0) {
                    QColor winColor = ((wx + wy) % 5 == 0) ? QColor(0, 240, 255, 200) : QColor(255, 210, 120, 160);
                    p.fillRect(wx, wy, 4, 6, winColor);
                }
            }
        }
    }

    QLinearGradient haze(0, H - 100, 0, H);
    haze.setColorAt(0.0, QColor(255, 30, 150, 0));
    haze.setColorAt(1.0, QColor(255, 30, 150, 70));
    p.fillRect(0, H - 100, W, 100, haze);

    p.end();
    return img;
}

QImage RasterAssets::createBackdropRedRock(TimeOfDay tod) {
    const int W = 1920;
    const int H = 500;
    QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor skyTop, skyMid, skyBot, rockTop, rockBot;
    if (tod == TIME_NIGHT) {
        skyTop = QColor(8, 8, 18);
        skyMid = QColor(18, 14, 28);
        skyBot = QColor(35, 20, 36);
        rockTop = QColor(48, 22, 18);
        rockBot = QColor(22, 10, 8);
    } else if (tod == TIME_DAY) {
        skyTop = QColor(30, 95, 185);
        skyMid = QColor(105, 170, 235);
        skyBot = QColor(200, 220, 245);
        rockTop = QColor(180, 80, 50);
        rockBot = QColor(110, 45, 30);
    } else {
        skyTop = QColor(30, 25, 45);
        skyMid = QColor(115, 65, 50);
        skyBot = QColor(240, 165, 80);
        rockTop = QColor(140, 55, 35);
        rockBot = QColor(65, 25, 18);
    }

    QLinearGradient sky(0, 0, 0, H);
    sky.setColorAt(0.0, skyTop);
    sky.setColorAt(0.5, skyMid);
    sky.setColorAt(1.0, skyBot);
    p.fillRect(0, 0, W, H, sky);

    if (tod == TIME_NIGHT) {
        p.setPen(Qt::white);
        for (int i = 0; i < 180; ++i) {
            p.drawPoint((i * 113) % W, (i * 71) % (int)(H * 0.65));
        }
    }

    QPainterPath canyon;
    canyon.moveTo(0, H * 0.75);
    canyon.lineTo(120, H * 0.75);
    canyon.lineTo(160, H * 0.45);
    canyon.lineTo(440, H * 0.45);
    canyon.lineTo(480, H * 0.72);
    canyon.lineTo(700, H * 0.72);
    canyon.lineTo(740, H * 0.40);
    canyon.lineTo(1050, H * 0.40);
    canyon.lineTo(1090, H * 0.70);
    canyon.lineTo(1350, H * 0.70);
    canyon.lineTo(1390, H * 0.48);
    canyon.lineTo(1700, H * 0.48);
    canyon.lineTo(1750, H * 0.75);
    canyon.lineTo(W, H * 0.75);
    canyon.lineTo(W, H);
    canyon.lineTo(0, H);
    canyon.closeSubpath();

    QLinearGradient rockGrad(0, H * 0.4, 0, H);
    rockGrad.setColorAt(0.0, rockTop);
    rockGrad.setColorAt(1.0, rockBot);
    p.fillPath(canyon, rockGrad);

    p.end();
    return img;
}

void RasterAssets::generateHeadlightBeam() {
    const int W = 800;
    const int H = 500;
    m_headlightBeam = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
    m_headlightBeam.fill(Qt::transparent);

    QPainter p(&m_headlightBeam);
    p.setRenderHint(QPainter::Antialiasing, true);

    auto drawBeamCone = [&](int originX, int spreadX) {
        QPainterPath beam;
        beam.moveTo(originX, H);
        beam.lineTo(spreadX - 120, 0);
        beam.lineTo(spreadX + 120, 0);
        beam.closeSubpath();

        QLinearGradient bGrad(originX, H, spreadX, 0);
        bGrad.setColorAt(0.0, QColor(255, 255, 240, 160));
        bGrad.setColorAt(0.35, QColor(255, 255, 220, 110));
        bGrad.setColorAt(0.75, QColor(240, 245, 255, 45));
        bGrad.setColorAt(1.0, QColor(230, 240, 255, 0));
        p.fillPath(beam, bGrad);
    };

    drawBeamCone(W / 2 - 120, W / 2 - 60);
    drawBeamCone(W / 2 + 120, W / 2 + 60);

    p.end();
}

void RasterAssets::generateWiperAndGlare() {
    // 1. Wiper Blade Arm (Rotates across windshield)
    const int WW = 20;
    const int WH = 320;
    m_wiperBlade = QImage(WW, WH, QImage::Format_ARGB32_Premultiplied);
    m_wiperBlade.fill(Qt::transparent);
    {
        QPainter p(&m_wiperBlade);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Metal arm
        p.setPen(QPen(QColor(18, 20, 24), 6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(WW / 2, WH - 10, WW / 2, 70);

        // Articulated joint
        p.setBrush(QColor(35, 38, 45));
        p.drawEllipse(WW / 2 - 6, 64, 12, 12);

        // Rubber squeegee blade
        p.setPen(QPen(QColor(12, 14, 16), 4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(WW / 2, 64, WW / 2, 10);
    }

    // 2. Anamorphic Sun Glare Streak (sweeps across windshield on curves)
    const int GW = 500;
    const int GH = 180;
    m_sunGlare = QImage(GW, GH, QImage::Format_ARGB32_Premultiplied);
    m_sunGlare.fill(Qt::transparent);
    {
        QPainter p(&m_sunGlare);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRadialGradient centerFlare(GW / 2.0, GH / 2.0, 75);
        centerFlare.setColorAt(0.0, QColor(255, 255, 240, 210));
        centerFlare.setColorAt(0.3, QColor(255, 230, 140, 110));
        centerFlare.setColorAt(0.7, QColor(255, 170, 70, 40));
        centerFlare.setColorAt(1.0, QColor(255, 140, 40, 0));
        p.setBrush(centerFlare);
        p.setPen(Qt::NoPen);
        p.drawEllipse(GW / 2 - 75, GH / 2 - 75, 150, 150);

        // Horizontal lens flare streak
        QLinearGradient streakGrad(0, GH / 2.0, GW, GH / 2.0);
        streakGrad.setColorAt(0.0, QColor(255, 220, 100, 0));
        streakGrad.setColorAt(0.5, QColor(255, 250, 220, 160));
        streakGrad.setColorAt(1.0, QColor(255, 220, 100, 0));
        p.fillRect(0, GH / 2 - 4, GW, 8, streakGrad);
    }
}

void RasterAssets::generateSceneryAssets() {
    // Palm Tree
    {
        const int W = 160;
        const int H = 280;
        QImage palm(W, H, QImage::Format_ARGB32_Premultiplied);
        palm.fill(Qt::transparent);
        QPainter p(&palm);
        p.setRenderHint(QPainter::Antialiasing, true);

        QPainterPath trunk;
        trunk.moveTo(W / 2 - 8, H);
        trunk.cubicTo(W / 2 + 15, H * 0.6, W / 2 - 20, H * 0.3, W / 2 - 10, H * 0.2);
        trunk.lineTo(W / 2 + 6, H * 0.2);
        trunk.cubicTo(W / 2 - 4, H * 0.3, W / 2 + 27, H * 0.6, W / 2 + 12, H);
        trunk.closeSubpath();
        p.fillPath(trunk, QColor(95, 65, 45));

        p.setBrush(QColor(35, 110, 45));
        p.setPen(Qt::NoPen);
        auto drawFrond = [&](qreal angleDeg, qreal len) {
            p.save();
            p.translate(W / 2 - 2, H * 0.2);
            p.rotate(angleDeg);
            QPainterPath f;
            f.moveTo(0, 0);
            f.cubicTo(len * 0.4, -18, len * 0.7, -12, len, 8);
            f.cubicTo(len * 0.7, 10, len * 0.4, 6, 0, 0);
            p.drawPath(f);
            p.restore();
        };
        drawFrond(-130, 95);
        drawFrond(-95, 110);
        drawFrond(-60, 105);
        drawFrond(-20, 115);
        drawFrond(20, 100);
        drawFrond(60, 85);
        drawFrond(160, 75);

        p.end();
        m_scenery[SCENERY_PALM_TREE] = palm;
    }

    // Pine Tree
    {
        const int W = 160;
        const int H = 280;
        QImage pine(W, H, QImage::Format_ARGB32_Premultiplied);
        pine.fill(Qt::transparent);
        QPainter p(&pine);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.fillRect(W / 2 - 8, H * 0.75, 16, H * 0.25, QColor(70, 48, 32));

        auto drawTier = [&](int y, int tw, int th, const QColor& col) {
            QPolygon tier;
            tier << QPoint(W / 2, y) << QPoint(W / 2 + tw / 2, y + th) << QPoint(W / 2 - tw / 2, y + th);
            p.setBrush(col);
            p.setPen(Qt::NoPen);
            p.drawPolygon(tier);
        };
        drawTier(H * 0.05, 55, 65, QColor(32, 75, 45));
        drawTier(H * 0.22, 85, 75, QColor(26, 68, 38));
        drawTier(H * 0.42, 115, 85, QColor(22, 60, 32));
        drawTier(H * 0.62, 140, 95, QColor(18, 52, 28));

        p.end();
        m_scenery[SCENERY_PINE_TREE] = pine;
    }

    // Streetlight
    {
        const int W = 120;
        const int H = 280;
        QImage light(W, H, QImage::Format_ARGB32_Premultiplied);
        light.fill(Qt::transparent);
        QPainter p(&light);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.setPen(QPen(QColor(130, 140, 155), 5));
        p.drawLine(W * 0.2, H, W * 0.2, H * 0.15);
        QPainterPath arm;
        arm.moveTo(W * 0.2, H * 0.15);
        arm.cubicTo(W * 0.2, H * 0.05, W * 0.6, H * 0.05, W * 0.8, H * 0.12);
        p.drawPath(arm);

        p.setBrush(QColor(40, 45, 55));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(W * 0.7, H * 0.11, 30, 10, 3, 3);
        p.setBrush(QColor(255, 240, 170));
        p.drawEllipse(W * 0.75, H * 0.14, 16, 8);

        p.end();
        m_scenery[SCENERY_STREETLIGHT] = light;
    }

    // Cactus
    {
        const int W = 140;
        const int H = 240;
        QImage cactus(W, H, QImage::Format_ARGB32_Premultiplied);
        cactus.fill(Qt::transparent);
        QPainter p(&cactus);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.setBrush(QColor(45, 105, 55));
        p.setPen(QPen(QColor(25, 75, 35), 2));
        p.drawRoundedRect(W / 2 - 14, 20, 28, H - 20, 12, 12);
        p.drawRoundedRect(20, 60, 22, 60, 10, 10);
        p.drawRoundedRect(20, 100, W / 2 - 20, 22, 8, 8);
        p.drawRoundedRect(W - 42, 40, 22, 70, 10, 10);
        p.drawRoundedRect(W / 2, 90, W / 2 - 20, 22, 8, 8);

        p.end();
        m_scenery[SCENERY_CACTUS] = cactus;
    }

    // Canyon Boulder
    {
        const int W = 180;
        const int H = 140;
        QImage rock(W, H, QImage::Format_ARGB32_Premultiplied);
        rock.fill(Qt::transparent);
        QPainter p(&rock);
        p.setRenderHint(QPainter::Antialiasing, true);

        QPolygon b;
        b << QPoint(20, H) << QPoint(10, H - 40) << QPoint(45, 25)
          << QPoint(110, 15) << QPoint(165, 45) << QPoint(175, H - 20)
          << QPoint(160, H);
        QLinearGradient rGrad(30, 20, 150, H);
        rGrad.setColorAt(0.0, QColor(165, 75, 50));
        rGrad.setColorAt(0.5, QColor(120, 50, 32));
        rGrad.setColorAt(1.0, QColor(70, 28, 18));
        p.setBrush(rGrad);
        p.setPen(QPen(QColor(40, 15, 10), 2));
        p.drawPolygon(b);

        p.end();
        m_scenery[SCENERY_BOULDER] = rock;
    }

    // High-Elevation Structural Highway Billboards
    {
        // 1. BILLBOARD 1: Super-Tall Dual Lattice Steel Truss Tower (Apex Velocity GT)
        const int W1 = 360;
        const int H1 = 1100;
        QImage board1(W1, H1, QImage::Format_ARGB32_Premultiplied);
        board1.fill(Qt::transparent);
        QPainter p1(&board1);
        p1.setRenderHint(QPainter::Antialiasing, true);

        // Concrete foundation pier at ground level
        p1.setBrush(QColor(135, 140, 148));
        p1.setPen(QColor(75, 80, 88));
        p1.drawRoundedRect(QRectF(W1 * 0.20, H1 - 28, W1 * 0.60, 28), 5, 5);

        // Dual heavy structural steel I-beam columns extending from under display (Y=185) to base (H1-28)
        int col1X = W1 * 0.30;
        int col2X = W1 * 0.70;
        p1.setPen(QPen(QColor(105, 112, 124), 12));
        p1.drawLine(col1X, 185, col1X, H1 - 28);
        p1.drawLine(col2X, 185, col2X, H1 - 28);

        // Steel column metallic highlights
        p1.setPen(QPen(QColor(170, 180, 195), 3));
        p1.drawLine(col1X - 4, 185, col1X - 4, H1 - 28);
        p1.drawLine(col2X - 4, 185, col2X - 4, H1 - 28);

        // Cross-lattice steel truss X-bracing between columns (high-altitude tower structure)
        p1.setPen(QPen(QColor(88, 94, 105), 3.5));
        for (int ty = 195; ty < H1 - 45; ty += 48) {
            p1.drawLine(col1X, ty, col2X, ty + 48);
            p1.drawLine(col2X, ty, col1X, ty + 48);
            p1.drawLine(col1X, ty, col2X, ty); // Horizontal cross strut
        }

        // Maintenance technician ladder on column 1
        p1.setPen(QPen(QColor(185, 192, 202), 1.8));
        for (int ly = 200; ly < H1 - 28; ly += 15) {
            p1.drawLine(col1X - 18, ly, col1X - 5, ly);
        }
        p1.drawLine(col1X - 18, 190, col1X - 18, H1 - 28);

        // Service catwalk platform directly beneath display
        p1.setBrush(QColor(52, 56, 65));
        p1.setPen(QPen(QColor(145, 150, 160), 2));
        p1.drawRect(14, 178, W1 - 28, 14);
        // Catwalk safety handrails
        p1.setPen(QPen(QColor(245, 205, 45), 2.2));
        p1.drawLine(14, 164, W1 - 14, 164);
        for (int rx = 20; rx < W1 - 18; rx += 30) {
            p1.drawLine(rx, 164, rx, 178);
        }

        // 4 Overhead gooseneck spotlight fixtures illuminating sign
        p1.setPen(QPen(QColor(92, 98, 108), 3.5));
        for (int lx = 48; lx <= W1 - 48; lx += 88) {
            p1.drawLine(lx, 26, lx, 8);
            p1.drawLine(lx, 8, lx + 16, 15);
            p1.setBrush(QColor(255, 248, 185));
            p1.drawEllipse(lx + 10, 13, 14, 9);
        }

        // High-Altitude Billboard Display Sign Face (Y = 24 to 175)
        p1.setBrush(QColor(14, 18, 26));
        p1.setPen(QPen(QColor(42, 48, 58), 5));
        p1.drawRoundedRect(12, 24, W1 - 24, 150, 6, 6);

        // LED digital screen background with subtle phosphor scanlines
        p1.setBrush(QColor(18, 22, 32));
        p1.setPen(Qt::NoPen);
        p1.drawRect(16, 28, W1 - 32, 142);
        p1.setPen(QColor(25, 32, 46, 120));
        for (int sy = 30; sy < 168; sy += 4) {
            p1.drawLine(16, sy, W1 - 16, sy);
        }

        // Glowing gold & cyan neon perimeter border
        p1.setPen(QPen(QColor(255, 205, 35), 3));
        p1.setBrush(Qt::NoBrush);
        p1.drawRoundedRect(16, 28, W1 - 32, 142, 4, 4);

        // High-impact neon advert & telemetry typography
        QFont hf1("Arial", 14, QFont::Black);
        p1.setFont(hf1);
        p1.setPen(QColor(255, 220, 50));
        p1.drawText(QRect(16, 34, W1 - 32, 28), Qt::AlignCenter, "★ APEX VELOCITY GT ★");

        QFont mf1("Arial", 11, QFont::Bold);
        p1.setFont(mf1);
        p1.setPen(QColor(0, 245, 255));
        p1.drawText(QRect(16, 64, W1 - 32, 24), Qt::AlignCenter, "HYPERDRIVE HIGHWAY");

        // Electronic Speed Radar Advisory Display Box
        p1.setBrush(QColor(25, 12, 12));
        p1.setPen(QPen(QColor(255, 60, 60), 1.5));
        p1.drawRoundedRect(36, 92, W1 - 72, 38, 4, 4);

        QFont rf1("Arial", 11, QFont::Black);
        p1.setFont(rf1);
        p1.setPen(QColor(255, 50, 50));
        p1.drawText(QRect(40, 94, W1 - 80, 20), Qt::AlignCenter, "SPEED RADAR: 295 KM/H");

        QFont sub1("Arial", 8, QFont::Bold);
        p1.setFont(sub1);
        p1.setPen(QColor(255, 220, 80));
        p1.drawText(QRect(40, 112, W1 - 80, 16), Qt::AlignCenter, "PHOTO ENFORCED • NEXT EXIT 2 MI");

        // Hazard chevron bottom trim
        for (int ch = 18; ch < W1 - 25; ch += 24) {
            p1.fillRect(ch, 160, 12, 8, QColor(255, 210, 40));
            p1.fillRect(ch + 12, 160, 12, 8, QColor(25, 25, 25));
        }

        p1.end();
        m_scenery[SCENERY_BILLBOARD_1] = board1;

        // 2. BILLBOARD 2: Super-Tall Monopole Pylon with Cantilever Outriggers (Turbo Charged)
        const int W2 = 360;
        const int H2 = 1150;
        QImage board2(W2, H2, QImage::Format_ARGB32_Premultiplied);
        board2.fill(Qt::transparent);
        QPainter p2(&board2);
        p2.setRenderHint(QPainter::Antialiasing, true);

        // Concrete base
        p2.setBrush(QColor(135, 140, 148));
        p2.setPen(QColor(75, 80, 88));
        p2.drawRoundedRect(QRectF(W2 * 0.30, H2 - 30, W2 * 0.40, 30), 5, 5);

        // Massive center cylindrical steel column with realistic lighting gradient
        QLinearGradient poleGrad2(W2 * 0.40, 0, W2 * 0.60, 0);
        poleGrad2.setColorAt(0.0, QColor(68, 74, 84));
        poleGrad2.setColorAt(0.35, QColor(165, 175, 192));
        poleGrad2.setColorAt(0.75, QColor(108, 116, 128));
        poleGrad2.setColorAt(1.0, QColor(54, 58, 68));
        p2.setBrush(poleGrad2);
        p2.setPen(Qt::NoPen);
        p2.drawRect(QRectF(W2 * 0.42, 185, W2 * 0.16, H2 - 215));

        // Diagonal cantilever support outriggers (multi-tiered towering pylon)
        p2.setPen(QPen(QColor(105, 114, 126), 9));
        p2.drawLine(W2 * 0.50, 260, W2 * 0.18, 185);
        p2.drawLine(W2 * 0.50, 260, W2 * 0.82, 185);
        p2.drawLine(W2 * 0.50, 380, W2 * 0.22, 185);
        p2.drawLine(W2 * 0.50, 380, W2 * 0.78, 185);
        p2.drawLine(W2 * 0.50, 520, W2 * 0.28, 185);
        p2.drawLine(W2 * 0.50, 520, W2 * 0.72, 185);
        p2.drawLine(W2 * 0.50, 680, W2 * 0.34, 185);
        p2.drawLine(W2 * 0.50, 680, W2 * 0.66, 185);

        // Catwalk & safety railings
        p2.setBrush(QColor(52, 56, 65));
        p2.setPen(QPen(QColor(145, 150, 160), 2));
        p2.drawRect(14, 178, W2 - 28, 14);
        p2.setPen(QPen(QColor(255, 65, 65), 2.2));
        p2.drawLine(14, 164, W2 - 14, 164);
        for (int rx = 20; rx < W2 - 18; rx += 30) {
            p2.drawLine(rx, 164, rx, 178);
        }

        // Overhead Floodlights
        p2.setPen(QPen(QColor(92, 98, 108), 3.5));
        for (int lx = 48; lx <= W2 - 48; lx += 88) {
            p2.drawLine(lx, 26, lx, 8);
            p2.drawLine(lx, 8, lx + 16, 15);
            p2.setBrush(QColor(255, 248, 185));
            p2.drawEllipse(lx + 10, 13, 14, 9);
        }

        // Display Sign Face (Turbo Charged / 250 KM/H)
        p2.setBrush(QColor(16, 14, 22));
        p2.setPen(QPen(QColor(230, 42, 42), 5));
        p2.drawRoundedRect(12, 24, W2 - 24, 150, 6, 6);

        // Crimson digital grid
        p2.setBrush(QColor(24, 16, 26));
        p2.setPen(Qt::NoPen);
        p2.drawRect(16, 28, W2 - 32, 142);
        p2.setPen(QColor(48, 24, 40, 120));
        for (int sy = 30; sy < 168; sy += 4) {
            p2.drawLine(16, sy, W2 - 16, sy);
        }

        // Glowing red neon frame
        p2.setPen(QPen(QColor(255, 50, 50), 3));
        p2.setBrush(Qt::NoBrush);
        p2.drawRoundedRect(16, 28, W2 - 32, 142, 4, 4);

        QFont hf2("Arial", 14, QFont::Black);
        p2.setFont(hf2);
        p2.setPen(QColor(255, 60, 60));
        p2.drawText(QRect(16, 34, W2 - 32, 28), Qt::AlignCenter, "TURBO CHARGED NITRO");

        QFont mf2("Arial", 11, QFont::Bold);
        p2.setFont(mf2);
        p2.setPen(QColor(255, 220, 60));
        p2.drawText(QRect(16, 64, W2 - 32, 24), Qt::AlignCenter, "COASTAL SUPER EXPRESSWAY");

        // Glowing Blue/Cyan Speed Warning Box
        p2.setBrush(QColor(10, 20, 36));
        p2.setPen(QPen(QColor(0, 240, 255), 1.5));
        p2.drawRoundedRect(36, 92, W2 - 72, 38, 4, 4);

        QFont rf2("Arial", 11, QFont::Black);
        p2.setFont(rf2);
        p2.setPen(QColor(0, 240, 255));
        p2.drawText(QRect(40, 94, W2 - 80, 20), Qt::AlignCenter, "MAX VELOCITY: 300 KM/H");

        QFont sub2("Arial", 8, QFont::Bold);
        p2.setFont(sub2);
        p2.setPen(QColor(220, 235, 255));
        p2.drawText(QRect(40, 112, W2 - 80, 16), Qt::AlignCenter, "RADAR ENFORCED • SPEEDWAY 99");

        // Bottom LED marquee dots
        for (int mx = 24; mx < W2 - 20; mx += 16) {
            p2.setBrush(QColor(255, 70, 70));
            p2.setPen(Qt::NoPen);
            p2.drawEllipse(mx, 162, 5, 5);
        }

        p2.end();
        m_scenery[SCENERY_BILLBOARD_2] = board2;

        // 3. SCENERY_HOARDING_NEON: Wide Panoramic Highway Advertising Hoarding
        const int W3 = 460;
        const int H3 = 1000;
        QImage hoard(W3, H3, QImage::Format_ARGB32_Premultiplied);
        hoard.fill(Qt::transparent);
        QPainter p3(&hoard);
        p3.setRenderHint(QPainter::Antialiasing, true);

        // Concrete footings
        p3.setBrush(QColor(135, 140, 148));
        p3.setPen(QColor(75, 80, 88));
        p3.drawRoundedRect(QRectF(W3 * 0.15, H3 - 26, W3 * 0.22, 26), 4, 4);
        p3.drawRoundedRect(QRectF(W3 * 0.63, H3 - 26, W3 * 0.22, 26), 4, 4);

        // Dual heavy structural steel A-frame columns
        int hcol1 = W3 * 0.26;
        int hcol2 = W3 * 0.74;
        p3.setPen(QPen(QColor(100, 108, 120), 12));
        p3.drawLine(hcol1, 180, hcol1, H3 - 26);
        p3.drawLine(hcol2, 180, hcol2, H3 - 26);

        // Cross-lattice steel trussing
        p3.setPen(QPen(QColor(84, 90, 102), 3));
        for (int hy = 195; hy < H3 - 40; hy += 46) {
            p3.drawLine(hcol1, hy, hcol2, hy + 46);
            p3.drawLine(hcol2, hy, hcol1, hy + 46);
            p3.drawLine(hcol1, hy, hcol2, hy);
        }

        // Catwalk
        p3.setBrush(QColor(50, 54, 62));
        p3.setPen(QPen(QColor(140, 145, 155), 2));
        p3.drawRect(12, 174, W3 - 24, 14);
        p3.setPen(QPen(QColor(255, 215, 45), 2));
        p3.drawLine(12, 160, W3 - 12, 160);
        for (int hx = 20; hx < W3 - 16; hx += 28) {
            p3.drawLine(hx, 160, hx, 174);
        }

        // 5 Overhead Spotlights
        p3.setPen(QPen(QColor(90, 95, 105), 3));
        for (int slx = 45; slx <= W3 - 45; slx += 92) {
            p3.drawLine(slx, 24, slx, 8);
            p3.drawLine(slx, 8, slx + 14, 14);
            p3.setBrush(QColor(255, 245, 180));
            p3.drawEllipse(slx + 8, 11, 12, 8);
        }

        // Wide Hoarding Sign Face (Y = 22 to 172)
        p3.setBrush(QColor(14, 16, 22));
        p3.setPen(QPen(QColor(40, 45, 55), 5));
        p3.drawRoundedRect(10, 22, W3 - 20, 150, 6, 6);

        // Dark cyber advert screen
        p3.setBrush(QColor(16, 20, 30));
        p3.setPen(Qt::NoPen);
        p3.drawRect(14, 26, W3 - 28, 142);

        // Glowing cyan & magenta neon frame
        p3.setPen(QPen(QColor(0, 240, 255), 3));
        p3.setBrush(Qt::NoBrush);
        p3.drawRoundedRect(14, 26, W3 - 28, 142, 4, 4);

        // Dynamic commercial advert typography
        QFont hf3("Arial", 15, QFont::Black);
        p3.setFont(hf3);
        p3.setPen(QColor(255, 215, 50));
        p3.drawText(QRect(14, 32, W3 - 28, 28), Qt::AlignCenter, "NITRO RACING RADIALS");

        QFont mf3("Arial", 11, QFont::Bold);
        p3.setFont(mf3);
        p3.setPen(QColor(255, 75, 180));
        p3.drawText(QRect(14, 62, W3 - 28, 24), Qt::AlignCenter, "ULTIMATE 300+ KM/H ROAD GRIP");

        // Commercial sponsor banner box
        p3.setBrush(QColor(20, 32, 50));
        p3.setPen(QPen(QColor(0, 220, 245), 1.5));
        p3.drawRoundedRect(28, 90, W3 - 56, 40, 4, 4);

        QFont bf3("Arial", 11, QFont::Black);
        p3.setFont(bf3);
        p3.setPen(QColor(0, 245, 255));
        p3.drawText(QRect(32, 93, W3 - 64, 20), Qt::AlignCenter, "CASTROL EDGE • APEX MOTORSPORT");

        QFont sub3("Arial", 8, QFont::DemiBold);
        p3.setFont(sub3);
        p3.setPen(QColor(255, 225, 100));
        p3.drawText(QRect(32, 112, W3 - 64, 16), Qt::AlignCenter, "OFFICIAL TRACK PARTNER • NEXT PIT 5 KM");

        // Bottom hazard bar
        for (int hz = 16; hz < W3 - 20; hz += 22) {
            p3.fillRect(hz, 158, 11, 8, QColor(0, 240, 255));
            p3.fillRect(hz + 11, 158, 11, 8, QColor(25, 25, 30));
        }

        p3.end();
        m_scenery[SCENERY_HOARDING_NEON] = hoard;

        // 4. SCENERY_UTILITY_POLE: Roadside Timber Utility/Telegraph Pole with Wires
        const int W4 = 160;
        const int H4 = 580;
        QImage pole(W4, H4, QImage::Format_ARGB32_Premultiplied);
        pole.fill(Qt::transparent);
        QPainter p4(&pole);
        p4.setRenderHint(QPainter::Antialiasing, true);

        // Ground anchor base
        p4.setBrush(QColor(70, 62, 52));
        p4.setPen(QColor(42, 38, 32));
        p4.drawRect(W4 / 2 - 8, H4 - 20, 16, 20);

        // Main Weathered Timber Pole
        QLinearGradient woodGrad(W4 / 2 - 7, 0, W4 / 2 + 7, 0);
        woodGrad.setColorAt(0.0, QColor(62, 54, 45));
        woodGrad.setColorAt(0.4, QColor(108, 96, 80));
        woodGrad.setColorAt(0.8, QColor(82, 72, 60));
        woodGrad.setColorAt(1.0, QColor(48, 42, 35));
        p4.setBrush(woodGrad);
        p4.setPen(Qt::NoPen);
        p4.drawRect(QRectF(W4 / 2 - 7, 30, 14, H4 - 50));

        // Woodgrain lines
        p4.setPen(QPen(QColor(40, 34, 28, 100), 1));
        p4.drawLine(W4 / 2 - 3, 30, W4 / 2 - 3, H4 - 20);
        p4.drawLine(W4 / 2 + 2, 30, W4 / 2 + 2, H4 - 20);

        // Steel climb steps on side of pole
        p4.setPen(QPen(QColor(150, 155, 165), 2));
        for (int py = 180; py < H4 - 60; py += 32) {
            if ((py / 32) % 2 == 0) {
                p4.drawLine(W4 / 2 + 7, py, W4 / 2 + 18, py);
                p4.drawLine(W4 / 2 + 18, py, W4 / 2 + 18, py - 6);
            } else {
                p4.drawLine(W4 / 2 - 7, py, W4 / 2 - 18, py);
                p4.drawLine(W4 / 2 - 18, py, W4 / 2 - 18, py - 6);
            }
        }

        // Upper Heavy Wooden Crossarm (Y = 65)
        p4.setBrush(QColor(78, 68, 56));
        p4.setPen(QPen(QColor(46, 40, 32), 1.5));
        p4.drawRect(18, 62, W4 - 36, 12);
        // Diagonal steel angle braces supporting upper crossarm
        p4.setPen(QPen(QColor(115, 122, 132), 2.5));
        p4.drawLine(35, 74, W4 / 2, 102);
        p4.drawLine(W4 - 35, 74, W4 / 2, 102);

        // Lower Secondary Crossarm (Y = 115)
        p4.setBrush(QColor(78, 68, 56));
        p4.setPen(QPen(QColor(46, 40, 32), 1.5));
        p4.drawRect(28, 112, W4 - 56, 10);
        // Braces
        p4.drawLine(45, 122, W4 / 2, 142);
        p4.drawLine(W4 - 45, 122, W4 / 2, 142);

        // Ceramic/Glass High-Voltage Insulators (Greenish glass & porcelain white)
        auto drawInsulator = [&](int ix, int iy) {
            p4.setBrush(QColor(180, 225, 210));
            p4.setPen(QPen(QColor(60, 95, 80), 1));
            p4.drawRoundedRect(ix - 5, iy - 14, 10, 14, 3, 3);
            p4.setBrush(QColor(235, 245, 240));
            p4.drawEllipse(ix - 3, iy - 16, 6, 5);
        };
        drawInsulator(26, 62);
        drawInsulator(58, 62);
        drawInsulator(W4 - 58, 62);
        drawInsulator(W4 - 26, 62);
        drawInsulator(38, 112);
        drawInsulator(W4 - 38, 112);

        // Cylindrical Electrical Distribution Transformer Can (Y = 138 to 195)
        p4.setBrush(QColor(120, 126, 136));
        p4.setPen(QPen(QColor(65, 70, 78), 2));
        p4.drawRoundedRect(W4 / 2 + 10, 138, 28, 56, 4, 4);
        // Transformer top cap & bushings
        p4.setBrush(QColor(155, 162, 172));
        p4.drawRect(W4 / 2 + 12, 134, 24, 5);
        p4.setBrush(QColor(200, 100, 60)); // Red bushing
        p4.drawRect(W4 / 2 + 18, 126, 6, 8);
        p4.drawRect(W4 / 2 + 28, 126, 6, 8);
        // Danger high voltage yellow label
        p4.setBrush(QColor(245, 205, 35));
        p4.setPen(Qt::NoPen);
        p4.drawRect(W4 / 2 + 16, 162, 16, 10);

        // Diagonal Galvanized Steel Guy Wire to ground
        p4.setPen(QPen(QColor(160, 168, 178), 2));
        p4.drawLine(W4 / 2 - 6, 115, 8, H4 - 10);
        // Yellow plastic guy wire guard at bottom
        p4.setPen(QPen(QColor(255, 220, 30), 5));
        p4.drawLine(18, H4 - 80, 8, H4 - 10);

        // Sagging Power & Telecom Wires extending to sides
        p4.setPen(QPen(QColor(32, 35, 42, 180), 1.8));
        // High voltage top wires
        p4.drawLine(0, 52, 26, 48);
        p4.drawLine(26, 48, 58, 48);
        p4.drawLine(W4 - 58, 48, W4 - 26, 48);
        p4.drawLine(W4 - 26, 48, W4, 52);
        // Secondary distribution wires
        p4.drawLine(0, 104, 38, 98);
        p4.drawLine(W4 - 38, 98, W4, 104);
        // Thick black fiber optic bundle at Y = 215
        p4.setPen(QPen(QColor(20, 22, 25), 3));
        p4.drawLine(0, 218, W4 / 2, 215);
        p4.drawLine(W4 / 2, 215, W4, 218);

        p4.end();
        m_scenery[SCENERY_UTILITY_POLE] = pole;
    }

    // Guardrail post
    {
        const int W = 50;
        const int H = 90;
        QImage rail(W, H, QImage::Format_ARGB32_Premultiplied);
        rail.fill(Qt::transparent);
        QPainter p(&rail);
        p.setRenderHint(QPainter::Antialiasing, true);

        p.fillRect(W / 2 - 4, 30, 8, H - 30, QColor(100, 105, 115));
        QLinearGradient gGrad(0, 20, 0, 45);
        gGrad.setColorAt(0.0, QColor(210, 215, 225));
        gGrad.setColorAt(0.5, QColor(140, 145, 155));
        gGrad.setColorAt(1.0, QColor(180, 185, 195));
        p.fillRect(0, 20, W, 25, gGrad);

        p.end();
        m_scenery[SCENERY_GUARDRAIL] = rail;
    }

    // Highway Gantry (Overhead Sign Bridge spanning across lanes)
    {
        const int W = 560;
        const int H = 200;
        QImage gantry(W, H, QImage::Format_ARGB32_Premultiplied);
        gantry.fill(Qt::transparent);
        QPainter p(&gantry);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Steel Pylons (Left & Right)
        auto drawPylon = [&](int px) {
            p.fillRect(px, 35, 14, H - 35, QColor(95, 105, 120));
            p.fillRect(px + 2, 35, 3, H - 35, QColor(160, 175, 195)); // Highlight
            // Truss cross bracing
            p.setPen(QPen(QColor(70, 80, 95), 2));
            for (int y = 45; y < H - 15; y += 22) {
                p.drawLine(px, y, px + 14, y + 20);
                p.drawLine(px + 14, y, px, y + 20);
            }
            // Heavy concrete foundation footing
            p.fillRect(px - 4, H - 18, 22, 18, QColor(130, 135, 145));
            p.fillRect(px - 2, H - 16, 18, 4, QColor(170, 175, 185));
        };
        drawPylon(28);
        drawPylon(W - 42);

        // Overhead Double Steel Crossbeam Truss
        QLinearGradient trussGrad(0, 25, 0, 60);
        trussGrad.setColorAt(0.0, QColor(135, 145, 160));
        trussGrad.setColorAt(0.5, QColor(90, 100, 115));
        trussGrad.setColorAt(1.0, QColor(65, 75, 90));
        p.fillRect(20, 30, W - 40, 26, trussGrad);
        p.setPen(QPen(QColor(180, 195, 215), 2));
        p.drawLine(20, 30, W - 20, 30);
        p.drawLine(20, 56, W - 20, 56);

        // Catwalk & Safety Railing on Top
        p.setPen(QPen(QColor(110, 120, 135), 2));
        p.drawLine(20, 18, W - 20, 18);
        for (int rx = 35; rx < W - 30; rx += 28) {
            p.drawLine(rx, 18, rx, 30);
        }

        // Truss Zigzag Bracing
        p.setPen(QPen(QColor(60, 70, 82), 2));
        for (int bx = 28; bx < W - 45; bx += 32) {
            p.drawLine(bx, 30, bx + 16, 56);
            p.drawLine(bx + 16, 56, bx + 32, 30);
        }

        // Green Highway Overhead Sign 1 (Left / Fast Lanes)
        {
            int sx = 65, sy = 60, sw = 210, sh = 82;
            p.fillRect(sx, sy, sw, sh, QColor(14, 118, 56));
            p.setPen(QPen(QColor(245, 245, 250), 3));
            p.drawRect(sx + 3, sy + 3, sw - 6, sh - 6);

            // Exit tab
            p.fillRect(sx + 6, sy - 14, 72, 16, QColor(14, 118, 56));
            p.setPen(QPen(QColor(245, 245, 250), 2));
            p.drawRect(sx + 6, sy - 14, 72, 16);
            QFont tabF("Arial", 8, QFont::Bold);
            p.setFont(tabF);
            p.setPen(Qt::white);
            p.drawText(QRect(sx + 6, sy - 14, 72, 16), Qt::AlignCenter, "EXIT 28A");

            QFont titleF("Arial", 11, QFont::Bold);
            p.setFont(titleF);
            p.drawText(QRect(sx + 8, sy + 8, sw - 16, 24), Qt::AlignCenter, "WEST 101 EXPRESS");
            QFont subF("Arial", 9, QFont::DemiBold);
            p.setFont(subF);
            p.drawText(QRect(sx + 8, sy + 32, sw - 16, 20), Qt::AlignCenter, "OCEAN SHORE PKWY");
            QFont arrowF("Arial", 12, QFont::Bold);
            p.setFont(arrowF);
            p.drawText(QRect(sx + 8, sy + 52, sw - 16, 24), Qt::AlignCenter, "⬆  THRU TRAFFIC  ⬆");
        }

        // Green Highway Overhead Sign 2 (Right / Exit Lanes)
        {
            int sx = 295, sy = 60, sw = 175, sh = 82;
            p.fillRect(sx, sy, sw, sh, QColor(14, 118, 56));
            p.setPen(QPen(QColor(245, 245, 250), 3));
            p.drawRect(sx + 3, sy + 3, sw - 6, sh - 6);

            // Exit tab
            p.fillRect(sx + 6, sy - 14, 72, 16, QColor(14, 118, 56));
            p.setPen(QPen(QColor(245, 245, 250), 2));
            p.drawRect(sx + 6, sy - 14, 72, 16);
            QFont tabF("Arial", 8, QFont::Bold);
            p.setFont(tabF);
            p.setPen(Qt::white);
            p.drawText(QRect(sx + 6, sy - 14, 72, 16), Qt::AlignCenter, "EXIT 28B");

            QFont titleF("Arial", 10, QFont::Bold);
            p.setFont(titleF);
            p.drawText(QRect(sx + 6, sy + 8, sw - 12, 22), Qt::AlignCenter, "MARINA & BAY");
            QFont subF("Arial", 8, QFont::DemiBold);
            p.setFont(subF);
            p.drawText(QRect(sx + 6, sy + 30, sw - 12, 20), Qt::AlignCenter, "HARBOR BLVD");
            QFont arrowF("Arial", 12, QFont::Bold);
            p.setFont(arrowF);
            p.setPen(QColor(255, 230, 80));
            p.drawText(QRect(sx + 6, sy + 52, sw - 12, 24), Qt::AlignCenter, "EXIT ONLY ↗");
        }

        // Amber LED Speed / Travel Time Matrix
        {
            int sx = 485, sy = 64, sw = 45, sh = 74;
            p.fillRect(sx, sy, sw, sh, QColor(15, 18, 24));
            p.setPen(QPen(QColor(45, 55, 70), 2));
            p.drawRect(sx, sy, sw, sh);
            QFont ledF("Arial", 8, QFont::Bold);
            p.setFont(ledF);
            p.setPen(QColor(255, 180, 20));
            p.drawText(QRect(sx + 2, sy + 6, sw - 4, 18), Qt::AlignCenter, "LIMIT");
            QFont numF("Arial", 14, QFont::Bold);
            p.setFont(numF);
            p.setPen(QColor(255, 220, 60));
            p.drawText(QRect(sx + 2, sy + 24, sw - 4, 26), Qt::AlignCenter, "120");
            QFont kmhF("Arial", 7, QFont::Normal);
            p.setFont(kmhF);
            p.setPen(QColor(255, 180, 20));
            p.drawText(QRect(sx + 2, sy + 50, sw - 4, 18), Qt::AlignCenter, "KM/H");
        }

        p.end();
        m_scenery[SCENERY_HIGHWAY_GANTRY] = gantry;
    }

    // Wind Turbine (Modern Aerodynamic Turbine with Rotor Blades)
    {
        const int W = 220;
        const int H = 340;
        QImage turbine(W, H, QImage::Format_ARGB32_Premultiplied);
        turbine.fill(Qt::transparent);
        QPainter p(&turbine);
        p.setRenderHint(QPainter::Antialiasing, true);

        int hubX = W / 2;
        int hubY = 120;

        // Tapered Steel Tubular Tower
        QPolygon tower;
        tower << QPoint(hubX - 6, hubY + 6) << QPoint(hubX + 6, hubY + 6)
              << QPoint(hubX + 16, H) << QPoint(hubX - 16, H);
        QLinearGradient towerGrad(hubX - 16, 0, hubX + 16, 0);
        towerGrad.setColorAt(0.0, QColor(190, 195, 205));
        towerGrad.setColorAt(0.4, QColor(245, 248, 252));
        towerGrad.setColorAt(1.0, QColor(165, 172, 182));
        p.setBrush(towerGrad);
        p.setPen(Qt::NoPen);
        p.drawPolygon(tower);

        // Concrete Foundation Pad
        p.fillRect(hubX - 22, H - 10, 44, 10, QColor(120, 125, 135));

        // Nacelle (Generator Housing)
        QLinearGradient nacGrad(hubX - 18, hubY - 12, hubX + 22, hubY + 8);
        nacGrad.setColorAt(0.0, QColor(250, 252, 255));
        nacGrad.setColorAt(1.0, QColor(175, 182, 192));
        p.setBrush(nacGrad);
        p.drawRoundedRect(hubX - 20, hubY - 10, 42, 18, 6, 6);

        // Aviation Red Strobe Beacon on Nacelle
        p.setBrush(QColor(255, 30, 30));
        p.drawEllipse(hubX - 2, hubY - 14, 4, 4);

        // Rotor Blades (3 blades at 120 degrees apart)
        auto drawBlade = [&](qreal angleDeg) {
            p.save();
            p.translate(hubX, hubY);
            p.rotate(angleDeg);

            QPainterPath blade;
            blade.moveTo(-2, 0);
            blade.cubicTo(-6, -30, -5, -75, -2, -105);
            blade.lineTo(0, -108);
            blade.lineTo(2, -105);
            blade.cubicTo(5, -75, 6, -30, 2, 0);
            blade.closeSubpath();

            QLinearGradient bGrad(-6, 0, 6, 0);
            bGrad.setColorAt(0.0, QColor(230, 235, 242));
            bGrad.setColorAt(0.5, QColor(255, 255, 255));
            bGrad.setColorAt(1.0, QColor(195, 202, 212));
            p.setBrush(bGrad);
            p.setPen(Qt::NoPen);
            p.drawPath(blade);

            // Red safety tip stripe
            p.fillRect(-2, -108, 4, 14, QColor(220, 35, 35));

            p.restore();
        };

        drawBlade(0);    // Blade 1: pointing up
        drawBlade(120);  // Blade 2: pointing down-right
        drawBlade(240);  // Blade 3: pointing down-left

        // Central Rotor Spinner Dome
        QRadialGradient hubGrad(hubX - 2, hubY - 2, 10);
        hubGrad.setColorAt(0.0, QColor(255, 255, 255));
        hubGrad.setColorAt(1.0, QColor(160, 168, 178));
        p.setBrush(hubGrad);
        p.drawEllipse(hubX - 8, hubY - 8, 16, 16);

        p.end();
        m_scenery[SCENERY_WIND_TURBINE] = turbine;
    }

    // Canyon Rock Arch (Majestic Sandstone Archway)
    {
        const int W = 320;
        const int H = 240;
        QImage arch(W, H, QImage::Format_ARGB32_Premultiplied);
        arch.fill(Qt::transparent);
        QPainter p(&arch);
        p.setRenderHint(QPainter::Antialiasing, true);

        // Natural Sandstone Arch Formation
        QPainterPath rockBody;
        rockBody.moveTo(10, H);
        rockBody.lineTo(20, H * 0.45);
        rockBody.cubicTo(35, H * 0.15, 80, 15, W * 0.5, 12);
        rockBody.cubicTo(W * 0.78, 15, W - 35, H * 0.18, W - 15, H * 0.5);
        rockBody.lineTo(W - 5, H);
        rockBody.lineTo(W - 75, H);
        rockBody.cubicTo(W - 85, H * 0.65, W - 110, H * 0.38, W * 0.5, H * 0.36);
        rockBody.cubicTo(110, H * 0.38, 85, H * 0.65, 75, H);
        rockBody.closeSubpath();

        QLinearGradient rGrad(50, 10, 260, H);
        rGrad.setColorAt(0.0, QColor(210, 115, 65));
        rGrad.setColorAt(0.35, QColor(180, 85, 45));
        rGrad.setColorAt(0.7, QColor(145, 58, 30));
        rGrad.setColorAt(1.0, QColor(90, 32, 16));
        p.setBrush(rGrad);
        p.setPen(QPen(QColor(65, 22, 12), 2));
        p.drawPath(rockBody);

        // Sedimentary Rock Striations & Crags
        p.setPen(QPen(QColor(235, 145, 85, 140), 3));
        p.drawLine(40, H * 0.32, W - 40, H * 0.34);
        p.setPen(QPen(QColor(115, 40, 20, 180), 3));
        p.drawLine(30, H * 0.48, 85, H * 0.52);
        p.drawLine(W - 85, H * 0.52, W - 25, H * 0.49);
        p.setPen(QPen(QColor(245, 165, 105, 120), 2));
        p.drawLine(70, H * 0.22, W - 70, H * 0.24);

        p.end();
        m_scenery[SCENERY_ROCK_ARCH] = arch;
    }
}

QImage RasterAssets::createVehicleSprite(VehicleType type, bool rearView, bool brakeLights, bool turnLeft, bool turnRight) {
    const int W = 280;
    const int H = 220;
    QImage car(W, H, QImage::Format_ARGB32_Premultiplied);
    car.fill(Qt::transparent);
    QPainter p(&car);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor bodyColor;
    int carWidth = 216;
    int carHeight = 106;
    int carY = 76;

    switch (type) {
        case VEHICLE_SPORTS_RED: // VORTEX V12 (Rosso Corsa Crimson Hypercar)
            bodyColor = QColor(220, 28, 34);
            carWidth = 236;
            carHeight = 88;
            carY = 94;
            break;
        case VEHICLE_SEDAN_SILVER: // APEX GT-R (Liquid Gunmetal Titanium Coupe)
            bodyColor = QColor(118, 128, 142);
            carWidth = 224;
            carHeight = 98;
            carY = 84;
            break;
        case VEHICLE_SUV_BLACK: // TITAN ENFORCER (Tactical Emerald-Obsidian Interceptor)
            bodyColor = QColor(28, 48, 38);
            carWidth = 242;
            carHeight = 124;
            carY = 56;
            break;
        case VEHICLE_COMPACT_BLUE: // DRIFT SPEC-R (Solar Amber-Gold JDM Widebody)
            bodyColor = QColor(235, 172, 38);
            carWidth = 234;
            carHeight = 96;
            carY = 86;
            break;
        case VEHICLE_TRUCK_SEMI: // Commercial Semi Truck
            bodyColor = QColor(226, 229, 235);
            carWidth = 248;
            carHeight = 158;
            carY = 22;
            break;
    }

    int cx = W / 2;
    int left = cx - carWidth / 2;
    int right = cx + carWidth / 2;

    // 1. Realistic Road Drop Shadow with Ambient Occlusion
    QRadialGradient shadowGrad(cx, carY + carHeight - 4, carWidth * 0.58);
    shadowGrad.setColorAt(0.0, QColor(8, 10, 14, 210));
    shadowGrad.setColorAt(0.7, QColor(8, 10, 14, 120));
    shadowGrad.setColorAt(1.0, QColor(8, 10, 14, 0));
    p.setBrush(shadowGrad);
    p.setPen(Qt::NoPen);
    p.drawEllipse(cx - carWidth / 2 - 8, carY + carHeight - 16, carWidth + 16, 28);

    // 2. Wide Rubber Performance Tires with Tread Grooves
    auto drawTire = [&](int tx, int ty, int tw, int th) {
        p.setBrush(QColor(18, 20, 24));
        p.setPen(QPen(QColor(10, 11, 14), 1));
        p.drawRoundedRect(tx, ty, tw, th, 5, 5);
        p.setPen(QColor(32, 35, 42));
        for (int i = tx + 4; i < tx + tw - 2; i += 5) {
            p.drawLine(i, ty + 4, i, ty + th - 4);
        }
    };

    if (type == VEHICLE_TRUCK_SEMI) {
        // Dual tandem rear wheels
        drawTire(left + 4, carY + carHeight - 34, 26, 34);
        drawTire(left + 32, carY + carHeight - 34, 26, 34);
        drawTire(right - 58, carY + carHeight - 34, 26, 34);
        drawTire(right - 30, carY + carHeight - 34, 26, 34);
    } else {
        int tireW = (type == VEHICLE_SPORTS_RED) ? 38 : 32;
        drawTire(left + 8, carY + carHeight - 32, tireW, 32);
        drawTire(right - 8 - tireW, carY + carHeight - 32, tireW, 32);
    }

    // 2b. Underglow Lighting (for DRIFT SPEC-R)
    if (type == VEHICLE_COMPACT_BLUE) {
        p.setBrush(QColor(0, 240, 255, 150));
        p.setPen(Qt::NoPen);
        p.drawEllipse(cx - carWidth / 2 + 10, carY + carHeight - 14, carWidth - 20, 16);
    }

    // 3. Sculpted Body Geometry (Distinct silhouettes per car model!)
    QPainterPath body;
    if (type == VEHICLE_TRUCK_SEMI) {
        body.addRoundedRect(left, carY, carWidth, carHeight - 10, 6, 6);
    } else if (type == VEHICLE_SPORTS_RED) {
        // VORTEX V12: Ultra-low wide exotic hypercar wedge
        body.moveTo(left + 8, carY + carHeight - 10);
        body.lineTo(right - 8, carY + carHeight - 10);
        body.cubicTo(right + 8, carY + carHeight - 12, right + 6, carY + carHeight * 0.40, right - 16, carY + carHeight * 0.36);
        body.lineTo(right - 44, carY + 8);
        body.lineTo(left + 44, carY + 8);
        body.lineTo(left + 16, carY + carHeight * 0.36);
        body.cubicTo(left - 6, carY + carHeight * 0.40, left - 8, carY + carHeight - 12, left + 8, carY + carHeight - 10);
        body.closeSubpath();
    } else if (type == VEHICLE_COMPACT_BLUE) {
        // DRIFT SPEC-R: Extreme widebody stance with flared rivet overfenders
        body.moveTo(left + 6, carY + carHeight - 10);
        body.lineTo(right - 6, carY + carHeight - 10);
        body.cubicTo(right + 12, carY + carHeight - 14, right + 8, carY + carHeight * 0.46, right - 14, carY + carHeight * 0.42);
        body.lineTo(right - 34, carY + 8);
        body.lineTo(left + 34, carY + 8);
        body.lineTo(left + 14, carY + carHeight * 0.42);
        body.cubicTo(left - 8, carY + carHeight * 0.46, left - 12, carY + carHeight - 14, left + 6, carY + carHeight - 10);
        body.closeSubpath();
    } else if (type == VEHICLE_SUV_BLACK) {
        // TITAN ENFORCER: Muscular, heavy-duty boxy interceptor shoulders
        body.moveTo(left + 8, carY + carHeight - 10);
        body.lineTo(right - 8, carY + carHeight - 10);
        body.lineTo(right - 8, carY + carHeight * 0.50);
        body.lineTo(right - 26, carY + 6);
        body.lineTo(left + 26, carY + 6);
        body.lineTo(left + 8, carY + carHeight * 0.50);
        body.closeSubpath();
    } else {
        // APEX GT-R: Sculpted Twin-Turbo Coupe
        body.moveTo(left + 14, carY + carHeight - 12);
        body.lineTo(right - 14, carY + carHeight - 12);
        body.cubicTo(right + 5, carY + carHeight - 14, right + 4, carY + carHeight * 0.44, right - 12, carY + carHeight * 0.40);
        body.lineTo(right - 36, carY + 6);
        body.lineTo(left + 36, carY + 6);
        body.lineTo(left + 12, carY + carHeight * 0.40);
        body.cubicTo(left - 4, carY + carHeight * 0.44, left - 5, carY + carHeight - 14, left + 14, carY + carHeight - 12);
        body.closeSubpath();
    }

    // Metallic Multi-Stop Shading
    QLinearGradient bGrad(cx - carWidth / 2, carY, cx + carWidth / 2, carY + carHeight);
    bGrad.setColorAt(0.0, bodyColor.lighter(138));
    bGrad.setColorAt(0.2, bodyColor.lighter(115));
    bGrad.setColorAt(0.55, bodyColor);
    bGrad.setColorAt(0.85, bodyColor.darker(130));
    bGrad.setColorAt(1.0, bodyColor.darker(165));
    p.setBrush(bGrad);
    p.setPen(QPen(bodyColor.darker(190), 2));
    p.drawPath(body);

    // Twin Gold Racing Stripes on APEX GT-R
    if (type == VEHICLE_SEDAN_SILVER) {
        p.fillRect(cx - 8, carY + 6, 5, carHeight - 18, QColor(228, 182, 86, 215));
        p.fillRect(cx + 3, carY + 6, 5, carHeight - 18, QColor(228, 182, 86, 215));
    }

    // Carbon fiber deck on DRIFT SPEC-R
    if (type == VEHICLE_COMPACT_BLUE) {
        p.fillRect(cx - 36, carY + 6, 72, 14, QColor(24, 26, 30));
    }

    // Le Mans Dorsal Shark Fin on VORTEX V12
    if (type == VEHICLE_SPORTS_RED) {
        QPolygon sharkFin;
        sharkFin << QPoint(cx - 2, carY - 6) << QPoint(cx + 2, carY - 6)
                 << QPoint(cx + 2, carY + qRound(carHeight * 0.42))
                 << QPoint(cx - 2, carY + qRound(carHeight * 0.42));
        p.setBrush(QColor(22, 25, 30));
        p.setPen(QPen(QColor(215, 38, 48), 1.2));
        p.drawPolygon(sharkFin);
    }

    // Roof Police Strobe Bar on TITAN ENFORCER
    if (type == VEHICLE_SUV_BLACK) {
        QRect strobeBar(cx - 32, carY - 6, 64, 8);
        p.setBrush(QColor(20, 24, 30));
        p.setPen(QPen(QColor(90, 100, 115), 1));
        p.drawRoundedRect(strobeBar, 2, 2);
        p.setBrush(QColor(0, 160, 255));
        p.drawRoundedRect(cx - 30, carY - 5, 28, 6, 1, 1);
        p.setBrush(QColor(255, 35, 45));
        p.drawRoundedRect(cx + 2, carY - 5, 28, 6, 1, 1);
    }

    // 3b. Professional Multi-Volume Automotive Shading:
    if (type != VEHICLE_TRUCK_SEMI) {
        // Exterior wing mirrors
        int mirY = carY + qRound(carHeight * 0.34);
        p.setBrush(bodyColor.darker(125));
        p.setPen(QPen(bodyColor.darker(185), 1.2));
        p.drawRoundedRect(left - 2, mirY, 15, 9, 3, 3);
        p.drawRoundedRect(right - 13, mirY, 15, 9, 3, 3);

        int beltY = carY + qRound(carHeight * 0.41);
        QLinearGradient beltGrad(left + 12, beltY, right - 12, beltY);
        beltGrad.setColorAt(0.0, QColor(255, 255, 255, 35));
        beltGrad.setColorAt(0.25, bodyColor.lighter(175));
        beltGrad.setColorAt(0.5, QColor(255, 255, 255, 195));
        beltGrad.setColorAt(0.75, bodyColor.lighter(175));
        beltGrad.setColorAt(1.0, QColor(255, 255, 255, 35));
        p.setPen(QPen(QBrush(beltGrad), 2.2));
        p.drawLine(left + 14, beltY, right - 14, beltY);

        p.setPen(QPen(QColor(255, 255, 255, 145), 1.8));
        p.drawLine(left + 40, carY + 8, right - 40, carY + 8);
    }

    // Rear Spoilers / Wings (Customized per Car Model)
    if (rearView) {
        if (type == VEHICLE_SEDAN_SILVER) {
            // APEX GT-R: Swan-Neck GT Wing on dual stanchions
            p.setBrush(QColor(18, 20, 24));
            p.setPen(Qt::NoPen);
            p.drawRect(cx - 48, carY + carHeight * 0.36 - 16, 7, 16);
            p.drawRect(cx + 41, carY + carHeight * 0.36 - 16, 7, 16);
            p.setBrush(QColor(36, 40, 48));
            p.setPen(QPen(QColor(218, 175, 92), 1.2));
            p.drawRoundedRect(left + 10, carY + carHeight * 0.36 - 20, carWidth - 20, 7, 2, 2);
        } else if (type == VEHICLE_COMPACT_BLUE) {
            // DRIFT SPEC-R: Massive High-Rise Drift Wing on tall alloy struts
            p.setBrush(QColor(185, 190, 200));
            p.setPen(Qt::NoPen);
            p.drawRect(cx - 56, carY + carHeight * 0.36 - 26, 7, 26);
            p.drawRect(cx + 49, carY + carHeight * 0.36 - 26, 7, 26);
            p.setBrush(QColor(28, 30, 36));
            p.setPen(QPen(QColor(245, 185, 45), 1.5));
            p.drawRoundedRect(left + 2, carY + carHeight * 0.36 - 30, carWidth - 4, 8, 2, 2);
            // Tall drift endplates
            p.setBrush(QColor(235, 172, 38));
            p.drawRoundedRect(left - 2, carY + carHeight * 0.36 - 36, 6, 20, 2, 2);
            p.drawRoundedRect(right - 4, carY + carHeight * 0.36 - 36, 6, 20, 2, 2);
        } else if (type == VEHICLE_SPORTS_RED) {
            // VORTEX V12: Active aero deck spoiler blade
            p.setBrush(QColor(18, 20, 24));
            p.setPen(QPen(QColor(255, 45, 45), 1.5));
            p.drawRoundedRect(left + 12, carY + carHeight * 0.38 - 10, carWidth - 24, 6, 2, 2);
        } else if (type == VEHICLE_SUV_BLACK) {
            // TITAN ENFORCER: Drag Ducktail Spoiler
            p.setBrush(QColor(20, 24, 22));
            p.setPen(QPen(QColor(75, 95, 85), 1.8));
            p.drawRoundedRect(left + 14, carY + carHeight * 0.42 - 8, carWidth - 28, 7, 2, 2);
        }
    }

    if (rearView) {
        // 4. Rear Glass with Defroster Filaments & Interior Headrests
        if (type != VEHICLE_TRUCK_SEMI) {
            QPainterPath rearGlass;
            rearGlass.moveTo(left + 42, carY + 14);
            rearGlass.lineTo(right - 42, carY + 14);
            rearGlass.lineTo(right - 26, carY + carHeight * 0.42);
            rearGlass.lineTo(left + 26, carY + carHeight * 0.42);
            rearGlass.closeSubpath();
            p.setBrush(QColor(16, 22, 32));
            p.setPen(QPen(QColor(36, 42, 52), 1));
            p.drawPath(rearGlass);

            // Diagonal automotive sky reflection streak across rear glass
            {
                QPolygon glassGlare;
                glassGlare << QPoint(left + 52, carY + 15)
                           << QPoint(left + 78, carY + 15)
                           << QPoint(left + 54, carY + qRound(carHeight * 0.41))
                           << QPoint(left + 32, carY + qRound(carHeight * 0.41));
                p.setBrush(QColor(215, 235, 255, 52));
                p.setPen(Qt::NoPen);
                p.drawPolygon(glassGlare);
            }

            // Defroster lines
            p.setPen(QColor(220, 160, 60, 45));
            for (int gy = carY + 20; gy < carY + carHeight * 0.40; gy += 6) {
                p.drawLine(left + 38, gy, right - 38, gy);
            }

            // Headrest silhouettes
            p.setBrush(QColor(10, 12, 16, 200));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(cx - 45, carY + 22, 22, 18, 5, 5);
            p.drawRoundedRect(cx + 23, carY + 22, 22, 18, 5, 5);

            // Center High-Mounted Stop Lamp (CHMSL)
            QColor chmslColor = brakeLights ? QColor(255, 30, 30) : QColor(140, 20, 20);
            p.setBrush(chmslColor);
            p.drawRoundedRect(cx - 24, carY + 14, 48, 4, 1, 1);
            if (brakeLights) {
                p.setBrush(QColor(255, 60, 60, 120));
                p.drawEllipse(cx - 30, carY + 11, 60, 10);
            }
        }

        // 5. Rear Diffuser & Stainless Exhaust Pipes
        if (type != VEHICLE_TRUCK_SEMI) {
            // Matte Carbon Diffuser
            p.setBrush(QColor(18, 20, 24));
            p.setPen(QPen(QColor(40, 44, 52), 1));
            QRect diffuserRect(cx - 65, carY + carHeight - 18, 130, 14);
            p.drawRoundedRect(diffuserRect, 2, 2);
            // Diffuser fins
            p.fillRect(cx - 45, carY + carHeight - 18, 4, 14, QColor(40, 44, 52));
            p.fillRect(cx - 15, carY + carHeight - 18, 4, 14, QColor(40, 44, 52));
            p.fillRect(cx + 11, carY + carHeight - 18, 4, 14, QColor(40, 44, 52));
            p.fillRect(cx + 41, carY + carHeight - 18, 4, 14, QColor(40, 44, 52));

            // Exhaust tips (Customized per Car Model)
            auto drawExhaust = [&](int ex, int ey, int ew, int eh) {
                p.setBrush(QColor(190, 195, 205));
                p.setPen(QPen(QColor(80, 85, 95), 1));
                p.drawEllipse(ex, ey, ew, eh);
                p.setBrush(QColor(14, 16, 20));
                p.drawEllipse(ex + 2, ey + 2, ew - 4, eh - 4);
            };

            if (type == VEHICLE_SPORTS_RED) {
                // VORTEX V12: Center Triple-Jet Exhaust Cluster (/\)
                drawExhaust(cx - 12, carY + carHeight - 14, 10, 8);
                drawExhaust(cx + 2,  carY + carHeight - 14, 10, 8);
                drawExhaust(cx - 5,  carY + carHeight - 22, 10, 8);
            } else if (type == VEHICLE_COMPACT_BLUE) {
                // DRIFT SPEC-R: Dual Angled JDM Blast Pipes on left side
                drawExhaust(left + 36, carY + carHeight - 13, 13, 10);
                drawExhaust(left + 52, carY + carHeight - 13, 13, 10);
            } else if (type == VEHICLE_SUV_BLACK) {
                // TITAN ENFORCER: Dual Heavy Rectangular Black-Chrome Exhausts
                p.setBrush(QColor(45, 50, 56));
                p.setPen(QPen(QColor(110, 120, 130), 1.5));
                p.drawRoundedRect(left + 36, carY + carHeight - 14, 20, 10, 2, 2);
                p.drawRoundedRect(right - 56, carY + carHeight - 14, 20, 10, 2, 2);
                p.setBrush(QColor(10, 12, 14));
                p.drawRoundedRect(left + 39, carY + carHeight - 12, 14, 6, 1, 1);
                p.drawRoundedRect(right - 53, carY + carHeight - 12, 14, 6, 1, 1);
            } else {
                // APEX GT-R: Quad Titanium Exhausts
                drawExhaust(left + 38, carY + carHeight - 14, 10, 8);
                drawExhaust(left + 50, carY + carHeight - 14, 10, 8);
                drawExhaust(right - 60, carY + carHeight - 14, 10, 8);
                drawExhaust(right - 48, carY + carHeight - 14, 10, 8);
            }
        }

        // 6. License Plate with Border, Tag & Dual LED Lamps
        p.setBrush(QColor(14, 16, 20));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(cx - 30, carY + carHeight - 32, 60, 20, 3, 3);
        p.setBrush(QColor(248, 248, 245));
        p.setPen(QPen(QColor(30, 32, 38), 1));
        p.drawRoundedRect(cx - 26, carY + carHeight - 30, 52, 16, 2, 2);
        // EU blue band or state bar
        p.setBrush(QColor(20, 70, 190));
        p.drawRect(cx - 25, carY + carHeight - 29, 6, 14);
        // Plate text
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.setPen(QColor(20, 22, 28));
        const char* pNames[5] = { "V12", "GT-R", "SPEC-R", "TITAN", "SEMI" };
        p.drawText(QRect(cx - 18, carY + carHeight - 30, 42, 16), Qt::AlignCenter, pNames[type]);
        // Plate LED lamps
        p.setBrush(QColor(255, 255, 240, 160));
        p.setPen(Qt::NoPen);
        p.drawEllipse(cx - 16, carY + carHeight - 32, 6, 3);
        p.drawEllipse(cx + 10, carY + carHeight - 32, 6, 3);

        // 7. Commercial Semi Trailer Rear Doors & Markings
        if (type == VEHICLE_TRUCK_SEMI) {
            // Aluminum corrugated panels
            p.setPen(QColor(190, 195, 205));
            for (int y = carY + 16; y < carY + carHeight - 24; y += 10) {
                p.drawLine(left + 12, y, right - 12, y);
            }
            // Vertical lock rods
            p.setPen(QPen(QColor(120, 125, 135), 4));
            p.drawLine(cx - 40, carY + 8, cx - 40, carY + carHeight - 16);
            p.drawLine(cx + 40, carY + 8, cx + 40, carY + carHeight - 16);
            // Cam handles
            p.setBrush(QColor(80, 85, 95));
            p.setPen(Qt::NoPen);
            p.drawRect(cx - 43, carY + carHeight * 0.55, 10, 18);
            p.drawRect(cx + 33, carY + carHeight * 0.55, 10, 18);
            // DOT-C2 red/white reflective tape along lower bumper
            for (int tx = left + 14; tx < right - 24; tx += 20) {
                p.fillRect(tx, carY + carHeight - 22, 10, 6, QColor(220, 30, 30));
                p.fillRect(tx + 10, carY + carHeight - 22, 10, 6, QColor(245, 245, 245));
            }
            // Rubber mud flaps with reflectors
            p.fillRect(left + 6, carY + carHeight - 14, 45, 20, QColor(20, 22, 26));
            p.fillRect(right - 51, carY + carHeight - 14, 45, 20, QColor(20, 22, 26));
            p.fillRect(left + 22, carY + carHeight - 6, 12, 6, QColor(220, 30, 30));
            p.fillRect(right - 34, carY + carHeight - 6, 12, 6, QColor(220, 30, 30));
        }

        // 8. Multi-Stage LED Taillights (Customized per Car Model)
        int tailW = 46;
        int tailH = 20;
        int tailY = carY + carHeight * 0.44;

        if (type == VEHICLE_COMPACT_BLUE) {
            // DRIFT SPEC-R: Skyline GT-R Dual Twin-Circle Round LED Taillights
            auto drawRoundLEDs = [&](int centerX) {
                for (int c = -1; c <= 1; c += 2) {
                    QPoint rCenter(centerX + c * 14, tailY + tailH / 2);
                    p.setBrush(brakeLights ? QColor(255, 30, 30) : QColor(195, 18, 24));
                    p.setPen(QPen(QColor(255, 180, 180), 1.2));
                    p.drawEllipse(rCenter, 9, 9);
                    p.setBrush(brakeLights ? QColor(255, 230, 200) : QColor(20, 10, 12));
                    p.setPen(Qt::NoPen);
                    p.drawEllipse(rCenter, brakeLights ? 6 : 4, brakeLights ? 6 : 4);
                }
            };
            drawRoundLEDs(left + 30);
            drawRoundLEDs(right - 30);

        } else if (type == VEHICLE_SPORTS_RED) {
            // VORTEX V12: Razor-Thin Angular Chevron LED Blades
            p.setPen(QPen(brakeLights ? QColor(255, 45, 45) : QColor(215, 25, 35), brakeLights ? 3.5 : 2.2));
            QPolygon leftChev;
            leftChev << QPoint(left + 46, tailY) << QPoint(left + 16, tailY + tailH / 2) << QPoint(left + 46, tailY + tailH);
            p.drawPolyline(leftChev);
            QPolygon rightChev;
            rightChev << QPoint(right - 46, tailY) << QPoint(right - 16, tailY + tailH / 2) << QPoint(right - 46, tailY + tailH);
            p.drawPolyline(rightChev);

        } else if (type == VEHICLE_SUV_BLACK) {
            // TITAN ENFORCER: Solid Blocky Armored Rectangular LED Tail Light Clusters
            auto drawArmoredLED = [&](int lx) {
                QRect br(lx, tailY, 44, 20);
                p.setBrush(brakeLights ? QColor(255, 25, 25) : QColor(160, 14, 20));
                p.setPen(QPen(QColor(85, 95, 105), 1.8));
                p.drawRoundedRect(br, 3, 3);
                p.setPen(QPen(QColor(30, 34, 40), 2.0));
                p.drawLine(lx + 15, tailY, lx + 15, tailY + 20);
                p.drawLine(lx + 29, tailY, lx + 29, tailY + 20);
            };
            drawArmoredLED(left + 14);
            drawArmoredLED(right - 58);

        } else {
            // APEX GT-R: Continuous LED light bar + Dual Halo Clusters
            auto drawLEDCluster = [&](int lx, int ly, bool isLeft) {
                p.setBrush(QColor(25, 15, 18));
                p.setPen(QPen(QColor(60, 25, 30), 1));
                p.drawRoundedRect(lx, ly, tailW, tailH, 4, 4);

                QColor tubeCol = brakeLights ? QColor(255, 40, 40) : QColor(190, 25, 25);
                p.setBrush(tubeCol);
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(lx + 4, ly + 4, tailW - 8, tailH - 8, 2, 2);

                if (brakeLights) {
                    p.setBrush(QColor(255, 230, 230));
                    p.drawRoundedRect(lx + 10, ly + 6, tailW - 20, tailH - 12, 2, 2);
                }

                if ((isLeft && turnLeft) || (!isLeft && turnRight)) {
                    int amberX = isLeft ? lx + 3 : lx + tailW - 17;
                    p.setBrush(QColor(255, 175, 10));
                    p.drawRoundedRect(amberX, ly + 3, 14, tailH - 6, 2, 2);
                }
            };

            drawLEDCluster(left + 14, tailY, true);
            drawLEDCluster(right - 14 - tailW, tailY, false);

            // Full-width connecting light bar
            QColor barCol = brakeLights ? QColor(255, 40, 40) : QColor(190, 25, 25);
            p.setBrush(barCol);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(left + 14 + tailW, tailY + tailH / 2 - 2, carWidth - 28 - tailW * 2, 4, 1, 1);
        }

    } else {
        // 9. FRONT VIEW (Seen in Rearview Mirror or Head-On)
        // Slanted Front Windshield
        QPainterPath frontGlass;
        frontGlass.moveTo(left + 42, carY + 14);
        frontGlass.lineTo(right - 42, carY + 14);
        frontGlass.lineTo(right - 26, carY + carHeight * 0.42);
        frontGlass.lineTo(left + 26, carY + carHeight * 0.42);
        frontGlass.closeSubpath();
        p.setBrush(QColor(22, 32, 46));
        p.setPen(QPen(QColor(45, 52, 65), 1));
        p.drawPath(frontGlass);

        // Radiator Grille
        p.setBrush(QColor(16, 18, 22));
        p.setPen(QPen(QColor(65, 70, 80), 2));
        QRect grilleRect(cx - 40, carY + carHeight * 0.50, 80, 26);
        p.drawRoundedRect(grilleRect, 5, 5);

        // Projector Headlights with LED Halo DRL Rings
        int hlW = 44;
        int hlH = 18;
        int hlY = carY + carHeight * 0.46;

        auto drawHeadlight = [&](int hx, int hy) {
            p.setBrush(QColor(240, 245, 255));
            p.setPen(QPen(QColor(180, 200, 220), 2));
            p.drawRoundedRect(hx, hy, hlW, hlH, 4, 4);

            // Projector lens
            p.setBrush(QColor(255, 255, 245));
            p.drawEllipse(hx + 10, hy + 2, 14, 14);

            // Headlight illumination beam halo
            QRadialGradient hlHalo(hx + hlW / 2, hy + hlH / 2, hlW * 0.85);
            hlHalo.setColorAt(0.0, QColor(255, 255, 220, 140));
            hlHalo.setColorAt(0.6, QColor(255, 255, 200, 50));
            hlHalo.setColorAt(1.0, QColor(255, 255, 200, 0));
            p.setBrush(hlHalo);
            p.setPen(Qt::NoPen);
            p.drawEllipse(hx - 12, hy - 10, hlW + 24, hlH + 20);
        };

        drawHeadlight(left + 16, hlY);
        drawHeadlight(right - 16 - hlW, hlY);

        // Front Lower Splitter
        p.setBrush(QColor(20, 22, 26));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(left + 20, carY + carHeight - 12, carWidth - 40, 8, 2, 2);
    }

    p.end();
    return car;
}

QImage RasterAssets::createSideVehicleSprite(VehicleType type, bool facingLeft) {
    const int W = 380;
    const int H = 160;
    QImage sideCar(W, H, QImage::Format_ARGB32_Premultiplied);
    sideCar.fill(Qt::transparent);

    QPainter p(&sideCar);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor bodyColor;
    switch (type) {
        case VEHICLE_SPORTS_RED: bodyColor = QColor(220, 28, 34); break;    // VORTEX V12 (Rosso Corsa)
        case VEHICLE_SEDAN_SILVER: bodyColor = QColor(118, 128, 142); break; // APEX GT-R (Gunmetal Titanium)
        case VEHICLE_SUV_BLACK: bodyColor = QColor(28, 48, 38); break;       // TITAN ENFORCER (Tactical Emerald)
        case VEHICLE_COMPACT_BLUE: bodyColor = QColor(235, 172, 38); break;  // DRIFT SPEC-R (Solar Gold)
        case VEHICLE_TRUCK_SEMI: bodyColor = QColor(226, 229, 235); break;
    }

    // 1. Soft Asphalt Drop Shadow
    QRadialGradient shadowGrad(W / 2, H - 18, W * 0.48);
    shadowGrad.setColorAt(0.0, QColor(8, 10, 14, 210));
    shadowGrad.setColorAt(0.7, QColor(8, 10, 14, 100));
    shadowGrad.setColorAt(1.0, QColor(8, 10, 14, 0));
    p.setBrush(shadowGrad);
    p.setPen(Qt::NoPen);
    p.drawEllipse(20, H - 28, W - 40, 26);

    // 2. High-Performance Alloy Wheels with Ventilated Rotors & Calipers
    int wheelY = H - 38;
    int wheelR = 29;

    auto drawRealisticWheel = [&](int wx, int wy) {
        // Outer Rubber Tire with Tread Profile
        p.setBrush(QColor(18, 20, 24));
        p.setPen(QPen(QColor(8, 9, 12), 2));
        p.drawEllipse(QPoint(wx, wy), wheelR, wheelR);

        // Tire sidewall rim
        p.setPen(QPen(QColor(36, 40, 48), 1));
        p.drawEllipse(QPoint(wx, wy), wheelR - 3, wheelR - 3);

        // Cross-Drilled Ventilated Silver Brake Rotor Disc
        p.setBrush(QColor(175, 180, 190));
        p.setPen(QPen(QColor(120, 125, 135), 1));
        p.drawEllipse(QPoint(wx, wy), 18, 18);

        // Rotor drill holes
        p.setPen(QColor(70, 75, 85));
        for (int a = 0; a < 360; a += 45) {
            qreal rad = qDegreesToRadians((qreal)a);
            p.drawPoint(QPointF(wx + 11 * qCos(rad), wy + 11 * qSin(rad)));
        }

        // Painted Racing Brake Caliper (Brembo Red)
        p.setBrush(QColor(220, 35, 35));
        p.setPen(QPen(QColor(120, 20, 20), 1));
        p.drawRoundedRect(wx + (facingLeft ? 4 : -12), wy - 14, 8, 14, 2, 2);

        // Machined 5-Twin-Spoke Alloy Rim
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(210, 215, 225), 3));
        p.drawEllipse(QPoint(wx, wy), 19, 19);

        p.setPen(QPen(QColor(225, 230, 240), 2));
        for (int a = 0; a < 360; a += 72) {
            qreal r1 = qDegreesToRadians((qreal)(a - 6));
            qreal r2 = qDegreesToRadians((qreal)(a + 6));
            p.drawLine(QPointF(wx + 6 * qCos(r1), wy + 6 * qSin(r1)), QPointF(wx + 18 * qCos(r1), wy + 18 * qSin(r1)));
            p.drawLine(QPointF(wx + 6 * qCos(r2), wy + 6 * qSin(r2)), QPointF(wx + 18 * qCos(r2), wy + 18 * qSin(r2)));
        }

        // Center Wheel Hub Cap
        p.setBrush(QColor(30, 34, 40));
        p.setPen(QPen(QColor(180, 185, 195), 1));
        p.drawEllipse(QPoint(wx, wy), 6, 6);
    };

    if (type == VEHICLE_TRUCK_SEMI) {
        // Semi-Truck Side Profile
        int cabW = 125;
        int cabX = facingLeft ? 25 : W - 25 - cabW;
        int trailerX = facingLeft ? cabX + cabW + 5 : 20;
        int trailerW = W - cabW - 55;

        // Cab
        QLinearGradient cabGrad(cabX, 25, cabX, H - 40);
        cabGrad.setColorAt(0.0, bodyColor.lighter(130));
        cabGrad.setColorAt(0.5, bodyColor);
        cabGrad.setColorAt(1.0, bodyColor.darker(140));
        p.setBrush(cabGrad);
        p.setPen(QPen(bodyColor.darker(180), 2));
        p.drawRoundedRect(cabX, 25, cabW, H - 65, 8, 8);

        // Cab Windshield & Side Glass
        p.setBrush(QColor(22, 32, 45));
        p.drawRoundedRect(facingLeft ? cabX + 10 : cabX + cabW - 55, 34, 45, 35, 4, 4);

        // Chrome Vertical Exhaust Stack
        int stackX = facingLeft ? cabX + cabW - 14 : cabX + 6;
        p.setBrush(QColor(210, 215, 225));
        p.setPen(QPen(QColor(120, 125, 135), 1));
        p.drawRoundedRect(stackX, 10, 8, 70, 2, 2);

        // Polished Fuel Tank
        p.setBrush(QColor(180, 185, 195));
        p.drawRoundedRect(cabX + (facingLeft ? 45 : 15), H - 55, 65, 22, 6, 6);

        // Trailer (Ribbed Aluminum Box)
        QLinearGradient trGrad(trailerX, 15, trailerX, H - 40);
        trGrad.setColorAt(0.0, QColor(235, 238, 244));
        trGrad.setColorAt(0.5, QColor(215, 220, 228));
        trGrad.setColorAt(1.0, QColor(190, 195, 204));
        p.setBrush(trGrad);
        p.setPen(QPen(QColor(140, 145, 155), 2));
        p.drawRect(trailerX, 15, trailerW, H - 50);

        // Trailer ribs
        p.setPen(QColor(170, 175, 185));
        for (int rx = trailerX + 15; rx < trailerX + trailerW - 10; rx += 18) {
            p.drawLine(rx, 16, rx, H - 36);
        }

        // Wheels
        drawRealisticWheel(facingLeft ? cabX + 40 : cabX + cabW - 40, wheelY);
        drawRealisticWheel(facingLeft ? trailerX + trailerW - 75 : trailerX + 35, wheelY);
        drawRealisticWheel(facingLeft ? trailerX + trailerW - 25 : trailerX + 85, wheelY);

    } else {
        // 3. Realistic Sports Car / Sedan / SUV Body Contours
        int noseX = facingLeft ? 20 : W - 20;
        int tailX = facingLeft ? W - 20 : 20;
        int hoodX = facingLeft ? 100 : W - 100;
        int roofStartX = facingLeft ? 150 : W - 150;
        int roofEndX = facingLeft ? 275 : W - 275;
        int trunkX = facingLeft ? 325 : W - 325;

        int roofY = (type == VEHICLE_SUV_BLACK) ? H - 128 : ((type == VEHICLE_SPORTS_RED) ? H - 108 : H - 116);
        int hoodY = H - 74;
        int beltY = H - 68;

        QPainterPath body;
        body.moveTo(noseX, H - 42);
        body.lineTo(noseX, beltY);
        body.cubicTo(noseX + (facingLeft ? 25 : -25), hoodY + 4, hoodX - (facingLeft ? 15 : -15), hoodY, hoodX, hoodY);
        body.lineTo(roofStartX, roofY);
        body.lineTo(roofEndX, roofY);
        body.cubicTo(roofEndX + (facingLeft ? 25 : -25), roofY + 6, trunkX - (facingLeft ? 15 : -15), beltY, trunkX, beltY);
        body.lineTo(tailX, beltY + 4);
        body.lineTo(tailX, H - 42);
        body.closeSubpath();

        // Shading with sharp metallic horizon reflection crease
        QLinearGradient bGrad(0, roofY, 0, H);
        bGrad.setColorAt(0.0, bodyColor.lighter(135));
        bGrad.setColorAt(0.35, bodyColor.lighter(110));
        bGrad.setColorAt(0.48, bodyColor.lighter(140)); // Sharp horizon glare streak
        bGrad.setColorAt(0.52, bodyColor);
        bGrad.setColorAt(0.85, bodyColor.darker(135));
        bGrad.setColorAt(1.0, bodyColor.darker(170));
        p.setBrush(bGrad);
        p.setPen(QPen(bodyColor.darker(190), 2));
        p.drawPath(body);

        // Sports Car Rear Wing in profile
        if (type == VEHICLE_SPORTS_RED) {
            p.setBrush(QColor(18, 20, 24));
            p.setPen(Qt::NoPen);
            int wingX = facingLeft ? W - 45 : 30;
            p.drawRect(wingX, H - 85, 4, 16);
            p.drawRoundedRect(wingX - 8, H - 90, 24, 6, 2, 2);
        }

        // Side Windows with Subtle Driver Silhouette
        QPolygon sideWindows;
        sideWindows << QPoint(roofStartX + (facingLeft ? 10 : -10), roofY + 6)
                    << QPoint(roofEndX + (facingLeft ? -10 : 10), roofY + 6)
                    << QPoint(trunkX + (facingLeft ? -18 : 18), beltY - 6)
                    << QPoint(hoodX + (facingLeft ? 22 : -22), beltY - 6);
        p.setBrush(QColor(18, 26, 36));
        p.setPen(QPen(QColor(42, 48, 60), 1));
        p.drawPolygon(sideWindows);

        // Driver headrest silhouette inside
        p.setBrush(QColor(10, 12, 16, 220));
        p.setPen(Qt::NoPen);
        int headX = facingLeft ? roofStartX + 35 : roofStartX - 50;
        p.drawRoundedRect(headX, roofY + 12, 18, 22, 5, 5);

        // B-Pillar divider
        int bPillarX = (roofStartX + roofEndX) / 2;
        p.setPen(QPen(QColor(22, 24, 28), 5));
        p.drawLine(bPillarX, roofY + 6, bPillarX, beltY - 6);

        // Aerodynamic Side Skirts
        p.setBrush(QColor(22, 24, 28));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(facingLeft ? 90 : 110, H - 44, 180, 6, 2, 2);

        // Recessed Door Handles
        p.setBrush(QColor(14, 16, 20));
        p.drawRoundedRect(facingLeft ? 160 : W - 185, beltY + 4, 24, 6, 2, 2);
        p.drawRoundedRect(facingLeft ? 230 : W - 255, beltY + 4, 24, 6, 2, 2);

        // Aerodynamic Side Mirror with Integrated Amber LED Indicator
        int mirX = facingLeft ? hoodX + 22 : hoodX - 34;
        p.setBrush(bodyColor.darker(120));
        p.setPen(QPen(bodyColor.darker(180), 1));
        p.drawRoundedRect(mirX, beltY - 14, 16, 10, 3, 3);
        p.setBrush(QColor(255, 175, 10)); // Amber LED repeater
        p.setPen(Qt::NoPen);
        p.drawRect(mirX + 2, beltY - 10, 12, 3);

        // Wrap-Around Front Headlight & Rear Taillight Lenses
        p.setBrush(QColor(255, 255, 230));
        p.drawRoundedRect(facingLeft ? noseX : noseX - 12, beltY - 4, 12, 8, 2, 2);
        p.setBrush(QColor(225, 30, 30));
        p.drawRoundedRect(facingLeft ? tailX - 12 : tailX, beltY - 4, 12, 8, 2, 2);

        // Wheels
        int frontWheelX = facingLeft ? 78 : W - 78;
        int rearWheelX = facingLeft ? W - 88 : 88;
        drawRealisticWheel(frontWheelX, wheelY);
        drawRealisticWheel(rearWheelX, wheelY);
    }

    p.end();
    return sideCar;
}

void RasterAssets::generateVehicles() {
    VehicleType types[] = {
        VEHICLE_SPORTS_RED,
        VEHICLE_SEDAN_SILVER,
        VEHICLE_SUV_BLACK,
        VEHICLE_COMPACT_BLUE,
        VEHICLE_TRUCK_SEMI
    };

    for (VehicleType t : types) {
        // Rear views with all lighting combinations
        int k1 = (int)t * 1000 + 100 + 0;           // Normal running lights
        m_vehicleSprites[k1] = createVehicleSprite(t, true, false, false, false);
        int k2 = (int)t * 1000 + 100 + 10;          // Braking
        m_vehicleSprites[k2] = createVehicleSprite(t, true, true, false, false);
        int k3 = (int)t * 1000 + 100 + 0 + 1;       // Turn left
        m_vehicleSprites[k3] = createVehicleSprite(t, true, false, true, false);
        int k4 = (int)t * 1000 + 100 + 0 + 2;       // Turn right
        m_vehicleSprites[k4] = createVehicleSprite(t, true, false, false, true);

        // Brake + Turn combinations
        int k5 = (int)t * 1000 + 100 + 10 + 1;      // Braking + Turn left
        m_vehicleSprites[k5] = createVehicleSprite(t, true, true, true, false);
        int k6 = (int)t * 1000 + 100 + 10 + 2;      // Braking + Turn right
        m_vehicleSprites[k6] = createVehicleSprite(t, true, true, false, true);

        // Front view (seen in rearview mirror or oncoming)
        int kFront = (int)t * 1000 + 0;
        m_vehicleSprites[kFront] = createVehicleSprite(t, false, false, false, false);
    }
}

void RasterAssets::generateSideVehicles() {
    VehicleType types[] = {
        VEHICLE_SPORTS_RED,
        VEHICLE_SEDAN_SILVER,
        VEHICLE_SUV_BLACK,
        VEHICLE_COMPACT_BLUE,
        VEHICLE_TRUCK_SEMI
    };

    for (VehicleType t : types) {
        m_vehicleSideSprites[(int)t * 10 + 1] = createSideVehicleSprite(t, true);  // Facing left
        m_vehicleSideSprites[(int)t * 10 + 0] = createSideVehicleSprite(t, false); // Facing right
    }
}

void RasterAssets::generateTextures() {
    const int S = 256;
    m_asphaltTex = QImage(S, S, QImage::Format_ARGB32_Premultiplied);
    m_asphaltTex.fill(QColor(42, 44, 48));
    for (int y = 0; y < S; ++y) {
        QRgb* line = (QRgb*)m_asphaltTex.scanLine(y);
        for (int x = 0; x < S; ++x) {
            int n = (QRandomGenerator::global()->generate() % 24) - 12;
            int r = qBound(0, 42 + n, 255);
            int g = qBound(0, 44 + n, 255);
            int b = qBound(0, 48 + n, 255);
            line[x] = qRgba(r, g, b, 255);
        }
    }

    auto makeTerrainTex = [](const QColor& baseCol, int noiseAmp) {
        QImage t(S, S, QImage::Format_ARGB32_Premultiplied);
        t.fill(baseCol);
        for (int y = 0; y < S; ++y) {
            QRgb* line = (QRgb*)t.scanLine(y);
            for (int x = 0; x < S; ++x) {
                int n = (QRandomGenerator::global()->generate() % (noiseAmp * 2 + 1)) - noiseAmp;
                int r = qBound(0, baseCol.red() + n, 255);
                int g = qBound(0, baseCol.green() + n, 255);
                int b = qBound(0, baseCol.blue() + n, 255);
                line[x] = qRgba(r, g, b, 255);
            }
        }
        return t;
    };

    m_grassSunsetTex = makeTerrainTex(QColor(65, 88, 48), 16);
    m_grassAlpineTex = makeTerrainTex(QColor(52, 75, 45), 20);
    m_grassNeonTex = makeTerrainTex(QColor(22, 24, 35), 10);
    m_grassDesertTex = makeTerrainTex(QColor(165, 115, 68), 22);
}

void RasterAssets::generateCrackTexture() {
    const int W = 600;
    const int H = 400;
    m_windshieldCrack = QImage(W, H, QImage::Format_ARGB32_Premultiplied);
    m_windshieldCrack.fill(Qt::transparent);
    QPainter p(&m_windshieldCrack);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPointF impact(W / 2.0, H / 2.0);
    p.setPen(QPen(QColor(255, 255, 255, 220), 2));

    p.drawEllipse(impact, 14, 14);
    p.drawEllipse(impact, 26, 26);

    for (int a = 0; a < 360; a += 30) {
        qreal rad = qDegreesToRadians((qreal)a);
        QPointF cur = impact;
        for (int step = 0; step < 5; ++step) {
            rad += qDegreesToRadians((qreal)((QRandomGenerator::global()->generate() % 20) - 10));
            qreal len = 25.0 + (QRandomGenerator::global()->generate() % 35);
            QPointF next = cur + QPointF(len * qCos(rad), len * qSin(rad));
            p.drawLine(cur, next);
            cur = next;
        }
    }
    p.end();
}
