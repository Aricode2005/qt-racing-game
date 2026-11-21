// =====================================================================
//  APEX VELOCITY - Standalone Cross-LAN / Internet Cloud Relay Server
//  Bridges hosts and clients across any two different LANs / NATs
//  without requiring router port-forwarding on either player's machine.
//  Usage: apex_relay_server [port]   (default port: 7779)
// =====================================================================

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QDateTime>
#include <QRandomGenerator>
#include <iostream>

struct HostedRoom {
    QString roomId;
    QTcpSocket* controlSocket = nullptr;
    QJsonObject lastBeacon;
};

struct PendingTunnel {
    QString tunnelId;
    QString roomId;
    QTcpSocket* clientSocket = nullptr;
    QByteArray initialAuthPayload;
    qint64 createdMs = 0;
};

class ApexRelayServer : public QObject {
public:
    explicit ApexRelayServer(quint16 port, QObject* parent = nullptr)
        : QObject(parent), m_port(port)
    {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &ApexRelayServer::onNewConnection);
    }

    bool start() {
        if (!m_server->listen(QHostAddress::Any, m_port)) {
            std::cerr << "[RELAY ERROR] Failed to bind TCP port " << m_port << "\n";
            return false;
        }
        std::cout << "=================================================================\n"
                  << "  APEX VELOCITY - GLOBAL WAN / CROSS-LAN RELAY SERVER\n"
                  << "  Listening on TCP 0.0.0.0:" << m_port << "\n"
                  << "  Players on any LAN can set WAN Relay to <your-ip>:" << m_port << "\n"
                  << "=================================================================\n" << std::flush;
        return true;
    }

