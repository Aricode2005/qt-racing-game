#ifndef PLAYERPROFILE_H
#define PLAYERPROFILE_H

#include <QString>
#include <QStringList>
#include <QtGlobal>

// =====================================================================
//  APEX VELOCITY - User Account & SQLite Database Profile Manager
//  Stores user accounts (username, salted SHA-256 password hash, coins,
//  unlocked cars, unlocked engines, and race history) in a real SQLite
//  relational database (apex_accounts.db).
// =====================================================================
class PlayerProfile {
public:
    static constexpr int kStartingCoins = 500;

    static QString databaseFilePath();

    void load();
    void save() const;
    void resetToDefaults();

    // User Account Authentication (SQLite Database backed)
    bool registerAccount(const QString& user, const QString& password, QString& outError);
    bool loginAccount(const QString& user, const QString& password, QString& outError);
    void logout();
    QStringList listRegisteredUsernames() const;

    bool ownsCar(int idx) const { return idx >= 0 && idx < 32 && (ownedCars & (1u << idx)); }
    bool ownsEngine(int idx) const { return idx >= 0 && idx < 32 && (ownedEngines & (1u << idx)); }

    // Returns true on success (deducts coins, unlocks item, and updates SQLite DB)
    bool buyCar(int idx);
    bool buyEngine(int idx);

    void addCoins(int amount);
    void recordRace(int position, int coinsEarned);

    bool isLoggedIn = false;
    QString username;           // Logged-in account username
    int coins = kStartingCoins;
    quint32 ownedCars = 1u;     // bit 0 = APEX GT-R
    quint32 ownedEngines = 1u;  // bit 0 = STREET I4
    int selectedCar = 0;
    int selectedEngine = 0;
    int racesCompleted = 0;
    int wins = 0;
    int podiums = 0;
    qint64 totalEarned = 0;
    QString driverName;
    QString relayServer;        // "host:port" of the online relay server

private:
    static void ensureDatabaseInitialized();
    static QString hashPasswordWithSalt(const QString& salt, const QString& password);
};

#endif // PLAYERPROFILE_H
