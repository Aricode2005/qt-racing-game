#include "NetworkManager.h"
#include <QNetworkDatagram>
#include <QRandomGenerator>

static const uint32_t APEX_MAGIC = 0x41504558; // "APEX"
static const quint16 BEACON_PORT = 7779;

NetworkManager::NetworkManager() {
    m_uptime.start();

    // Default friendly racer callsign
    QStringList names = { "VeloRacer", "ApexDrifter", "TurboNomad", "CyberGhost", "PhantomPilot", "NitroWolf" };
    m_localName = names[QRandomGenerator::global()->generate() % names.size()] + QString::number(QRandomGenerator::global()->generate() % 90 + 10);

    m_pingTimer = new QTimer(this);
    connect(m_pingTimer, &QTimer::timeout, this, &NetworkManager::onPingTimer);
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

qint64 NetworkManager::getAveragePingMs() const {
    if (m_peers.isEmpty()) return 0;
    qint64 total = 0;
    for (const RemotePeerInfo& p : m_peers) {
        total += p.pingMs;
    }
    return total / m_peers.size();
}

bool NetworkManager::startHostRoom(const QString& roomId, const QString& password, quint16 tcpPort, quint16 udpPort) {
    disconnectSession();

    m_roomId = roomId.trimmed().isEmpty() ? "APEX-101" : roomId.trimmed().toUpper();
    m_roomPassword = password.trimmed();
    m_localTcpPort = tcpPort;
    m_localUdpPort = udpPort;
    m_myPeerId = 0; // Host is always Player 1 / Peer 0

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
    m_peers.clear();
    m_peerTargets.clear();
    m_pendingClients.clear();

    QString passInfo = m_roomPassword.isEmpty() ? "Open (No Password)" : "Protected (Password Set)";
    emit statusMessage(QString("🏁 Hosted Room '%1' on %2:%3 • %4").arg(m_roomId).arg(getLocalIpAddress()).arg(m_localTcpPort).arg(passInfo));
    return true;
}

bool NetworkManager::startHost(quint16 tcpPort, quint16 udpPort) {
    return startHostRoom(m_roomId.isEmpty() ? "APEX-101" : m_roomId, m_roomPassword, tcpPort, udpPort);
}

bool NetworkManager::joinCustomRoom(const QString& hostAddress, const QString& roomId, const QString& password, quint16 tcpPort, quint16 udpPort) {
    disconnectSession();

    m_roomId = roomId.trimmed().isEmpty() ? "APEX-101" : roomId.trimmed().toUpper();
    m_roomPassword = password.trimmed();
    m_localUdpPort = udpPort;
    m_hostUdpAddress = QHostAddress(hostAddress);
    m_hostUdpPort = 7778; // Host default UDP telemetry port
    m_myPeerId = -1;      // Will be assigned by host

    m_clientTcpSocket = new QTcpSocket(this);
    connect(m_clientTcpSocket, &QTcpSocket::readyRead, this, &NetworkManager::onTcpReadyRead);
    connect(m_clientTcpSocket, &QTcpSocket::disconnected, this, &NetworkManager::onTcpDisconnected);
    connect(m_clientTcpSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (m_clientTcpSocket && m_state != MP_STATE_DISCONNECTED) {
            QString err = m_clientTcpSocket->errorString();
            emit statusMessage(QString("❌ Connection Failed: %1").arg(err), true);
            emit authFailed(err);
        }
    });

    connect(m_clientTcpSocket, &QTcpSocket::connected, this, [this]() {
        m_role = NET_ROLE_CLIENT;
        m_state = MP_STATE_LOBBY;

        // Send Authentication Handshake with Room ID & Password
        QJsonObject auth;
        auth["type"] = "AUTH";
        auth["roomId"] = m_roomId;
        auth["password"] = m_roomPassword;
        auth["name"] = m_localName;
        auth["udpPort"] = m_localUdpPort;
        sendTcpJson(auth, m_clientTcpSocket);

        emit statusMessage(QString("🔑 Authenticating with Room '%1' at %2...").arg(m_roomId).arg(m_clientTcpSocket->peerAddress().toString()));
    });

    m_udpSocket = new QUdpSocket(this);
    if (!m_udpSocket->bind(QHostAddress::AnyIPv4, m_localUdpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        if (!m_udpSocket->bind(QHostAddress::AnyIPv4, 0)) {
            emit statusMessage(QString("Client UDP bind warning on %1").arg(m_localUdpPort), true);
        }
    }
    m_localUdpPort = m_udpSocket->localPort();
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onUdpReadyRead);

    emit statusMessage(QString("Connecting to %1:%2 for Room '%3'...").arg(hostAddress).arg(tcpPort).arg(m_roomId));
    m_clientTcpSocket->connectToHost(hostAddress, tcpPort);
    return true;
}

