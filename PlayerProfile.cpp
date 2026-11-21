#include "PlayerProfile.h"
#include "GameData.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

static const char* kDbConnName = "ApexVelocityAccountsConnection";

QString PlayerProfile::databaseFilePath() {
    // Prefer the project root directory so apex_accounts.db is easy to inspect & back up
    QString projectDir = QStringLiteral("C:/Users/aritr/Downloads/ApexRacingGame/qt-racing-game");
    if (QDir(projectDir).exists()) {
        return projectDir + QStringLiteral("/apex_accounts.db");
    }
    QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        return appDir + QStringLiteral("/apex_accounts.db");
    }
    return QDir::currentPath() + QStringLiteral("/apex_accounts.db");
}

void PlayerProfile::ensureDatabaseInitialized() {
    QSqlDatabase db;
    if (QSqlDatabase::contains(QLatin1String(kDbConnName))) {
        db = QSqlDatabase::database(QLatin1String(kDbConnName));
    } else {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QLatin1String(kDbConnName));
        db.setDatabaseName(databaseFilePath());
    }

    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "[DB ERROR] Failed to open SQLite database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery q(db);
    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username TEXT UNIQUE NOT NULL COLLATE NOCASE,"
        "  password_salt TEXT NOT NULL,"
        "  password_hash TEXT NOT NULL,"
        "  coins INTEGER NOT NULL DEFAULT 500,"
        "  owned_cars INTEGER NOT NULL DEFAULT 1,"
        "  owned_engines INTEGER NOT NULL DEFAULT 1,"
        "  selected_car INTEGER NOT NULL DEFAULT 0,"
        "  selected_engine INTEGER NOT NULL DEFAULT 0,"
        "  races_completed INTEGER NOT NULL DEFAULT 0,"
        "  wins INTEGER NOT NULL DEFAULT 0,"
        "  podiums INTEGER NOT NULL DEFAULT 0,"
        "  total_earned INTEGER NOT NULL DEFAULT 500,"
        "  relay_server TEXT DEFAULT '',"
        "  created_at TEXT NOT NULL,"
        "  last_login_at TEXT NOT NULL"
        ");"
    ));

    q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS race_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username TEXT NOT NULL COLLATE NOCASE,"
        "  position INTEGER NOT NULL,"
        "  coins_earned INTEGER NOT NULL,"
        "  car_id INTEGER NOT NULL,"
        "  engine_id INTEGER NOT NULL,"
        "  played_at TEXT NOT NULL"
        ");"
    ));
}

