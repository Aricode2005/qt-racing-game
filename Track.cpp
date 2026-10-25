#include "Track.h"
#include <QtMath>
#include <QRandomGenerator>

Track::Track() {
    buildTrack(THEME_SUNSET_COAST);
}

void Track::buildTrack(TrackTheme theme) {
    m_segments.clear();
    m_currentY = 0;
    m_info.theme = theme;
    m_info.segmentLength = 200;

    switch (theme) {
        case THEME_ALPINE_CLIMB:
            buildAlpineClimb();
            break;
        case THEME_NEON_METROPOLIS:
            buildNeonMetropolis();
            break;
        case THEME_RED_ROCK:
            buildRedRock();
            break;
        case THEME_SUNSET_COAST:
        default:
            buildSunsetCoast();
            break;
    }
}

void Track::addStraight(int num) {
    qreal baseY = m_currentY;
    for (int i = 0; i < num; ++i) {
        RoadSegment seg;
        seg.index = m_segments.size();
        seg.p1.worldZ = seg.index * m_info.segmentLength;
        seg.p1.worldY = m_currentY;
        if (seg.index > 20) {
            qreal p = (i + 1) / (qreal)qMax(1, num);
            m_currentY = baseY + 220.0 * qSin(p * 2.0 * M_PI) + 65.0 * qSin(seg.index * 0.18);
        }
        seg.p2.worldZ = (seg.index + 1) * m_info.segmentLength;
        seg.p2.worldY = m_currentY;
        seg.curve = 0;
        seg.roadBank = (seg.index > 20) ? (0.022 * qSin(seg.index * 0.15)) : 0.0;
        seg.elevation = m_currentY;
        seg.isRumbleRed = (seg.index / 3) % 2 == 0;
        m_segments.append(seg);
    }
    m_currentY = baseY;
}

void Track::addCurve(int num, qreal curve) {
    qreal baseY = m_currentY;
    for (int i = 0; i < num; ++i) {
        RoadSegment seg;
        seg.index = m_segments.size();
        seg.p1.worldZ = seg.index * m_info.segmentLength;
        seg.p1.worldY = m_currentY;
        qreal p = (i + 1) / (qreal)qMax(1, num);
        m_currentY = baseY + 260.0 * qSin(p * 2.0 * M_PI) + 70.0 * qSin(seg.index * 0.16);
        seg.p2.worldZ = (seg.index + 1) * m_info.segmentLength;
        seg.p2.worldY = m_currentY;
        // Ease in / ease out curvature
        qreal factor = qSin(M_PI * (i / (qreal)num));
        seg.curve = curve * factor;
        seg.roadBank = -seg.curve * 0.11 + 0.025 * qSin(seg.index * 0.16);
        seg.elevation = m_currentY;
        seg.isRumbleRed = (seg.index / 3) % 2 == 0;
        m_segments.append(seg);
    }
    m_currentY = baseY;
}

void Track::addHill(int num, qreal height) {
    qreal startY = m_currentY;
    qreal endY = startY + height;
    for (int i = 0; i < num; ++i) {
        RoadSegment seg;
        seg.index = m_segments.size();
        seg.p1.worldZ = seg.index * m_info.segmentLength;
        seg.p1.worldY = m_currentY;

        // Smooth sinusoidal curve for hill profile
        qreal progress = (i + 1) / (qreal)num;
        m_currentY = startY + (height / 2.0) * (1.0 - qCos(progress * M_PI));

        seg.p2.worldZ = (seg.index + 1) * m_info.segmentLength;
        seg.p2.worldY = m_currentY;
        seg.curve = 0;
        seg.elevation = m_currentY;
        seg.isRumbleRed = (seg.index / 3) % 2 == 0;
        m_segments.append(seg);
    }
}

