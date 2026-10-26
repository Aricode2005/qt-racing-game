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
    QRect m_hostBtn;
    QRect m_joinLocalBtn;
    QRect m_joinDirectBtn;
    QRect m_readyBtn;
    QRect m_startMpBtn;
    QRect m_disconnectBtn;
};

#endif // GAMEWINDOW_H
