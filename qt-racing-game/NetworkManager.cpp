#include "NetworkManager.h"
#include <QNetworkDatagram>
#include <QRandomGenerator>

static const uint32_t APEX_MAGIC = 0x41504558; // "APEX"
static const quint16 BEACON_PORT = 7779;

QHostAddress NetworkManager::normalizeIPv4(const QHostAddress& addr) {
    if (addr.isNull()) return QHostAddress::LocalHost;
    if (addr == QHostAddress::LocalHost || addr == QHostAddress::LocalHostIPv6) {
        return QHostAddress::LocalHost;
    }
    bool ok = false;
    quint32 ip4 = addr.toIPv4Address(&ok);
    if (ok && ip4 != 0) {
        return QHostAddress(ip4);
    }
    QString str = addr.toString();
    if (str.startsWith("::ffff:")) {
        QHostAddress mapped(str.mid(7));
        if (!mapped.isNull()) return mapped;
    }
    if (str == "::1") {
        return QHostAddress::LocalHost;
    }
    return addr;
}

void NetworkManager::getGridSlotPosition(int peerId, qreal& outX, qreal& outZ) {
    static const qreal kGridLanes[4] = { -0.25, 0.25, -0.75, 0.75 };
    int safeId = qMax(0, peerId);
    outX = kGridLanes[safeId % 4];
    int row = safeId / 4;
    outZ = -row * 160.0;
}