void Track::addHillAndCurve(int num, qreal curve, qreal height) {
    qreal startY = m_currentY;
    for (int i = 0; i < num; ++i) {
        RoadSegment seg;
        seg.index = m_segments.size();
        seg.p1.worldZ = seg.index * m_info.segmentLength;
        seg.p1.worldY = m_currentY;

        qreal progress = (i + 1) / (qreal)num;
        m_currentY = startY + (height / 2.0) * (1.0 - qCos(progress * M_PI));

        seg.p2.worldZ = (seg.index + 1) * m_info.segmentLength;
        seg.p2.worldY = m_currentY;

        qreal factor = qSin(M_PI * (i / (qreal)num));
        seg.curve = curve * factor;
        seg.roadBank = -seg.curve * 0.11 + 0.025 * qSin(seg.index * 0.16);
        seg.elevation = m_currentY;
        seg.isRumbleRed = (seg.index / 3) % 2 == 0;
        m_segments.append(seg);
    }
}

void Track::addRollingHills(int num, int waves, qreal amplitude, qreal curve) {
    if (num <= 0) return;
    qreal startY = m_currentY;
    int wCount = qBound(1, waves, 2);
    qreal hillAmp = qBound(-1150.0, amplitude * 0.65, 1150.0);
    for (int i = 0; i < num; ++i) {
        RoadSegment seg;
        seg.index = m_segments.size();
        seg.p1.worldZ = seg.index * m_info.segmentLength;
        seg.p1.worldY = m_currentY;

        qreal progress = (i + 1) / (qreal)num;
        qreal waveY = hillAmp * qSin(progress * wCount * 2.0 * M_PI)
                    + (hillAmp * 0.16) * qSin(progress * (wCount * 2) * 2.0 * M_PI);
        m_currentY = startY + waveY;

        seg.p2.worldZ = (seg.index + 1) * m_info.segmentLength;
        seg.p2.worldY = m_currentY;

        qreal factor = qSin(M_PI * (i / (qreal)num));
        seg.curve = curve * factor;
        seg.roadBank = -seg.curve * 0.11 + 0.032 * qSin(progress * wCount * 4.0 * M_PI);
        seg.elevation = m_currentY;
        seg.isRumbleRed = (seg.index / 3) % 2 == 0;
        m_segments.append(seg);
    }
    m_currentY = startY;
}

