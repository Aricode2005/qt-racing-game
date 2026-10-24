#ifndef GAMEENGINE_H
#define GAMEENGINE_H

#include "Track.h"
#include <QObject>
#include <QVector>
#include <QString>

struct CrashSpark {
    qreal worldZ = 0;
    qreal x = 0;
    qreal y = 0;      // World elevation height above road
    qreal vx = 0;
    qreal vy = 0;
    qreal vz = 0;
    qreal life = 1.0;
    qreal maxLife = 1.0;
    int type = 0;     // 0: golden spark, 1: glass fragment, 2: smoke puff
    qreal size = 4.0;
};

struct NPCCar {
    int id = 0;
    VehicleType type = VEHICLE_SEDAN_SILVER;
    qreal worldZ = 0;
    qreal x = 0;          // -1.0 to 1.0 (lanes)
    qreal targetX = 0;    // For lane changes
    qreal speed = 100;    // km/h
    qreal maxSpeed = 130;
    int assignedLane = 0; // 0: Far Left (Oncoming), 1: Mid Left, 2: Mid Right, 3: Far Right
    bool isOncoming = false;
    bool isSideLanePacer = false;
    bool isBraking = false;
    bool turnLeft = false;
    bool turnRight = false;
    bool blinkerFlash = false;
    qreal blinkerTimer = 0;
    qreal laneChangeTimer = 0;
    bool passedByPlayer = false;
    bool nearMissAwarded = false;

    // Realistic Post-Crash Reaction & Dynamics
    bool isCrashing = false;
    qreal crashRecoveryTimer = 0;
    qreal lateralVelocity = 0;    // Knockback sideways speed across lanes
    qreal skidAngle = 0;          // Fishtailing rotation angle in radians
    bool hazardLights = false;    // Emergency 4-way flashers
    qreal signalWarnTimer = 0;    // Pre-lane-change signal warning duration
    bool isOvertaking = false;
    QString racerName;
    int currentPosition = 1;
    bool hasFinished = false;
    qreal finishTime = 0.0;
    qreal accelRate = 48.0;
};

enum InfotainmentMode {
    INFO_MODE_WEB = 0,
    INFO_MODE_MUSIC = 1,
    INFO_MODE_RADAR = 2,
    INFO_MODE_CAR = 3
};

struct InfotainmentState {
    InfotainmentMode mode = INFO_MODE_WEB;

    // Music Player State
    int currentTrack = 0;
    bool isPlaying = true;
    qreal trackElapsed = 48.0;
    qreal trackDuration = 210.0;
    qreal volume = 0.85;
    qreal visualizer[16] = {0};

    // Web Browser State
    int currentWebPage = 0; // 0: Chatter Feed, 1: News/Leaderboard, 2: Doppler Radar, 3: Specs
    QString currentUrl = "https://speedr-highway.io/live";
    qreal webScrollY = 0;
    qreal webAnimTimer = 0;
};

struct PlayerCar {
    qreal worldZ = 0;
    qreal x = 0;               // Road position: -1.0 to 1.0 (lanes)
    qreal speed = 0;           // km/h (0 to 300)
    qreal maxSpeed = 295;      // km/h
    qreal accelRate = 45;      // Base acceleration
    qreal brakeRate = 95;      // Braking deceleration
    qreal steerAngle = 0;      // -1.0 (left) to 1.0 (right)
    qreal steerTarget = 0;

    int gear = 1;              // 1 to 6, 0 for Neutral, -1 for Reverse
    qreal rpm = 900;           // 800 to 8000
    qreal targetRpm = 900;
    qreal boostBar = 0.0;      // 0.0 to 1.0 (turbo boost)

    // Cockpit dynamic suspension & track momentum
    qreal pitchG = 0;          // Acceleration squat / braking dive / hill crest pitch
    qreal rollG = 0;           // Lateral sway into turns
    qreal roadBanking = 0;     // Dynamic track banking on curves
    qreal vibration = 0;       // Road and engine vibration
    qreal suspensionRebound = 0;

    // Sideways POV glance
    bool lookingLeft = false;
    bool lookingRight = false;
    qreal povPan = 0;          // -1.0 (left window glance), 0.0 (forward), +1.0 (right window glance)