bool NetworkManager::connectToHost(const QString& hostAddress, quint16 tcpPort, quint16 udpPort) {
    return joinCustomRoom(hostAddress, m_roomId.isEmpty() ? "APEX-101" : m_roomId, m_roomPassword, tcpPort, udpPort);
}

void NetworkManager::disconnectSession() {
    m_pingTimer->stop();

    for (QTcpSocket* s : m_pendingClients) {
        if (s) {
            s->disconnectFromHost();
            s->deleteLater();
        }
    }
    m_pendingClients.clear();

    if (m_clientTcpSocket) {
        m_clientTcpSocket->disconnectFromHost();
        m_clientTcpSocket->deleteLater();
        m_clientTcpSocket = nullptr;
    }
    if (m_tcpServer) {
        // Disconnect all client sockets
        for (auto& peer : m_peers) {
            if (peer.tcpSocket) {
                peer.tcpSocket->disconnectFromHost();
                peer.tcpSocket->deleteLater();
            }
        }
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
    m_myPeerId = 0;
    m_peers.clear();
    m_peerTargets.clear();
    m_countdownTimer = -1.0;
}

void NetworkManager::onNewTcpConnection() {
    if (!m_tcpServer) return;
    QTcpSocket* newSocket = m_tcpServer->nextPendingConnection();
    if (!newSocket) return;

    if (m_peers.size() >= 7) {
        // Room full (1 Host + 7 Clients = 8 Players max)
        QJsonObject rej;
        rej["type"] = "ROOM_FULL";
        rej["reason"] = "Room is full (Maximum 8 racers reached)";
        QByteArray data = QJsonDocument(rej).toJson(QJsonDocument::Compact) + "\n";
        newSocket->write(data);
        newSocket->disconnectFromHost();
        newSocket->deleteLater();
        return;
    }

    m_pendingClients.append(newSocket);
    connect(newSocket, &QTcpSocket::readyRead, this, &NetworkManager::onTcpReadyRead);
    connect(newSocket, &QTcpSocket::disconnected, this, &NetworkManager::onTcpDisconnected);
}

void NetworkManager::onTcpDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    if (m_pendingClients.contains(socket)) {
        m_pendingClients.removeOne(socket);
        socket->deleteLater();
        return;
    }

    if (m_role == NET_ROLE_CLIENT) {
        emit statusMessage("Disconnected from room.");
        disconnectSession();
        return;
    }

    // Find which client disconnected
    int disconnectedId = -1;
    QString disconnectedName;
    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        if (it.value().tcpSocket == socket) {
            disconnectedId = it.key();
            disconnectedName = it.value().name;
            break;
        }
    }

    if (disconnectedId != -1) {
        m_peers.remove(disconnectedId);
        m_peerTargets.remove(disconnectedId);

        // Notify other clients
        QJsonObject leftObj;
        leftObj["type"] = "PEER_LEFT";
        leftObj["id"] = disconnectedId;
        leftObj["name"] = disconnectedName;
        broadcastTcpJson(leftObj);

        emit peerLeft(disconnectedId, disconnectedName);
        emit statusMessage(QString("Pilot %1 left Room [%2].").arg(disconnectedName).arg(m_roomId));
    }

    socket->deleteLater();
}

