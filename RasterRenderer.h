#ifndef RASTERRENDERER_H
#define RASTERRENDERER_H

#include "GameEngine.h"
#include "RasterAssets.h"
#include <QPainter>
#include <QImage>
#include <QVector>

class RasterRenderer {
public:
    RasterRenderer();

    // Main render call
    void render(QPainter& painter, const GameEngine& engine, int screenWidth, int screenHeight);

private:
    void renderBackdrop(QPainter& p, const GameEngine& engine, int w, int h);
    void renderApproachingMountains(QPainter& p, const GameEngine& engine, int w, int h, qreal horizonY);
    void renderRoad(QPainter& p, const GameEngine& engine, int w, int h, bool isMirror, qreal mirrorOffsetAngle = 0);
    void renderSceneryAndCars(QPainter& p, const GameEngine& engine, int w, int h, bool isMirror, qreal mirrorOffsetAngle = 0);
    void renderNightHeadlights(QPainter& p, const GameEngine& engine, int w, int h);
    void renderRearviewMirror(QPainter& p, const GameEngine& engine, int w, int h);
    void renderSideMirrors(QPainter& p, const GameEngine& engine, int w, int h);
    void renderSideWindowsAndAdjacentCars(QPainter& p, const GameEngine& engine, int w, int h);
    void renderSidewaysPOV(QPainter& p, const GameEngine& engine, int w, int h, bool lookLeft);
    void renderCockpit(QPainter& p, const GameEngine& engine, int w, int h);
    void renderRearCockpitView(QPainter& p, const GameEngine& engine, int w, int h);
    void renderDashboardInstruments(QPainter& p, const GameEngine& engine, int dashX, int dashY, int w, int h);
    void renderInfotainmentScreen(QPainter& p, const GameEngine& engine, const QRect& screenRect);
    void renderSteeringWheel(QPainter& p, const GameEngine& engine, int w, int h);
    void renderDynamicGlassReflections(QPainter& p, const GameEngine& engine, int w, int h);
    void renderAnimatedWipers(QPainter& p, const GameEngine& engine, int w, int h, int dashX, int dashY);
    void renderWindshieldHolographicHUD(QPainter& p, const GameEngine& engine, int w, int h, int dashX, int dashY);
    void renderWindshieldEffects(QPainter& p, const GameEngine& engine, int w, int h);
    void renderHUD(QPainter& p, const GameEngine& engine, int w, int h);

    qreal m_cameraHeading = 0;
    qreal m_rainAnimTime = 0;
};

#endif // RASTERRENDERER_H
