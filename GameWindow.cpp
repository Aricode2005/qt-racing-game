#include "GameWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFont>
#include <QRandomGenerator>

GameWindow::GameWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("APEX VELOCITY - First Person Qt Racing Simulator");
    resize(1280, 720);
    setMinimumSize(960, 540);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    // Initialize procedural raster assets
    RasterAssets::instance().initialize();

    // Initialize audio synthesizer
    m_audio.initialize();

    // Connect signals
    connect(&m_engine, &GameEngine::collisionOccurred, this, [this]() {
        m_audio.playCrash();
    });

    // Multiplayer Network Signals
    NetworkManager& net = NetworkManager::instance();
    connect(&net, &NetworkManager::peerJoined, this, [this](int peerId, const QString& peerName) {
        m_engine.getPlayerMutable().lastAnnouncement = QString("🔥 PILOT [P%1] %2 JOINED ROOM!").arg(peerId + 1).arg(peerName);
        m_engine.getPlayerMutable().announcementTimer = 3.5;
        update();
    });
    connect(&net, &NetworkManager::peerLeft, this, [this](int peerId, const QString& peerName) {
        m_engine.getPlayerMutable().lastAnnouncement = QString("⚠️ PILOT [P%1] %2 LEFT ROOM").arg(peerId + 1).arg(peerName);
        m_engine.getPlayerMutable().announcementTimer = 3.0;
        m_engine.removeRemotePlayer(peerId);
        update();
    });
    connect(&net, &NetworkManager::trackSyncRequested, this, [this](int theme, int tod) {
        selectTrack(static_cast<TrackTheme>(theme));
        m_engine.setTimeOfDay(static_cast<TimeOfDay>(tod));
        update();
    });
    connect(&net, &NetworkManager::countdownStarted, this, [this](qreal) {
        m_showMainMenu = false;
        m_engine.resetPlayer();
        update();
    });
    connect(&net, &NetworkManager::raceStarted, this, [this]() {
        m_showMainMenu = false;
        m_engine.getPlayerMutable().lastAnnouncement = "🏁 GREEN LIGHT! GO GO GO!";
        m_engine.getPlayerMutable().announcementTimer = 2.0;
        update();
    });
    connect(&net, &NetworkManager::raceRestarted, this, [this]() {
        m_engine.resetPlayer();
        update();
    });
    connect(&net, &NetworkManager::chatMessageReceived, this, [this](const QString& sender, const QString& msg) {
        m_engine.getPlayerMutable().lastAnnouncement = QString("💬 %1: %2").arg(sender).arg(msg);
        m_engine.getPlayerMutable().announcementTimer = 3.5;
        update();
    });
    connect(&net, &NetworkManager::statusMessage, this, [this](const QString& msg, bool isError) {
        m_engine.getPlayerMutable().lastAnnouncement = (isError ? "⚠️ " : "ℹ️ ") + msg;
        m_engine.getPlayerMutable().announcementTimer = 3.0;
        update();
    });

    // High Refresh Rate Smooth Game Loop (up to 120 FPS capable)
    connect(&m_loopTimer, &QTimer::timeout, this, &GameWindow::onGameLoop);
    m_frameTimer.start();
    m_loopTimer.setTimerType(Qt::PreciseTimer);
    m_loopTimer.start(8);
}

void GameWindow::onGameLoop() {
    qint64 elapsedMs = m_frameTimer.restart();
    qreal rawDt = elapsedMs / 1000.0;
    if (rawDt > 0.05) rawDt = 0.05;

    // High-framerate responsive delta filter: eliminates timer quantization jitter without clamping to 30 FPS
    if (m_smoothDt <= 0.001) m_smoothDt = 0.0166;
    m_smoothDt = m_smoothDt * 0.35 + qBound(0.004, rawDt, 0.040) * 0.65;
    qreal dt = m_smoothDt;
    m_lastDt = dt;

    // Network Telemetry & Sync Update
    NetworkManager::instance().update(dt, m_engine);
    if (NetworkManager::instance().isConnected()) {
        NetworkManager::instance().sendTelemetry(m_engine.getPlayer());
    }

    if (!m_showMainMenu) {
        m_engine.update(dt);

        const PlayerCar& p = m_engine.getPlayer();
        m_audio.updateEngine(p.rpm, p.isAccelerating, p.isBraking, p.speed);
        m_audio.setHorn(p.hornActive);
    } else {
        m_audio.updateEngine(800, false, false, 0); // Idle engine purr in menu
        m_audio.setHorn(false);
    }

    // Synchronize procedural synthwave music and 16-band visualizer
    const InfotainmentState& info = m_engine.getInfotainment();
    m_audio.updateMusic(info.currentTrack, info.isPlaying, info.volume);
    qreal vizLevels[16];
    m_audio.getVisualizer(vizLevels, 16);
    m_engine.setVisualizerLevels(vizLevels, 16);

    update(); // Request repaint
}

void GameWindow::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);

    int pSize = m_engine.getPlayer().pixelSize;
    if (pSize > 1 && !m_showMainMenu) {
        // Render at native window dimensions to maintain 100% identical camera FOV, horizon, and cockpit framing
        static QImage s_fullImg;
        if (s_fullImg.size() != size()) {
            s_fullImg = QImage(width(), height(), QImage::Format_RGB32);
        }
        {
            QPainter fp(&s_fullImg);
            m_renderer.render(fp, m_engine, width(), height(), m_lastDt);
        }

        // Fast nearest-neighbor downscale to chunky retro pixel block resolution
        int lowW = qMax(64, width() / pSize);
        int lowH = qMax(36, height() / pSize);
        QImage lowImg = s_fullImg.scaled(lowW, lowH, Qt::IgnoreAspectRatio, Qt::FastTransformation);

        // Blit back with sharp nearest-neighbor pixels (no zoom, authentic retro software rasterizer)
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(rect(), lowImg);
    } else {
        // Render pure raster 3D game scene at native resolution
        m_renderer.render(painter, m_engine, width(), height(), m_lastDt);
    }

    // Render Main Menu & Difficulty Overlay if open
    if (m_showMainMenu) {
        renderMainMenuOverlay(painter);
    }
}