void Track::buildSunsetCoast() {
    m_info.name = "Pacific Coast Highway";
    m_info.description = "Golden hour oceanfront cruise with rolling coastal hills, banked curves, and 4 distinct zones.";
    m_info.laneCount = 4;
    m_info.roadWidth = 2400;
    m_info.hillRating = 3;
    m_info.trafficRating = 3;
    m_info.difficulty = 2;

    addStraight(22);
    addRollingHills(68, 2, 1450, 1.8);
    addHillAndCurve(85, -2.6, 1450);
    addRollingHills(56, 1, 1100, 1.4);
    addCurve(70, -3.0);
    addHill(75, -1450);
    addRollingHills(68, 2, 1350, -1.6);
    addHillAndCurve(100, -3.0, 1600);
    addRollingHills(60, 1, 1150, 1.8);
    addCurve(85, 3.2);
    addHill(80, -1600);
    addRollingHills(72, 2, 1400, -1.4);
    addHillAndCurve(105, 2.6, 1500);
    addCurve(85, -3.2);
    addHill(80, -1500);
    addRollingHills(70, 2, 1300, 2.0);
    addCurve(75, 2.2);
    addRollingHills(65, 1, 1150, -1.4);
    addHillAndCurve(88, -2.5, 1500);
    addCurve(60, 3.4);
    addHill(75, -1500);
    addRollingHills(64, 2, 1300, 1.8);
    addCurve(60, 2.0);
    addRollingHills(60, 1, 1100, -1.2);
    addHillAndCurve(95, -3.0, 1550);
    addRollingHills(64, 2, 1250, 1.8);
    addCurve(85, 3.2);
    addHill(85, -1550);
    addRollingHills(72, 2, 1350, -1.6);
    addHillAndCurve(100, 2.6, 1450);
    addCurve(85, -3.2);
    addHill(85, -1450);
    addRollingHills(68, 2, 1300, 1.8);
    addStraight(50);

    int total = m_segments.size();
    int s1 = total * 0.25;
    int s2 = total * 0.50;
    int s3 = total * 0.75;

    for (int i = 0; i < total; ++i) {
        RoadSegment& seg = m_segments[i];

        if (i < s1) {
            // SECTOR 1: Coastal Palm Boulevard & Beach Strip
            seg.sector = 1;
            seg.sectorName = "COASTAL PALM BOULEVARD";
            if (i % 8 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_GUARDRAIL;
                seg.sceneryLeftX = -1.25;

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_PALM_TREE;
                seg.sceneryRightX = 1.55 + ((i % 16 == 0) ? 0.3 : 0.7);
            }
            // Roadside utility poles along left shoulder
            if (i % 14 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.6;
            }
            // High billboards and wide highway hoardings
            if (i % 32 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = (i % 64 == 0) ? SCENERY_BILLBOARD_1 : SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.95;
            }
            if (i % 48 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_BILLBOARD_2;
                seg.sceneryLeftX = -2.05;
            }
            if (i == 80) seg.hasGantry = true;
        } else if (i < s2) {
            // SECTOR 2: Rocky Ocean Cliffs & Natural Sea Arches
            seg.sector = 2;
            seg.sectorName = "OCEAN CLIFFS & SEA ARCHES";
            if (i % 7 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = (i % 28 == 0) ? SCENERY_ROCK_ARCH : SCENERY_BOULDER;
                seg.sceneryLeftX = -1.6 - ((i % 14 == 0) ? 0.3 : 0.8);

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_GUARDRAIL;
                seg.sceneryRightX = 1.25;
            }
            if (i % 16 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_UTILITY_POLE;
                seg.sceneryRightX = 1.65;
            }
            if (i % 36 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_HOARDING_NEON;
                seg.sceneryLeftX = -2.1;
            }
            if (i % 18 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_BOULDER;
                seg.sceneryRightX = 1.7;
            }
            if (i == s1 + 80) seg.hasGantry = true;
        } else if (i < s3) {
            // SECTOR 3: Coastal Wind Farm Ridge
            seg.sector = 3;
            seg.sectorName = "COASTAL WIND RIDGE";
            if (i % 16 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_WIND_TURBINE;
                seg.sceneryLeftX = -2.1;
            }
            if (i % 14 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_WIND_TURBINE;
                seg.sceneryRightX = 2.1;
            }
            if (i % 12 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.65;
            }
            if (i % 38 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_BILLBOARD_1;
                seg.sceneryRightX = 1.95;
            }
            if (i % 8 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_PALM_TREE;
                seg.sceneryLeftX = -1.5;
            }
            if (i == s2 + 80) seg.hasGantry = true;
        } else {
            // SECTOR 4: Marina Bay Expressway
            seg.sector = 4;
            seg.sectorName = "MARINA BAY EXPRESSWAY";
            if (i % 10 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_STREETLIGHT;
                seg.sceneryLeftX = -1.35;

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_STREETLIGHT;
                seg.sceneryRightX = 1.35;
            }
            if (i % 14 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.7;
            }
            if (i % 30 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = (i % 60 == 0) ? SCENERY_BILLBOARD_2 : SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.95;
            }
            if (i == s3 + 80) seg.hasGantry = true;
        }
    }
}