NetworkManager::NetworkManager() {
    m_uptime.start();
    m_instanceId = QRandomGenerator::global()->generate64();

    // Default racer callsign
    QStringList names = { "VeloRacer", "ApexDrifter", "TurboNomad", "CyberGhost", "PhantomPilot", "NitroWolf" };
    m_localName = names[QRandomGenerator::global()->generate() % names.size()] + QString::number(QRandomGenerator::global()->generate() % 90 + 10);

    m_beaconTimer = new QTimer(this);
    connect(m_beaconTimer, &QTimer::timeout, this, &NetworkManager::onBeaconTimer);

    m_pingTimer = new QTimer(this);
    connect(m_pingTimer, &QTimer::timeout, this, &NetworkManager::onPingTimer);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    connect(m_searchTimer, &QTimer::timeout, this, &NetworkManager::onSearchTimeout);

    // Initialize UDP beacon socket for zero-IP Room ID discovery across local machine & LAN
    m_beaconSocket = new QUdpSocket(this);
    m_beaconSocket->bind(QHostAddress::AnyIPv4, BEACON_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(m_beaconSocket, &QUdpSocket::readyRead, this, [this]() {
        while (m_beaconSocket->hasPendingDatagrams()) {
            QNetworkDatagram datagram = m_beaconSocket->receiveDatagram();
            QByteArray data = datagram.data();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isObject()) continue;
            QJsonObject obj = doc.object();
            QString type = obj["type"].toString();
            QHostAddress senderAddr = normalizeIPv4(datagram.senderAddress());

            if (type == "APEX_BEACON") {
                handleBeaconJson(obj, senderAddr);
            } else if (type == "ROOM_QUERY" && m_role == NET_ROLE_HOST && m_state != MP_STATE_DISCONNECTED) {
                QString reqRoom = obj["roomId"].toString().trimmed();
                if (reqRoom.isEmpty() || reqRoom.compare(m_roomId, Qt::CaseInsensitive) == 0) {
                    quint16 replyPort = (quint16)obj["replyPort"].toInt(datagram.senderPort());
                    sendBeaconBroadcast(senderAddr, replyPort);
                }
            }
        }
    });

    m_beaconTimer->start(450);
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

void NetworkManager::initPeerTargetOnGrid(int peerId, int carModel) {
    qreal gx = 0.0, gz = 0.0;
    getGridSlotPosition(peerId, gx, gz);
    PeerInterpTarget& tgt = m_peerTargets[peerId];
    tgt.targetX = gx;
    tgt.targetZ = gz;
    tgt.targetSpeed = 0.0;
    tgt.targetSteer = 0.0;
    tgt.carModel = carModel;
    tgt.hasFinished = false;
    tgt.finishTime = 0.0;
    tgt.lastPacketTimeMs = m_uptime.elapsed();
}

void NetworkManager::resetPeersToStartingGrid(GameEngine& engine) {
    if (!isConnected()) return;

    qreal myX = 0.25, myZ = 0.0;
    getGridSlotPosition(m_myPeerId, myX, myZ);
    engine.getPlayerMutable().x = myX;
    engine.getPlayerMutable().worldZ = myZ;
    engine.getPlayerMutable().speed = 0.0;

    for (auto it = m_peers.begin(); it != m_peers.end(); ++it) {
        int pid = it.key();
        if (pid == m_myPeerId) continue;
        initPeerTargetOnGrid(pid, it.value().carModel);

        const PeerInterpTarget& tgt = m_peerTargets[pid];
        NPCCar remoteCar;
        remoteCar.id = 9000 + pid;
        remoteCar.isRemotePlayer = true;
        remoteCar.racerName = it.value().name;
        remoteCar.worldZ = tgt.targetZ;
        remoteCar.x = tgt.targetX;
        remoteCar.targetX = tgt.targetX;
        remoteCar.speed = 0.0;
        remoteCar.maxSpeed = 200.0;
        switch (it.value().carModel) {
            case 0: remoteCar.type = VEHICLE_SEDAN_SILVER; break;
            case 1: remoteCar.type = VEHICLE_SPORTS_RED;   break;
            case 2: remoteCar.type = VEHICLE_COMPACT_BLUE; break;
            case 3: remoteCar.type = VEHICLE_SUV_BLACK;    break;
            default: remoteCar.type = VEHICLE_SEDAN_SILVER; break;
        }
        engine.setRemotePlayerCar(pid, remoteCar);
    }
}

bool NetworkManager::startHostRoom(const QString& roomId, const QString& password, quint16 tcpPort, quint16 udpPort) {
    disconnectSession();

    m_roomId = roomId.trimmed().isEmpty() ? "APEX-101" : roomId.trimmed().toUpper();
    m_roomPassword = password.trimmed();
    m_localTcpPort = tcpPort;
    m_localUdpPort = udpPort;
    m_myPeerId = 0; // Host is always Player 1 / Peer 0
    m_localReady = true;

    m_tcpServer = new QTcpServer(this);
    bool tcpBound = m_tcpServer->listen(QHostAddress::AnyIPv4, m_localTcpPort);
    if (!tcpBound && tcpPort == 7777) {
        // Fallback port scan so multiple rooms can be hosted on the same machine without conflict
        for (quint16 p = 7777; p <= 7786; ++p) {
            if (m_tcpServer->listen(QHostAddress::AnyIPv4, p)) {
                m_localTcpPort = p;
                tcpBound = true;
                break;
            }
        }
    }
    if (!tcpBound) {
        emit statusMessage(QString("Failed to bind TCP server on port %1").arg(m_localTcpPort), true);
        delete m_tcpServer;
        m_tcpServer = nullptr;
        return false;
    }
    connect(m_tcpServer, &QTcpServer::newConnection, this, &NetworkManager::onNewTcpConnection);

    m_udpSocket = new QUdpSocket(this);
    bool udpBound = false;
    for (int offset = 0; offset < 10; ++offset) {
        if (m_udpSocket->bind(QHostAddress::AnyIPv4, udpPort + offset)) {
            m_localUdpPort = m_udpSocket->localPort();
            udpBound = true;
            break;
        }
    }
    if (!udpBound) {
        m_udpSocket->bind(QHostAddress::AnyIPv4, 0);
        m_localUdpPort = m_udpSocket->localPort();
    }
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onUdpReadyRead);

    m_role = NET_ROLE_HOST;
    m_state = MP_STATE_LOBBY;
    m_peers.clear();
    m_peerTargets.clear();
    m_pendingClients.clear();

    sendBeaconBroadcast();

    QString passInfo = m_roomPassword.isEmpty() ? "Open Access" : "Password Protected";
    emit statusMessage(QString("HOSTED ROOM [%1] - %2").arg(m_roomId).arg(passInfo));
    return true;
}

bool NetworkManager::startHost(quint16 tcpPort, quint16 udpPort) {
    return startHostRoom(m_roomId.isEmpty() ? "APEX-101" : m_roomId, m_roomPassword, tcpPort, udpPort);
}

bool NetworkManager::joinRoom(const QString& roomId, const QString& password) {
    return joinCustomRoom(QString(), roomId, password, 7777, 0);
}

