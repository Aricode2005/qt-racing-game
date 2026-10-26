#include "NetworkManager.h"
#include <QNetworkDatagram>
#include <QRandomGenerator>

static const uint32_t APEX_MAGIC = 0x41504558; // "APEX"
static const quint16 BEACON_PORT = 7779;

NetworkManager::NetworkManager() {
    m_uptime.start();

    // Default friendly racer name
    QStringList names = { "VeloRacer", "ApexDrifter", "TurboNomad", "CyberGhost", "PhantomPilot" };
    m_localName = names[QRandomGenerator::global()->generate() % names.size()] + QString::number(QRandomGenerator::global()->generate() % 90 + 10);

    m_beaconTimer = new QTimer(this);
    connect(m_beaconTimer, &QTimer::timeout, this, &NetworkManager::onBeaconTimer);

    m_pingTimer = new QTimer(this);
    connect(m_pingTimer, &QTimer::timeout, this, &NetworkManager::onPingTimer);

    // Initialize beacon listener socket to discover LAN games automatically
    m_beaconSocket = new QUdpSocket(this);
    m_beaconSocket->bind(QHostAddress::AnyIPv4, BEACON_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(m_beaconSocket, &QUdpSocket::readyRead, this, [this]() {
        while (m_beaconSocket->hasPendingDatagrams()) {
            QNetworkDatagram datagram = m_beaconSocket->receiveDatagram();
            QByteArray data = datagram.data();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isObject()) continue;
            QJsonObject obj = doc.object();
            if (obj["type"].toString() == "APEX_BEACON") {
                QHostAddress senderAddr = datagram.senderAddress();
                // Normalize IPv4-mapped IPv6 addresses (e.g. ::ffff:192.168.1.5 -> 192.168.1.5)
                bool ok = false;
                quint32 ip4 = senderAddr.toIPv4Address(&ok);
                if (ok) senderAddr = QHostAddress(ip4);

                QString hostName = obj["name"].toString();
                quint16 port = (quint16)obj["port"].toInt(7777);

                // Do not discover ourselves if broadcasting
                if (m_role == NET_ROLE_HOST && senderAddr == QHostAddress::LocalHost) continue;

                bool found = false;
                qint64 now = m_uptime.elapsed();
                for (DiscoveredHost& h : m_discoveredHosts) {
                    if (h.address == senderAddr && h.port == port) {
                        h.hostName = hostName;
                        h.trackTheme = obj["track"].toInt();
                        h.timeOfDay = obj["tod"].toInt();
                        h.carModel = obj["car"].toInt();
                        h.lastSeenMs = now;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    DiscoveredHost newHost;
                    newHost.hostName = hostName;
                    newHost.address = senderAddr;
                    newHost.port = port;
                    newHost.trackTheme = obj["track"].toInt();
                    newHost.timeOfDay = obj["tod"].toInt();
                    newHost.carModel = obj["car"].toInt();
                    newHost.lastSeenMs = now;
                    m_discoveredHosts.append(newHost);
                    emit hostDiscovered(newHost);
                }
            }
        }
    });

    m_beaconTimer->start(1000);
}

NetworkManager::~NetworkManager() {
    disconnectSession();
}

NetworkManager& NetworkManager::instance() {
    static NetworkManager s_instance;
    return s_instance;
}

QString NetworkManager::getLocalIpAddress() const {
    const QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (const QHostAddress& addr : list) {
        if (addr != QHostAddress::LocalHost && addr.toIPv4Address()) {
            QString str = addr.toString();
            if (str.startsWith("192.168.") || str.startsWith("10.") || str.startsWith("172.")) {
                return str;
            }
        }
    }
    return "127.0.0.1";
}

bool NetworkManager::startHost(quint16 tcpPort, quint16 udpPort) {
    disconnectSession();

    m_localTcpPort = tcpPort;
    m_localUdpPort = udpPort;
    m_peerUdpPort = 7780; // Default expected client UDP port

    m_tcpServer = new QTcpServer(this);
    if (!m_tcpServer->listen(QHostAddress::AnyIPv4, m_localTcpPort)) {
        emit statusMessage(QString("Failed to bind TCP server on port %1").arg(m_localTcpPort), true);
        delete m_tcpServer;
        m_tcpServer = nullptr;
        return false;
    }
    connect(m_tcpServer, &QTcpServer::newConnection, this, &NetworkManager::onNewTcpConnection);

    m_udpSocket = new QUdpSocket(this);
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, m_localUdpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit statusMessage(QString("Failed to bind UDP telemetry on port %1").arg(m_localUdpPort), true);
    }
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onUdpReadyRead);

    m_role = NET_ROLE_HOST;
    m_state = MP_STATE_LOBBY;
    m_remotePeer.isReady = false;

    emit statusMessage(QString("Hosting race on %1:%2 — Waiting for racer...").arg(getLocalIpAddress()).arg(m_localTcpPort));
    return true;
}