    // Dynamic front windshield glass states
    qreal wiperAngle = 0;      // 0 to 75 deg
    bool wipersActive = true;
    qreal wiperDirection = 1.0;
    qreal glassReflectionOffset = 0;
    qreal sunGlareX = 0;

    bool isBraking = false;
    bool isAccelerating = false;
    bool isHandbraking = false;
    bool isOffroad = false;
    bool hornActive = false;
    bool lookingBack = false;
    bool thirdPersonView = false;

    // Realistic Car Health & Crash Counter
    int health = 100;              // 0 to 100%
    int crashCount = 0;           // Total collisions
    bool isWrecked = false;       // Health == 0 (engine totaled, must press R)
    qreal collisionCooldown = 0;  // Debounce cooldown between crash impacts
    qreal damageFlashTimer = 0;   // Red damage vignette on impact

    // Stats & Scoring (Traffic Rider inspired)
    int score = 0;
    int nearMissCombo = 1;
    qreal comboTimer = 0;
    qreal distanceTraveled = 0;   // km
    int currentSector = 1;
    int lastSectorAnnounced = 1;
    QString lastAnnouncement;
    qreal announcementTimer = 0;
    qreal crashTimer = 0;
    qreal impactCameraShake = 0;  // High-frequency crash camera jolt
    qreal lateralRecoil = 0;      // Lateral displacement recoil from collision
    qreal raceTime = 0.0;
    qreal finishTime = 0.0;
    bool raceFinished = false;
    int currentPosition = 8;
    int totalRacers = 8;
    qreal finishLineZ = 48000.0;

    // Merged Contributor Features: 4 Selectable Supercars, ADAS Assist, AR Trajectory Guides & 3D Z-Buffer Mode
    int selectedCar = 0;               // 0: APEX GT-R, 1: VORTEX V12, 2: DRIFT SPEC-R, 3: TITAN ENFORCER
    QString carModelName = "APEX GT-R";
    qreal armorMultiplier = 1.0;       // Damage resistance scaling per car model
    qreal gripMultiplier = 1.0;        // Cornering & steering response scaling per car model
    bool adasEnabled = true;           // Toggleable ADAS Apex & Lane-Keeping Assist ([G])
    bool arGuidesEnabled = true;       // Toggleable 3D AR Bumper Trajectory Rails & Passing Brackets ([F])
    bool zBuffer3DMode = true;         // Toggleable Full-Depth 3D Z-Buffer & Polygon Shading Pipeline ([E]/[L])
    bool slipstreamActive = false;     // Active aerodynamic drafting lock behind lead car
};

struct RaceStandingEntry {
    int position = 1;
    QString name;
    bool isPlayer = false;
    bool hasFinished = false;
    qreal finishTime = 0.0;
    qreal progressZ = 0.0;
    qreal speed = 0.0;
};

enum GameDifficulty {
    DIFFICULTY_EASY = 0,
    DIFFICULTY_NORMAL = 1,
    DIFFICULTY_PRO = 2
};

class GameEngine : public QObject {
    Q_OBJECT
public:
    explicit GameEngine(QObject *parent = nullptr);

    void setTrackTheme(TrackTheme theme);
    void setTimeOfDay(TimeOfDay tod) { m_timeOfDay = tod; }
    TimeOfDay getTimeOfDay() const { return m_timeOfDay; }
    void cycleTimeOfDay() {
        m_timeOfDay = static_cast<TimeOfDay>((m_timeOfDay + 1) % 3);
    }

    void setDifficulty(GameDifficulty diff);
    GameDifficulty getDifficulty() const { return m_difficulty; }

    void update(qreal dt); // 60 FPS update step

    // Inputs
    void setThrottle(bool active);
    void setBrake(bool active);
    void setSteerLeft(bool active);
    void setSteerRight(bool active);
    void setHandbrake(bool active);
    void setLookBack(bool active);
    void setLookLeft(bool active);
    void setLookRight(bool active);
    void setHorn(bool active);
    void toggleWipers() { m_player.wipersActive = !m_player.wipersActive; }
    void toggleCameraView() { m_player.thirdPersonView = !m_player.thirdPersonView; }
    void setThirdPersonView(bool active) { m_player.thirdPersonView = active; }
    bool isThirdPersonView() const { return m_player.thirdPersonView; }

