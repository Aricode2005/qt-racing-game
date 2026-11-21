#ifndef GAMEDATA_H
#define GAMEDATA_H

// =====================================================================
//  APEX VELOCITY - Garage Catalog & Career Economy
//  Cars, engines, unlock prices and race coin rewards.
// =====================================================================

#include <QtGlobal>
#include <QString>
#include "RasterAssets.h"

struct CarSpec {
    const char* name;
    const char* tagline;
    const char* feature;     // Signature handling trait
    int   price;             // 0 = unlocked from the start
    qreal maxSpeed;          // km/h (before engine bonus)
    qreal accel;             // base acceleration
    qreal grip;              // steering / cornering multiplier
    qreal armor;             // crash damage resistance multiplier
    quint32 accentRgb;       // UI accent colour
    VehicleType sprite;      // visual model (matches renderer car index)
};

struct EngineSpec {
    const char* name;
    const char* layout;
    const char* feature;
    int   price;
    int   horsepower;
    qreal topSpeedBonus;     // km/h added to the car's top speed
    qreal accelBonus;        // added to base acceleration
    qreal gripMod;           // multiplies handling
    qreal armorMod;          // multiplies armour
    qreal lowSpeedTorque;    // extra accel below 90 km/h (launch)
    qreal highSpeedBoost;    // extra accel above 130 km/h (top end)
    quint32 accentRgb;
};

// Car index MUST match the renderer's visual model index:
// 0 = APEX GT-R, 1 = VORTEX V12, 2 = DRIFT SPEC-R, 3 = TITAN ENFORCER
static const int kCarCount = 4;
static const CarSpec kCars[kCarCount] = {
    { "APEX GT-R",      "Twin-turbo grand tourer",
      "BALANCED CHASSIS - no weak spots, perfect first car",
      0,    200.0,  96.0, 1.00, 1.00, 0x9FB2CC, VEHICLE_SEDAN_SILVER },
    { "VORTEX V12",     "Mid-engine carbon hypercar",
      "LAUNCH CONTROL - fastest car in the game, fragile carbon shell",
      5000, 206.0, 108.0, 0.97, 0.82, 0xFF3B47, VEHICLE_SPORTS_RED },
    { "DRIFT SPEC-R",   "JDM touge weapon",
      "DRIFT GRIP - +28% cornering grip, carves through hairpins",
      1200, 197.0,  95.0, 1.28, 0.95, 0xFFB52E, VEHICLE_COMPACT_BLUE },
    { "TITAN ENFORCER", "Armoured pursuit interceptor",
      "REINFORCED CAGE - shrugs off 35% of all crash damage",
      2500, 199.0,  92.0, 1.04, 1.55, 0x37D58A, VEHICLE_SUV_BLACK },
};

// Garage display order (cheapest -> most exclusive)
static const int kCarDisplayOrder[kCarCount] = { 0, 2, 3, 1 };

static const int kEngineCount = 5;
static const EngineSpec kEngines[kEngineCount] = {
    { "STREET I4 TURBO",   "2.0L inline-4 single turbo",
      "LIGHTWEIGHT BLOCK - +4% handling from reduced front weight",
      0,     280,  0.0,  0.0, 1.04, 1.00,  0.0,  0.0, 0x7FD8FF },
    { "TWIN-TURBO V6",     "3.0L V6 twin-turbo",
      "TWIN-SCROLL TURBOS - strong, even pull through every gear",
      800,   420,  6.0,  6.0, 1.00, 1.00,  6.0,  4.0, 0x4FE0B0 },
    { "SUPERCHARGED V8",   "5.2L V8 roots supercharger",
      "INSTANT TORQUE - brutal launches and corner exits",
      2200,  610, 12.0, 10.0, 0.98, 1.05, 22.0,  0.0, 0xFFC247 },
    { "HYBRID V10 KERS",   "5.0L V10 + electric motor",
      "KERS E-BOOST - electric surge above 130 km/h",
      4500,  780, 18.0, 14.0, 1.00, 1.00, 10.0, 16.0, 0x9C7BFF },
    { "QUAD-TURBO W16",    "8.0L W16 quad-turbo",
      "HYPER POWER - highest top speed, heavy & heat-sensitive (-8% armour)",
      8000, 1500, 30.0, 22.0, 0.96, 0.92,  4.0, 24.0, 0xFF5A36 },
};

// ---------------------------------------------------------------------
//  Race coin rewards
//  Base by finishing position, multiplied by difficulty, +clean bonus,
//  +25% when racing other humans online/LAN.
// ---------------------------------------------------------------------
inline qreal difficultyCoinMultiplier(int difficulty) {
    return (difficulty <= 0) ? 1.0 : ((difficulty == 1) ? 1.5 : 2.5);
}

inline int basePositionCoins(int position) {
    switch (position) {
        case 1: return 500;
        case 2: return 350;
        case 3: return 250;
        case 4: return 160;
        case 5: return 110;
        default: return 70; // finishing bonus
    }
}

struct CoinReward {
    int base = 0;
    int cleanBonus = 0;
    qreal difficultyMult = 1.0;
    qreal multiplayerMult = 1.0;
    int total = 0;
};

inline CoinReward computeCoinReward(int position, int difficulty, bool multiplayer, int crashCount) {
    CoinReward r;
    r.base = basePositionCoins(position);
    r.cleanBonus = (crashCount == 0) ? 100 : 0;
    r.difficultyMult = difficultyCoinMultiplier(difficulty);
    r.multiplayerMult = multiplayer ? 1.25 : 1.0;
    r.total = qRound((r.base + r.cleanBonus) * r.difficultyMult * r.multiplayerMult);
    return r;
}

#endif // GAMEDATA_H
