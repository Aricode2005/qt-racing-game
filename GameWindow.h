#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QVector>
#include <QPoint>
#include "GameEngine.h"
#include "RasterRenderer.h"
#include "AudioSynthesizer.h"
#include "NetworkManager.h"

class GameWindow : public QWidget {
    Q_OBJECT
public:
    explicit GameWindow(QWidget *parent = nullptr);
    ~GameWindow() = default;

    void setShowMainMenu(bool show) {
        m_showMainMenu = show;
        m_isTransitioningToRace = false;
        m_menuOpacity = show ? 1.0 : 0.0;
        m_raceTransitionFlash = 0.0;
    }
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
    void renderRaceTransitionOverlay(QPainter& p);
    void startTransitionToRace(bool resetPlayerFirst = false);
    void selectTrack(TrackTheme theme);

    GameEngine m_engine;
    RasterRenderer m_renderer;
    AudioSynthesizer m_audio;

    QTimer m_loopTimer;
    QElapsedTimer m_frameTimer;
    qreal m_lastDt = 0.016;
    qreal m_smoothDt = 0.016;
    qreal m_menuAnimTime = 0.0;

    bool m_keyGas = false;
    bool m_keyBrake = false;
    bool m_mouseGas = false;
    bool m_mouseBrake = false;
    QPoint m_mousePos = QPoint(-1, -1);
    QRect m_brakePedalRect;
    QRect m_gasPedalRect;

    bool m_showMainMenu = true;
    bool m_isTransitioningToRace = false;
    qreal m_menuOpacity = 1.0;
    qreal m_raceTransitionFlash = 0.0;

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
    QRect m_mpCarButtons[4];
    QRect m_mpTrackButtons[4];
    QRect m_readyBtn;
    QRect m_startMpBtn;
    QRect m_disconnectBtn;

    // Room ID & Password variables (Zero IP required)
    QString m_hostRoomId = "APEX-101";
    QString m_hostRoomPassword = "1234";
    QString m_joinRoomId = "APEX-101";
    QString m_joinRoomPassword = "1234";
    QString m_lastAuthError;
    qreal m_authErrorTimer = 0.0;

    // Room UI Rectangles
    QRect m_editNameBtn;
    QRect m_hostRoomIdBox;
    QRect m_hostRandomIdBtn;
    QRect m_hostPassBox;
    QRect m_createRoomBtn;

    QRect m_joinRoomIdBox;
    QRect m_joinPassBox;
    QRect m_enterRoomBtn;
    QVector<QPair<QRect, QString>> m_discoveredRoomChips;
};

#endif // GAMEWINDOW_H