void NetworkManager::sendTcpJson(const QJsonObject& json, QTcpSocket* dest) {
    if (!dest) dest = m_clientTcpSocket;
    if (!dest || dest->state() != QAbstractSocket::ConnectedState) return;
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact) + "\n";
    dest->write(data);
}

void NetworkManager::broadcastTcpJson(const QJsonObject& json, QTcpSocket* exclude) {
    if (m_role != NET_ROLE_HOST) return;
    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact) + "\n";
    for (auto& peer : m_peers) {
        if (peer.tcpSocket && peer.tcpSocket != exclude && peer.tcpSocket->state() == QAbstractSocket::ConnectedState) {
            peer.tcpSocket->write(data);
        }
    }
}

void NetworkManager::onTcpReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) socket = m_clientTcpSocket;
    if (!socket) return;

    while (socket->canReadLine()) {
        QByteArray line = socket->readLine().trimmed();
        if (line.isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject()) {
            processTcpMessage(doc.object(), socket);
        }
    }
}

void NetworkManager::processTcpMessage(const QJsonObject& json, QTcpSocket* senderSocket) {
    QString type = json["type"].toString();

    // 1. Host side handling of AUTH / HELLO
    if ((type == "AUTH" || type == "HELLO") && m_role == NET_ROLE_HOST) {
        QString reqRoomId = json["roomId"].toString().trimmed();
        QString reqPassword = json["password"].toString().trimmed();

        bool roomIdMatches = (reqRoomId.compare(m_roomId, Qt::CaseInsensitive) == 0);
        bool passwordMatches = (m_roomPassword.isEmpty() || reqPassword == m_roomPassword);

        if (!roomIdMatches || !passwordMatches) {
            QString failReason = !roomIdMatches
                ? QString("Room ID '%1' does not exist").arg(reqRoomId)
                : "Incorrect Room Password";
            QJsonObject rej;
            rej["type"] = "AUTH_FAILED";
            rej["reason"] = failReason;
            sendTcpJson(rej, senderSocket);
            senderSocket->flush();
            senderSocket->disconnectFromHost();
            m_pendingClients.removeOne(senderSocket);

            emit statusMessage(QString("⚠️ Access Denied for %1: %2").arg(senderSocket->peerAddress().toString()).arg(failReason), true);
            return;
        }

        m_pendingClients.removeOne(senderSocket);

        if (m_peers.size() >= 7) {
            QJsonObject rej;
            rej["type"] = "ROOM_FULL";
            rej["reason"] = "Room is full (Maximum 8 racers reached)";
            sendTcpJson(rej, senderSocket);
            senderSocket->flush();
            senderSocket->disconnectFromHost();
            return;
        }

        int assignedId = 1;
        while (m_peers.contains(assignedId)) {
            assignedId++;
        }

        RemotePeerInfo info;
        info.peerId = assignedId;
        info.name = json["name"].toString(QString("Racer %1").arg(assignedId + 1));
        info.tcpSocket = senderSocket;
        QHostAddress peerAddr = senderSocket->peerAddress();
        bool ok = false;
        quint32 ip4 = peerAddr.toIPv4Address(&ok);
        if (ok) peerAddr = QHostAddress(ip4);
        info.udpAddress = peerAddr;
        info.udpPort = (quint16)json["udpPort"].toInt(7780 + (assignedId - 1));

        m_peers[assignedId] = info;

        QJsonObject welcome;
        welcome["type"] = "WELCOME";
        welcome["assignedId"] = assignedId;
        welcome["roomId"] = m_roomId;
        welcome["hostName"] = m_localName;
        welcome["udpPort"] = m_localUdpPort;

        QJsonArray rosterArr;
        QJsonObject hostEntry;
        hostEntry["id"] = 0;
        hostEntry["name"] = m_localName;
        hostEntry["car"] = 0;
        hostEntry["ready"] = true;
        rosterArr.append(hostEntry);

        for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
            if (it.key() == assignedId) continue;
            QJsonObject pe;
            pe["id"] = it.key();
            pe["name"] = it.value().name;
            pe["car"] = it.value().carModel;
            pe["ready"] = it.value().isReady;
            rosterArr.append(pe);
        }
        welcome["roster"] = rosterArr;

        sendTcpJson(welcome, senderSocket);

        QJsonObject joinedObj;
        joinedObj["type"] = "PEER_JOINED";
        joinedObj["id"] = assignedId;
        joinedObj["name"] = info.name;
        joinedObj["car"] = info.carModel;
        broadcastTcpJson(joinedObj, senderSocket);

        emit peerJoined(assignedId, info.name);
        emit statusMessage(QString("🔑 Pilot %1 entered Room [%2]!").arg(info.name).arg(m_roomId));
        m_pingTimer->start(1500);
        return;
    }

    if (type == "AUTH_FAILED" && m_role == NET_ROLE_CLIENT) {
        QString reason = json["reason"].toString("Invalid Room ID or Password");
        emit statusMessage(QString("❌ Access Denied: %1").arg(reason), true);
        emit authFailed(reason);
        disconnectSession();
        return;
    }

    if (type == "ROOM_FULL" && m_role == NET_ROLE_CLIENT) {
        QString reason = json["reason"].toString("Room is full");
        emit statusMessage(QString("❌ %1").arg(reason), true);
        emit authFailed(reason);
        disconnectSession();
        return;
    }

    if (type == "WELCOME" && m_role == NET_ROLE_CLIENT) {
        m_myPeerId = json["assignedId"].toInt(1);
        m_roomId = json["roomId"].toString(m_roomId);
        m_hostUdpPort = (quint16)json["udpPort"].toInt(7778);

        // Host is Peer 0
        RemotePeerInfo hostInfo;
        hostInfo.peerId = 0;
        hostInfo.name = json["hostName"].toString("Host Pilot");
        hostInfo.udpAddress = m_clientTcpSocket->peerAddress();
        hostInfo.udpPort = m_hostUdpPort;
        m_peers[0] = hostInfo;

        // Import existing room roster
        QJsonArray rosterArr = json["roster"].toArray();
        for (int i = 0; i < rosterArr.size(); ++i) {
            QJsonObject pe = rosterArr[i].toObject();
            int pid = pe["id"].toInt();
            if (pid == m_myPeerId) continue;
            RemotePeerInfo pInfo;
            pInfo.peerId = pid;
            pInfo.name = pe["name"].toString();
            pInfo.carModel = pe["car"].toInt();
            pInfo.isReady = pe["ready"].toBool();
            pInfo.udpAddress = m_clientTcpSocket->peerAddress();
            pInfo.udpPort = m_hostUdpPort; // Client communicates via Host relay
            m_peers[pid] = pInfo;
        }

        emit peerJoined(0, hostInfo.name);
        emit statusMessage(QString("✅ Joined Room '%1' as Player %2!").arg(m_roomId).arg(m_myPeerId + 1));
        m_pingTimer->start(1500);
        return;
    } else if (type == "PEER_JOINED") {
        int id = json["id"].toInt();
        QString name = json["name"].toString();
        RemotePeerInfo info;
        info.peerId = id;
        info.name = name;
        info.carModel = json["car"].toInt();
        info.udpAddress = m_hostUdpAddress;
        info.udpPort = m_hostUdpPort;
        m_peers[id] = info;

        emit peerJoined(id, name);
        emit statusMessage(QString("Racer %1 joined the lobby!").arg(name));
    } else if (type == "PEER_LEFT") {
        int id = json["id"].toInt();
        QString name = json["name"].toString();
        m_peers.remove(id);
        m_peerTargets.remove(id);
        emit peerLeft(id, name);
    } else if (type == "READY") {
        int id = json["id"].toInt();
        bool ready = json["ready"].toBool();
        if (m_peers.contains(id)) {
            m_peers[id].isReady = ready;
            emit peerReadyChanged(id, ready);
        }
        if (m_role == NET_ROLE_HOST) {
            broadcastTcpJson(json, senderSocket);
        }
    } else if (type == "TRACK_SYNC") {
        int theme = json["theme"].toInt();
        int tod = json["tod"].toInt();
        emit trackSyncRequested(theme, tod);
        if (m_role == NET_ROLE_HOST) {
            broadcastTcpJson(json, senderSocket);
        }
    } else if (type == "CAR_SYNC") {
        int id = json["id"].toInt();
        int car = json["car"].toInt();
        if (m_peers.contains(id)) {
            m_peers[id].carModel = car;
            emit carSyncRequested(id, car);
        }
        if (m_role == NET_ROLE_HOST) {
            broadcastTcpJson(json, senderSocket);
        }
    } else if (type == "COUNTDOWN_START") {
        qreal duration = json["duration"].toDouble(3.5);
        m_countdownTimer = duration;
        m_state = MP_STATE_COUNTDOWN;
        emit countdownStarted(duration);
    } else if (type == "RACE_RESTART") {
        m_state = MP_STATE_LOBBY;
        for (auto& p : m_peers) p.isReady = false;
        emit raceRestarted();
    } else if (type == "CHAT") {
        emit chatMessageReceived(json["sender"].toString(), json["text"].toString());
        if (m_role == NET_ROLE_HOST) {
            broadcastTcpJson(json, senderSocket);
        }
    } else if (type == "PING") {
        QJsonObject pong;
        pong["type"] = "PONG";
        pong["id"] = json["id"].toInt();
        pong["t"] = json["t"].toDouble();
        sendTcpJson(pong, senderSocket);
    } else if (type == "PONG") {
        int id = json["id"].toInt();
        qint64 sendTime = (qint64)json["t"].toDouble();
        if (m_peers.contains(id)) {
            m_peers[id].pingMs = qBound(1LL, m_uptime.elapsed() - sendTime, 999LL);
        }
    }
}