void Track::buildAlpineClimb() {
    m_info.name = "Alpine Summit Ridge";
    m_info.description = "Hill Climb challenge with extreme gradients, pine valleys, rocky crags and alpine wind farms.";
    m_info.laneCount = 4;
    m_info.roadWidth = 2400;
    m_info.hillRating = 5;
    m_info.trafficRating = 2;
    m_info.difficulty = 5;

    addStraight(22);
    addRollingHills(60, 2, 2200, 3.2);
    addHillAndCurve(90, 4.5, 3200);
    addHillAndCurve(80, -4.8, 2800);
    addRollingHills(52, 2, 1800, -2.5);
    addHill(50, -2500);
    addCurve(70, 5.2);
    addHillAndCurve(90, -4.0, 3000);
    addHill(60, -3500);
    addRollingHills(64, 3, 2100, 3.0);
    addHillAndCurve(80, 5.0, 2400);
    addHill(70, -5400);
    addRollingHills(72, 2, 1950, -2.8);
    addHillAndCurve(110, 4.2, 2600);
    addCurve(95, -4.4);
    addHill(90, -2600);
    addRollingHills(80, 3, 2200, 2.4);
    addHillAndCurve(115, -3.8, 2200);
    addCurve(90, 4.0);
    addHill(85, -2200);
    addStraight(40);

    int total = m_segments.size();
    int s1 = total * 0.25;
    int s2 = total * 0.50;
    int s3 = total * 0.75;

    for (int i = 0; i < total; ++i) {
        RoadSegment& seg = m_segments[i];

        if (i < s1) {
            // SECTOR 1: Pine Valley Approach
            seg.sector = 1;
            seg.sectorName = "PINE VALLEY APPROACH";
            if (i % 6 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_PINE_TREE;
                seg.sceneryLeftX = -1.55 - ((i % 12 == 0) ? 0.3 : 0.1);

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_GUARDRAIL;
                seg.sceneryRightX = 1.25;
            }
            if (i % 12 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_PINE_TREE;
                seg.sceneryRightX = 1.7;
            }
            if (i % 14 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.65;
            }
            if (i % 30 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = (i % 60 == 0) ? SCENERY_BILLBOARD_1 : SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.95;
            }
            if (i == 70) seg.hasGantry = true;
        } else if (i < s2) {
            // SECTOR 2: Granite Gorge & Mountain Crags
            seg.sector = 2;
            seg.sectorName = "GRANITE GORGE & CRAGS";
            if (i % 6 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = (i % 24 == 0) ? SCENERY_ROCK_ARCH : SCENERY_BOULDER;
                seg.sceneryLeftX = -1.5 - ((i % 12 == 0) ? 0.4 : 0.1);

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_BOULDER;
                seg.sceneryRightX = 1.5;
            }
            if (i % 16 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_UTILITY_POLE;
                seg.sceneryRightX = 1.6;
            }
            if (i % 36 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_BILLBOARD_2;
                seg.sceneryLeftX = -2.1;
            }
            if (i == s1 + 70) seg.hasGantry = true;
        } else if (i < s3) {
            // SECTOR 3: High Pass Wind Ridge
            seg.sector = 3;
            seg.sectorName = "HIGH PASS WIND RIDGE";
            if (i % 15 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_WIND_TURBINE;
                seg.sceneryLeftX = -2.1;
            }
            if (i % 18 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_WIND_TURBINE;
                seg.sceneryRightX = 2.0;
            }
            if (i % 14 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.6;
            }
            if (i % 34 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.9;
            }
            if (i % 7 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_PINE_TREE;
                seg.sceneryLeftX = -1.5;
            }
            if (i == s2 + 70) seg.hasGantry = true;
        } else {
            // SECTOR 4: Summit Lookout & Descent
            seg.sector = 4;
            seg.sectorName = "SUMMIT LOOKOUT FREEWAY";
            if (i % 8 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_STREETLIGHT;
                seg.sceneryLeftX = -1.35;

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_GUARDRAIL;
                seg.sceneryRightX = 1.25;
            }
            if (i % 14 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.65;
            }
            if (i % 28 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = (i % 56 == 0) ? SCENERY_BILLBOARD_1 : SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.9;
            }
            if (i == s3 + 70) seg.hasGantry = true;
        }
    }
}