private:
    void sendJson(QTcpSocket* sock, const QJsonObject& obj) {
        if (!sock || sock->state() != QAbstractSocket::ConnectedState) return;
        QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
        sock->write(data);
        sock->flush();
    }

    void broadcastBeaconsToLobby(const QJsonObject& beacon) {
        QByteArray data = QJsonDocument(beacon).toJson(QJsonDocument::Compact) + "\n";
        for (QTcpSocket* sub : m_lobbySubscribers) {
            if (sub && sub->state() == QAbstractSocket::ConnectedState) {
                sub->write(data);
            }
        }
    }

    void onNewConnection() {
        while (m_server->hasPendingConnections()) {
            QTcpSocket* sock = m_server->nextPendingConnection();
            if (!sock) continue;
            sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);

            connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
                onSocketReadyRead(sock);
            });
            connect(sock, &QTcpSocket::disconnected, this, [this, sock]() {
                onSocketDisconnected(sock);
            });
        }
    }

    void onSocketReadyRead(QTcpSocket* sock) {
        // 1. If this socket is already bridged in a live tunnel, pipe raw bytes directly!
        if (m_bridgedPeers.contains(sock)) {
            QTcpSocket* partner = m_bridgedPeers.value(sock);
            QByteArray raw = sock->readAll();
            if (partner && partner->state() == QAbstractSocket::ConnectedState) {
                partner->write(raw);
            }
            return;
        }

        while (sock->canReadLine()) {
            QByteArray rawLine = sock->readLine();
            QByteArray trimmed = rawLine.trimmed();
            if (trimmed.isEmpty()) continue;

            QJsonDocument doc = QJsonDocument::fromJson(trimmed);
            if (!doc.isObject()) continue;
            QJsonObject obj = doc.object();
            QString type = obj["type"].toString();

            if (type == "RELAY_SUBSCRIBE_LOBBY") {
                m_lobbySubscribers.insert(sock);
                for (const HostedRoom& r : m_rooms) {
                    if (!r.lastBeacon.isEmpty()) {
                        sendJson(sock, r.lastBeacon);
                    }
                }
            } else if (type == "RELAY_REGISTER_HOST") {
                QString roomId = obj["roomId"].toString("APEX-101").trimmed().toUpper();
                HostedRoom room;
                room.roomId = roomId;
                room.controlSocket = sock;

                QJsonObject beacon = obj;
                beacon["type"] = "APEX_BEACON";
                room.lastBeacon = beacon;
                m_rooms[roomId] = room;
                m_hostSocketToRoom[sock] = roomId;

                std::cout << "[RELAY] Host registered Room [" << roomId.toStdString()
                          << "] (" << obj["name"].toString().toStdString() << ")\n" << std::flush;
                broadcastBeaconsToLobby(beacon);

            } else if (type == "APEX_BEACON") {
                QString roomId = obj["roomId"].toString().trimmed().toUpper();
                if (m_rooms.contains(roomId) && m_rooms[roomId].controlSocket == sock) {
                    m_rooms[roomId].lastBeacon = obj;
                    broadcastBeaconsToLobby(obj);
                }

            } else if (type == "AUTH" || type == "HELLO") {
                // A remote Joiner connected and sent AUTH for a Room ID!
                QString roomId = obj["roomId"].toString("APEX-101").trimmed().toUpper();
                if (!m_rooms.contains(roomId) || !m_rooms[roomId].controlSocket ||
                    m_rooms[roomId].controlSocket->state() != QAbstractSocket::ConnectedState) {
                    QJsonObject rej;
                    rej["type"] = "AUTH_FAILED";
                    rej["code"] = "WRONG_ROOM";
                    rej["reason"] = QString("Room '%1' is not currently hosted on this Relay Server").arg(roomId);
                    sendJson(sock, rej);
                    sock->disconnectFromHost();
                    return;
                }

                QString tunnelId = QString("T-%1-%2")
                    .arg(QDateTime::currentMSecsSinceEpoch())
                    .arg(QRandomGenerator::global()->bounded(1000, 9999));

                PendingTunnel pt;
                pt.tunnelId = tunnelId;
                pt.roomId = roomId;
                pt.clientSocket = sock;
                pt.initialAuthPayload = rawLine + sock->readAll();
                pt.createdMs = QDateTime::currentMSecsSinceEpoch();
                m_pendingTunnels[tunnelId] = pt;

                QJsonObject req;
                req["type"] = "RELAY_NEW_CLIENT";
                req["roomId"] = roomId;
                req["tunnelId"] = tunnelId;
                sendJson(m_rooms[roomId].controlSocket, req);
                return;

            } else if (type == "RELAY_BIND_HOST") {
                QString tunnelId = obj["tunnelId"].toString();
                if (!m_pendingTunnels.contains(tunnelId)) {
                    sock->disconnectFromHost();
                    return;
                }
                PendingTunnel pt = m_pendingTunnels.take(tunnelId);
                QTcpSocket* clientSock = pt.clientSocket;
                if (!clientSock || clientSock->state() != QAbstractSocket::ConnectedState) {
                    sock->disconnectFromHost();
                    return;
                }

                // Bridge clientSock <-> sock (Host tunnel socket)
                m_bridgedPeers[clientSock] = sock;
                m_bridgedPeers[sock] = clientSock;

                // Forward the buffered AUTH line + any trailing bytes to the Host!
                sock->write(pt.initialAuthPayload);
                if (clientSock->bytesAvailable() > 0) {
                    sock->write(clientSock->readAll());
                }
                sock->flush();
                std::cout << "[RELAY] Bridged WAN client into Room [" << pt.roomId.toStdString()
                          << "] via tunnel " << tunnelId.toStdString() << "\n" << std::flush;
                return;
            }
        }
    }

    void onSocketDisconnected(QTcpSocket* sock) {
        m_lobbySubscribers.remove(sock);

        if (m_hostSocketToRoom.contains(sock)) {
            QString roomId = m_hostSocketToRoom.take(sock);
            m_rooms.remove(roomId);
            std::cout << "[RELAY] Room [" << roomId.toStdString() << "] closed (Host disconnected)\n" << std::flush;
        }

        if (m_bridgedPeers.contains(sock)) {
            QTcpSocket* partner = m_bridgedPeers.take(sock);
            m_bridgedPeers.remove(partner);
            if (partner && partner->state() != QAbstractSocket::UnconnectedState) {
                partner->disconnectFromHost();
            }
        }

        for (auto it = m_pendingTunnels.begin(); it != m_pendingTunnels.end(); ) {
            if (it.value().clientSocket == sock) {
                it = m_pendingTunnels.erase(it);
            } else {
                ++it;
            }
        }
        sock->deleteLater();
    }

    quint16 m_port = 7779;
    QTcpServer* m_server = nullptr;
    QMap<QString, HostedRoom> m_rooms;
    QMap<QTcpSocket*, QString> m_hostSocketToRoom;
    QSet<QTcpSocket*> m_lobbySubscribers;
    QMap<QString, PendingTunnel> m_pendingTunnels;
    QMap<QTcpSocket*, QTcpSocket*> m_bridgedPeers;
};

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    quint16 port = 7779;
    if (argc > 1) {
        bool ok = false;
        quint16 p = QString::fromLocal8Bit(argv[1]).toUShort(&ok);
        if (ok && p > 0) port = p;
    } else if (qEnvironmentVariableIsSet("PORT")) {
        bool ok = false;
        quint16 p = qEnvironmentVariable("PORT").toUShort(&ok);
        if (ok && p > 0) port = p;
    }

    ApexRelayServer server(port);
    if (!server.start()) return 1;
    return app.exec();
}
