#include <QCoreApplication>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>
#include <cassert>
#include "NetworkManager.h"
#include "PlayerProfile.h"
#include "GameData.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    qDebug() << "=== [PART 1] Verifying SQLite User Account & Coin Database (apex_accounts.db) ===";
    qDebug() << "[DB] SQLite database file path:" << PlayerProfile::databaseFilePath();

    {
        PlayerProfile profile;
        profile.load();

        QString testUser = QString("TestRacer_%1").arg(QDateTime::currentMSecsSinceEpoch() % 100000);
        QString err;

        // 1. Register new account
        bool regOk = profile.registerAccount(testUser, "TurboPass99", err);
        if (!regOk) {
            qCritical() << "[DB TEST FAILED] registerAccount failed:" << err;
            return 1;
        }
        assert(profile.isLoggedIn);
        assert(profile.coins == 500);
        qDebug() << "[DB TEST 1 PASSED] Registered user" << testUser << "with starting coins:" << profile.coins;

        // 2. Award race coins & unlock Twin-Turbo V6 engine (800 coins)
        profile.recordRace(1, 750); // P1 win (+750 coins -> 1250 total)
        assert(profile.coins == 1250);
        assert(profile.wins == 1);
        bool boughtEng = profile.buyEngine(1); // TWIN-TURBO V6 costs 800 coins
        assert(boughtEng);
        assert(profile.ownsEngine(1));
        assert(profile.coins == 450);
        profile.logout();

        // 3. Verify wrong password rejection
        bool badLogin = profile.loginAccount(testUser, "WrongPassword!", err);
        assert(!badLogin);
        qDebug() << "[DB TEST 2 PASSED] Rejected invalid password:" << err;

        // 4. Verify valid login restores exact coins, wins, and unlocked engine from SQLite DB
        bool goodLogin = profile.loginAccount(testUser, "TurboPass99", err);
        assert(goodLogin);
        assert(profile.coins == 450);
        assert(profile.wins == 1);
        assert(profile.ownsEngine(1));
        qDebug() << "[DB TEST 3 PASSED] Logged back in from SQLite DB! Coins:" << profile.coins << "Wins:" << profile.wins;
    }

    qDebug() << "=== [PART 2] Starting Custom Room Authentication Verification Test ===";

    NetworkManager& hostNet = NetworkManager::instance();
    const QString TEST_ROOM = "APEX-TEST-ROOM";
    const QString TEST_PASS = "SuperSecret123";
    const quint16 TCP_PORT = 17777;
    const quint16 UDP_PORT = 17778;

    bool hostStarted = hostNet.startHostRoom(TEST_ROOM, TEST_PASS, TCP_PORT, UDP_PORT);
    if (!hostStarted) {
        qCritical() << "Failed to start host on port" << TCP_PORT;
        return 1;
    }
    qDebug() << "[HOST] Host started with Room ID:" << TEST_ROOM << "Password protected:" << hostNet.isPasswordProtected();

    int testStep = 1;
    auto* socket = new QTcpSocket(&app);

    auto sendAuthPacket = [&](QTcpSocket* sock, const QString& roomId, const QString& pass) {
        QJsonObject authObj;
        authObj["type"] = "AUTH";
        authObj["roomId"] = roomId;
        authObj["password"] = pass;
        authObj["name"] = "TesterBot";
        authObj["carModel"] = 1;
        authObj["udpPort"] = 17780;
        QByteArray data = QJsonDocument(authObj).toJson(QJsonDocument::Compact) + "\n";
        sock->write(data);
        sock->flush();
    };

    QObject::connect(socket, &QTcpSocket::connected, [&]() {
        qDebug() << "[CLIENT] Connected to host on port" << TCP_PORT << "for test step" << testStep;
        if (testStep == 1) {
            qDebug() << "[TEST 1] Sending invalid Room ID rejection test...";
            sendAuthPacket(socket, "WRONG-ROOM-ID", TEST_PASS);
        } else if (testStep == 2) {
            qDebug() << "[TEST 2] Sending invalid password rejection test...";
            sendAuthPacket(socket, TEST_ROOM, "INCORRECT-PASSWORD");
        } else if (testStep == 3) {
            qDebug() << "[TEST 3] Sending valid room ID and password...";
            sendAuthPacket(socket, TEST_ROOM, TEST_PASS);
        }
    });

    QObject::connect(socket, &QTcpSocket::readyRead, [&]() {
        while (socket->canReadLine()) {
            QByteArray line = socket->readLine().trimmed();
            QJsonDocument doc = QJsonDocument::fromJson(line);
            QJsonObject obj = doc.object();
            QString type = obj["type"].toString();

            if (testStep == 1) {
                if (type == "AUTH_FAILED") {
                    qDebug() << "[TEST 1 PASSED] Server rejected wrong room ID:" << obj["reason"].toString();
                    testStep = 2;
                } else {
                    qCritical() << "[TEST 1 FAILED] Expected AUTH_FAILED, received:" << type;
                    exit(1);
                }
            } else if (testStep == 2) {
                if (type == "AUTH_FAILED") {
                    qDebug() << "[TEST 2 PASSED] Server rejected wrong password:" << obj["reason"].toString();
                    testStep = 3;
                } else {
                    qCritical() << "[TEST 2 FAILED] Expected AUTH_FAILED, received:" << type;
                    exit(1);
                }
            } else if (testStep == 3) {
                if (type == "WELCOME") {
                    qDebug() << "[TEST 3 PASSED] Server accepted credentials! Welcome payload:"
                             << "peerId=" << obj["peerId"].toInt()
                             << "roomId=" << obj["roomId"].toString();
                    assert(obj["roomId"].toString().toUpper() == TEST_ROOM);
                    assert(hostNet.getConnectedPlayerCount() == 2);
                    qDebug() << "=== ALL SQLITE DB & MULTIPLAYER TESTS PASSED SUCCESSFULLY! ===";
                    exit(0);
                } else {
                    qCritical() << "[TEST 3 FAILED] Expected WELCOME, received:" << type;
                    exit(1);
                }
            }
        }
    });

    QObject::connect(socket, &QTcpSocket::disconnected, [&]() {
        if (testStep == 2 || testStep == 3) {
            QTimer::singleShot(100, [&]() {
                socket->connectToHost(QHostAddress::LocalHost, TCP_PORT);
            });
        }
    });

    socket->connectToHost(QHostAddress::LocalHost, TCP_PORT);

    // Timeout safety
    QTimer::singleShot(5000, [&]() {
        qCritical() << "Test timed out!";
        exit(1);
    });

    return app.exec();
}