void GameWindow::keyPressEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) {
        QWidget::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
        case Qt::Key_W:
        case Qt::Key_Up:
            m_keyGas = true;
            m_engine.setThrottle(true);
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            m_keyBrake = true;
            m_engine.setBrake(true);
            break;
        case Qt::Key_A:
        case Qt::Key_Left:
            m_engine.setSteerLeft(true);
            break;
        case Qt::Key_D:
        case Qt::Key_Right:
            m_engine.setSteerRight(true);
            break;
        case Qt::Key_B:
            m_engine.setLookBack(true);
            break;
        case Qt::Key_Q:
            m_engine.setLookLeft(true);
            break;
        case Qt::Key_E:
            m_engine.setLookRight(true);
            break;
        case Qt::Key_X:
            m_engine.toggleWipers();
            break;
        case Qt::Key_Space:
            m_engine.setHandbrake(true);
            break;
        case Qt::Key_H:
            m_engine.setHorn(true);
            break;
        case Qt::Key_N:
            m_engine.cycleTimeOfDay(); // Instant Day / Sunset / Night switch
            break;
        case Qt::Key_1:
            m_engine.toggleZBuffer3DMode();
            update();
            break;
        case Qt::Key_2:
            m_engine.toggleAdas();
            update();
            break;
        case Qt::Key_3:
            m_engine.toggleArGuides();
            update();
            break;
        case Qt::Key_4:
            m_engine.cyclePixelSize(1);
            update();
            break;
        case Qt::Key_P:
            m_engine.toggleMusicPlay();
            break;
        case Qt::Key_BracketRight:
            m_engine.nextMusicTrack();
            break;
        case Qt::Key_BracketLeft:
            m_engine.prevMusicTrack();
            break;
        case Qt::Key_I:
            m_engine.nextWebPage();
            break;
        case Qt::Key_Tab:
            m_engine.cycleInfotainmentMode();
            break;
        case Qt::Key_M:
        case Qt::Key_T:
        case Qt::Key_Escape:
            m_showMainMenu = !m_showMainMenu;
            break;
        case Qt::Key_O:
            m_showMultiplayerTab = true;
            m_showMainMenu = true;
            update();
            break;
        case Qt::Key_F5:
            NetworkManager::instance().sendQuickChat("Nice driving!");
            break;
        case Qt::Key_F6:
            NetworkManager::instance().sendQuickChat("Eat my dust!");
            break;
        case Qt::Key_F7:
            NetworkManager::instance().sendQuickChat("See you at the finish line!");
            break;
        case Qt::Key_C:
        case Qt::Key_V:
            m_engine.toggleCameraView();
            update();
            break;
        case Qt::Key_G:
            m_engine.toggleAdas();
            update();
            break;
        case Qt::Key_F:
            m_engine.toggleArGuides();
            update();
            break;
        case Qt::Key_Z:
        case Qt::Key_L:
            m_engine.toggleZBuffer3DMode();
            update();
            break;
        case Qt::Key_J:
            m_engine.cyclePixelSize(1);
            update();
            break;
        case Qt::Key_K:
            m_engine.toggleCrtScanlines();
            update();
            break;
        case Qt::Key_5:
            m_engine.setPixelSizePreset(0);
            update();
            break;
        case Qt::Key_6:
            m_engine.setPixelSizePreset(1);
            update();
            break;
        case Qt::Key_7:
            m_engine.setPixelSizePreset(2);
            update();
            break;
        case Qt::Key_8:
            m_engine.setPixelSizePreset(3);
            update();
            break;
        case Qt::Key_9:
            m_engine.setPixelSizePreset(4);
            update();
            break;
        case Qt::Key_Y:
            m_engine.fieldRepair();
            update();
            break;
        case Qt::Key_F1:
            m_engine.setSelectedCar(0);
            update();
            break;
        case Qt::Key_F2:
            m_engine.setSelectedCar(1);
            update();
            break;
        case Qt::Key_F3:
            m_engine.setSelectedCar(2);
            update();
            break;
        case Qt::Key_F4:
            m_engine.setSelectedCar(3);
            update();
            break;
        case Qt::Key_R:
            m_engine.resetPlayer();
            break;
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void GameWindow::keyReleaseEvent(QKeyEvent *event) {
    if (event->isAutoRepeat()) {
        QWidget::keyReleaseEvent(event);
        return;
    }

    switch (event->key()) {
        case Qt::Key_W:
        case Qt::Key_Up:
            m_keyGas = false;
            m_engine.setThrottle(m_mouseGas);
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            m_keyBrake = false;
            m_engine.setBrake(m_mouseBrake);
            break;
        case Qt::Key_A:
        case Qt::Key_Left:
            m_engine.setSteerLeft(false);
            break;
        case Qt::Key_D:
        case Qt::Key_Right:
            m_engine.setSteerRight(false);
            break;
        case Qt::Key_B:
            m_engine.setLookBack(false);
            break;
        case Qt::Key_Q:
            m_engine.setLookLeft(false);
            break;
        case Qt::Key_E:
            m_engine.setLookRight(false);
            break;
        case Qt::Key_Space:
            m_engine.setHandbrake(false);
            break;
        case Qt::Key_H:
            m_engine.setHorn(false);
            break;
        default:
            QWidget::keyReleaseEvent(event);
            break;
    }
}

void GameWindow::focusOutEvent(QFocusEvent *event) {
    Q_UNUSED(event);
    // Reset all input states when window loses focus to prevent stuck keys
    m_keyGas = false;
    m_keyBrake = false;
    m_mouseGas = false;
    m_mouseBrake = false;
    m_engine.setThrottle(false);
    m_engine.setBrake(false);
    m_engine.setSteerLeft(false);
    m_engine.setSteerRight(false);
    m_engine.setHandbrake(false);
    m_engine.setLookBack(false);
    m_engine.setLookLeft(false);
    m_engine.setLookRight(false);
    m_engine.setHorn(false);
    QWidget::focusOutEvent(event);
}

void GameWindow::mousePressEvent(QMouseEvent *event) {
    QPoint pos = event->pos();

    if (m_showMainMenu) {
        // Tab switching: [ SOLO CIRCUITS ] vs [ MULTIPLAYER LIVE ARENA ]
        if (m_soloTabBtn.contains(pos)) {
            m_showMultiplayerTab = false;
            update();
            return;
        }
        if (m_mpTabBtn.contains(pos)) {
            m_showMultiplayerTab = true;
            update();
            return;
        }

        if (m_showMultiplayerTab) {
            NetworkManager& net = NetworkManager::instance();
            if (m_hostBtn.contains(pos)) {
                if (!net.isHost()) {
                    net.startHost(7777, 7778);
                }
                update();
                return;
            }
            if (m_joinLocalBtn.contains(pos)) {
                if (net.isHost()) {
                    return; // Already hosting, waiting for peer
                }
                const auto& hosts = net.getDiscoveredHosts();
                if (!hosts.isEmpty()) {
                    net.connectToHost(hosts.first().address.toString(), hosts.first().port, 7780);
                } else {
                    net.connectToHost("127.0.0.1", 7777, 7780);
                }
                update();
                return;
            }
            if (m_joinDirectBtn.contains(pos)) {
                net.connectToHost("127.0.0.1", 7777, 7780);
                update();
                return;
            }
            if (m_readyBtn.contains(pos)) {
                static bool s_ready = false;
                s_ready = !s_ready;
                net.setReady(s_ready);
                update();
                return;
            }
            if (m_startMpBtn.contains(pos)) {
                if (net.isHost()) {
                    net.startRaceCountdown();
                }
                update();
                return;
            }
            if (m_disconnectBtn.contains(pos)) {
                net.disconnectSession();
                update();
                return;
            }
            if (m_resumeBtn.contains(pos)) {
                m_showMainMenu = false;
                update();
                return;
            }
            return;
        }

        // 1. Difficulty Buttons
        for (int i = 0; i < 3; ++i) {
            if (m_diffButtons[i].contains(pos)) {
                m_engine.setDifficulty(static_cast<GameDifficulty>(i));
                update();
                return;
            }
        }

        // 2. Time of Day Selection Buttons
        for (int i = 0; i < 3; ++i) {
            if (m_todButtons[i].contains(pos)) {
                m_engine.setTimeOfDay(static_cast<TimeOfDay>(i));
                update();
                return;
            }
        }

        // 2b. Supercar Selection Buttons (4 Selectable Supercars)
        for (int i = 0; i < 4; ++i) {
            if (m_carButtons[i].contains(pos)) {
                m_engine.setSelectedCar(i);
                update();
                return;
            }
        }

        // 3. Track Cards
        for (int i = 0; i < 4; ++i) {
            if (m_trackCards[i].contains(pos)) {
                selectTrack(static_cast<TrackTheme>(i));
                m_showMainMenu = false;
                update();
                return;
            }
        }

        // 4. Quick Action Buttons
        if (m_resumeBtn.contains(pos)) {
            m_showMainMenu = false;
            update();
            return;
        }
        if (m_repairBtn.contains(pos)) {
            m_engine.resetPlayer();
            m_showMainMenu = false;
            update();
            return;
        }
        if (m_lightsBtn.contains(pos)) {
            m_engine.cycleTimeOfDay();
            update();
            return;
        }
        return;
    }

    int w = width();
    int h = height();
    const PlayerCar& player = m_engine.getPlayer();

    // Finish Line Scoreboard Action Button Click
    if (player.raceFinished) {
        int bw = qMin(640, w - 40);
        int bh = 360;
        int bx = (w - bw) / 2;
        int by = (h - bh) / 2 - 10;
        QRect footRect(bx + 14, by + bh - 38, bw - 28, 26);
        if (footRect.contains(pos)) {
            m_engine.resetPlayer(); // Restart new race on starting grid
            update();
            return;
        }
    }

    // Touchscreen Dashboard Center Console Interaction (Web browser, Music player, GPS radar)
    int pitchOffset = qRound(player.pitchG * 90.0);
    int rollOffset = qRound(player.rollG * 60.0);
    int vibX = (player.vibration > 0) ? (QRandomGenerator::global()->generate() % 5 - 2) : 0;
    int vibY = (player.vibration > 0) ? (QRandomGenerator::global()->generate() % 5 - 2) : 0;
    int dashX = rollOffset + vibX;
    int dashY = pitchOffset + vibY;
    qreal scaleX = w / 1280.0;
    qreal scaleY = h / 720.0;
    int sx = dashX + qRound(615 * scaleX);
    int sy = dashY + qRound(505 * scaleY);
    int sw = qRound(355 * scaleX);
    int sh = qRound(175 * scaleY);
    QRect screenRect(sx, sy, sw, sh);

    // Top HUD Camera View Switch Button: [ 🎥 3RD PERSON (C) ] / [ 🎥 1ST PERSON (C) ]
    QRect camSwitchBtn(288, 4, 156, 30);
    if (camSwitchBtn.contains(pos)) {
        m_engine.toggleCameraView();
        update();
        return;
    }

    // Top-Center Interactive Feature Pills: [1] 3D MESH, [2] ADAS, [3] AR GAP, [4] PIXEL
    {
        int btnW = 132, btnH = 22, btnGap = 8;
        int totalW = 4 * btnW + 3 * btnGap;
        int startX = (w / 2) - totalW / 2;
        int btnY = 50;
        for (int i = 0; i < 4; ++i) {
            QRect r(startX + i * (btnW + btnGap), btnY, btnW, btnH);
            if (r.contains(pos)) {
                if (i == 0) m_engine.toggleZBuffer3DMode();
                else if (i == 1) m_engine.toggleAdas();
                else if (i == 2) m_engine.toggleArGuides();
                else if (i == 3) m_engine.cyclePixelSize(1);
                update();
                return;
            }
        }
    }

    if (!m_engine.isThirdPersonView() && screenRect.contains(pos)) {
        int contentY = sy + 24;

        // 1. Top Mode Tabs: [WEB] [MUSIC] [GPS] [CAR]
        if (pos.y() >= sy && pos.y() < contentY) {
            int tabW = sw / 4;
            int tabIdx = (pos.x() - sx) / tabW;
            if (tabIdx >= 0 && tabIdx < 4) {
                m_engine.setInfotainmentMode(static_cast<InfotainmentMode>(tabIdx));
                update();
                return;
            }
        }

        const InfotainmentState& info = m_engine.getInfotainment();

        // 2. Mode: WEB BROWSER
        if (info.mode == INFO_MODE_WEB) {
            QRect urlBar(sx + 4, contentY + 2, sw - 8, 19);
            if (urlBar.contains(pos)) {
                m_engine.nextWebPage();
                update();
                return;
            }

            int catY = contentY + 24;
            int pillW = (sw - 18) / 4;
            for (int c = 0; c < 4; ++c) {
                int px = sx + 4 + c * (pillW + 3);
                QRect pRect(px, catY, pillW, 16);
                if (pRect.contains(pos)) {
                    m_engine.setWebPage(c);
                    update();
                    return;
                }
            }

            // Web page body click -> cycles web page
            int pageY = catY + 19;
            int pageH = sh - (pageY - sy) - 4;
            QRect pageRect(sx + 4, pageY, sw - 8, pageH);
            if (pageRect.contains(pos)) {
                m_engine.nextWebPage();
                update();
                return;
            }
        }
        // 3. Mode: MUSIC PLAYER
        else if (info.mode == INFO_MODE_MUSIC) {
            int scrubY = contentY + 84;
            int btnW = 55;
            int btnH = 18;
            int btnY = scrubY + 18;
            int btnStartX = (sw - (3 * btnW + 2 * 10)) / 2 + sx;

            QRect prevBtn(btnStartX, btnY, btnW, btnH);
            QRect playBtn(btnStartX + btnW + 10, btnY, btnW, btnH);
            QRect nextBtn(btnStartX + 2 * (btnW + 10), btnY, btnW, btnH);
            QRect artRect(sx + 10, contentY + 8, 52, 52);

            if (prevBtn.contains(pos)) {
                m_engine.prevMusicTrack();
                update();
                return;
            }
            if (playBtn.contains(pos) || artRect.contains(pos)) {
                m_engine.toggleMusicPlay();
                update();
                return;
            }
            if (nextBtn.contains(pos)) {
                m_engine.nextMusicTrack();
                update();
                return;
            }

            int scrubW = sw - 24;
            QRect scrubHit(sx + 12, scrubY - 4, scrubW, 16);
            if (scrubHit.contains(pos)) {
                qreal frac = (qreal)(pos.x() - (sx + 12)) / scrubW;
                m_engine.seekMusic(frac);
                update();
                return;
            }
        }
        // 4. GPS / CAR mode
        else {
            m_engine.cycleInfotainmentMode();
            update();
            return;
        }
    }

    // 5. Sport Racing Pedals & Mouse Throttle / Brake Select
    m_brakePedalRect = QRect(w - 215, h - 110, 85, 82);
    m_gasPedalRect = QRect(w - 115, h - 122, 75, 96);

    if (m_gasPedalRect.contains(pos) || event->button() == Qt::LeftButton) {
        m_mouseGas = true;
        m_engine.setThrottle(true);
        update();
        return;
    }
    if (m_brakePedalRect.contains(pos) || event->button() == Qt::RightButton) {
        m_mouseBrake = true;
        m_engine.setBrake(true);
        update();
        return;
    }

    QWidget::mousePressEvent(event);
}

void GameWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_mouseGas = false;
        m_engine.setThrottle(m_keyGas);
        update();
    }
    if (event->button() == Qt::RightButton) {
        m_mouseBrake = false;
        m_engine.setBrake(m_keyBrake);
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void GameWindow::mouseMoveEvent(QMouseEvent *event) {
    if (!m_showMainMenu) {
        QPoint pos = event->pos();
        int w = width();
        int h = height();
        m_brakePedalRect = QRect(w - 215, h - 110, 85, 82);
        m_gasPedalRect = QRect(w - 115, h - 122, 75, 96);

        if (event->buttons() & Qt::LeftButton) {
            if (!m_mouseGas) {
                m_mouseGas = true;
                m_engine.setThrottle(true);
                update();
            }
        }
        if (event->buttons() & Qt::RightButton) {
            if (!m_mouseBrake) {
                m_mouseBrake = true;
                m_engine.setBrake(true);
                update();
            }
        }
    }
    QWidget::mouseMoveEvent(event);
}

void GameWindow::selectTrack(TrackTheme theme) {
    m_engine.setTrackTheme(theme);
}

void GameWindow::renderMainMenuOverlay(QPainter& p) {
    int w = width();
    int h = height();

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1. Dark Sports Carbon & Frosted Glass Backdrop
    p.fillRect(0, 0, w, h, QColor(6, 9, 15, 246));

    // Subtle sports speed slant grid lines in background
    p.setPen(QPen(QColor(0, 240, 255, 10), 1));
    for (int gx = -h; gx < w + h; gx += 40) {
        p.drawLine(gx, 0, gx + h, h);
    }

    // Top Header Badge & Title
    p.setBrush(QColor(0, 240, 255, 25));
    p.setPen(QPen(QColor(0, 240, 255, 180), 1.5));
    QRect headerPill(w / 2 - 140, 10, 280, 22);
    p.drawRoundedRect(headerPill, 11, 11);
    QFont pf("Arial", 8, QFont::Bold);
    p.setFont(pf);
    p.setPen(QColor(0, 240, 255));
    p.drawText(headerPill, Qt::AlignCenter, "★ APEX VELOCITY : HIGHWAY SIMULATOR ★");

    QFont titleFont("Arial", 20, QFont::Black);
    p.setFont(titleFont);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 32, w, 24), Qt::AlignCenter, "RACING EVENT COMMAND CENTER");

    // Mode Switcher Tabs: [ 🏎️ SOLO CIRCUITS & CAREER ] vs [ 🌐 LIVE 1V1 MULTIPLAYER ARENA ]
    int tabW = 240;
    int tabH = 25;
    m_soloTabBtn = QRect(w / 2 - tabW - 8, 58, tabW, tabH);
    m_mpTabBtn   = QRect(w / 2 + 8, 58, tabW, tabH);

    // Solo Tab
    p.setBrush(!m_showMultiplayerTab ? QColor(0, 220, 255, 35) : QColor(14, 20, 30, 180));
    p.setPen(QPen(!m_showMultiplayerTab ? QColor(0, 230, 255) : QColor(45, 55, 75), !m_showMultiplayerTab ? 2 : 1));
    p.drawRoundedRect(m_soloTabBtn, 5, 5);
    QFont tabF("Arial", 8, QFont::Bold);
    p.setFont(tabF);
    p.setPen(!m_showMultiplayerTab ? QColor(0, 240, 255) : QColor(150, 170, 195));
    p.drawText(m_soloTabBtn, Qt::AlignCenter, "🏎️ SOLO CIRCUITS & CAREER");

    // MP Tab
    bool hasPeer = NetworkManager::instance().isConnected();
    QColor mpAccent = hasPeer ? QColor(45, 240, 110) : QColor(255, 185, 45);
    p.setBrush(m_showMultiplayerTab ? QColor(mpAccent.red(), mpAccent.green(), mpAccent.blue(), 35) : QColor(14, 20, 30, 180));
    p.setPen(QPen(m_showMultiplayerTab ? mpAccent : QColor(45, 55, 75), m_showMultiplayerTab ? 2 : 1));
    p.drawRoundedRect(m_mpTabBtn, 5, 5);
    p.setPen(m_showMultiplayerTab ? mpAccent : (hasPeer ? QColor(45, 240, 110) : QColor(150, 170, 195)));
    p.drawText(m_mpTabBtn, Qt::AlignCenter, hasPeer ? "🟢 LIVE 1V1 MULTIPLAYER (CONNECTED)" : "🌐 LIVE 1V1 MULTIPLAYER (KEY O)");

    if (m_showMultiplayerTab) {
        NetworkManager& net = NetworkManager::instance();
        int paneW = qMin(960, w - 80);
        int paneX = (w - paneW) / 2;
        int paneY = 90;

        // 1. Callsign & Network Identity Header Card
        QRect idCard(paneX, paneY, paneW, 58);
        p.setBrush(QColor(12, 17, 26, 230));
        p.setPen(QPen(QColor(38, 52, 74), 1));
        p.drawRoundedRect(idCard, 8, 8);

        p.setFont(QFont("Arial", 10, QFont::Bold));
        p.setPen(QColor(0, 230, 255));
        p.drawText(paneX + 16, paneY + 24, QString("PILOT CALLSIGN:  %1").arg(net.getLocalPlayerName()));

        p.setFont(QFont("Arial", 8, QFont::Normal));
        p.setPen(QColor(160, 180, 205));
        p.drawText(paneX + 16, paneY + 44, QString("SELECTED VEHICLE: %1 (Press F1-F4 to change)").arg(m_engine.getPlayer().carModelName));

        // Network Info on the right
        p.setFont(QFont("Arial", 9, QFont::Bold));
        p.setPen(QColor(255, 205, 55));
        p.drawText(QRect(paneX + paneW - 320, paneY + 8, 305, 22), Qt::AlignRight, QString("📡 LOCAL LAN IP:  %1").arg(net.getLocalIpAddress()));
        p.setFont(QFont("Arial", 8, QFont::Normal));
        p.setPen(QColor(140, 160, 185));
        p.drawText(QRect(paneX + paneW - 320, paneY + 30, 305, 20), Qt::AlignRight, QString("TCP Port: 7777 • UDP Telemetry: 60 Hz"));

        // 2. Action Modes (3 Cards side by side)
        int cardY = paneY + 70;
        int cardW = (paneW - 2 * 14) / 3;
        int cardH = 120;

        // Mode 1: Host LAN Race
        m_hostBtn = QRect(paneX, cardY, cardW, cardH);
        bool isHostActive = (net.getRole() == NET_ROLE_HOST);
        p.setBrush(isHostActive ? QColor(20, 38, 28) : QColor(14, 20, 30));
        p.setPen(QPen(isHostActive ? QColor(45, 240, 110) : QColor(45, 60, 85), isHostActive ? 2 : 1));
        p.drawRoundedRect(m_hostBtn, 8, 8);
        p.setFont(QFont("Arial", 10, QFont::Bold));
        p.setPen(isHostActive ? QColor(45, 240, 110) : Qt::white);
        p.drawText(paneX + 14, cardY + 26, "📡 1. HOST LAN RACE");
        p.setFont(QFont("Arial", 7, QFont::Normal));
        p.setPen(QColor(150, 170, 195));
        p.drawText(QRect(paneX + 14, cardY + 36, cardW - 28, 42), Qt::AlignLeft | Qt::TextWordWrap,
                   "Create a local server on port 7777 and broadcast beacon over your Wi-Fi.");
        // Host Status pill
        QRect hPill(paneX + 14, cardY + cardH - 32, cardW - 28, 22);
        p.setBrush(isHostActive ? QColor(45, 240, 110, 45) : QColor(25, 36, 52));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(hPill, 4, 4);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(isHostActive ? QColor(45, 240, 110) : QColor(180, 200, 225));
        p.drawText(hPill, Qt::AlignCenter, isHostActive ? "ACTIVE: LISTENING ✓" : "CLICK TO HOST RACE");

        // Mode 2: Quick-Join Local Race
        m_joinLocalBtn = QRect(paneX + cardW + 14, cardY, cardW, cardH);
        const auto& hosts = net.getDiscoveredHosts();
        bool hasDiscovered = !hosts.isEmpty();
        bool isClientActive = (net.getRole() == NET_ROLE_CLIENT);
        p.setBrush(isClientActive ? QColor(20, 38, 52) : QColor(14, 20, 30));
        p.setPen(QPen(isClientActive ? QColor(0, 230, 255) : (hasDiscovered ? QColor(255, 205, 55) : QColor(45, 60, 85)), (isClientActive || hasDiscovered) ? 2 : 1));
        p.drawRoundedRect(m_joinLocalBtn, 8, 8);
        p.setFont(QFont("Arial", 10, QFont::Bold));
        p.setPen(isClientActive ? QColor(0, 230, 255) : (hasDiscovered ? QColor(255, 205, 55) : Qt::white));
        p.drawText(paneX + cardW + 14 + 14, cardY + 26, "🔍 2. QUICK-JOIN LAN");
        p.setFont(QFont("Arial", 7, QFont::Normal));
        p.setPen(QColor(150, 170, 195));
        QString joinDesc = isHostActive
            ? "You are currently HOSTING. Tell Player 2 to click 'QUICK-JOIN' on their PC!"
            : (hasDiscovered
                ? QString("FOUND: %1 on %2:%3!").arg(hosts.first().hostName).arg(hosts.first().address.toString()).arg(hosts.first().port)
                : "Auto-scanning Wi-Fi broadcast for available games... (Click to join)");
        p.drawText(QRect(paneX + cardW + 14 + 14, cardY + 36, cardW - 28, 42), Qt::AlignLeft | Qt::TextWordWrap, joinDesc);
        QRect jPill(paneX + cardW + 14 + 14, cardY + cardH - 32, cardW - 28, 22);
        p.setBrush((hasDiscovered && !isHostActive) ? QColor(255, 205, 55, 45) : QColor(25, 36, 52));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(jPill, 4, 4);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen((hasDiscovered && !isHostActive) ? QColor(255, 205, 55) : QColor(180, 200, 225));
        QString pillText = isHostActive
            ? "CURRENTLY HOSTING"
            : (isClientActive ? "CONNECTED ✓" : (hasDiscovered ? "ONE-CLICK JOIN ✓" : "SCAN & CONNECT"));
        p.drawText(jPill, Qt::AlignCenter, pillText);

        // Mode 3: Localhost Loopback (127.0.0.1)
        m_joinDirectBtn = QRect(paneX + 2 * (cardW + 14), cardY, cardW, cardH);
        p.setBrush(QColor(14, 20, 30));
        p.setPen(QPen(QColor(45, 60, 85), 1));
        p.drawRoundedRect(m_joinDirectBtn, 8, 8);
        p.setFont(QFont("Arial", 10, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(paneX + 2 * (cardW + 14) + 14, cardY + 26, "💻 3. LOCAL LOOPBACK");
        p.setFont(QFont("Arial", 7, QFont::Normal));
        p.setPen(QColor(150, 170, 195));
        p.drawText(QRect(paneX + 2 * (cardW + 14) + 14, cardY + 36, cardW - 28, 42), Qt::AlignLeft | Qt::TextWordWrap,
                   "Connect to 127.0.0.1. Open two game windows to test multiplayer on 1 PC!");
        QRect lPill(paneX + 2 * (cardW + 14) + 14, cardY + cardH - 32, cardW - 28, 22);
        p.setBrush(QColor(25, 36, 52));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(lPill, 4, 4);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(QColor(180, 200, 225));
        p.drawText(lPill, Qt::AlignCenter, "CONNECT 127.0.0.1");

        // 3. Matchmaking / Connected Status Panel (Supports up to 8 racers)
        int lobbyY = cardY + cardH + 12;
        int lobbyH = 162;
        QRect lobbyCard(paneX, lobbyY, paneW, lobbyH);
        p.setBrush(QColor(12, 17, 26, 240));
        p.setPen(QPen(hasPeer ? QColor(45, 240, 110, 180) : QColor(38, 52, 74), hasPeer ? 2 : 1));
        p.drawRoundedRect(lobbyCard, 8, 8);

        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(QColor(0, 240, 255));
        int totalConnected = net.getConnectedPlayerCount();
        p.drawText(paneX + 16, lobbyY + 20, QString("🏁 ROOM ROSTER: %1 / 8 PLAYERS CONNECTED • %2")
            .arg(totalConnected)
            .arg(net.isHost() ? "HOSTING SESSION" : QString("CONNECTED AS PLAYER %1").arg(net.getMyPeerId() + 1)));

        p.setFont(QFont("Arial", 8, QFont::Normal));
        p.setPen(QColor(140, 160, 185));
        p.drawText(QRect(paneX + paneW - 320, lobbyY + 8, 305, 18), Qt::AlignRight,
            QString("Avg Ping: %1 ms • 60Hz UDP Relay").arg(net.getAveragePingMs()));

        // 8-Player Slot Grid (2 rows x 4 columns)
        int slotCols = 4;
        int slotGap = 8;
        int slotW = (paneW - 32 - (slotCols - 1) * slotGap) / slotCols;
        int slotH = 40;
        const auto& peers = net.getPeers();

        for (int slot = 0; slot < 8; ++slot) {
            int col = slot % slotCols;
            int row = slot / slotCols;
            int sx = paneX + 16 + col * (slotW + slotGap);
            int sy = lobbyY + 28 + row * (slotH + 6);
            QRect sRect(sx, sy, slotW, slotH);

            if (slot == net.getMyPeerId()) {
                // Local player (YOU)
                p.setBrush(QColor(18, 32, 48));
                p.setPen(QPen(QColor(0, 240, 255), 1.5));
                p.drawRoundedRect(sRect, 5, 5);
                p.setFont(QFont("Arial", 8, QFont::Bold));
                p.setPen(QColor(0, 240, 255));
                p.drawText(sx + 8, sy + 16, QString("👑 [P%1] %2 (YOU)").arg(slot + 1).arg(net.getLocalPlayerName()));
                p.setFont(QFont("Arial", 7, QFont::Normal));
                p.setPen(QColor(160, 190, 220));
                p.drawText(sx + 8, sy + 32, QString("%1 • READY ✓").arg(m_engine.getPlayer().carModelName));
            } else if (peers.contains(slot)) {
                // Connected Remote Peer
                const auto& pr = peers[slot];
                p.setBrush(QColor(22, 28, 38));
                p.setPen(QPen(QColor(255, 205, 55), 1.2));
                p.drawRoundedRect(sRect, 5, 5);
                p.setFont(QFont("Arial", 8, QFont::Bold));
                p.setPen(QColor(255, 205, 55));
                p.drawText(sx + 8, sy + 16, QString("👑 [P%1] %2").arg(slot + 1).arg(pr.name));
                p.setFont(QFont("Arial", 7, QFont::Normal));
                p.setPen(QColor(160, 180, 205));
                const char* carNames[4] = { "APEX GT-R", "VORTEX V12", "DRIFT SPEC-R", "TITAN ENFORCER" };
                QString cName = (pr.carModel >= 0 && pr.carModel < 4) ? carNames[pr.carModel] : "APEX GT-R";
                p.drawText(sx + 8, sy + 32, QString("%1 • %2ms").arg(cName).arg(pr.pingMs));
            } else {
                // Empty Open Slot
                p.setBrush(QColor(14, 18, 25, 140));
                p.setPen(QPen(QColor(35, 45, 62), 1, Qt::DashLine));
                p.drawRoundedRect(sRect, 5, 5);
                p.setFont(QFont("Arial", 7, QFont::Normal));
                p.setPen(QColor(80, 100, 130));
                p.drawText(sRect, Qt::AlignCenter, QString("[ Slot %1 Open ]").arg(slot + 1));
            }
        }

        // Action Controls Bar
        int actY = lobbyY + 122;
        int actH = 30;

        // Ready toggle button on left
        m_readyBtn = QRect(paneX + 16, actY, 130, actH);
        p.setBrush(QColor(24, 36, 52));
        p.setPen(QPen(QColor(0, 230, 255), 1.2));
        p.drawRoundedRect(m_readyBtn, 5, 5);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(QColor(0, 240, 255));
        p.drawText(m_readyBtn, Qt::AlignCenter, "👍 TOGGLE READY");

        // Start Race in Center
        int startW = 320;
        m_startMpBtn = QRect(paneX + (paneW - startW) / 2, actY, startW, actH);
        bool canStart = (net.isHost() && totalConnected >= 2);
        QLinearGradient sGrad(m_startMpBtn.topLeft(), m_startMpBtn.bottomRight());
        if (canStart) {
            sGrad.setColorAt(0.0, QColor(0, 220, 120));
            sGrad.setColorAt(1.0, QColor(0, 160, 75));
        } else if (net.isHost()) {
            sGrad.setColorAt(0.0, QColor(60, 110, 80));
            sGrad.setColorAt(1.0, QColor(40, 80, 55));
        } else {
            sGrad.setColorAt(0.0, QColor(30, 45, 65));
            sGrad.setColorAt(1.0, QColor(20, 30, 45));
        }
        p.setBrush(sGrad);
        p.setPen(QPen(canStart ? QColor(100, 255, 180) : QColor(50, 75, 105), canStart ? 2 : 1));
        p.drawRoundedRect(m_startMpBtn, 5, 5);
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(canStart ? QColor(8, 20, 12) : QColor(160, 180, 205));
        QString startText = net.isHost()
            ? (totalConnected >= 2 ? QString("🏁 LAUNCH RACE (%1 RACERS)").arg(totalConnected) : "⏳ WAITING FOR RACERS TO JOIN...")
            : "⏳ WAITING FOR HOST TO LAUNCH RACE...";
        p.drawText(m_startMpBtn, Qt::AlignCenter, startText);

        // Disconnect on Right
        int discW = 140;
        m_disconnectBtn = QRect(paneX + paneW - 16 - discW, actY, discW, actH);
        p.setBrush(QColor(80, 24, 28));
        p.setPen(QPen(QColor(220, 60, 60), 1));
        p.drawRoundedRect(m_disconnectBtn, 5, 5);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(QColor(255, 180, 180));
        p.drawText(m_disconnectBtn, Qt::AlignCenter, net.isHost() ? "CANCEL HOSTING" : "❌ DISCONNECT");

        // Return button at bottom of multiplayer tab
        m_resumeBtn = QRect(paneX + paneW / 2 - 110, lobbyY + lobbyH + 14, 220, 34);
        p.setBrush(QColor(28, 38, 54));
        p.setPen(QPen(QColor(55, 75, 105), 1.5));
        p.drawRoundedRect(m_resumeBtn, 6, 6);
        p.setFont(QFont("Arial", 9, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(m_resumeBtn, Qt::AlignCenter, "◀ RETURN TO RACE (M / ESC)");

        p.restore();
        return;
    }

    // -------------------------------------------------------------
    // 1. DIFFICULTY SELECTION ROW (EASY, NORMAL, PRO)
    // -------------------------------------------------------------
    int diffW = qMin(340, (w - 100) / 3);
    int diffH = 48;
    int diffTotalW = 3 * diffW + 2 * 14;
    int diffStartX = (w - diffTotalW) / 2;
    int diffY = 84;

    struct DiffData {
        GameDifficulty diff;
        QString title;
        QString desc;
        QColor accent;
    } diffs[] = {
        { DIFFICULTY_EASY, "🟢 LEVEL 1: EASY / ROOKIE", "6 Cars Grid • Shorter Circuit • Easy Overtakes", QColor(40, 225, 90) },
        { DIFFICULTY_NORMAL, "🟡 LEVEL 2: NORMAL / CIRCUIT", "10 Cars Grid • Faster Rivals • 1.5x Score", QColor(255, 195, 30) },
        { DIFFICULTY_PRO, "🔴 LEVEL 3: PRO / CHAMPIONSHIP", "16 Cars Grid • Full Endurance • 2.5x Score", QColor(255, 45, 65) }
    };

    GameDifficulty curDiff = m_engine.getDifficulty();

    for (int i = 0; i < 3; ++i) {
        m_diffButtons[i] = QRect(diffStartX + i * (diffW + 14), diffY, diffW, diffH);
        bool isCur = (curDiff == diffs[i].diff);

        p.setBrush(isCur ? QColor(20, 28, 42) : QColor(12, 16, 24));
        p.setPen(QPen(isCur ? diffs[i].accent : QColor(38, 48, 64), isCur ? 2 : 1));
        p.drawRoundedRect(m_diffButtons[i], 6, 6);

        // Header Title
        QFont dtf("Arial", 9, QFont::Bold);
        p.setFont(dtf);
        p.setPen(isCur ? diffs[i].accent : Qt::white);
        p.drawText(QRect(m_diffButtons[i].x() + 8, m_diffButtons[i].y() + 5, m_diffButtons[i].width() - 16, 18), Qt::AlignLeft, diffs[i].title);

        if (isCur) {
            p.setPen(diffs[i].accent);
            p.drawText(QRect(m_diffButtons[i].x() + m_diffButtons[i].width() - 85, m_diffButtons[i].y() + 5, 75, 18), Qt::AlignRight, "ACTIVE ✓");
        }

        // Subtitle description
        QFont ddf("Arial", 8, QFont::Normal);
        p.setFont(ddf);
        p.setPen(QColor(150, 165, 185));
        p.drawText(QRect(m_diffButtons[i].x() + 8, m_diffButtons[i].y() + 25, m_diffButtons[i].width() - 16, 18), Qt::AlignLeft, diffs[i].desc);
    }

    // -------------------------------------------------------------
    // 2. TIME OF DAY SELECTION ROW
    // -------------------------------------------------------------
    int todW = 140;
    int todH = 30;
    int todStartX = 95;
    int todY = 138;
    TimeOfDay currentTod = m_engine.getTimeOfDay();

    struct TodData {
        TimeOfDay tod;
        QString label;
        QColor activeCol;
    } tods[] = {
        { TIME_DAY, "☀️ DAYLIGHT", QColor(30, 140, 240) },
        { TIME_SUNSET, "🌅 SUNSET GLOW", QColor(240, 120, 35) },
        { TIME_NIGHT, "🌙 MIDNIGHT RUN", QColor(140, 50, 240) }
    };

    for (int i = 0; i < 3; ++i) {
        m_todButtons[i] = QRect(todStartX + i * (todW + 14), todY, todW, todH);
        bool isCur = (currentTod == tods[i].tod);

        p.setBrush(isCur ? tods[i].activeCol : QColor(16, 20, 28));
        p.setPen(QPen(isCur ? Qt::white : QColor(42, 52, 68), isCur ? 2 : 1));
        p.drawRoundedRect(m_todButtons[i], 5, 5);

        QFont todf("Arial", 8, QFont::Bold);
        p.setFont(todf);
        p.setPen(Qt::white);
        p.drawText(m_todButtons[i], Qt::AlignCenter, tods[i].label);
        if (i == 2) {
            struct CarOption { QString label; QColor col; } carOpts[4] = {
                { "🏎️ APEX GT-R [300K]", QColor(0, 215, 255) },
                { "🔥 VORTEX V12 [345K]", QColor(255, 55, 70) },
                { "⚡ DRIFT SPEC [GRIP]", QColor(255, 185, 35) },
                { "🛡️ TITAN GT [ARMOR]", QColor(45, 225, 130) }
            };
            int carBtnW = 158;
            int carStartX = todStartX + 3 * (todW + 14) + 18;
            int curCar = m_engine.getPlayer().selectedCar;
            for (int c = 0; c < 4; ++c) {
                m_carButtons[c] = QRect(carStartX + c * (carBtnW + 8), todY, carBtnW, todH);
                bool isSel = (curCar == c);
                p.setBrush(isSel ? QColor(22, 30, 44) : QColor(14, 18, 26));
                p.setPen(QPen(isSel ? carOpts[c].col : QColor(42, 52, 68), isSel ? 2.0 : 1.0));
                p.drawRoundedRect(m_carButtons[c], 5, 5);
                p.setFont(QFont("Arial", 7, QFont::Bold));
                p.setPen(isSel ? carOpts[c].col : QColor(210, 220, 235));
                p.drawText(m_carButtons[c], Qt::AlignCenter, carOpts[c].label);
            }
        }
    }

    // -------------------------------------------------------------
    // 3. 4 TRACK CARDS
    // -------------------------------------------------------------
    int cardW = qMin(265, (w - 110) / 4);
    int cardH = qMin(320, (h - 260) * 3 / 4);
    int totalCardsW = 4 * cardW + 3 * 16;
    int startX = (w - totalCardsW) / 2;
    int cardY = 176;

    struct CardData {
        TrackTheme theme;
        QString title;
        QString subtitle;
        QString tag;
        int hills;
        int traffic;
        int difficulty;
        QColor accent;
    } cards[] = {
        { THEME_SUNSET_COAST, "Pacific Coast", "Ocean Highway • Rolling Slopes & Palms", "CRUISING", 2, 3, 2, QColor(255, 140, 40) },
        { THEME_ALPINE_CLIMB, "Alpine Summit", "Hill Climb • Snow Peaks & Crests", "CHALLENGE", 5, 2, 5, QColor(80, 170, 255) },
        { THEME_NEON_METROPOLIS, "Neo City", "Midnight Expressway • 4 Lanes & Skyline", "RUSH HOUR", 1, 5, 4, QColor(255, 40, 160) },
        { THEME_RED_ROCK, "Red Rock Canyon", "Desert Rollercoaster • Sandstone Mesas", "SPEEDWAY", 3, 3, 3, QColor(240, 100, 50) }
    };

    TrackTheme currentTheme = m_engine.getCurrentTheme();

    for (int i = 0; i < 4; ++i) {
        int cx = startX + i * (cardW + 16);
        m_trackCards[i] = QRect(cx, cardY, cardW, cardH);

        bool isSelected = (cards[i].theme == currentTheme);

        p.setBrush(isSelected ? QColor(24, 32, 46) : QColor(14, 18, 26));
        p.setPen(QPen(isSelected ? cards[i].accent : QColor(38, 46, 60), isSelected ? 2.5 : 1));
        p.drawRoundedRect(m_trackCards[i], 8, 8);

        // Preview thumbnail
        const QImage& bgImg = RasterAssets::instance().getBackdrop(cards[i].theme, currentTod);
        QRect thumbRect(cx + 6, cardY + 6, cardW - 12, qMin(105, cardH / 3));
        p.drawImage(thumbRect, bgImg);

        // Tag pill
        p.setBrush(cards[i].accent);
        p.setPen(Qt::NoPen);
        QRect tagRect(cx + 10, cardY + 10, 80, 18);
        p.drawRoundedRect(tagRect, 3, 3);
        QFont tf("Arial", 7, QFont::Bold);
        p.setFont(tf);
        p.setPen(Qt::white);
        p.drawText(tagRect, Qt::AlignCenter, cards[i].tag);

        // Card Title
        int textStartY = cardY + thumbRect.height() + 10;
        QFont ctf("Arial", 11, QFont::Bold);
        p.setFont(ctf);
        p.setPen(Qt::white);
        p.drawText(QRect(cx + 10, textStartY, cardW - 20, 20), Qt::AlignLeft, cards[i].title);

        // Subtitle
        QFont csf("Arial", 7, QFont::Normal);
        p.setFont(csf);
        p.setPen(QColor(155, 170, 190));
        p.drawText(QRect(cx + 10, textStartY + 20, cardW - 20, 28), Qt::AlignLeft, cards[i].subtitle);

        // Rating bars (Hills, Traffic, Difficulty)
        auto drawRating = [&](int ry, const QString& label, int rating, const QColor& col) {
            QFont rf("Arial", 7, QFont::Bold);
            p.setFont(rf);
            p.setPen(QColor(135, 150, 175));
            p.drawText(cx + 10, ry + 9, label);
            for (int dot = 0; dot < 5; ++dot) {
                p.setBrush((dot < rating) ? col : QColor(32, 40, 52));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(cx + cardW - 70 + dot * 11, ry + 1, 7, 8, 2, 2);
            }
        };

        int ratingBaseY = textStartY + 50;
        drawRating(ratingBaseY, "HILLS", cards[i].hills, QColor(0, 220, 255));
        drawRating(ratingBaseY + 18, "TRAFFIC", cards[i].traffic, QColor(255, 180, 40));
        drawRating(ratingBaseY + 36, "DIFFICULTY", cards[i].difficulty, cards[i].accent);

        // Select button
        int btnH = 32;
        QRect btnRect(cx + 10, cardY + cardH - btnH - 10, cardW - 20, btnH);
        p.setBrush(isSelected ? cards[i].accent : QColor(28, 36, 50));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(btnRect, 5, 5);
        QFont bf("Arial", 8, QFont::Bold);
        p.setFont(bf);
        p.setPen(isSelected ? QColor(10, 14, 20) : Qt::white);
        p.drawText(btnRect, Qt::AlignCenter, isSelected ? "CURRENT CIRCUIT ✓" : "SELECT CIRCUIT");
    }

    // -------------------------------------------------------------
    // 4. HERO ACTION BUTTONS ROW (Start/Resume, Repair, Cycle Lights)
    // -------------------------------------------------------------
    int actW = 220;
    int actH = 40;
    int actTotalW = 3 * actW + 2 * 16;
    int actStartX = (w - actTotalW) / 2;
    int actY = cardY + cardH + 12;

    // Hero Start/Resume button
    m_resumeBtn = QRect(actStartX, actY, actW, actH);
    QLinearGradient resumeGrad(actStartX, actY, actStartX + actW, actY + actH);
    resumeGrad.setColorAt(0.0, QColor(0, 220, 120));
    resumeGrad.setColorAt(1.0, QColor(0, 170, 80));
    p.setBrush(resumeGrad);
    p.setPen(QPen(QColor(100, 255, 180), 2));
    p.drawRoundedRect(m_resumeBtn, 6, 6);
    QFont actF("Arial", 10, QFont::Bold);
    p.setFont(actF);
    p.setPen(QColor(8, 20, 12));
    p.drawText(m_resumeBtn, Qt::AlignCenter, (m_engine.getPlayer().speed > 0) ? "▶ RESUME RACE (M / ESC)" : "🏁 START RACE (M / ESC)");

    // Repair Button
    m_repairBtn = QRect(actStartX + actW + 16, actY, actW, actH);
    p.setBrush(QColor(210, 95, 25));
    p.setPen(QPen(QColor(255, 145, 55), 1.5));
    p.drawRoundedRect(m_repairBtn, 6, 6);
    p.setPen(Qt::white);
    p.drawText(m_repairBtn, Qt::AlignCenter, "🔄 REPAIR VEHICLE (KEY R)");

    // Lights Button
    m_lightsBtn = QRect(actStartX + 2 * (actW + 16), actY, actW, actH);
    p.setBrush(QColor(25, 85, 160));
    p.setPen(QPen(QColor(55, 145, 240), 1.5));
    p.drawRoundedRect(m_lightsBtn, 6, 6);
    p.setPen(Qt::white);
    p.drawText(m_lightsBtn, Qt::AlignCenter, "💡 TIME OF DAY (KEY N)");

    // -------------------------------------------------------------
    // 5. BOTTOM CONTROLS CHEATSHEET BANNER
    // -------------------------------------------------------------
    int bottomY = h - 34;
    p.setPen(QColor(135, 155, 180));
    QFont hf("Arial", 8, QFont::Normal);
    p.setFont(hf);
    p.drawText(QRect(0, bottomY, w, 22), Qt::AlignCenter,
               "CONTROLS: [W/S] Gas/Brake • [A/D] Steer • [C] Cam • [F1-F4] Car • [O] 1v1 Multiplayer • [F5-F7] Chat • [M] Menu • [R] Reset");

    p.restore();
}