void Track::buildNeonMetropolis() {
    m_info.name = "Neo City Expressway";
    m_info.description = "4-lane wide midnight urban highway with wet asphalt, industrial harbor and skyline flyovers.";
    m_info.laneCount = 4;
    m_info.roadWidth = 2600;
    m_info.hillRating = 1;
    m_info.trafficRating = 5;
    m_info.difficulty = 4;

    addStraight(22);
    addRollingHills(64, 2, 1450, 2.0);
    addCurve(60, 2.0);
    addHill(50, 1600);
    addRollingHills(60, 2, 1300, -1.8);
    addCurve(80, -2.5);
    addHill(50, -1600);
    addRollingHills(72, 3, 1550, 2.4);
    addCurve(70, 3.0);
    addRollingHills(56, 2, 1350, -2.0);
    addCurve(60, -2.0);
    addRollingHills(76, 2, 1650, 2.2);
    addCurve(95, 2.8);
    addHill(70, 1800);
    addRollingHills(64, 2, 1400, -2.5);
    addCurve(105, -3.1);
    addHill(70, -1800);
    addRollingHills(80, 3, 1600, 2.5);
    addCurve(90, 2.5);
    addRollingHills(68, 2, 1350, -1.8);
    addStraight(50);

    int total = m_segments.size();
    int s1 = total * 0.25;
    int s2 = total * 0.50;
    int s3 = total * 0.75;

    for (int i = 0; i < total; ++i) {
        RoadSegment& seg = m_segments[i];

        if (i < s1) {
            // SECTOR 1: Downtown Cyber Core
            seg.sector = 1;
            seg.sectorName = "DOWNTOWN CYBER CORE";
            if (i % 8 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_STREETLIGHT;
                seg.sceneryLeftX = -1.35;

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_STREETLIGHT;
                seg.sceneryRightX = 1.35;
            }
            if (i % 28 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = (i % 56 == 0) ? SCENERY_BILLBOARD_2 : SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.85;
            }
            if (i % 42 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_HOARDING_NEON;
                seg.sceneryLeftX = -1.95;
            }
            if (i == 75) seg.hasGantry = true;
        } else if (i < s2) {
            // SECTOR 2: Industrial Harbor & Cargo Docks
            seg.sector = 2;
            seg.sectorName = "INDUSTRIAL HARBOR & DOCKS";
            if (i % 16 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_WIND_TURBINE;
                seg.sceneryLeftX = -2.2;
            }
            if (i % 12 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.65;
            }
            if (i % 10 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_STREETLIGHT;
                seg.sceneryLeftX = -1.35;

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_GUARDRAIL;
                seg.sceneryRightX = 1.25;
            }
            if (i % 32 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.95;
            }
            if (i == s1 + 75) seg.hasGantry = true;
        } else if (i < s3) {
            // SECTOR 3: Skyway Elevated Flyover
            seg.sector = 3;
            seg.sectorName = "SKYWAY ELEVATED FLYOVER";
            if (i % 8 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_GUARDRAIL;
                seg.sceneryLeftX = -1.25;

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_GUARDRAIL;
                seg.sceneryRightX = 1.25;
            }
            if (i % 16 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_STREETLIGHT;
                seg.sceneryRightX = 1.4;
            }
            if (i % 30 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = (i % 60 == 0) ? SCENERY_BILLBOARD_1 : SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.85;
            }
            if (i == s2 + 75) seg.hasGantry = true;
        } else {
            // SECTOR 4: Outer Ring Expressway
            seg.sector = 4;
            seg.sectorName = "OUTER RING EXPRESSWAY";
            if (i % 9 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_STREETLIGHT;
                seg.sceneryLeftX = -1.35;

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_STREETLIGHT;
                seg.sceneryRightX = 1.35;
            }
            if (i % 14 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.65;
            }
            if (i % 28 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = (i % 56 == 0) ? SCENERY_BILLBOARD_1 : SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.9;
            }
            if (i == s3 + 75) seg.hasGantry = true;
        }
    }
}