bool NetworkManager::connectToHost(const QString& hostAddress, quint16 tcpPort, quint16 udpPort) {
    disconnectSession();

    m_localUdpPort = udpPort;
    m_peerUdpPort = 7778; // Host default UDP port

    m_tcpSocket = new QTcpSocket(this);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &NetworkManager::onTcpReadyRead);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &NetworkManager::onTcpDisconnected);

    connect(m_tcpSocket, &QTcpSocket::connected, this, [this]() {
        m_role = NET_ROLE_CLIENT;
        m_state = MP_STATE_LOBBY;
        m_remotePeer.udpAddress = m_tcpSocket->peerAddress();

        // Send Handshake
        QJsonObject hello;
        hello["type"] = "HELLO";
        hello["name"] = m_localName;
        hello["udpPort"] = m_localUdpPort;
        sendTcpJson(hello);

        emit statusMessage(QString("Connected to host %1! In Lobby.").arg(m_tcpSocket->peerAddress().toString()));
        m_pingTimer->start(1500);
    });

    m_udpSocket = new QUdpSocket(this);
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, m_localUdpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit statusMessage(QString("Client UDP bind warning on %1").arg(m_localUdpPort), true);
    }
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onUdpReadyRead);

    emit statusMessage(QString("Connecting to %1:%2...").arg(hostAddress).arg(tcpPort));
    m_tcpSocket->connectToHost(hostAddress, tcpPort);
    return true;
}

void NetworkManager::disconnectSession() {
    m_pingTimer->stop();

    if (m_tcpSocket) {
        m_tcpSocket->disconnectFromHost();
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }
    if (m_tcpServer) {
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
    }
    if (m_udpSocket) {
        m_udpSocket->close();
        m_udpSocket->deleteLater();
        m_udpSocket = nullptr;
    }

    m_role = NET_ROLE_NONE;
    m_state = MP_STATE_DISCONNECTED;
    m_remotePeer.isReady = false;
    m_countdownTimer = -1.0;
}

void NetworkManager::onNewTcpConnection() {
    if (!m_tcpServer) return;
    QTcpSocket* newSocket = m_tcpServer->nextPendingConnection();
    if (!newSocket) return;

    if (m_tcpSocket) {
        // Only 1 rival peer for 1v1 multiplayer
        newSocket->disconnectFromHost();
        newSocket->deleteLater();
        return;
    }

    m_tcpSocket = newSocket;
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &NetworkManager::onTcpReadyRead);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &NetworkManager::onTcpDisconnected);

    QHostAddress peerAddr = m_tcpSocket->peerAddress();
    bool ok = false;
    quint32 ip4 = peerAddr.toIPv4Address(&ok);
    if (ok) peerAddr = QHostAddress(ip4);
    m_remotePeer.udpAddress = peerAddr;

    // Send Welcome & current circuit configuration
    QJsonObject welcome;
    welcome["type"] = "WELCOME";
    welcome["hostName"] = m_localName;
    welcome["udpPort"] = m_localUdpPort;
    sendTcpJson(welcome);

    m_pingTimer->start(1500);
    emit peerConnected(m_remotePeer.name);
    emit statusMessage(QString("Rival racer joined from %1!").arg(peerAddr.toString()));
}

void NetworkManager::onTcpDisconnected() {
    emit statusMessage("Rival racer disconnected.");
    emit peerDisconnected();
    if (m_role == NET_ROLE_CLIENT) {
        disconnectSession();
    } else {
        if (m_tcpSocket) {
            m_tcpSocket->deleteLater();
            m_tcpSocket = nullptr;
        }
        m_remotePeer.isReady = false;
        m_state = MP_STATE_LOBBY;
    }
}

void NetworkManager::sendTcpJson(const QJsonObject& json) {
    if (!m_tcpSocket || m_tcpSocket->state() != QAbstractSocket::ConnectedState) return;
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact) + "\n";
    m_tcpSocket->write(data);
}

void NetworkManager::onTcpReadyRead() {
    if (!m_tcpSocket) return;
    while (m_tcpSocket->canReadLine()) {
        QByteArray line = m_tcpSocket->readLine().trimmed();
        if (line.isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject()) {
            processTcpMessage(doc.object());
        }
    }
}