bool NetworkManager::joinCustomRoom(const QString& hostAddress, const QString& roomId, const QString& password, quint16 tcpPort, quint16 udpPort) {
    disconnectSession();

    m_roomId = roomId.trimmed().isEmpty() ? "APEX-101" : roomId.trimmed().toUpper();
    m_roomPassword = password.trimmed();
    m_myPeerId = -1;
    m_localReady = false;
    m_isSearchingRoom = true;
    m_lastRejectReason.clear();
    m_probedEndpoints.clear();

    m_udpSocket = new QUdpSocket(this);
    bool bound = false;
    if (udpPort > 0 && udpPort != BEACON_PORT && udpPort != 7778) {
        bound = m_udpSocket->bind(QHostAddress::AnyIPv4, udpPort);
    }
    if (!bound) {
        m_udpSocket->bind(QHostAddress::AnyIPv4, 0);
    }
    m_localUdpPort = m_udpSocket->localPort();
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onUdpReadyRead);

    emit statusMessage(QString("LOCATING ROOM [%1]...").arg(m_roomId));

    // 1. Broadcast ROOM_QUERY over UDP to local loopback & LAN so hosts respond immediately
    sendRoomQueryBroadcast();

    // 2. If a matching Room ID is already in discovered hosts, probe it immediately
    for (const DiscoveredHost& h : m_discoveredHosts) {
        if (h.roomId.compare(m_roomId, Qt::CaseInsensitive) == 0) {
            probeCandidateEndpoint(h.address, h.port);
            if (QNetworkInterface::allAddresses().contains(h.address)) {
                probeCandidateEndpoint(QHostAddress::LocalHost, h.port);
            }
        }
    }

    // 3. If an explicit hostAddress was provided, probe it directly
    QString trimmedHost = hostAddress.trimmed();
    if (!trimmedHost.isEmpty() && trimmedHost.compare("AUTO", Qt::CaseInsensitive) != 0) {
        probeCandidateEndpoint(QHostAddress(trimmedHost), tcpPort);
    }

    // 4. Always probe local machine ports 7777..7782 for zero-config single-PC & local rooms
    for (quint16 p = 7777; p <= 7782; ++p) {
        probeCandidateEndpoint(QHostAddress::LocalHost, p);
    }

    // 5. Also probe any other discovered LAN hosts
    for (const DiscoveredHost& h : m_discoveredHosts) {
        probeCandidateEndpoint(h.address, h.port);
    }

    m_searchTimer->start(2500);
    return true;
}

bool NetworkManager::connectToHost(const QString& hostAddress, quint16 tcpPort, quint16 udpPort) {
    return joinCustomRoom(hostAddress, m_roomId.isEmpty() ? "APEX-101" : m_roomId, m_roomPassword, tcpPort, udpPort);
}

void NetworkManager::probeCandidateEndpoint(const QHostAddress& address, quint16 port) {
    if (!m_isSearchingRoom || isConnected()) return;
    QHostAddress ipv4 = normalizeIPv4(address);
    if (ipv4.isNull() || port == 0) return;

    QString key = QString("%1:%2").arg(ipv4.toString()).arg(port);
    if (m_probedEndpoints.contains(key)) return;
    m_probedEndpoints.insert(key);

    QTcpSocket* sock = new QTcpSocket(this);
    m_probeSockets.append(sock);

    connect(sock, &QTcpSocket::connected, this, [this, sock]() {
        if (!m_isSearchingRoom || isConnected()) return;

        QJsonObject auth;
        auth["type"] = "AUTH";
        auth["roomId"] = m_roomId;
        auth["password"] = m_roomPassword;
        auth["name"] = m_localName;
        auth["carModel"] = m_localCarModel;
        auth["udpPort"] = m_localUdpPort;
        sendTcpJson(auth, sock);
    });

    connect(sock, &QTcpSocket::readyRead, this, &NetworkManager::onTcpReadyRead);
    connect(sock, &QTcpSocket::disconnected, this, &NetworkManager::onTcpDisconnected);
    connect(sock, &QTcpSocket::errorOccurred, this, [this, sock](QAbstractSocket::SocketError) {
        if (sock == m_clientTcpSocket && m_state != MP_STATE_DISCONNECTED) {
            QString err = sock->errorString();
            emit statusMessage(QString("CONNECTION ERROR: %1").arg(err), true);
            emit authFailed(err);
            return;
        }
        if (m_probeSockets.contains(sock)) {
            m_probeSockets.removeOne(sock);
            sock->deleteLater();
        }
    });

    sock->connectToHost(ipv4, port);
}

void NetworkManager::cleanupProbeSockets(QTcpSocket* keepSocket) {
    for (QTcpSocket* s : m_probeSockets) {
        if (s && s != keepSocket) {
            s->disconnect(this);
            s->abort();
            s->deleteLater();
        }
    }
    m_probeSockets.clear();
    m_probedEndpoints.clear();
}

void NetworkManager::onSearchTimeout() {
    if (!m_isSearchingRoom || isConnected()) return;
    m_isSearchingRoom = false;
    cleanupProbeSockets(nullptr);

    QString reason = m_lastRejectReason.isEmpty()
        ? QString("Room '%1' not found on this machine or local network").arg(m_roomId)
        : m_lastRejectReason;
    emit statusMessage(QString("JOIN FAILED: %1").arg(reason), true);
    emit authFailed(reason);
    disconnectSession();
}