void NetworkManager::onPingTimer() {
    if (isConnected()) {
        if (m_role == NET_ROLE_CLIENT && m_clientTcpSocket) {
            QJsonObject ping;
            ping["type"] = "PING";
            ping["id"] = m_myPeerId;
            ping["t"] = (double)m_uptime.elapsed();
            sendTcpJson(ping, m_clientTcpSocket);
        } else if (m_role == NET_ROLE_HOST) {
            for (auto& peer : m_peers) {
                if (peer.tcpSocket) {
                    QJsonObject ping;
                    ping["type"] = "PING";
                    ping["id"] = 0;
                    ping["t"] = (double)m_uptime.elapsed();
                    sendTcpJson(ping, peer.tcpSocket);
                }
            }
        }
    }
}

void NetworkManager::onBeaconTimer() {
    qint64 now = m_uptime.elapsed();
    for (int i = m_discoveredHosts.size() - 1; i >= 0; --i) {
        if (now - m_discoveredHosts[i].lastSeenMs > 4000) {
            m_discoveredHosts.removeAt(i);
        }
    }

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
    beacon["players"] = getConnectedPlayerCount();
    beacon["max"] = 8;
    beacon["car"] = 0;
    QByteArray data = QJsonDocument(beacon).toJson(QJsonDocument::Compact);
    m_beaconSocket->writeDatagram(data, QHostAddress::Broadcast, BEACON_PORT);
}

