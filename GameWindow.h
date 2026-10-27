#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QKeyEvent>
#include "GameEngine.h"
#include "RasterRenderer.h"
#include "AudioSynthesizer.h"
#include "NetworkManager.h"

class GameWindow : public QWidget {
    Q_OBJECT
public:
    explicit GameWindow(QWidget *parent = nullptr);
    ~GameWindow() = default;

    void setShowMainMenu(bool show) { m_showMainMenu = show; }
    bool isMainMenuShown() const { return m_showMainMenu; }
    GameEngine& getEngine() { return m_engine; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onGameLoop();

private:
    void renderMainMenuOverlay(QPainter& p);
    void selectTrack(TrackTheme theme);

    GameEngine m_engine;
    RasterRenderer m_renderer;
    AudioSynthesizer m_audio;

    QTimer m_loopTimer;
    QElapsedTimer m_frameTimer;
    qreal m_lastDt = 0.016;
    qreal m_smoothDt = 0.016;

    bool m_keyGas = false;
    bool m_keyBrake = false;
    bool m_mouseGas = false;
    bool m_mouseBrake = false;
    QRect m_brakePedalRect;
    QRect m_gasPedalRect;

    bool m_showMainMenu = true;
    QRect m_diffButtons[3];
    QRect m_todButtons[3];
    QRect m_carButtons[4];
    QRect m_trackCards[4];
    QRect m_resumeBtn;
    QRect m_repairBtn;
    QRect m_lightsBtn;

    bool m_showMultiplayerTab = false;
    QRect m_soloTabBtn;
    QRect m_mpTabBtn;
    QRect m_readyBtn;
    QRect m_startMpBtn;
    QRect m_disconnectBtn;

    // Custom Room variables & state
    QString m_hostRoomId = "APEX-101";
    QString m_hostRoomPassword = "1234";
    QString m_joinRoomId = "APEX-101";
    QString m_joinRoomPassword = "1234";
    QString m_joinTargetIp = "127.0.0.1";
    QString m_lastAuthError;
    qreal m_authErrorTimer = 0.0;

    // Custom Room UI Rectangles
    QRect m_editNameBtn;
    QRect m_hostRoomIdBox;
    QRect m_hostRandomIdBtn;
    QRect m_hostPassBox;
    QRect m_createRoomBtn;

    QRect m_joinIpBox;
    QRect m_joinLocalhostQuickBtn;
    QRect m_joinRoomIdBox;
    QRect m_joinPassBox;
    QRect m_enterRoomBtn;
};

#endif // GAMEWINDOW_H