void NetworkManager::disconnectSession() {
    m_pingTimer->stop();
    if (m_searchTimer) m_searchTimer->stop();
    m_isSearchingRoom = false;

    cleanupProbeSockets(nullptr);

    for (QTcpSocket* s : m_pendingClients) {
        if (s) {
            s->disconnect(this);
            s->disconnectFromHost();
            s->deleteLater();
        }
    }
    m_pendingClients.clear();

    if (m_clientTcpSocket) {
        m_clientTcpSocket->disconnect(this);
        m_clientTcpSocket->disconnectFromHost();
        m_clientTcpSocket->deleteLater();
        m_clientTcpSocket = nullptr;
    }
    if (m_tcpServer) {
        for (auto& peer : m_peers) {
            if (peer.tcpSocket) {
                peer.tcpSocket->disconnect(this);
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
    m_localReady = false;
    m_peers.clear();
    m_peerTargets.clear();
    m_countdownTimer = -1.0;
}

void NetworkManager::onNewTcpConnection() {
    if (!m_tcpServer) return;
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket* newSocket = m_tcpServer->nextPendingConnection();
        if (!newSocket) continue;

        if (m_peers.size() >= 7) {
            QJsonObject rej;
            rej["type"] = "ROOM_FULL";
            rej["reason"] = "Room is full (Maximum 8 racers reached)";
            QByteArray data = QJsonDocument(rej).toJson(QJsonDocument::Compact) + "\n";
            newSocket->write(data);
            newSocket->flush();
            newSocket->disconnectFromHost();
            newSocket->deleteLater();
            continue;
        }

        m_pendingClients.append(newSocket);
        connect(newSocket, &QTcpSocket::readyRead, this, &NetworkManager::onTcpReadyRead);
        connect(newSocket, &QTcpSocket::disconnected, this, &NetworkManager::onTcpDisconnected);
    }
}

void NetworkManager::onTcpDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    if (m_probeSockets.contains(socket)) {
        m_probeSockets.removeOne(socket);
        socket->deleteLater();
        return;
    }

    if (m_pendingClients.contains(socket)) {
        m_pendingClients.removeOne(socket);
        socket->deleteLater();
        return;
    }

    if (m_role == NET_ROLE_CLIENT && socket == m_clientTcpSocket) {
        emit statusMessage("DISCONNECTED FROM ROOM");
        disconnectSession();
        return;
    }

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

        QJsonObject leftObj;
        leftObj["type"] = "PEER_LEFT";
        leftObj["id"] = disconnectedId;
        leftObj["name"] = disconnectedName;
        broadcastTcpJson(leftObj);

        emit peerLeft(disconnectedId, disconnectedName);
        emit statusMessage(QString("DRIVER %1 LEFT ROOM [%2]").arg(disconnectedName).arg(m_roomId));
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

        bool roomIdMatches = (type == "HELLO") || (reqRoomId.compare(m_roomId, Qt::CaseInsensitive) == 0);
        bool passwordMatches = (m_roomPassword.isEmpty() || reqPassword == m_roomPassword);

        if (!roomIdMatches || !passwordMatches) {
            QString failReason = !roomIdMatches
                ? QString("Room ID '%1' does not exist").arg(reqRoomId)
                : "Incorrect Room Password";
            QJsonObject rej;
            rej["type"] = "AUTH_FAILED";
            rej["code"] = !roomIdMatches ? "WRONG_ROOM" : "WRONG_PASSWORD";
            rej["reason"] = failReason;
            sendTcpJson(rej, senderSocket);
            senderSocket->flush();
            senderSocket->disconnectFromHost();
            m_pendingClients.removeOne(senderSocket);

            if (roomIdMatches) {
                emit statusMessage(QString("ACCESS DENIED: %1").arg(failReason), true);
            }
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
        info.carModel = json["carModel"].toInt(json["car"].toInt(0));
        info.tcpSocket = senderSocket;
        info.udpAddress = normalizeIPv4(senderSocket->peerAddress());
        info.udpPort = (quint16)json["udpPort"].toInt(7780 + (assignedId - 1));

        m_peers[assignedId] = info;
        initPeerTargetOnGrid(assignedId, info.carModel);

        QJsonObject welcome;
        welcome["type"] = "WELCOME";
        welcome["assignedId"] = assignedId;
        welcome["peerId"] = assignedId;
        welcome["roomId"] = m_roomId;
        welcome["hostName"] = m_localName;
        welcome["udpPort"] = m_localUdpPort;
        welcome["trackTheme"] = m_hostTrackTheme;
        welcome["timeOfDay"] = m_hostTimeOfDay;

        QJsonArray rosterArr;
        QJsonObject hostEntry;
        hostEntry["id"] = 0;
        hostEntry["name"] = m_localName;
        hostEntry["car"] = m_localCarModel;
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
        emit carSyncRequested(assignedId, info.carModel);
        emit statusMessage(QString("DRIVER %1 JOINED ROOM [%2]").arg(info.name).arg(m_roomId));
        m_pingTimer->start(1500);
        sendBeaconBroadcast();
        return;
    }

    if (type == "AUTH_FAILED" && (m_isSearchingRoom || m_role == NET_ROLE_CLIENT)) {
        QString reason = json["reason"].toString("Invalid Room ID or Password");
        QString code = json["code"].toString();
        m_lastRejectReason = reason;

        if (code == "WRONG_PASSWORD") {
            m_isSearchingRoom = false;
            if (m_searchTimer) m_searchTimer->stop();
            cleanupProbeSockets(nullptr);
            emit statusMessage(QString("ACCESS DENIED: %1").arg(reason), true);
            emit authFailed(reason);
            disconnectSession();
            return;
        }

        if (m_probeSockets.contains(senderSocket)) {
            m_probeSockets.removeOne(senderSocket);
            senderSocket->disconnect(this);
            senderSocket->abort();
            senderSocket->deleteLater();
        }
        return;
    }

    if (type == "ROOM_FULL" && (m_isSearchingRoom || m_role == NET_ROLE_CLIENT)) {
        QString reason = json["reason"].toString("Room is full");
        m_isSearchingRoom = false;
        if (m_searchTimer) m_searchTimer->stop();
        cleanupProbeSockets(nullptr);
        emit statusMessage(QString("ROOM FULL: %1").arg(reason), true);
        emit authFailed(reason);
        disconnectSession();
        return;
    }

    if (type == "WELCOME" && (m_isSearchingRoom || m_role == NET_ROLE_CLIENT)) {
        m_isSearchingRoom = false;
        if (m_searchTimer) m_searchTimer->stop();

        if (m_probeSockets.contains(senderSocket)) {
            m_probeSockets.removeOne(senderSocket);
            cleanupProbeSockets(senderSocket);
            m_clientTcpSocket = senderSocket;
        }

        m_role = NET_ROLE_CLIENT;
        m_state = MP_STATE_LOBBY;
        m_myPeerId = json["assignedId"].toInt(json["peerId"].toInt(1));
        m_roomId = json["roomId"].toString(m_roomId);
        m_hostUdpPort = (quint16)json["udpPort"].toInt(7778);
        m_hostUdpAddress = normalizeIPv4(m_clientTcpSocket ? m_clientTcpSocket->peerAddress() : QHostAddress::LocalHost);

        m_peers.clear();
        m_peerTargets.clear();

        // Host is Peer 0
        RemotePeerInfo hostInfo;
        hostInfo.peerId = 0;
        hostInfo.name = json["hostName"].toString("Host Driver");
        hostInfo.udpAddress = m_hostUdpAddress;
        hostInfo.udpPort = m_hostUdpPort;
        hostInfo.isReady = true;
        m_peers[0] = hostInfo;

        // Import existing room roster
        QJsonArray rosterArr = json["roster"].toArray();
        for (int i = 0; i < rosterArr.size(); ++i) {
            QJsonObject pe = rosterArr[i].toObject();
            int pid = pe["id"].toInt();
            if (pid == m_myPeerId) continue;
            RemotePeerInfo pInfo;
            pInfo.peerId = pid;
            pInfo.name = pe["name"].toString(pid == 0 ? hostInfo.name : QString("Driver %1").arg(pid + 1));
            pInfo.carModel = pe["car"].toInt(0);
            pInfo.isReady = pe["ready"].toBool(pid == 0);
            pInfo.udpAddress = m_hostUdpAddress;
            pInfo.udpPort = m_hostUdpPort;
            m_peers[pid] = pInfo;
            initPeerTargetOnGrid(pid, pInfo.carModel);
        }
        if (!m_peerTargets.contains(0)) {
            initPeerTargetOnGrid(0, m_peers[0].carModel);
        }

        if (json.contains("trackTheme") && json.contains("timeOfDay")) {
            emit trackSyncRequested(json["trackTheme"].toInt(0), json["timeOfDay"].toInt(1));
        }

        emit peerJoined(0, m_peers[0].name);
        emit statusMessage(QString("JOINED ROOM [%1] AS DRIVER P%2").arg(m_roomId).arg(m_myPeerId + 1));
        m_pingTimer->start(1500);
        return;
    } else if (type == "PEER_JOINED") {
        int id = json["id"].toInt();
        if (id == m_myPeerId) return;
        QString name = json["name"].toString();
        int car = json["car"].toInt(0);
        RemotePeerInfo info;
        info.peerId = id;
        info.name = name;
        info.carModel = car;
        info.udpAddress = m_hostUdpAddress;
        info.udpPort = m_hostUdpPort;
        m_peers[id] = info;
        initPeerTargetOnGrid(id, car);

        emit peerJoined(id, name);
        emit carSyncRequested(id, car);
        emit statusMessage(QString("DRIVER %1 JOINED LOBBY").arg(name));
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
        m_hostTrackTheme = theme;
        m_hostTimeOfDay = tod;
        emit trackSyncRequested(theme, tod);
        if (m_role == NET_ROLE_HOST) {
            broadcastTcpJson(json, senderSocket);
        }
    } else if (type == "CAR_SYNC") {
        int id = json["id"].toInt();
        int car = json["car"].toInt();
        if (m_peers.contains(id)) {
            m_peers[id].carModel = car;
            if (m_peerTargets.contains(id)) {
                m_peerTargets[id].carModel = car;
            }
            emit carSyncRequested(id, car);
        }
        if (m_role == NET_ROLE_HOST) {
            broadcastTcpJson(json, senderSocket);
        }
    } else if (type == "TELEM") {
        int pid = json["id"].toInt(-1);
        if (pid >= 0 && pid != m_myPeerId) {
            PeerInterpTarget& tgt = m_peerTargets[pid];
            tgt.targetZ = json["z"].toDouble();
            tgt.targetX = json["x"].toDouble();
            tgt.targetSpeed = json["s"].toDouble();
            tgt.targetSteer = json["st"].toDouble();
            tgt.carModel = json["c"].toInt();
            int flags = json["f"].toInt();
            tgt.isBraking = (flags & 0x01) != 0;
            tgt.isCrashing = (flags & 0x02) != 0;
            tgt.hazardLights = (flags & 0x04) != 0;
            tgt.horn = (flags & 0x08) != 0;
            tgt.hasFinished = (flags & 0x10) != 0;
            tgt.finishTime = json["ft"].toDouble();
            tgt.health = json["hp"].toDouble(100.0);
            tgt.lastPacketTimeMs = m_uptime.elapsed();

            if (m_role == NET_ROLE_HOST) {
                broadcastTcpJson(json, senderSocket);
            }
        }
    } else if (type == "COUNTDOWN_START") {
        if (json.contains("theme") && json.contains("tod")) {
            emit trackSyncRequested(json["theme"].toInt(0), json["tod"].toInt(1));
        }
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
        pong["id"] = m_myPeerId;
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

void NetworkManager::handleBeaconJson(const QJsonObject& obj, const QHostAddress& senderAddr) {
    quint64 senderInstance = obj["instanceId"].toString().toULongLong();
    if (senderInstance != 0 && senderInstance == m_instanceId) {
        return; // Ignore our own process instance
    }
    if (m_role == NET_ROLE_HOST) {
        return;
    }

    QString roomId = obj["roomId"].toString("APEX-101").trimmed().toUpper();
    QString hostName = obj["name"].toString("Host Driver");
    quint16 port = (quint16)obj["port"].toInt(7777);
    quint16 udpPort = (quint16)obj["udpPort"].toInt(7778);
    QHostAddress normAddr = normalizeIPv4(senderAddr);

    bool found = false;
    qint64 now = m_uptime.elapsed();
    for (DiscoveredHost& h : m_discoveredHosts) {
        if (h.roomId.compare(roomId, Qt::CaseInsensitive) == 0 && h.port == port) {
            h.hostName = hostName;
            h.address = normAddr;
            h.udpPort = udpPort;
            h.playerCount = obj["players"].toInt(1);
            h.maxPlayers = obj["max"].toInt(8);
            h.trackTheme = obj["track"].toInt(0);
            h.timeOfDay = obj["tod"].toInt(1);
            h.carModel = obj["car"].toInt(0);
            h.passwordProtected = obj["protected"].toBool(false);
            h.lastSeenMs = now;
            found = true;
            break;
        }
    }

    if (!found) {
        DiscoveredHost newHost;
        newHost.roomId = roomId;
        newHost.hostName = hostName;
        newHost.address = normAddr;
        newHost.port = port;
        newHost.udpPort = udpPort;
        newHost.playerCount = obj["players"].toInt(1);
        newHost.maxPlayers = obj["max"].toInt(8);
        newHost.trackTheme = obj["track"].toInt(0);
        newHost.timeOfDay = obj["tod"].toInt(1);
        newHost.carModel = obj["car"].toInt(0);
        newHost.passwordProtected = obj["protected"].toBool(false);
        newHost.lastSeenMs = now;
        m_discoveredHosts.append(newHost);
        emit hostDiscovered(newHost);
    }

    // If we are actively searching for this Room ID, connect immediately!
    if (m_isSearchingRoom && !isConnected() && roomId.compare(m_roomId, Qt::CaseInsensitive) == 0) {
        probeCandidateEndpoint(normAddr, port);
        if (QNetworkInterface::allAddresses().contains(normAddr)) {
            probeCandidateEndpoint(QHostAddress::LocalHost, port);
        }
    }
}

void NetworkManager::onBeaconTimer() {
    qint64 now = m_uptime.elapsed();
    for (int i = m_discoveredHosts.size() - 1; i >= 0; --i) {
        if (now - m_discoveredHosts[i].lastSeenMs > 4500) {
            m_discoveredHosts.removeAt(i);
        }
    }

    if (m_role == NET_ROLE_HOST && m_state != MP_STATE_DISCONNECTED) {
        sendBeaconBroadcast();
    } else if (m_isSearchingRoom && !isConnected()) {
        sendRoomQueryBroadcast();
    }
}

void NetworkManager::sendBeaconBroadcast(const QHostAddress& directTarget, quint16 directPort) {
    if (!m_beaconSocket && !m_udpSocket) return;
    QJsonObject beacon;
    beacon["type"] = "APEX_BEACON";
    beacon["instanceId"] = QString::number(m_instanceId);
    beacon["roomId"] = m_roomId;
    beacon["name"] = m_localName;
    beacon["port"] = m_localTcpPort;
    beacon["udpPort"] = m_localUdpPort;
    beacon["players"] = getConnectedPlayerCount();
    beacon["max"] = 8;
    beacon["track"] = m_hostTrackTheme;
    beacon["tod"] = m_hostTimeOfDay;
    beacon["car"] = m_localCarModel;
    beacon["protected"] = isPasswordProtected();
    QByteArray data = QJsonDocument(beacon).toJson(QJsonDocument::Compact);

    QUdpSocket* senderSock = m_udpSocket ? m_udpSocket : m_beaconSocket;

    if (!directTarget.isNull() && directPort > 0) {
        senderSock->writeDatagram(data, normalizeIPv4(directTarget), directPort);
    }

    senderSock->writeDatagram(data, QHostAddress::LocalHost, BEACON_PORT);
    senderSock->writeDatagram(data, QHostAddress::Broadcast, BEACON_PORT);

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning)) continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            QHostAddress bcast = entry.broadcast();
            if (!bcast.isNull() && entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                senderSock->writeDatagram(data, bcast, BEACON_PORT);
            }
        }
    }
}