void NetworkManager::processTcpMessage(const QJsonObject& json) {
    QString type = json["type"].toString();

    if (type == "HELLO") {
        m_remotePeer.name = json["name"].toString("Rival");
        m_peerUdpPort = (quint16)json["udpPort"].toInt(7780);
        emit peerConnected(m_remotePeer.name);
    } else if (type == "WELCOME") {
        m_remotePeer.name = json["hostName"].toString("Host Driver");
        m_peerUdpPort = (quint16)json["udpPort"].toInt(7778);
        emit peerConnected(m_remotePeer.name);
    } else if (type == "READY") {
        m_remotePeer.isReady = json["ready"].toBool();
        emit peerReadyChanged(m_remotePeer.isReady);
    } else if (type == "TRACK_SYNC") {
        int theme = json["theme"].toInt();
        int tod = json["tod"].toInt();
        emit trackSyncRequested(theme, tod);
    } else if (type == "CAR_SYNC") {
        int car = json["car"].toInt();
        m_remotePeer.carModel = car;
        emit carSyncRequested(car);
    } else if (type == "COUNTDOWN_START") {
        qreal duration = json["duration"].toDouble(3.0);
        m_countdownTimer = duration;
        m_state = MP_STATE_COUNTDOWN;
        emit countdownStarted(duration);
    } else if (type == "RACE_RESTART") {
        m_state = MP_STATE_LOBBY;
        m_remotePeer.isReady = false;
        emit raceRestarted();
    } else if (type == "CHAT") {
        emit chatMessageReceived(json["sender"].toString(), json["text"].toString());
    } else if (type == "PING") {
        QJsonObject pong;
        pong["type"] = "PONG";
        pong["t"] = json["t"].toDouble();
        sendTcpJson(pong);
    } else if (type == "PONG") {
        qint64 sendTime = (qint64)json["t"].toDouble();
        m_remotePeer.pingMs = qBound(1LL, m_uptime.elapsed() - sendTime, 999LL);
    }
}

void NetworkManager::onPingTimer() {
    if (isConnected()) {
        QJsonObject ping;
        ping["type"] = "PING";
        ping["t"] = (double)m_uptime.elapsed();
        sendTcpJson(ping);
    }
}

void NetworkManager::onBeaconTimer() {
    // Clean up stale discovered hosts older than 4 seconds
    qint64 now = m_uptime.elapsed();
    for (int i = m_discoveredHosts.size() - 1; i >= 0; --i) {
        if (now - m_discoveredHosts[i].lastSeenMs > 4000) {
            m_discoveredHosts.removeAt(i);
        }
    }

    // If hosting, broadcast LAN beacon
    if (m_role == NET_ROLE_HOST && m_state == MP_STATE_LOBBY) {
        sendBeaconBroadcast();
    }
}

void NetworkManager::sendBeaconBroadcast() {
    if (!m_beaconSocket) return;
    QJsonObject beacon;
    beacon["type"] = "APEX_BEACON";
    beacon["name"] = m_localName;
    beacon["port"] = m_localTcpPort;
    beacon["car"] = 0;
    QByteArray data = QJsonDocument(beacon).toJson(QJsonDocument::Compact);
    m_beaconSocket->writeDatagram(data, QHostAddress::Broadcast, BEACON_PORT);
}

void NetworkManager::setReady(bool ready) {
    QJsonObject obj;
    obj["type"] = "READY";
    obj["ready"] = ready;
    sendTcpJson(obj);
}

void NetworkManager::notifyTrackChange(int trackTheme, int timeOfDay) {
    if (!isHost()) return;
    QJsonObject obj;
    obj["type"] = "TRACK_SYNC";
    obj["theme"] = trackTheme;
    obj["tod"] = timeOfDay;
    sendTcpJson(obj);
}

void NetworkManager::notifyCarChange(int carModel) {
    QJsonObject obj;
    obj["type"] = "CAR_SYNC";
    obj["car"] = carModel;
    sendTcpJson(obj);
}

void NetworkManager::startRaceCountdown() {
    if (!isHost()) return;
    m_countdownTimer = 3.5;
    m_state = MP_STATE_COUNTDOWN;

    QJsonObject obj;
    obj["type"] = "COUNTDOWN_START";
    obj["duration"] = 3.5;
    sendTcpJson(obj);

    emit countdownStarted(3.5);
}

void NetworkManager::notifyRaceRestart() {
    m_state = MP_STATE_LOBBY;
    m_remotePeer.isReady = false;
    QJsonObject obj;
    obj["type"] = "RACE_RESTART";
    sendTcpJson(obj);
    emit raceRestarted();
}

void NetworkManager::sendQuickChat(const QString& text) {
    QJsonObject obj;
    obj["type"] = "CHAT";
    obj["sender"] = m_localName;
    obj["text"] = text;
    sendTcpJson(obj);
}