void NetworkManager::setReady(bool ready) {
    QJsonObject obj;
    obj["type"] = "READY";
    obj["id"] = m_myPeerId;
    obj["ready"] = ready;
    if (m_role == NET_ROLE_CLIENT) {
        sendTcpJson(obj, m_clientTcpSocket);
    } else if (m_role == NET_ROLE_HOST) {
        broadcastTcpJson(obj);
    }
}

void NetworkManager::notifyTrackChange(int trackTheme, int timeOfDay) {
    if (!isHost()) return;
    QJsonObject obj;
    obj["type"] = "TRACK_SYNC";
    obj["theme"] = trackTheme;
    obj["tod"] = timeOfDay;
    broadcastTcpJson(obj);
}

void NetworkManager::notifyCarChange(int carModel) {
    QJsonObject obj;
    obj["type"] = "CAR_SYNC";
    obj["id"] = m_myPeerId;
    obj["car"] = carModel;
    if (m_role == NET_ROLE_CLIENT) {
        sendTcpJson(obj, m_clientTcpSocket);
    } else if (m_role == NET_ROLE_HOST) {
        broadcastTcpJson(obj);
    }
}

void NetworkManager::startRaceCountdown() {
    if (!isHost()) return;
    m_countdownTimer = 3.5;
    m_state = MP_STATE_COUNTDOWN;

    QJsonObject obj;
    obj["type"] = "COUNTDOWN_START";
    obj["duration"] = 3.5;
    broadcastTcpJson(obj);

    emit countdownStarted(3.5);
}

