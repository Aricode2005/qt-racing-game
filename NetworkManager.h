#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVector>
#include <QElapsedTimer>
#include "GameEngine.h"

enum NetworkRole {
    NET_ROLE_NONE = 0,
    NET_ROLE_HOST,
    NET_ROLE_CLIENT
};

enum MultiplayerState {
    MP_STATE_DISCONNECTED = 0,
    MP_STATE_LOBBY,
    MP_STATE_COUNTDOWN,
    MP_STATE_RACING,
    MP_STATE_FINISHED
};

#pragma pack(push, 1)
struct TelemetryPacket {
    uint32_t magic;       // 0x41504558 ("APEX")
    uint32_t sequence;    // Monotonic packet ID
    uint32_t timestampMs; // Elapsed ms for round-trip latency
    float    worldZ;
    float    x;
    float    speed;
    float    steerAngle;
    float    pitchG;
    float    rollG;
    float    skidAngle;
    uint8_t  carModel;
    uint8_t  flags;       // bit 0: isBraking, 1: isCrashing, 2: hazardLights, 3: horn, 4: hasFinished
    float    finishTime;
    float    health;
};
#pragma pack(pop)

struct RemotePeerInfo {
    QString name = "Rival Racer";
    int carModel = 0;
    bool isReady = false;
    qint64 pingMs = 0;
    QHostAddress udpAddress;
    quint16 udpPort = 7778;
};

struct DiscoveredHost {
    QString hostName;
    QHostAddress address;
    quint16 port;
    int trackTheme;
    int timeOfDay;
    int carModel;
    qint64 lastSeenMs;
};

class NetworkManager : public QObject {
    Q_OBJECT

public:
    static NetworkManager& instance();

    bool isConnected() const { return m_role != NET_ROLE_NONE && m_state != MP_STATE_DISCONNECTED; }
    bool isHost() const { return m_role == NET_ROLE_HOST; }
    bool isClient() const { return m_role == NET_ROLE_CLIENT; }
    NetworkRole getRole() const { return m_role; }
    MultiplayerState getState() const { return m_state; }

    const RemotePeerInfo& getRemotePeer() const { return m_remotePeer; }
    const QVector<DiscoveredHost>& getDiscoveredHosts() const { return m_discoveredHosts; }
    QString getLocalIpAddress() const;
    qint64 getPingMs() const { return m_remotePeer.pingMs; }
    qreal getCountdownTimer() const { return m_countdownTimer; }

    void setLocalPlayerName(const QString& name) { m_localName = name; }
    const QString& getLocalPlayerName() const { return m_localName; }

    // Session controls
    bool startHost(quint16 tcpPort = 7777, quint16 udpPort = 7778);
    bool connectToHost(const QString& hostAddress, quint16 tcpPort = 7777, quint16 udpPort = 7780);
    void disconnectSession();

    // Lobby actions
    void setReady(bool ready);
    void notifyTrackChange(int trackTheme, int timeOfDay);
    void notifyCarChange(int carModel);
    void startRaceCountdown();
    void notifyRaceRestart();
    void sendQuickChat(const QString& text);

    // In-game 60Hz Telemetry
    void sendTelemetry(const PlayerCar& player);
    void update(qreal dt, GameEngine& engine);

signals:
    void hostDiscovered(const DiscoveredHost& host);
    void peerConnected(const QString& peerName);
    void peerDisconnected();
    void peerReadyChanged(bool ready);
    void trackSyncRequested(int trackTheme, int timeOfDay);
    void carSyncRequested(int carModel);
    void countdownStarted(qreal durationSecs);
    void raceStarted();
    void raceRestarted();
    void chatMessageReceived(const QString& sender, const QString& message);
    void statusMessage(const QString& msg, bool isError = false);

private slots:
    void onNewTcpConnection();
    void onTcpReadyRead();
    void onTcpDisconnected();
    void onUdpReadyRead();
    void onBeaconTimer();
    void onPingTimer();

private:
    NetworkManager();
    ~NetworkManager();

    void sendTcpJson(const QJsonObject& json);
    void processTcpMessage(const QJsonObject& json);
    void sendBeaconBroadcast();

    NetworkRole m_role = NET_ROLE_NONE;
    MultiplayerState m_state = MP_STATE_DISCONNECTED;
    QString m_localName = "Driver 1";

    QTcpServer* m_tcpServer = nullptr;
    QTcpSocket* m_tcpSocket = nullptr; // For client, or connected peer for host
    QUdpSocket* m_udpSocket = nullptr;
    QUdpSocket* m_beaconSocket = nullptr;

    quint16 m_localTcpPort = 7777;
    quint16 m_localUdpPort = 7778;
    quint16 m_peerUdpPort = 7778;

    RemotePeerInfo m_remotePeer;
    QVector<DiscoveredHost> m_discoveredHosts;

    QTimer* m_beaconTimer = nullptr;
    QTimer* m_pingTimer = nullptr;
    QElapsedTimer m_uptime;
    uint32_t m_packetSequence = 0;

    // Countdown sync
    qreal m_countdownTimer = -1.0;

    // Interpolation targets for smooth rendering
    struct {
        qreal targetZ = 0;
        qreal targetX = 0;
        qreal targetSpeed = 0;
        qreal targetSteer = 0;
        qreal targetPitchG = 0;
        qreal targetRollG = 0;
        qreal targetSkid = 0;
        int   carModel = 0;
        bool  isBraking = false;
        bool  isCrashing = false;
        bool  hazardLights = false;
        bool  horn = false;
        bool  hasFinished = false;
        qreal finishTime = 0;
        qreal health = 100.0;
        qint64 lastPacketTimeMs = 0;
    } m_interp;
};
