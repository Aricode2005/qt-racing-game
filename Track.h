#ifndef TRACK_H
#define TRACK_H

#include "RasterAssets.h"
#include <QVector>
#include <QString>

struct ProjectedPoint {
    qreal worldX = 0;
    qreal worldY = 0;
    qreal worldZ = 0;

    qreal camX = 0;
    qreal camY = 0;
    qreal camZ = 0;

    qreal screenX = 0;
    qreal screenY = 0;
    qreal screenW = 0;
    qreal scale = 0;
};

struct RoadSegment {
    int index = 0;
    ProjectedPoint p1;
    ProjectedPoint p2;

    qreal curve = 0;      // Horizontal curvature
    qreal elevation = 0;  // Height Y in world space
    qreal clip = 0;       // Screen clipping plane for hill occlusions

    int sector = 1;
    QString sectorName;
    bool hasGantry = false;

    SceneryType sceneryLeft = SCENERY_PALM_TREE;
    bool hasSceneryLeft = false;
    qreal sceneryLeftX = -1.6;

    SceneryType sceneryRight = SCENERY_PALM_TREE;
    bool hasSceneryRight = false;
    qreal sceneryRightX = 1.6;

    bool isRumbleRed = false; // Checkerboard rumble strips (red/white)
};

struct TrackInfo {
    TrackTheme theme = THEME_SUNSET_COAST;
    QString name;
    QString description;
    int laneCount = 3;
    qreal roadWidth = 2200;
    int totalSegments = 1600;
    int segmentLength = 200;
    int hillRating = 2;       // 1 - 5 stars
    int trafficRating = 3;    // 1 - 5 stars
    int difficulty = 2;       // 1 - 5 stars
};

class Track {
public:
    Track();
    void buildTrack(TrackTheme theme);

    const TrackInfo& getInfo() const { return m_info; }
    const QVector<RoadSegment>& getSegments() const { return m_segments; }
    int segmentCount() const { return m_segments.size(); }
    int trackLength() const { return m_segments.size() * m_info.segmentLength; }

    const RoadSegment& getSegment(int index) const {
        int idx = (index % m_segments.size() + m_segments.size()) % m_segments.size();
        return m_segments[idx];
    }

    const RoadSegment& findSegment(qreal worldZ) const {
        int idx = (int)(worldZ / m_info.segmentLength) % m_segments.size();
        if (idx < 0) idx += m_segments.size();
        return m_segments[idx];
    }

private:
    void addStraight(int num);
    void addCurve(int num, qreal curve);
    void addHill(int num, qreal height);
    void addHillAndCurve(int num, qreal curve, qreal height);

    void buildSunsetCoast();
    void buildAlpineClimb();
    void buildNeonMetropolis();
    void buildRedRock();

    TrackInfo m_info;
    QVector<RoadSegment> m_segments;
    qreal m_currentY = 0;
};

#endif // TRACK_H