void NetworkManager::notifyRaceRestart() {
    m_state = MP_STATE_LOBBY;
    for (auto& p : m_peers) p.isReady = false;
    QJsonObject obj;
    obj["type"] = "RACE_RESTART";
    if (m_role == NET_ROLE_HOST) {
        broadcastTcpJson(obj);
    } else {
        sendTcpJson(obj, m_clientTcpSocket);
    }
    emit raceRestarted();
}

void NetworkManager::sendQuickChat(const QString& text) {
    QJsonObject obj;
    obj["type"] = "CHAT";
    obj["sender"] = QString("[P%1] %2").arg(m_myPeerId + 1).arg(m_localName);
    obj["text"] = text;
    if (m_role == NET_ROLE_CLIENT) {
        sendTcpJson(obj, m_clientTcpSocket);
    } else if (m_role == NET_ROLE_HOST) {
        broadcastTcpJson(obj);
        emit chatMessageReceived(obj["sender"].toString(), text);
    }
}

void NetworkManager::sendTelemetry(const PlayerCar& player) {
    if (!m_udpSocket || !isConnected()) return;

    TelemetryPacket pkt;
    pkt.magic = APEX_MAGIC;
    pkt.peerId = (uint8_t)qMax(0, m_myPeerId);
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

    if (m_role == NET_ROLE_CLIENT) {
        // Send to host
        m_udpSocket->writeDatagram((const char*)&pkt, sizeof(pkt), m_hostUdpAddress, m_hostUdpPort);
    } else if (m_role == NET_ROLE_HOST) {
        // Broadcast to all connected clients
        for (const auto& peer : m_peers) {
            if (!peer.udpAddress.isNull()) {
                m_udpSocket->writeDatagram((const char*)&pkt, sizeof(pkt), peer.udpAddress, peer.udpPort);
            }
        }
    }
}