void NetworkManager::sendRoomQueryBroadcast() {
    QUdpSocket* sock = m_udpSocket ? m_udpSocket : m_beaconSocket;
    if (!sock) return;

    QJsonObject query;
    query["type"] = "ROOM_QUERY";
    query["instanceId"] = QString::number(m_instanceId);
    query["roomId"] = m_roomId;
    query["replyPort"] = (int)sock->localPort();
    QByteArray data = QJsonDocument(query).toJson(QJsonDocument::Compact);

    // Send query to BEACON_PORT and host UDP ports on localhost and LAN broadcast
    for (quint16 port : { BEACON_PORT, (quint16)7778, (quint16)7780, (quint16)7781 }) {
        sock->writeDatagram(data, QHostAddress::LocalHost, port);
        sock->writeDatagram(data, QHostAddress::Broadcast, port);
    }

    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning)) continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            QHostAddress bcast = entry.broadcast();
            if (!bcast.isNull() && entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                sock->writeDatagram(data, bcast, BEACON_PORT);
                sock->writeDatagram(data, bcast, 7778);
            }
        }
    }
}

void NetworkManager::setReady(bool ready) {
    m_localReady = ready;
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
    m_hostTrackTheme = trackTheme;
    m_hostTimeOfDay = timeOfDay;
    if (!isHost()) return;
    QJsonObject obj;
    obj["type"] = "TRACK_SYNC";
    obj["theme"] = trackTheme;
    obj["tod"] = timeOfDay;
    broadcastTcpJson(obj);
}

