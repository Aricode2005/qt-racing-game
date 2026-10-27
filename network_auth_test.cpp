#include <QCoreApplication>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>
#include <cassert>
#include "NetworkManager.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    qDebug() << "=== Starting Custom Room Authentication Verification Test ===";

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
                    qDebug() << "=== ALL CUSTOM ROOM MULTIPLAYER TESTS PASSED SUCCESSFULLY! ===";
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
