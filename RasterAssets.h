#ifndef RASTERASSETS_H
#define RASTERASSETS_H

#include <QImage>
#include <QPainter>
#include <QVector>
#include <QMap>
#include <QString>

enum TrackTheme {
    THEME_SUNSET_COAST = 0,
    THEME_ALPINE_CLIMB = 1,
    THEME_NEON_METROPOLIS = 2,
    THEME_RED_ROCK = 3
};

enum TimeOfDay {
    TIME_DAY = 0,
    TIME_SUNSET = 1,
    TIME_NIGHT = 2
};

enum VehicleType {
    VEHICLE_SPORTS_RED = 0,
    VEHICLE_SEDAN_SILVER = 1,
    VEHICLE_SUV_BLACK = 2,
    VEHICLE_COMPACT_BLUE = 3,
    VEHICLE_TRUCK_SEMI = 4
};

enum SceneryType {
    SCENERY_PALM_TREE = 0,
    SCENERY_PINE_TREE = 1,
    SCENERY_STREETLIGHT = 2,
    SCENERY_CACTUS = 3,
    SCENERY_BOULDER = 4,
    SCENERY_BILLBOARD_1 = 5,
    SCENERY_BILLBOARD_2 = 6,
    SCENERY_GUARDRAIL = 7,
    SCENERY_HIGHWAY_GANTRY = 8,
    SCENERY_WIND_TURBINE = 9,
    SCENERY_ROCK_ARCH = 10,
    SCENERY_UTILITY_POLE = 11,
    SCENERY_HOARDING_NEON = 12
};

class RasterAssets {
public:
    static RasterAssets& instance();

    void initialize();

    // Cockpit & Dashboard raster images
    const QImage& getDashboard() const { return m_dashboard; }
    const QImage& getSteeringWheel() const { return m_steeringWheel; }
    const QImage& getRearviewMirrorFrame() const { return m_mirrorFrame; }
    const QImage& getSideMirrorLeft() const { return m_sideMirrorLeft; }
    const QImage& getSideMirrorRight() const { return m_sideMirrorRight; }
    const QImage& getSideGlassOverlay() const { return m_sideGlassOverlay; }
    const QImage& getLeftCockpitPOV() const { return m_leftCockpitPOV; }
    const QImage& getRightCockpitPOV() const { return m_rightCockpitPOV; }
    const QImage& getRearCockpitView() const { return m_rearCockpitView; }

    const QImage& getGaugeDialSpeed(bool night = false) const { return night ? m_gaugeDialSpeedNight : m_gaugeDialSpeed; }
    const QImage& getGaugeDialRpm(bool night = false) const { return night ? m_gaugeDialRpmNight : m_gaugeDialRpm; }
    const QImage& getNeedle() const { return m_needle; }
    const QImage& getWindshieldCrack() const { return m_windshieldCrack; }
    const QImage& getHeadlightBeam() const { return m_headlightBeam; }
    const QImage& getWiperBlade() const { return m_wiperBlade; }
    const QImage& getSunGlare() const { return m_sunGlare; }

    // Road & Environment raster textures
    const QImage& getBackdrop(TrackTheme theme, TimeOfDay tod = TIME_SUNSET) const;
    const QImage& getScenery(SceneryType type) const;
    const QImage& getVehicle(VehicleType type, bool rearView, bool brakeLights, bool turnLeft, bool turnRight) const;
    const QImage& getVehicleSide(VehicleType type, bool facingLeft) const;
    const QImage& getAsphaltTexture() const { return m_asphaltTex; }
    const QImage& getGrassTexture(TrackTheme theme) const;

private:
    RasterAssets();
    ~RasterAssets() = default;

    void generateCockpitAssets();
    void generateSteeringWheel();
    void generateSideMirrors();
    void generateSideGlass();
    void generateSidePOVs();
    void generateRearCockpit();
    void generateGauges();
    void generateBackdrops();
    void generateSceneryAssets();
    void generateVehicles();
    void generateSideVehicles();
    void generateTextures();
    void generateCrackTexture();
    void generateHeadlightBeam();
    void generateWiperAndGlare();

    // Pure raster procedural generation helpers
    QImage createBackdropSunset();
    QImage createBackdropDay();
    QImage createBackdropNight();
    QImage createBackdropAlpine(TimeOfDay tod);
    QImage createBackdropNeon();
    QImage createBackdropRedRock(TimeOfDay tod);

    QImage createVehicleSprite(VehicleType type, bool rearView, bool brakeLights, bool turnLeft, bool turnRight);
    QImage createSideVehicleSprite(VehicleType type, bool facingLeft);

    QImage m_dashboard;
    QImage m_steeringWheel;
    QImage m_mirrorFrame;
    QImage m_sideMirrorLeft;
    QImage m_sideMirrorRight;
    QImage m_sideGlassOverlay;
    QImage m_leftCockpitPOV;
    QImage m_rightCockpitPOV;
    QImage m_rearCockpitView;
    QImage m_gaugeDialSpeed;
    QImage m_gaugeDialSpeedNight;
    QImage m_gaugeDialRpm;
    QImage m_gaugeDialRpmNight;
    QImage m_needle;
    QImage m_windshieldCrack;
    QImage m_headlightBeam;
    QImage m_wiperBlade;
    QImage m_sunGlare;
    QImage m_asphaltTex;
    QImage m_grassSunsetTex;
    QImage m_grassAlpineTex;
    QImage m_grassNeonTex;
    QImage m_grassDesertTex;

    // Key: (int)theme * 10 + (int)tod
    QMap<int, QImage> m_backdrops;
    QMap<SceneryType, QImage> m_scenery;
    // Key: vehicleType*1000 + rearView*100 + brakeLights*10 + (turnLeft ? 1 : (turnRight ? 2 : 0))
    QMap<int, QImage> m_vehicleSprites;
    // Key: (int)vehicleType * 10 + (facingLeft ? 1 : 0)
    QMap<int, QImage> m_vehicleSideSprites;
};

#endif // RASTERASSETS_H