void NetworkManager::notifyCarChange(int carModel) {
    m_localCarModel = carModel;
    if (!isConnected()) return;
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
    obj["theme"] = m_hostTrackTheme;
    obj["tod"] = m_hostTimeOfDay;
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
    if (!isConnected()) return;
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
    if (!isConnected()) return;
    m_localCarModel = player.selectedCar;

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

    if (m_udpSocket) {
        if (m_role == NET_ROLE_CLIENT) {
            m_udpSocket->writeDatagram((const char*)&pkt, sizeof(pkt), m_hostUdpAddress, m_hostUdpPort);
        } else if (m_role == NET_ROLE_HOST) {
            for (const auto& peer : m_peers) {
                if (!peer.udpAddress.isNull()) {
                    m_udpSocket->writeDatagram((const char*)&pkt, sizeof(pkt), peer.udpAddress, peer.udpPort);
                }
            }
        }
    }

    // Reliable 20Hz TCP telemetry relay so cars always sync even if UDP is filtered
    qint64 nowMs = m_uptime.elapsed();
    if (nowMs - m_lastTcpTelemetryMs >= 45) {
        m_lastTcpTelemetryMs = nowMs;
        QJsonObject tObj;
        tObj["type"] = "TELEM";
        tObj["id"] = qMax(0, m_myPeerId);
        tObj["z"] = (double)player.worldZ;
        tObj["x"] = (double)player.x;
        tObj["s"] = (double)player.speed;
        tObj["st"] = (double)player.steerAngle;
        tObj["c"] = player.selectedCar;
        tObj["f"] = (int)flags;
        tObj["ft"] = (double)player.finishTime;
        tObj["hp"] = (double)player.health;
        if (m_role == NET_ROLE_CLIENT) {
            sendTcpJson(tObj, m_clientTcpSocket);
        } else if (m_role == NET_ROLE_HOST) {
            broadcastTcpJson(tObj);
        }
    }
}