    // Contributor Features: Car Selection, ADAS, AR Trajectory Guides, 3D Z-Buffer Mode & Field Repair
    void setSelectedCar(int carIdx);
    void cycleSelectedCar() { setSelectedCar((m_player.selectedCar + 1) % 4); }
    void toggleAdas();
    void toggleArGuides();
    void toggleZBuffer3DMode();
    void fieldRepair();

    // Getters
    const Track& getTrack() const { return m_track; }
    const PlayerCar& getPlayer() const { return m_player; }
    PlayerCar& getPlayerMutable() { return m_player; }
    const QVector<NPCCar>& getTraffic() const { return m_traffic; }
    QVector<NPCCar>& getTrafficMutable() { return m_traffic; }
    const QVector<CrashSpark>& getSparks() const { return m_sparks; }
    TrackTheme getCurrentTheme() const { return m_track.getInfo().theme; }
    bool isPaused() const { return m_paused; }
    void togglePause() { m_paused = !m_paused; }
    void resetPlayer();
    QVector<RaceStandingEntry> getRaceStandings() const;
    void spawnCrashSparks(qreal z, qreal x, int count, bool isHeadOn, qreal severity);

    // Infotainment controls
    const InfotainmentState& getInfotainment() const { return m_infoState; }
    InfotainmentState& getInfotainmentMutable() { return m_infoState; }
    void setInfotainmentMode(InfotainmentMode mode) { m_infoState.mode = mode; }
    void cycleInfotainmentMode() {
        m_infoState.mode = static_cast<InfotainmentMode>((static_cast<int>(m_infoState.mode) + 1) % 4);
    }
    void nextMusicTrack() {
        m_infoState.currentTrack = (m_infoState.currentTrack + 1) % 4;
        m_infoState.trackElapsed = 0;
    }
    void prevMusicTrack() {
        m_infoState.currentTrack = (m_infoState.currentTrack + 3) % 4;
        m_infoState.trackElapsed = 0;
    }
    void toggleMusicPlay() {
        m_infoState.isPlaying = !m_infoState.isPlaying;
    }
    void setMusicVolume(qreal vol) {
        m_infoState.volume = qBound(0.0, vol, 1.0);
    }
    void seekMusic(qreal fraction) {
        m_infoState.trackElapsed = qBound(0.0, fraction * m_infoState.trackDuration, m_infoState.trackDuration);
    }
    void setVisualizerLevels(const qreal *levels, int count) {
        int n = qMin(count, 16);
        for (int i = 0; i < n; ++i) {
            m_infoState.visualizer[i] = levels[i];
        }
    }
    void setWebPage(int page);
    void nextWebPage() { setWebPage((m_infoState.currentWebPage + 1) % 4); }
    void prevWebPage() { setWebPage((m_infoState.currentWebPage + 3) % 4); }

signals:
    void collisionOccurred();
    void nearMissOccurred(int points, int combo);
    void gearChanged(int newGear);

private:
    void updatePlayerPhysics(qreal dt);
    void updateGearAndRpm(qreal dt);
    void updateTraffic(qreal dt);
    void checkCollisionsAndNearMisses(qreal dt);
    void spawnTraffic();

    Track m_track;
    PlayerCar m_player;
    QVector<NPCCar> m_traffic;
    QVector<CrashSpark> m_sparks;
    InfotainmentState m_infoState;
    TimeOfDay m_timeOfDay = TIME_SUNSET;

    // Input states
    bool m_keyUp = false;
    bool m_keyDown = false;
    bool m_keyLeft = false;
    bool m_keyRight = false;
    bool m_keySpace = false;
    bool m_keyLookBack = false;
    bool m_keyLookLeft = false;
    bool m_keyLookRight = false;
    bool m_keyHorn = false;

    bool m_paused = false;
    int m_nextNpcId = 1;
    GameDifficulty m_difficulty = DIFFICULTY_NORMAL;
};

#endif // GAMEENGINE_H