void NetworkManager::onUdpReadyRead() {
    if (!m_udpSocket) return;
    while (m_udpSocket->hasPendingDatagrams()) {
        TelemetryPacket pkt;
        QHostAddress sender;
        quint16 senderPort;
        qint64 bytes = m_udpSocket->readDatagram((char*)&pkt, sizeof(pkt), &sender, &senderPort);
        if (bytes == sizeof(TelemetryPacket) && pkt.magic == APEX_MAGIC) {
            int pid = (int)pkt.peerId;

            // Host auto-learns client UDP port if received
            if (m_role == NET_ROLE_HOST && m_peers.contains(pid)) {
                m_peers[pid].udpAddress = sender;
                m_peers[pid].udpPort = senderPort;

                // Relay client packet to all OTHER clients!
                for (const auto& peer : m_peers) {
                    if (peer.peerId != pid && !peer.udpAddress.isNull()) {
                        m_udpSocket->writeDatagram((const char*)&pkt, sizeof(pkt), peer.udpAddress, peer.udpPort);
                    }
                }
            }

            // Update peer target coordinates
            PeerInterpTarget& tgt = m_peerTargets[pid];
            tgt.targetZ = pkt.worldZ;
            tgt.targetX = pkt.x;
            tgt.targetSpeed = pkt.speed;
            tgt.targetSteer = pkt.steerAngle;
            tgt.targetPitchG = pkt.pitchG;
            tgt.targetRollG = pkt.rollG;
            tgt.targetSkid = pkt.skidAngle;
            tgt.carModel = pkt.carModel;
            tgt.isBraking = (pkt.flags & 0x01) != 0;
            tgt.isCrashing = (pkt.flags & 0x02) != 0;
            tgt.hazardLights = (pkt.flags & 0x04) != 0;
            tgt.horn = (pkt.flags & 0x08) != 0;
            tgt.hasFinished = (pkt.flags & 0x10) != 0;
            tgt.finishTime = pkt.finishTime;
            tgt.health = pkt.health;
            tgt.lastPacketTimeMs = m_uptime.elapsed();
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

    // 2. Synchronize all connected Remote Player Cars into GameEngine
    if (isConnected()) {
        for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
            int pid = it.key();
            if (pid == m_myPeerId) continue;

            if (m_peerTargets.contains(pid)) {
                const PeerInterpTarget& tgt = m_peerTargets[pid];
                qreal dtSincePacket = (m_uptime.elapsed() - tgt.lastPacketTimeMs) / 1000.0;
                if (dtSincePacket < 0.8) {
                    // Dead reckoning extrapolation forward
                    qreal extrapolatedZ = tgt.targetZ + (tgt.targetSpeed / 3.6) * dtSincePacket * 62.0;

                    NPCCar remoteCar;
                    remoteCar.id = 9000 + pid;
                    remoteCar.isRemotePlayer = true;
                    remoteCar.racerName = it.value().name;
                    remoteCar.worldZ = extrapolatedZ;
                    remoteCar.x = tgt.targetX;
                    remoteCar.targetX = tgt.targetX;
                    remoteCar.speed = tgt.targetSpeed;
                    remoteCar.maxSpeed = 200.0;
                    remoteCar.isBraking = tgt.isBraking;
                    remoteCar.isCrashing = tgt.isCrashing;
                    remoteCar.hazardLights = tgt.hazardLights;
                    remoteCar.hasFinished = tgt.hasFinished;
                    remoteCar.finishTime = tgt.finishTime;

                    switch (tgt.carModel) {
                        case 0: remoteCar.type = VEHICLE_SEDAN_SILVER; break;  // APEX GT-R (Gunmetal Titanium, Swan-Neck Wing, Gold Stripes)
                        case 1: remoteCar.type = VEHICLE_SPORTS_RED;   break;  // VORTEX V12 (Rosso Corsa Crimson, Le Mans Shark Fin, Jet Exhausts)
                        case 2: remoteCar.type = VEHICLE_COMPACT_BLUE; break;  // DRIFT SPEC-R (Solar Amber-Gold, High Drift Wing, Blast Pipes, Underglow)
                        case 3: remoteCar.type = VEHICLE_SUV_BLACK;    break;  // TITAN ENFORCER (Tactical Interceptor, Roof Strobe, Window Louvers)
                        default: remoteCar.type = VEHICLE_SEDAN_SILVER; break;
                    }

                    engine.setRemotePlayerCar(pid, remoteCar);
                }
            }
        }
    }
}