void NetworkManager::onUdpReadyRead() {
    if (!m_udpSocket) return;
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();
        QHostAddress sender = normalizeIPv4(datagram.senderAddress());
        quint16 senderPort = (quint16)datagram.senderPort();

        if (data.size() == (int)sizeof(TelemetryPacket)) {
            TelemetryPacket pkt;
            memcpy(&pkt, data.constData(), sizeof(TelemetryPacket));
            if (pkt.magic == APEX_MAGIC) {
                int pid = (int)pkt.peerId;
                if (pid == m_myPeerId) continue;

                // Host auto-learns client UDP port if received
                if (m_role == NET_ROLE_HOST && m_peers.contains(pid)) {
                    m_peers[pid].udpAddress = sender;
                    m_peers[pid].udpPort = senderPort;

                    // Relay client packet to all OTHER clients
                    for (const auto& peer : m_peers) {
                        if (peer.peerId != pid && !peer.udpAddress.isNull()) {
                            m_udpSocket->writeDatagram((const char*)&pkt, sizeof(pkt), peer.udpAddress, peer.udpPort);
                        }
                    }
                }

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
                continue;
            }
        }

        // Handle JSON discovery / query packets arriving on m_udpSocket
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString type = obj["type"].toString();
            if (type == "APEX_BEACON") {
                handleBeaconJson(obj, sender);
            } else if (type == "ROOM_QUERY" && m_role == NET_ROLE_HOST && m_state != MP_STATE_DISCONNECTED) {
                QString reqRoom = obj["roomId"].toString().trimmed();
                if (reqRoom.isEmpty() || reqRoom.compare(m_roomId, Qt::CaseInsensitive) == 0) {
                    quint16 replyPort = (quint16)obj["replyPort"].toInt(senderPort);
                    sendBeaconBroadcast(sender, replyPort);
                }
            }
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

            if (!m_peerTargets.contains(pid)) {
                initPeerTargetOnGrid(pid, it.value().carModel);
            }

            const PeerInterpTarget& tgt = m_peerTargets[pid];
            qreal dtSincePacket = qBound(0.0, (m_uptime.elapsed() - tgt.lastPacketTimeMs) / 1000.0, 0.25);
            qreal absSpd = qAbs(tgt.targetSpeed);
            qreal signSpd = (tgt.targetSpeed >= 0.0) ? 1.0 : -1.0;
            qreal worldVel = (absSpd > 0.2)
                ? signSpd * ((absSpd / 3.6) * 62.0 + 240.0 * qMin(1.0, absSpd / 22.0))
                : 0.0;
            qreal extrapolatedZ = tgt.targetZ + worldVel * dtSincePacket;

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

            int carIdx = tgt.carModel;
            switch (carIdx) {
                case 0: remoteCar.type = VEHICLE_SEDAN_SILVER; break;
                case 1: remoteCar.type = VEHICLE_SPORTS_RED;   break;
                case 2: remoteCar.type = VEHICLE_COMPACT_BLUE; break;
                case 3: remoteCar.type = VEHICLE_SUV_BLACK;    break;
                default: remoteCar.type = VEHICLE_SEDAN_SILVER; break;
            }

            engine.setRemotePlayerCar(pid, remoteCar);
        }
    }
}