void NetworkManager::sendTelemetry(const PlayerCar& player) {
    if (!m_udpSocket || !isConnected() || m_remotePeer.udpAddress.isNull()) return;

    TelemetryPacket pkt;
    pkt.magic = APEX_MAGIC;
    pkt.sequence = ++m_packetSequence;
    pkt.timestampMs = (uint32_t)(m_uptime.elapsed() & 0xFFFFFFFF);
    pkt.worldZ = (float)player.worldZ;
    pkt.x = (float)player.x;
    pkt.speed = (float)player.speed;
    pkt.steerAngle = (float)player.steerAngle;
    pkt.pitchG = (float)player.pitchG;
    pkt.rollG = (float)player.rollG;
    pkt.skidAngle = (float)player.steerAngle * 0.25f;
    pkt.carModel = (uint8_t)player.selectedCar;

    uint8_t flags = 0;
    if (player.isBraking) flags |= 0x01;
    if (player.crashTimer > 0) flags |= 0x02;
    if (player.isWrecked) flags |= 0x04;
    if (player.hornActive) flags |= 0x08;
    if (player.raceFinished) flags |= 0x10;
    pkt.flags = flags;
    pkt.finishTime = (float)player.finishTime;
    pkt.health = (float)player.health;

    m_udpSocket->writeDatagram((const char*)&pkt, sizeof(pkt), m_remotePeer.udpAddress, m_peerUdpPort);
}

void NetworkManager::onUdpReadyRead() {
    if (!m_udpSocket) return;
    while (m_udpSocket->hasPendingDatagrams()) {
        TelemetryPacket pkt;
        QHostAddress sender;
        quint16 senderPort;
        qint64 bytes = m_udpSocket->readDatagram((char*)&pkt, sizeof(pkt), &sender, &senderPort);
        if (bytes == sizeof(TelemetryPacket) && pkt.magic == APEX_MAGIC) {
            // Update peer target coordinates
            m_interp.targetZ = pkt.worldZ;
            m_interp.targetX = pkt.x;
            m_interp.targetSpeed = pkt.speed;
            m_interp.targetSteer = pkt.steerAngle;
            m_interp.targetPitchG = pkt.pitchG;
            m_interp.targetRollG = pkt.rollG;
            m_interp.targetSkid = pkt.skidAngle;
            m_interp.carModel = pkt.carModel;
            m_interp.isBraking = (pkt.flags & 0x01) != 0;
            m_interp.isCrashing = (pkt.flags & 0x02) != 0;
            m_interp.hazardLights = (pkt.flags & 0x04) != 0;
            m_interp.horn = (pkt.flags & 0x08) != 0;
            m_interp.hasFinished = (pkt.flags & 0x10) != 0;
            m_interp.finishTime = pkt.finishTime;
            m_interp.health = pkt.health;
            m_interp.lastPacketTimeMs = m_uptime.elapsed();
        }
    }
}

void NetworkManager::update(qreal dt, GameEngine& engine) {
    // 1. Synchronized Race Countdown
    if (m_countdownTimer > 0) {
        m_countdownTimer -= dt;
        if (m_countdownTimer <= 0) {
            m_countdownTimer = 0;
            m_state = MP_STATE_RACING;
            emit raceStarted();
        }
    }

    // 2. Synchronize Remote Rival Car into GameEngine
    if (isConnected() && m_interp.lastPacketTimeMs > 0) {
        // Dead Reckoning: extrapolate forward based on remote player's current speed
        qreal dtSincePacket = (m_uptime.elapsed() - m_interp.lastPacketTimeMs) / 1000.0;
        if (dtSincePacket < 0.6) {
            qreal extrapolatedZ = m_interp.targetZ + (m_interp.targetSpeed / 3.6) * dtSincePacket * 62.0;

            NPCCar remoteCar;
            remoteCar.id = 9999; // Dedicated ID for remote multiplayer peer
            remoteCar.isRemotePlayer = true;
            remoteCar.racerName = m_remotePeer.name;
            remoteCar.worldZ = extrapolatedZ;
            remoteCar.x = m_interp.targetX;
            remoteCar.targetX = m_interp.targetX;
            remoteCar.speed = m_interp.targetSpeed;
            remoteCar.maxSpeed = 200.0;
            remoteCar.isBraking = m_interp.isBraking;
            remoteCar.isCrashing = m_interp.isCrashing;
            remoteCar.hazardLights = m_interp.hazardLights;
            remoteCar.hasFinished = m_interp.hasFinished;
            remoteCar.finishTime = m_interp.finishTime;

            // Map selectedCar to vehicle model type
            switch (m_interp.carModel) {
                case 1: remoteCar.type = VEHICLE_SPORTS_RED; break;
                case 2: remoteCar.type = VEHICLE_COMPACT_BLUE; break;
                case 3: remoteCar.type = VEHICLE_SUV_BLACK; break;
                default: remoteCar.type = VEHICLE_SEDAN_SILVER; break;
            }

            engine.setRemotePlayerCar(remoteCar);
        }
    }
}