void Track::buildRedRock() {
    m_info.name = "Red Rock Canyon";
    m_info.description = "High-speed desert sprint with rollercoaster whoops, majestic sandstone arches, and desert wind farms.";
    m_info.laneCount = 4;
    m_info.roadWidth = 2400;
    m_info.hillRating = 3;
    m_info.trafficRating = 3;
    m_info.difficulty = 3;

    addStraight(22);
    addRollingHills(64, 3, 2100, -2.4);
    addHillAndCurve(110, -3.4, 2600);
    addRollingHills(60, 2, 1850, 2.8);
    addCurve(100, 3.8);
    addHill(90, -2600);
    addRollingHills(72, 3, 2200, -2.2);
    addHillAndCurve(120, 2.9, 2200);
    addCurve(95, -3.6);
    addHill(85, -2200);
    addRollingHills(80, 3, 2000, 2.4);
    addCurve(85, 2.4);
    addRollingHills(64, 2, 1750, -2.0);
    for (int k = 0; k < 4; ++k) {
        addHill(35, 1950);
        addHill(35, -1950);
    }
    addCurve(80, 3.5);
    addRollingHills(48, 2, 1800, -2.5);
    addHillAndCurve(90, -4.2, 2400);
    addHill(60, -2400);
    addRollingHills(64, 2, 1700, 2.8);
    addCurve(70, 2.8);
    addStraight(45);

    int total = m_segments.size();
    int s1 = total * 0.25;
    int s2 = total * 0.50;
    int s3 = total * 0.75;

    for (int i = 0; i < total; ++i) {
        RoadSegment& seg = m_segments[i];

        if (i < s1) {
            // SECTOR 1: Saguaro Desert Plains
            seg.sector = 1;
            seg.sectorName = "SAGUARO DESERT PLAINS";
            if (i % 7 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_CACTUS;
                seg.sceneryLeftX = -1.55 - ((i % 14 == 0) ? 0.3 : 0.6);
            }
            if (i % 9 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_CACTUS;
                seg.sceneryRightX = 1.6 + ((i % 18 == 0) ? 0.4 : 0.1);
            }
            // Desert telephone / utility poles along Route 66
            if (i % 14 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.6;
            }
            if (i % 32 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = (i % 64 == 0) ? SCENERY_BILLBOARD_1 : SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.95;
            }
            if (i == 80) seg.hasGantry = true;
        } else if (i < s2) {
            // SECTOR 2: Monument Valley Sandstone Arches
            seg.sector = 2;
            seg.sectorName = "MONUMENT VALLEY ARCHES";
            if (i % 7 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = (i % 28 == 0) ? SCENERY_ROCK_ARCH : SCENERY_BOULDER;
                seg.sceneryLeftX = -1.6 - ((i % 14 == 0) ? 0.3 : 0.8);
            }
            if (i % 8 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = (i % 24 == 0) ? SCENERY_ROCK_ARCH : SCENERY_BOULDER;
                seg.sceneryRightX = 1.6 + ((i % 16 == 0) ? 0.4 : 0.1);
            }
            if (i % 16 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_UTILITY_POLE;
                seg.sceneryRightX = 1.65;
            }
            if (i % 36 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_HOARDING_NEON;
                seg.sceneryLeftX = -2.1;
            }
            if (i == s1 + 80) seg.hasGantry = true;
        } else if (i < s3) {
            // SECTOR 3: Mojave Wind Basin
            seg.sector = 3;
            seg.sectorName = "MOJAVE WIND BASIN";
            if (i % 14 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_WIND_TURBINE;
                seg.sceneryLeftX = -2.1;
            }
            if (i % 16 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_WIND_TURBINE;
                seg.sceneryRightX = 2.1;
            }
            if (i % 14 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.6;
            }
            if (i % 34 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_BILLBOARD_1;
                seg.sceneryRightX = 1.95;
            }
            if (i % 8 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_CACTUS;
                seg.sceneryLeftX = -1.5;
            }
            if (i == s2 + 80) seg.hasGantry = true;
        } else {
            // SECTOR 4: Red Rock Canyon Gorge
            seg.sector = 4;
            seg.sectorName = "RED ROCK CANYON GORGE";
            if (i % 7 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_BOULDER;
                seg.sceneryLeftX = -1.5;

                seg.hasSceneryRight = true;
                seg.sceneryRight = SCENERY_GUARDRAIL;
                seg.sceneryRightX = 1.25;
            }
            if (i % 14 == 0) {
                seg.hasSceneryLeft = true;
                seg.sceneryLeft = SCENERY_UTILITY_POLE;
                seg.sceneryLeftX = -1.65;
            }
            if (i % 28 == 0) {
                seg.hasSceneryRight = true;
                seg.sceneryRight = (i % 56 == 0) ? SCENERY_BILLBOARD_2 : SCENERY_HOARDING_NEON;
                seg.sceneryRightX = 1.9;
            }
            if (i == s3 + 80) seg.hasGantry = true;
        }
    }
}
