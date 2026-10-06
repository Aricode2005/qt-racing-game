#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QKeyEvent>
#include "GameEngine.h"
#include "RasterRenderer.h"
#include "AudioSynthesizer.h"

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
    void mousePressEvent(QMouseEvent *event) override;

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

    bool m_showMainMenu = false;
    QRect m_diffButtons[3];
    QRect m_todButtons[3];
    QRect m_trackCards[4];
    QRect m_resumeBtn;
    QRect m_repairBtn;
    QRect m_lightsBtn;
};

#endif // GAMEWINDOW_H
