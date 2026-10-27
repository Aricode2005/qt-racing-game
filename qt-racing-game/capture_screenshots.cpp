#include <QApplication>
#include <QPixmap>
#include <QPainter>
#include <QDebug>
#include "GameWindow.h"

int main(int argc, char *argv[]) {
    // Note: Do not use "offscreen" platform on Windows MinGW as it lacks a font engine and produces glyph boxes
    QApplication app(argc, argv);

    GameWindow window;
    window.resize(1280, 720);

    GameEngine& engine = window.getEngine();
    engine.setTrackTheme(THEME_SUNSET_COAST);
    engine.setTimeOfDay(TIME_SUNSET);
    engine.setDifficulty(DIFFICULTY_PRO);

    // Position player approaching segment 45 billboard (Z = 45 * 200 = 9000)
    PlayerCar& p = engine.getPlayerMutable();
    p.worldZ = 8520.0; // 480 units away -> super-tall elevated view soaring high
    p.speed = 175.0;
    p.rpm = 6600;
    p.gear = 5;
    p.x = 0.05;
    p.steerAngle = 0.08;
    p.health = 96;
    p.score = 28400;
    p.crashCount = 0;

    // Position traffic cars: one ahead in left lane, two behind reflecting in mirrors
    auto& traffic = engine.getTrafficMutable();
    if (!traffic.isEmpty()) {
        traffic[0].worldZ = 9250.0; // ahead past the billboard
        traffic[0].assignedLane = 1;
        traffic[0].x = -0.15;
        traffic[0].speed = 120.0;
        traffic[0].type = VEHICLE_SEDAN_SILVER;
    }
    if (traffic.size() > 1) {
        traffic[1].worldZ = 8250.0; // behind player in left lane -> clear in left mirror & rearview
        traffic[1].assignedLane = 0;
        traffic[1].x = -0.65;
        traffic[1].speed = 135.0;
        traffic[1].type = VEHICLE_SUV_BLACK;
    }
    if (traffic.size() > 2) {
        traffic[2].worldZ = 8290.0; // behind player in right lane -> clear in right mirror & rearview
        traffic[2].assignedLane = 2;
        traffic[2].x = 0.65;
        traffic[2].speed = 140.0;
        traffic[2].type = VEHICLE_COMPACT_BLUE;
    }
    if (traffic.size() > 3) {
        traffic[3].worldZ = 8530.0; // alongside player -> clearly visible in left quarter window!
        traffic[3].assignedLane = 1;
        traffic[3].x = -0.42;
        traffic[3].speed = 185.0;
        traffic[3].type = VEHICLE_SPORTS_RED;
    }

    // 0. Capture Driving View with Passing Cars, Side Mirrors & Interactive Sports Pedals
    if (traffic.size() >= 7) {
        traffic[0].worldZ = 10450.0; traffic[0].x = -0.32; traffic[0].currentPosition = 1;
        traffic[4].worldZ =  9950.0; traffic[4].x =  0.38; traffic[4].currentPosition = 2;
        traffic[5].worldZ =  7600.0; traffic[5].x =  0.12; traffic[5].currentPosition = 6;
        traffic[6].worldZ =  6900.0; traffic[6].x = -0.25; traffic[6].currentPosition = 7;
    }
    p.currentPosition = 3;
    p.raceTime = 14.6;
    window.setShowMainMenu(false);
    p.isAccelerating = true; // Shows Gas Pedal depressed with glowing electric neon aura
    p.lookingLeft = false;
    p.lookingRight = false;
    p.povPan = 0.0;
    QPixmap pixPedals(1280, 720);
    pixPedals.fill(Qt::black);
    window.render(&pixPedals);
    pixPedals.save("C:/Users/aritr/.gemini/antigravity/brain/168d448c-b04b-47bd-9aaa-9cc446db8ad1/side_mirrors_and_pedals_view.png");
    qDebug() << "Saved side mirrors and pedals screenshot";
    p.thirdPersonView = true;
    p.worldZ = 0.0;
    p.x = 0.25;
    p.raceTime = 2.4;
    p.currentPosition = 6;
    if (traffic.size() >= 7) {
        traffic[0].worldZ = 3380.0; traffic[0].x = -0.46; traffic[0].currentPosition = 1;
        traffic[1].worldZ = 2880.0; traffic[1].x =  0.46; traffic[1].currentPosition = 2;
        traffic[2].worldZ = 2380.0; traffic[2].x = -0.22; traffic[2].currentPosition = 3;
        traffic[3].worldZ = 1880.0; traffic[3].x =  0.24; traffic[3].currentPosition = 4;
        traffic[4].worldZ = 1420.0; traffic[4].x = -0.54; traffic[4].currentPosition = 5;
        for (int k = 5; k < traffic.size(); ++k) {
            traffic[k].worldZ = -140.0 - (k - 4) * 180.0;
            traffic[k].currentPosition = k + 2;
        }
    }
    p.boostBar = 0.72;
    QPixmap pixThird(1280, 720);
    pixThird.fill(Qt::black);
    window.render(&pixThird);
    pixThird.save("C:/Users/aritr/.gemini/antigravity/brain/168d448c-b04b-47bd-9aaa-9cc446db8ad1/third_person_chase_view.png");
    p.thirdPersonView = false;
    p.isAccelerating = false;

    // 1. Capture driving view with super-tall elevated billboard & Infotainment WEB BROWSER
    if (traffic.size() >= 7) {
        traffic[0].worldZ = 10450.0; traffic[0].x = -0.32; traffic[0].currentPosition = 1;
        traffic[4].worldZ =  9950.0; traffic[4].x =  0.38; traffic[4].currentPosition = 2;
        traffic[5].worldZ =  7600.0; traffic[5].x =  0.12; traffic[5].currentPosition = 6;
        traffic[6].worldZ =  6900.0; traffic[6].x = -0.25; traffic[6].currentPosition = 7;
    }
    p.currentPosition = 3;
    p.raceTime = 14.6;
    window.setShowMainMenu(false);
    p.lookingLeft = false;
    p.lookingRight = false;
    p.povPan = 0.0;
    engine.setInfotainmentMode(INFO_MODE_WEB);
    engine.setWebPage(0); // Speedr social feed
    QPixmap pixWeb(1280, 720);
    pixWeb.fill(Qt::black);
    window.render(&pixWeb);
    QString pathWeb = "C:/Users/aritr/.gemini/antigravity/brain/168d448c-b04b-47bd-9aaa-9cc446db8ad1/dashboard_web_browser_view.png";
    pixWeb.save(pathWeb);
    qDebug() << "Saved web browser screenshot to" << pathWeb;

    // 1b. Capture driving view with super-tall billboard & Infotainment MUSIC PLAYER
    engine.setInfotainmentMode(INFO_MODE_MUSIC);
    qreal mockViz[16] = { 0.45, 0.75, 0.95, 0.82, 0.60, 0.40, 0.70, 0.90, 0.85, 0.50, 0.65, 0.80, 0.70, 0.55, 0.40, 0.30 };
    engine.setVisualizerLevels(mockViz, 16);
    QPixmap pixMusic(1280, 720);
    pixMusic.fill(Qt::black);
    window.render(&pixMusic);
    QString pathMusic = "C:/Users/aritr/.gemini/antigravity/brain/168d448c-b04b-47bd-9aaa-9cc446db8ad1/dashboard_music_player_view.png";
    pixMusic.save(pathMusic);
    qDebug() << "Saved music player screenshot to" << pathMusic;

    // 1c. Capture Infotainment Weather Doppler Radar Page
    engine.setInfotainmentMode(INFO_MODE_WEB);
    engine.setWebPage(2); // Doppler Weather Radar
    QPixmap pixWx(1280, 720);
    pixWx.fill(Qt::black);
    window.render(&pixWx);
    pixWx.save("C:/Users/aritr/.gemini/antigravity/brain/168d448c-b04b-47bd-9aaa-9cc446db8ad1/dashboard_weather_radar_view.png");

    // 1d. Capture Infotainment GPS Navigation Mode
    engine.setInfotainmentMode(INFO_MODE_RADAR);
    QPixmap pixGps(1280, 720);
    pixGps.fill(Qt::black);
    window.render(&pixGps);
    pixGps.save("C:/Users/aritr/.gemini/antigravity/brain/168d448c-b04b-47bd-9aaa-9cc446db8ad1/dashboard_gps_nav_view.png");

    // Reset to web page 0 for subsequent driving scenes
    engine.setInfotainmentMode(INFO_MODE_WEB);
    engine.setWebPage(0);

    // 2. Capture Sideways POV glance (Key Q: Look Left) showing roadside utility poles & hoardings
    p.lookingLeft = true;
    p.povPan = -1.0;
    QPixmap pixSide(1280, 720);
    pixSide.fill(Qt::black);
    window.render(&pixSide);
    QString pathSide = "C:/Users/aritr/.gemini/antigravity/brain/168d448c-b04b-47bd-9aaa-9cc446db8ad1/sideways_poles_hoardings_view.png";
    pixSide.save(pathSide);
    qDebug() << "Saved sideways glance screenshot to" << pathSide;

    // 3. Reset to forward and position player approaching wide highway neon hoarding (segment 96 = 19200)
    p.lookingLeft = false;
    p.povPan = 0.0;
    p.worldZ = 6400.0 - 450.0; // segment 32 (32*200 = 6400) is SCENERY_HOARDING_NEON
    QPixmap pixHoard(1280, 720);
    pixHoard.fill(Qt::black);
    window.render(&pixHoard);
    QString pathHoard = "C:/Users/aritr/.gemini/antigravity/brain/168d448c-b04b-47bd-9aaa-9cc446db8ad1/neon_hoarding_front_view.png";
    pixHoard.save(pathHoard);
    qDebug() << "Saved neon hoarding screenshot to" << pathHoard;

    // 4. Capture Main Menu with Difficulty selection & Track cards
    window.setShowMainMenu(true);
    QPixmap pix2(1280, 720);
    pix2.fill(Qt::black);
    window.render(&pix2);
    QString path2 = "C:/Users/aritr/.gemini/antigravity/brain/168d448c-b04b-47bd-9aaa-9cc446db8ad1/main_menu_difficulty_view.png";
    pix2.save(path2);
    qDebug() << "Saved main menu screenshot to" << path2;

    // 5. Capture Realistic Crash Impact with 3D Sparks, Glass Shards, Fishtailing NPC & Skid Smoke
    if (traffic.size() >= 7) {
        traffic[0].worldZ = 10450.0; traffic[0].x = -0.32; traffic[0].currentPosition = 1;
        traffic[4].worldZ =  9950.0; traffic[4].x =  0.38; traffic[4].currentPosition = 2;
        traffic[5].worldZ =  7600.0; traffic[5].x =  0.12; traffic[5].currentPosition = 6;
        traffic[6].worldZ =  6900.0; traffic[6].x = -0.25; traffic[6].currentPosition = 7;
    }
    p.currentPosition = 3;
    p.raceTime = 14.6;
    window.setShowMainMenu(false);
    p.worldZ = 5000.0;
    p.x = 0.18;
    p.speed = 85.0;
    p.impactCameraShake = 0.75;
    p.damageFlashTimer = 0.50;
    p.crashTimer = 1.8;
    p.health = 68;
    p.crashCount = 1;
    p.lastAnnouncement = "CRASH IMPACT! -28% HEALTH (1 HITS)";
    p.announcementTimer = 2.0;

    // Spawn crash sparks at close-range collision zone (flies across windshield) and on road
    engine.spawnCrashSparks(5040.0, 0.18, 55, false, 1.5);

    // Position the collided NPC right in front (relZ = 900), fishtailing with hazard lights & skid smoke
    if (!traffic.isEmpty()) {
        traffic[0].worldZ = 5900.0;
        traffic[0].x = 0.20;
        traffic[0].isCrashing = true;
        traffic[0].skidAngle = -0.32; // Fishtailing sideways
        traffic[0].hazardLights = true;
        traffic[0].blinkerFlash = true;
        traffic[0].type = VEHICLE_SEDAN_SILVER;
        traffic[0].isOncoming = false;
    }

    QPixmap pixCrash(1280, 720);
    pixCrash.fill(Qt::black);
    window.render(&pixCrash);
    QString pathCrash = "C:/Users/aritr/.gemini/antigravity/brain/168d448c-b04b-47bd-9aaa-9cc446db8ad1/realistic_crash_impact_view.png";
    pixCrash.save(pathCrash);
    qDebug() << "Saved realistic crash screenshot to" << pathCrash;

    return 0;
}