QString PlayerProfile::hashPasswordWithSalt(const QString& salt, const QString& password) {
    QByteArray payload = (salt + QStringLiteral(":APEX_AUTH_v1:") + password).toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

void PlayerProfile::resetToDefaults() {
    coins = kStartingCoins;
    ownedCars = 1u;
    ownedEngines = 1u;
    selectedCar = 0;
    selectedEngine = 0;
    racesCompleted = 0;
    wins = 0;
    podiums = 0;
    totalEarned = kStartingCoins;
}

void PlayerProfile::load() {
    ensureDatabaseInitialized();
    resetToDefaults();
    // Account starts logged out until the user authenticates with username + password
    isLoggedIn = false;
    username.clear();
}

bool PlayerProfile::registerAccount(const QString& user, const QString& password, QString& outError) {
    ensureDatabaseInitialized();
    QString cleanUser = user.trimmed();
    if (cleanUser.length() < 2) {
        outError = QStringLiteral("Username must be at least 2 characters long.");
        return false;
    }
    if (password.length() < 4) {
        outError = QStringLiteral("Password must be at least 4 characters long.");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(QLatin1String(kDbConnName));
    if (!db.isOpen()) {
        outError = QStringLiteral("Database connection is not open.");
        return false;
    }

    // Check if username already exists
    {
        QSqlQuery checkQ(db);
        checkQ.prepare(QStringLiteral("SELECT id FROM users WHERE username = :u COLLATE NOCASE"));
        checkQ.bindValue(QStringLiteral(":u"), cleanUser);
        if (checkQ.exec() && checkQ.next()) {
            outError = QString("Username '%1' is already registered. Click [LOGIN TO ACCOUNT] to sign in!").arg(cleanUser);
            return false;
        }
    }

    // Generate random 128-bit hex salt
    QString salt = QString::number(QRandomGenerator::global()->generate64(), 16)
                 + QString::number(QRandomGenerator::global()->generate64(), 16);
    QString passHash = hashPasswordWithSalt(salt, password);
    QString nowIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    resetToDefaults();

    QSqlQuery ins(db);
    ins.prepare(QStringLiteral(
        "INSERT INTO users ("
        "  username, password_salt, password_hash, coins, owned_cars, owned_engines,"
        "  selected_car, selected_engine, races_completed, wins, podiums, total_earned,"
        "  relay_server, created_at, last_login_at"
        ") VALUES ("
        "  :u, :salt, :hash, :coins, :oc, :oe, :sc, :se, 0, 0, 0, :te, :relay, :now, :now"
        ")"
    ));
    ins.bindValue(QStringLiteral(":u"), cleanUser);
    ins.bindValue(QStringLiteral(":salt"), salt);
    ins.bindValue(QStringLiteral(":hash"), passHash);
    ins.bindValue(QStringLiteral(":coins"), coins);
    ins.bindValue(QStringLiteral(":oc"), ownedCars);
    ins.bindValue(QStringLiteral(":oe"), ownedEngines);
    ins.bindValue(QStringLiteral(":sc"), selectedCar);
    ins.bindValue(QStringLiteral(":se"), selectedEngine);
    ins.bindValue(QStringLiteral(":te"), totalEarned);
    ins.bindValue(QStringLiteral(":relay"), relayServer.isEmpty() ? QStringLiteral("") : relayServer);
    ins.bindValue(QStringLiteral(":now"), nowIso);

    if (!ins.exec()) {
        outError = QString("Database error creating account: %1").arg(ins.lastError().text());
        return false;
    }

    isLoggedIn = true;
    username = cleanUser;
    driverName = cleanUser;
    outError.clear();
    return true;
}

bool PlayerProfile::loginAccount(const QString& user, const QString& password, QString& outError) {
    ensureDatabaseInitialized();
    QString cleanUser = user.trimmed();
    if (cleanUser.isEmpty() || password.isEmpty()) {
        outError = QStringLiteral("Please enter both Username and Password.");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(QLatin1String(kDbConnName));
    if (!db.isOpen()) {
        outError = QStringLiteral("Database connection is not open.");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT username, password_salt, password_hash, coins, owned_cars, owned_engines,"
        "       selected_car, selected_engine, races_completed, wins, podiums, total_earned, relay_server "
        "FROM users WHERE username = :u COLLATE NOCASE"
    ));
    q.bindValue(QStringLiteral(":u"), cleanUser);

    if (!q.exec() || !q.next()) {
        outError = QString("Account '%1' does not exist. Click [REGISTER NEW ACCOUNT] to create it!").arg(cleanUser);
        return false;
    }

    QString dbUser = q.value(0).toString();
    QString salt = q.value(1).toString();
    QString expectedHash = q.value(2).toString();
    QString actualHash = hashPasswordWithSalt(salt, password);

    if (actualHash != expectedHash) {
        outError = QString("Invalid password for account '%1'.").arg(dbUser);
        return false;
    }

    isLoggedIn = true;
    username = dbUser;
    driverName = dbUser;
    coins = q.value(3).toInt();
    ownedCars = q.value(4).toUInt() | 1u;
    ownedEngines = q.value(5).toUInt() | 1u;
    selectedCar = qBound(0, q.value(6).toInt(), kCarCount - 1);
    selectedEngine = qBound(0, q.value(7).toInt(), kEngineCount - 1);
    if (!ownsCar(selectedCar)) selectedCar = 0;
    if (!ownsEngine(selectedEngine)) selectedEngine = 0;
    racesCompleted = q.value(8).toInt();
    wins = q.value(9).toInt();
    podiums = q.value(10).toInt();
    totalEarned = q.value(11).toLongLong();
    relayServer = q.value(12).toString();

    // Update last_login_at timestamp in database
    QSqlQuery up(db);
    up.prepare(QStringLiteral("UPDATE users SET last_login_at = :now WHERE username = :u COLLATE NOCASE"));
    up.bindValue(QStringLiteral(":now"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    up.bindValue(QStringLiteral(":u"), username);
    up.exec();

    outError.clear();
    return true;
}

void PlayerProfile::logout() {
    if (isLoggedIn) {
        save();
    }
    isLoggedIn = false;
    username.clear();
    driverName.clear();
    resetToDefaults();
}

QStringList PlayerProfile::listRegisteredUsernames() const {
    ensureDatabaseInitialized();
    QStringList list;
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(kDbConnName));
    if (!db.isOpen()) return list;

    QSqlQuery q(db);
    if (q.exec(QStringLiteral("SELECT username FROM users ORDER BY last_login_at DESC LIMIT 6"))) {
        while (q.next()) {
            list.append(q.value(0).toString());
        }
    }
    return list;
}

void PlayerProfile::save() const {
    if (!isLoggedIn || username.isEmpty()) return;
    ensureDatabaseInitialized();
    QSqlDatabase db = QSqlDatabase::database(QLatin1String(kDbConnName));
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE users SET "
        "  coins = :coins, "
        "  owned_cars = :oc, "
        "  owned_engines = :oe, "
        "  selected_car = :sc, "
        "  selected_engine = :se, "
        "  races_completed = :rc, "
        "  wins = :w, "
        "  podiums = :p, "
        "  total_earned = :te, "
        "  relay_server = :relay "
        "WHERE username = :u COLLATE NOCASE"
    ));
    q.bindValue(QStringLiteral(":coins"), coins);
    q.bindValue(QStringLiteral(":oc"), ownedCars);
    q.bindValue(QStringLiteral(":oe"), ownedEngines);
    q.bindValue(QStringLiteral(":sc"), selectedCar);
    q.bindValue(QStringLiteral(":se"), selectedEngine);
    q.bindValue(QStringLiteral(":rc"), racesCompleted);
    q.bindValue(QStringLiteral(":w"), wins);
    q.bindValue(QStringLiteral(":p"), podiums);
    q.bindValue(QStringLiteral(":te"), totalEarned);
    q.bindValue(QStringLiteral(":relay"), relayServer.isEmpty() ? QStringLiteral("") : relayServer);
    q.bindValue(QStringLiteral(":u"), username);
    q.exec();
}

bool PlayerProfile::buyCar(int idx) {
    if (idx < 0 || idx >= kCarCount || ownsCar(idx)) return false;
    if (coins < kCars[idx].price) return false;
    coins -= kCars[idx].price;
    ownedCars |= (1u << idx);
    save();
    return true;
}

bool PlayerProfile::buyEngine(int idx) {
    if (idx < 0 || idx >= kEngineCount || ownsEngine(idx)) return false;
    if (coins < kEngines[idx].price) return false;
    coins -= kEngines[idx].price;
    ownedEngines |= (1u << idx);
    save();
    return true;
}

void PlayerProfile::addCoins(int amount) {
    if (amount <= 0) return;
    coins += amount;
    totalEarned += amount;
}

void PlayerProfile::recordRace(int position, int coinsEarned) {
    racesCompleted++;
    if (position == 1) wins++;
    if (position >= 1 && position <= 3) podiums++;
    addCoins(coinsEarned);
    save();

    if (isLoggedIn && !username.isEmpty()) {
        QSqlDatabase db = QSqlDatabase::database(QLatin1String(kDbConnName));
        if (db.isOpen()) {
            QSqlQuery ins(db);
            ins.prepare(QStringLiteral(
                "INSERT INTO race_history (username, position, coins_earned, car_id, engine_id, played_at) "
                "VALUES (:u, :pos, :coins, :car, :eng, :now)"
            ));
            ins.bindValue(QStringLiteral(":u"), username);
            ins.bindValue(QStringLiteral(":pos"), position);
            ins.bindValue(QStringLiteral(":coins"), coinsEarned);
            ins.bindValue(QStringLiteral(":car"), selectedCar);
            ins.bindValue(QStringLiteral(":eng"), selectedEngine);
            ins.bindValue(QStringLiteral(":now"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            ins.exec();
        }
    }
}
