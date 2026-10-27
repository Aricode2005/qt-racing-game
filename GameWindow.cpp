#include "GameWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFont>
#include <QRandomGenerator>
#include <QInputDialog>

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
    connect(&net, &NetworkManager::authFailed, this, [this](const QString& reason) {
        m_lastAuthError = reason;
        m_authErrorTimer = 6.0;
        m_engine.getPlayerMutable().lastAnnouncement = QString("AUTH FAILED: %1").arg(reason);
        m_engine.getPlayerMutable().announcementTimer = 3.5;
        update();
    });
    connect(&net, &NetworkManager::hostDiscovered, this, [this](const DiscoveredHost&) {
        if (m_showMainMenu) update();
    });
    connect(&net, &NetworkManager::peerJoined, this, [this](int peerId, const QString& peerName) {
        m_engine.getPlayerMutable().lastAnnouncement = QString("DRIVER [P%1] %2 JOINED ROOM").arg(peerId + 1).arg(peerName);
        m_engine.getPlayerMutable().announcementTimer = 3.5;
        NetworkManager::instance().resetPeersToStartingGrid(m_engine);
        update();
    });
    connect(&net, &NetworkManager::peerLeft, this, [this](int peerId, const QString& peerName) {
        m_engine.getPlayerMutable().lastAnnouncement = QString("DRIVER [P%1] %2 LEFT ROOM").arg(peerId + 1).arg(peerName);
        m_engine.getPlayerMutable().announcementTimer = 3.0;
        m_engine.removeRemotePlayer(peerId);
        update();
    });
    connect(&net, &NetworkManager::peerReadyChanged, this, [this](int, bool) {
        update();
    });
    connect(&net, &NetworkManager::carSyncRequested, this, [this](int, int) {
        NetworkManager::instance().resetPeersToStartingGrid(m_engine);
        update();
    });
    connect(&net, &NetworkManager::trackSyncRequested, this, [this](int theme, int tod) {
        m_engine.setTrackTheme(static_cast<TrackTheme>(theme));
        m_engine.setTimeOfDay(static_cast<TimeOfDay>(tod));
        NetworkManager::instance().resetPeersToStartingGrid(m_engine);
        update();
    });
    connect(&net, &NetworkManager::countdownStarted, this, [this](qreal) {
        m_showMainMenu = false;
        m_engine.resetPlayer();
        NetworkManager::instance().resetPeersToStartingGrid(m_engine);
        update();
    });
    connect(&net, &NetworkManager::raceStarted, this, [this]() {
        m_showMainMenu = false;
        m_engine.getPlayerMutable().lastAnnouncement = "GREEN LIGHT - RACE STARTED";
        m_engine.getPlayerMutable().announcementTimer = 2.0;
        update();
    });
    connect(&net, &NetworkManager::raceRestarted, this, [this]() {
        m_engine.resetPlayer();
        NetworkManager::instance().resetPeersToStartingGrid(m_engine);
        update();
    });
    connect(&net, &NetworkManager::chatMessageReceived, this, [this](const QString& sender, const QString& msg) {
        m_engine.getPlayerMutable().lastAnnouncement = QString("RADIO %1: %2").arg(sender).arg(msg);
        m_engine.getPlayerMutable().announcementTimer = 3.5;
        update();
    });
    connect(&net, &NetworkManager::statusMessage, this, [this](const QString& msg, bool isError) {
        m_engine.getPlayerMutable().lastAnnouncement = (isError ? "ALERT: " : "STATUS: ") + msg;
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

    if (m_authErrorTimer > 0) {
        m_authErrorTimer -= rawDt;
    }

    // High-framerate responsive delta filter
    if (m_smoothDt <= 0.001) m_smoothDt = 0.0166;
    m_smoothDt = m_smoothDt * 0.35 + qBound(0.004, rawDt, 0.040) * 0.65;
    qreal dt = m_smoothDt;
    m_lastDt = dt;

    NetworkManager& net = NetworkManager::instance();

    // Hold vehicles on starting grid during pre-race 3-2-1 countdown
    bool countdownLocked = net.isConnected() && (net.getCountdownTimer() > 0.5);
    if (countdownLocked) {
        m_engine.setThrottle(false);
        m_engine.setBrake(false);
        m_engine.getPlayerMutable().speed = 0.0;
    } else if (!m_showMainMenu) {
        m_engine.setThrottle(m_keyGas || m_mouseGas);
        m_engine.setBrake(m_keyBrake || m_mouseBrake);
    }

    // Network Telemetry & Sync Update
    net.update(dt, m_engine);
    if (net.isConnected()) {
        net.sendTelemetry(m_engine.getPlayer());
    }

    if (!m_showMainMenu) {
        if (!countdownLocked) {
            m_engine.update(dt);
        }

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
        static QImage s_fullImg;
        if (s_fullImg.size() != size()) {
            s_fullImg = QImage(width(), height(), QImage::Format_RGB32);
        }
        {
            QPainter fp(&s_fullImg);
            m_renderer.render(fp, m_engine, width(), height(), m_lastDt);
        }

        int lowW = qMax(64, width() / pSize);
        int lowH = qMax(36, height() / pSize);
        QImage lowImg = s_fullImg.scaled(lowW, lowH, Qt::IgnoreAspectRatio, Qt::FastTransformation);

        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(rect(), lowImg);
    } else {
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
            m_engine.cycleTimeOfDay();
            NetworkManager::instance().notifyTrackChange(
                static_cast<int>(m_engine.getCurrentTheme()),
                static_cast<int>(m_engine.getTimeOfDay()));
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
            NetworkManager::instance().notifyCarChange(0);
            update();
            break;
        case Qt::Key_F2:
            m_engine.setSelectedCar(1);
            NetworkManager::instance().notifyCarChange(1);
            update();
            break;
        case Qt::Key_F3:
            m_engine.setSelectedCar(2);
            NetworkManager::instance().notifyCarChange(2);
            update();
            break;
        case Qt::Key_F4:
            m_engine.setSelectedCar(3);
            NetworkManager::instance().notifyCarChange(3);
            update();
            break;
        case Qt::Key_R:
            m_engine.resetPlayer();
            if (NetworkManager::instance().isConnected()) {
                NetworkManager::instance().resetPeersToStartingGrid(m_engine);
            }
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

            if (m_editNameBtn.contains(pos)) {
                bool ok = false;
                QString text = QInputDialog::getText(this, "Driver Callsign", "Enter your Driver Callsign:", QLineEdit::Normal, net.getLocalPlayerName(), &ok);
                if (ok && !text.trimmed().isEmpty()) {
                    net.setLocalPlayerName(text.trimmed());
                    update();
                }
                return;
            }

            if (!net.isConnected()) {
                if (m_hostRoomIdBox.contains(pos)) {
                    bool ok = false;
                    QString text = QInputDialog::getText(this, "Room ID", "Enter Room ID:", QLineEdit::Normal, m_hostRoomId, &ok);
                    if (ok && !text.trimmed().isEmpty()) {
                        m_hostRoomId = text.trimmed().toUpper();
                        update();
                    }
                    return;
                }
                if (m_hostRandomIdBtn.contains(pos)) {
                    static const QString prefixes[] = { "APEX", "TURBO", "VORTEX", "DRIFT", "NITRO", "CYBER", "HYPER", "MACH" };
                    int pfx = QRandomGenerator::global()->generate() % 8;
                    int num = QRandomGenerator::global()->bounded(100, 999);
                    m_hostRoomId = QString("%1-%2").arg(prefixes[pfx]).arg(num);
                    update();
                    return;
                }
                if (m_hostPassBox.contains(pos)) {
                    bool ok = false;
                    QString text = QInputDialog::getText(this, "Room Password", "Enter Room Password (leave blank for open room):", QLineEdit::Normal, m_hostRoomPassword, &ok);
                    if (ok) {
                        m_hostRoomPassword = text.trimmed();
                        update();
                    }
                    return;
                }
                if (m_createRoomBtn.contains(pos)) {
                    m_lastAuthError.clear();
                    net.notifyCarChange(m_engine.getPlayer().selectedCar);
                    net.notifyTrackChange(
                        static_cast<int>(m_engine.getCurrentTheme()),
                        static_cast<int>(m_engine.getTimeOfDay()));
                    net.startHostRoom(m_hostRoomId, m_hostRoomPassword, 7777, 7778);
                    update();
                    return;
                }
                for (const auto& chip : m_discoveredRoomChips) {
                    if (chip.first.contains(pos)) {
                        m_joinRoomId = chip.second;
                        update();
                        return;
                    }
                }
                if (m_joinRoomIdBox.contains(pos)) {
                    bool ok = false;
                    QString text = QInputDialog::getText(this, "Room ID to Join", "Enter Room ID to join:", QLineEdit::Normal, m_joinRoomId, &ok);
                    if (ok && !text.trimmed().isEmpty()) {
                        m_joinRoomId = text.trimmed().toUpper();
                        update();
                    }
                    return;
                }
                if (m_joinPassBox.contains(pos)) {
                    bool ok = false;
                    QString text = QInputDialog::getText(this, "Room Password", "Enter Room Password:", QLineEdit::Password, m_joinRoomPassword, &ok);
                    if (ok) {
                        m_joinRoomPassword = text.trimmed();
                        update();
                    }
                    return;
                }
                if (m_enterRoomBtn.contains(pos)) {
                    m_lastAuthError.clear();
                    net.notifyCarChange(m_engine.getPlayer().selectedCar);
                    net.joinRoom(m_joinRoomId, m_joinRoomPassword);
                    update();
                    return;
                }
            } else {
                if (m_readyBtn.contains(pos)) {
                    net.setReady(!net.isLocalReady());
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
                    m_engine.clearAllRemotePlayers();
                    update();
                    return;
                }
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
                NetworkManager::instance().notifyTrackChange(
                    static_cast<int>(m_engine.getCurrentTheme()), i);
                update();
                return;
            }
        }

        // 2b. Supercar Selection Buttons (4 Selectable Supercars)
        for (int i = 0; i < 4; ++i) {
            if (m_carButtons[i].contains(pos)) {
                m_engine.setSelectedCar(i);
                NetworkManager::instance().notifyCarChange(i);
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
            if (NetworkManager::instance().isConnected()) {
                NetworkManager::instance().resetPeersToStartingGrid(m_engine);
            }
            m_showMainMenu = false;
            update();
            return;
        }
        if (m_lightsBtn.contains(pos)) {
            m_engine.cycleTimeOfDay();
            NetworkManager::instance().notifyTrackChange(
                static_cast<int>(m_engine.getCurrentTheme()),
                static_cast<int>(m_engine.getTimeOfDay()));
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
            m_engine.resetPlayer();
            if (NetworkManager::instance().isConnected()) {
                NetworkManager::instance().resetPeersToStartingGrid(m_engine);
            }
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

    // Top HUD Camera View Switch Button
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
    NetworkManager::instance().notifyTrackChange(
        static_cast<int>(theme),
        static_cast<int>(m_engine.getTimeOfDay()));
    if (NetworkManager::instance().isConnected()) {
        NetworkManager::instance().resetPeersToStartingGrid(m_engine);
    }
}

void GameWindow::renderMainMenuOverlay(QPainter& p) {
    int w = width();
    int h = height();

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1. Deep Carbon-Slate Telemetry Backdrop
    QLinearGradient bgGrad(0, 0, 0, h);
    bgGrad.setColorAt(0.0, QColor(6, 9, 15, 248));
    bgGrad.setColorAt(0.5, QColor(9, 13, 22, 246));
    bgGrad.setColorAt(1.0, QColor(5, 8, 14, 250));
    p.fillRect(0, 0, w, h, bgGrad);

    // Subtle geometric telemetry grid lines
    p.setPen(QPen(QColor(0, 210, 255, 8), 1));
    for (int gx = -h; gx < w + h; gx += 48) {
        p.drawLine(gx, 0, gx + h, h);
    }

    // Top Accent Horizon Line
    QLinearGradient topBarGrad(0, 0, w, 0);
    topBarGrad.setColorAt(0.0, QColor(0, 210, 255, 0));
    topBarGrad.setColorAt(0.3, QColor(0, 210, 255, 200));
    topBarGrad.setColorAt(0.7, QColor(0, 210, 255, 200));
    topBarGrad.setColorAt(1.0, QColor(0, 210, 255, 0));
    p.fillRect(0, 0, w, 3, topBarGrad);

    // Top Header Tag & Title
    QRect headerPill(w / 2 - 150, 10, 300, 20);
    p.setBrush(QColor(0, 210, 255, 20));
    p.setPen(QPen(QColor(0, 210, 255, 130), 1));
    p.drawRoundedRect(headerPill, 4, 4);
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.setPen(QColor(0, 225, 255));
    p.drawText(headerPill, Qt::AlignCenter, "APEX VELOCITY  //  MOTORSPORT TELEMETRY");

    p.setFont(QFont("Arial", 18, QFont::Black));
    p.setPen(Qt::white);
    p.drawText(QRect(0, 31, w, 24), Qt::AlignCenter, "RACE CONTROL COMMAND CENTER");

    // Segmented Mode Navigation Bar: [ SOLO CHAMPIONSHIP ] | [ MULTIPLAYER ROOMS ]
    int tabW = 248;
    int tabH = 26;
    m_soloTabBtn = QRect(w / 2 - tabW - 6, 57, tabW, tabH);
    m_mpTabBtn   = QRect(w / 2 + 6, 57, tabW, tabH);

    // Solo Tab
    bool soloActive = !m_showMultiplayerTab;
    p.setBrush(soloActive ? QColor(0, 200, 245, 38) : QColor(14, 20, 30, 200));
    p.setPen(QPen(soloActive ? QColor(0, 225, 255) : QColor(42, 54, 72), soloActive ? 1.8 : 1.0));
    p.drawRoundedRect(m_soloTabBtn, 4, 4);
    if (soloActive) {
        p.fillRect(QRect(m_soloTabBtn.x() + 6, m_soloTabBtn.y() + 6, 3, tabH - 12), QColor(0, 235, 255));
    }
    p.setFont(QFont("Arial", 8, QFont::Bold));
    p.setPen(soloActive ? QColor(0, 235, 255) : QColor(145, 165, 190));
    p.drawText(m_soloTabBtn, Qt::AlignCenter, "SOLO CHAMPIONSHIP");

    // Multiplayer Tab
    bool hasPeer = NetworkManager::instance().isConnected();
    QColor mpAccent = hasPeer ? QColor(40, 230, 115) : QColor(255, 185, 40);
    p.setBrush(m_showMultiplayerTab ? QColor(mpAccent.red(), mpAccent.green(), mpAccent.blue(), 38) : QColor(14, 20, 30, 200));
    p.setPen(QPen(m_showMultiplayerTab ? mpAccent : QColor(42, 54, 72), m_showMultiplayerTab ? 1.8 : 1.0));
    p.drawRoundedRect(m_mpTabBtn, 4, 4);
    if (m_showMultiplayerTab || hasPeer) {
        p.fillRect(QRect(m_mpTabBtn.x() + 6, m_mpTabBtn.y() + 6, 3, tabH - 12), mpAccent);
    }
    p.setPen(m_showMultiplayerTab ? mpAccent : (hasPeer ? QColor(40, 230, 115) : QColor(145, 165, 190)));
    p.drawText(m_mpTabBtn, Qt::AlignCenter, hasPeer ? "MULTIPLAYER LOBBY  [ONLINE]" : "MULTIPLAYER ROOMS  [KEY O]");

    if (m_showMultiplayerTab) {
        NetworkManager& net = NetworkManager::instance();
        m_discoveredRoomChips.clear();

        int paneW = qMin(980, w - 60);
        int paneX = (w - paneW) / 2;
        int paneY = 90;

        // 1. Driver Profile & Session Telemetry Bar
        QRect idCard(paneX, paneY, paneW, 54);
        p.setBrush(QColor(13, 18, 28, 235));
        p.setPen(QPen(QColor(38, 52, 74), 1));
        p.drawRoundedRect(idCard, 6, 6);
        p.fillRect(QRect(paneX, paneY + 8, 3, 38), QColor(0, 220, 255));

        // Driver Callsign (clickable to edit)
        m_editNameBtn = QRect(paneX + 14, paneY + 8, 255, 38);
        p.setBrush(QColor(19, 27, 40));
        p.setPen(QPen(QColor(0, 200, 240, 110), 1));
        p.drawRoundedRect(m_editNameBtn, 4, 4);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(QColor(0, 220, 255));
        p.drawText(m_editNameBtn.adjusted(10, 5, -10, -19), Qt::AlignLeft, "DRIVER CALLSIGN  [CLICK TO EDIT]");
        p.setFont(QFont("Arial", 10, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(m_editNameBtn.adjusted(10, 19, -10, -3), Qt::AlignLeft, net.getLocalPlayerName());

        // Vehicle & Circuit Spec
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(QColor(215, 225, 240));
        p.drawText(paneX + 284, paneY + 22, QString("CHASSIS: %1  (F1-F4)").arg(m_engine.getPlayer().carModelName));
        p.setFont(QFont("Arial", 8, QFont::Normal));
        p.setPen(QColor(125, 145, 175));
        p.drawText(paneX + 284, paneY + 39, QString("CIRCUIT: %1  |  ROOM ID & PASSWORD AUTHENTICATION").arg(m_engine.getTrack().getInfo().name.toUpper()));

        // Right Status Indicator (No IP address shown)
        QRect statusBox(paneX + paneW - 265, paneY + 9, 250, 36);
        p.setBrush(QColor(16, 23, 34));
        p.setPen(QPen(hasPeer ? QColor(40, 230, 115, 140) : QColor(45, 62, 86), 1));
        p.drawRoundedRect(statusBox, 4, 4);
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(hasPeer ? QColor(40, 230, 115) : QColor(0, 220, 255));
        p.drawText(statusBox.adjusted(10, 4, -10, -16), Qt::AlignRight,
                   hasPeer ? QString("ROOM ACTIVE: %1").arg(net.getRoomId()) : "ROOM DISCOVERY: AUTOMATIC");
        p.setFont(QFont("Arial", 7, QFont::Normal));
        p.setPen(QColor(140, 160, 185));
        int discCount = net.getDiscoveredHosts().size();
        p.drawText(statusBox.adjusted(10, 20, -10, -2), Qt::AlignRight,
                   hasPeer ? QString("CONNECTED DRIVERS: %1 / 8").arg(net.getConnectedPlayerCount())
                           : (discCount > 0 ? QString("%1 ROOM(S) DETECTED NEARBY").arg(discCount) : "ZERO-IP ROOM ID PROTOCOL"));

        int currentY = paneY + 62;

        // 2. Authentication Error or Connecting Banner
        if (m_authErrorTimer > 0.0 && !m_lastAuthError.isEmpty()) {
            QRect errRect(paneX, currentY, paneW, 34);
            p.setBrush(QColor(150, 22, 32, 235));
            p.setPen(QPen(QColor(255, 80, 90), 1.5));
            p.drawRoundedRect(errRect, 5, 5);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(errRect, Qt::AlignCenter, QString("AUTHENTICATION REJECTED: %1  (VERIFY ROOM ID & PASSWORD)").arg(m_lastAuthError));
            currentY += 40;
        } else if (net.isSearchingRoom()) {
            QRect searchRect(paneX, currentY, paneW, 34);
            p.setBrush(QColor(16, 48, 72, 235));
            p.setPen(QPen(QColor(0, 220, 255), 1.5));
            p.drawRoundedRect(searchRect, 5, 5);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(QColor(0, 235, 255));
            p.drawText(searchRect, Qt::AlignCenter, QString("SEARCHING & AUTHENTICATING WITH ROOM [%1]...").arg(m_joinRoomId));
            currentY += 40;
        }

        // 3. Room Creation & Join Panels (when not connected)
        if (!net.isConnected()) {
            int cardGap = 16;
            int cardW = (paneW - cardGap) / 2;
            int cardH = 264;

            // --- LEFT CARD: CREATE & HOST ROOM ---
            QRect hostCard(paneX, currentY, cardW, cardH);
            p.setBrush(QColor(13, 19, 29, 238));
            p.setPen(QPen(QColor(42, 58, 82), 1.5));
            p.drawRoundedRect(hostCard, 6, 6);
            p.fillRect(QRect(hostCard.x(), hostCard.y(), cardW, 3), QColor(0, 220, 255));

            p.setFont(QFont("Arial", 11, QFont::Bold));
            p.setPen(QColor(0, 225, 255));
            p.drawText(hostCard.x() + 18, currentY + 26, "01  //  HOST PRIVATE ROOM");
            p.setFont(QFont("Arial", 8, QFont::Normal));
            p.setPen(QColor(145, 165, 190));
            p.drawText(hostCard.x() + 18, currentY + 43, "Create a multiplayer lobby secured by Room ID and Password.");

            // Host Room ID Field
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(175, 195, 220));
            p.drawText(hostCard.x() + 18, currentY + 66, "ROOM ID  (CLICK TO EDIT)");
            int idBtnW = 110;
            m_hostRoomIdBox = QRect(hostCard.x() + 18, currentY + 72, cardW - 36 - idBtnW - 8, 32);
            p.setBrush(QColor(19, 27, 40));
            p.setPen(QPen(QColor(0, 200, 240, 140), 1));
            p.drawRoundedRect(m_hostRoomIdBox, 4, 4);
            p.setFont(QFont("Arial", 10, QFont::Bold));
            p.setPen(QColor(0, 235, 255));
            p.drawText(m_hostRoomIdBox.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, m_hostRoomId);

            // Generate Random Room ID Button
            m_hostRandomIdBtn = QRect(hostCard.x() + cardW - 18 - idBtnW, currentY + 72, idBtnW, 32);
            p.setBrush(QColor(24, 36, 54));
            p.setPen(QPen(QColor(55, 85, 125), 1));
            p.drawRoundedRect(m_hostRandomIdBtn, 4, 4);
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(185, 215, 245));
            p.drawText(m_hostRandomIdBtn, Qt::AlignCenter, "GENERATE ID");

            // Host Room Password Field
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(175, 195, 220));
            p.drawText(hostCard.x() + 18, currentY + 122, "ROOM PASSWORD  (CLICK TO EDIT  |  BLANK = OPEN ROOM)");
            m_hostPassBox = QRect(hostCard.x() + 18, currentY + 128, cardW - 36, 32);
            p.setBrush(QColor(19, 27, 40));
            p.setPen(QPen(QColor(0, 200, 240, 140), 1));
            p.drawRoundedRect(m_hostPassBox, 4, 4);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(m_hostRoomPassword.isEmpty() ? QColor(110, 135, 165) : QColor(255, 205, 55));
            p.drawText(m_hostPassBox.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       m_hostRoomPassword.isEmpty() ? "OPEN ACCESS (NO PASSWORD)" : QString("PASSWORD: %1").arg(m_hostRoomPassword));

            // Protocol Info Box
            QRect infoBox(hostCard.x() + 18, currentY + 170, cardW - 36, 34);
            p.setBrush(QColor(10, 15, 24, 190));
            p.setPen(QPen(QColor(30, 42, 60), 1));
            p.drawRoundedRect(infoBox, 4, 4);
            p.setFont(QFont("Arial", 7, QFont::Normal));
            p.setPen(QColor(135, 160, 190));
            p.drawText(infoBox.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       QString("Share Room ID [%1] and Password with other drivers. No IP setup required.").arg(m_hostRoomId));

            // Create & Host Button
            m_createRoomBtn = QRect(hostCard.x() + 18, currentY + cardH - 48, cardW - 36, 36);
            QLinearGradient hostGrad(m_createRoomBtn.topLeft(), m_createRoomBtn.bottomRight());
            hostGrad.setColorAt(0.0, QColor(0, 185, 230));
            hostGrad.setColorAt(1.0, QColor(0, 120, 185));
            p.setBrush(hostGrad);
            p.setPen(QPen(QColor(80, 240, 255), 1.2));
            p.drawRoundedRect(m_createRoomBtn, 5, 5);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(m_createRoomBtn, Qt::AlignCenter, "CREATE & HOST ROOM");

            // --- RIGHT CARD: JOIN ROOM BY ID & PASSWORD ---
            QRect joinCard(paneX + cardW + cardGap, currentY, cardW, cardH);
            p.setBrush(QColor(13, 19, 29, 238));
            p.setPen(QPen(QColor(42, 58, 82), 1.5));
            p.drawRoundedRect(joinCard, 6, 6);
            p.fillRect(QRect(joinCard.x(), joinCard.y(), cardW, 3), QColor(255, 195, 40));

            p.setFont(QFont("Arial", 11, QFont::Bold));
            p.setPen(QColor(255, 205, 55));
            p.drawText(joinCard.x() + 18, currentY + 26, "02  //  JOIN EXISTING ROOM");
            p.setFont(QFont("Arial", 8, QFont::Normal));
            p.setPen(QColor(145, 165, 190));
            p.drawText(joinCard.x() + 18, currentY + 43, "Enter the Room ID and Password to connect automatically.");

            // Target Room ID Input
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(175, 195, 220));
            p.drawText(joinCard.x() + 18, currentY + 66, "ROOM ID  (CLICK TO EDIT)");
            m_joinRoomIdBox = QRect(joinCard.x() + 18, currentY + 72, cardW - 36, 32);
            p.setBrush(QColor(19, 27, 40));
            p.setPen(QPen(QColor(255, 195, 40, 150), 1));
            p.drawRoundedRect(m_joinRoomIdBox, 4, 4);
            p.setFont(QFont("Arial", 10, QFont::Bold));
            p.setPen(QColor(255, 210, 65));
            p.drawText(m_joinRoomIdBox.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, m_joinRoomId);

            // Target Room Password Input
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(175, 195, 220));
            p.drawText(joinCard.x() + 18, currentY + 122, "ROOM PASSWORD  (CLICK TO EDIT)");
            m_joinPassBox = QRect(joinCard.x() + 18, currentY + 128, cardW - 36, 32);
            p.setBrush(QColor(19, 27, 40));
            p.setPen(QPen(QColor(255, 195, 40, 150), 1));
            p.drawRoundedRect(m_joinPassBox, 4, 4);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(m_joinRoomPassword.isEmpty() ? QColor(110, 135, 165) : QColor(255, 210, 65));
            p.drawText(m_joinPassBox.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       m_joinRoomPassword.isEmpty() ? "NO PASSWORD" : QString("PASSWORD: %1").arg(m_joinRoomPassword));

            // Active Rooms Detected Bar (Clickable quick-select pills if any rooms are broadcasting)
            QRect discBox(joinCard.x() + 18, currentY + 170, cardW - 36, 34);
            p.setBrush(QColor(10, 15, 24, 190));
            p.setPen(QPen(QColor(30, 42, 60), 1));
            p.drawRoundedRect(discBox, 4, 4);

            const auto& hosts = net.getDiscoveredHosts();
            if (hosts.isEmpty()) {
                p.setFont(QFont("Arial", 7, QFont::Normal));
                p.setPen(QColor(135, 160, 190));
                p.drawText(discBox.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                           "Auto-resolves rooms on this PC and across local network.");
            } else {
                p.setFont(QFont("Arial", 7, QFont::Bold));
                p.setPen(QColor(40, 230, 115));
                p.drawText(discBox.x() + 10, discBox.y() + 21, "DETECTED ROOMS:");
                int chipX = discBox.x() + 108;
                for (int hi = 0; hi < qMin(3, (int)hosts.size()); ++hi) {
                    QString rId = hosts[hi].roomId;
                    int cw = qMax(76, rId.length() * 8 + 20);
                    if (chipX + cw > discBox.right() - 6) break;
                    QRect chipRect(chipX, discBox.y() + 5, cw, 24);
                    bool isSelectedRoom = (m_joinRoomId.compare(rId, Qt::CaseInsensitive) == 0);
                    p.setBrush(isSelectedRoom ? QColor(40, 230, 115, 50) : QColor(24, 36, 52));
                    p.setPen(QPen(isSelectedRoom ? QColor(40, 230, 115) : QColor(65, 95, 130), 1));
                    p.drawRoundedRect(chipRect, 3, 3);
                    p.setPen(Qt::white);
                    p.drawText(chipRect, Qt::AlignCenter, rId);
                    m_discoveredRoomChips.append(qMakePair(chipRect, rId));
                    chipX += cw + 6;
                }
            }

            // Enter & Join Button
            m_enterRoomBtn = QRect(joinCard.x() + 18, currentY + cardH - 48, cardW - 36, 36);
            QLinearGradient joinGrad(m_enterRoomBtn.topLeft(), m_enterRoomBtn.bottomRight());
            joinGrad.setColorAt(0.0, QColor(235, 155, 20));
            joinGrad.setColorAt(1.0, QColor(180, 100, 10));
            p.setBrush(joinGrad);
            p.setPen(QPen(QColor(255, 210, 65), 1.2));
            p.drawRoundedRect(m_enterRoomBtn, 5, 5);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(m_enterRoomBtn, Qt::AlignCenter, net.isSearchingRoom() ? "LOCATING ROOM..." : "JOIN ROOM");

            currentY += cardH + 12;

            // Guide Footer Banner
            QRect tipCard(paneX, currentY, paneW, 32);
            p.setBrush(QColor(12, 16, 24, 210));
            p.setPen(QPen(QColor(32, 44, 60), 1));
            p.drawRoundedRect(tipCard, 5, 5);
            p.setFont(QFont("Arial", 8, QFont::Normal));
            p.setPen(QColor(145, 170, 195));
            p.drawText(tipCard, Qt::AlignCenter,
                       "QUICK GUIDE: Click [CREATE & HOST ROOM] in one instance, then click [JOIN ROOM] with the same Room ID & Password in another.");

            currentY += 42;
        } else {
            // 4. ACTIVE ROOM LOBBY ROSTER & CONTROLS (when connected)
            int lobbyH = 250;
            QRect lobbyCard(paneX, currentY, paneW, lobbyH);
            p.setBrush(QColor(12, 17, 26, 245));
            p.setPen(QPen(QColor(40, 230, 115, 170), 1.5));
            p.drawRoundedRect(lobbyCard, 6, 6);
            p.fillRect(QRect(paneX, currentY, paneW, 3), QColor(40, 230, 115));

            // Room Info Header Banner inside lobby
            QRect roomBadge(paneX + 16, currentY + 12, 210, 26);
            p.setBrush(QColor(18, 44, 28));
            p.setPen(QPen(QColor(40, 230, 115), 1.2));
            p.drawRoundedRect(roomBadge, 4, 4);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(QColor(40, 230, 115));
            p.drawText(roomBadge, Qt::AlignCenter, QString("ROOM ID: %1").arg(net.getRoomId().isEmpty() ? m_hostRoomId : net.getRoomId()));

            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(Qt::white);
            int totalConnected = net.getConnectedPlayerCount();
            p.drawText(paneX + 240, currentY + 29, QString("ROSTER: %1 / 8 DRIVERS CONNECTED   |   ROLE: %2")
                .arg(totalConnected)
                .arg(net.isHost() ? "ROOM HOST" : QString("DRIVER P%1").arg(net.getMyPeerId() + 1)));

            p.setFont(QFont("Arial", 8, QFont::Bold));
            p.setPen(QColor(0, 220, 255));
            p.drawText(QRect(paneX + paneW - 310, currentY + 16, 294, 18), Qt::AlignRight,
                QString("LATENCY: %1 MS   |   TELEMETRY: LIVE").arg(net.getAveragePingMs()));

            // 8-Player Slot Grid (2 rows x 4 columns)
            int slotCols = 4;
            int slotGap = 8;
            int slotW = (paneW - 32 - (slotCols - 1) * slotGap) / slotCols;
            int slotH = 46;
            const auto& peers = net.getPeers();
            const char* carNames[4] = { "APEX GT-R", "VORTEX V12", "DRIFT SPEC-R", "TITAN ENFORCER" };

            for (int slot = 0; slot < 8; ++slot) {
                int col = slot % slotCols;
                int row = slot / slotCols;
                int sx = paneX + 16 + col * (slotW + slotGap);
                int sy = currentY + 50 + row * (slotH + 8);
                QRect sRect(sx, sy, slotW, slotH);

                if (slot == net.getMyPeerId()) {
                    p.setBrush(QColor(18, 34, 52));
                    p.setPen(QPen(QColor(0, 230, 255), 1.5));
                    p.drawRoundedRect(sRect, 5, 5);
                    p.fillRect(QRect(sx, sy + 6, 3, slotH - 12), QColor(0, 230, 255));
                    p.setFont(QFont("Arial", 8, QFont::Bold));
                    p.setPen(QColor(0, 235, 255));
                    p.drawText(sx + 10, sy + 18, QString("[P%1] %2 (YOU)").arg(slot + 1).arg(net.getLocalPlayerName()));
                    p.setFont(QFont("Arial", 7, QFont::Normal));
                    p.setPen(QColor(175, 200, 225));
                    QString readyStr = (net.isHost() || net.isLocalReady()) ? "READY" : "STANDBY";
                    p.drawText(sx + 10, sy + 36, QString("%1  |  %2").arg(m_engine.getPlayer().carModelName).arg(readyStr));
                } else if (peers.contains(slot)) {
                    const auto& pr = peers[slot];
                    p.setBrush(QColor(20, 28, 40));
                    p.setPen(QPen(QColor(255, 200, 50), 1.2));
                    p.drawRoundedRect(sRect, 5, 5);
                    p.fillRect(QRect(sx, sy + 6, 3, slotH - 12), QColor(255, 200, 50));
                    p.setFont(QFont("Arial", 8, QFont::Bold));
                    p.setPen(QColor(255, 210, 65));
                    p.drawText(sx + 10, sy + 18, QString("[P%1] %2").arg(slot + 1).arg(pr.name));
                    p.setFont(QFont("Arial", 7, QFont::Normal));
                    p.setPen(QColor(165, 185, 210));
                    QString cName = (pr.carModel >= 0 && pr.carModel < 4) ? carNames[pr.carModel] : "APEX GT-R";
                    QString statusStr = (slot == 0) ? "HOST" : (pr.isReady ? "READY" : "STANDBY");
                    p.drawText(sx + 10, sy + 36, QString("%1  |  %2 ms  |  %3").arg(cName).arg(pr.pingMs).arg(statusStr));
                } else {
                    p.setBrush(QColor(14, 18, 25, 150));
                    p.setPen(QPen(QColor(34, 44, 60), 1));
                    p.drawRoundedRect(sRect, 5, 5);
                    p.setFont(QFont("Arial", 7, QFont::Bold));
                    p.setPen(QColor(75, 95, 120));
                    p.drawText(sRect, Qt::AlignCenter, QString("SLOT P%1  //  OPEN").arg(slot + 1));
                }
            }

            // Action Controls Bar
            int actY = currentY + lobbyH - 50;
            int actH = 36;

            // Ready toggle button on left
            m_readyBtn = QRect(paneX + 16, actY, 150, actH);
            bool locReady = net.isLocalReady();
            p.setBrush(locReady ? QColor(20, 55, 38) : QColor(22, 34, 50));
            p.setPen(QPen(locReady ? QColor(40, 230, 115) : QColor(0, 220, 255), 1.2));
            p.drawRoundedRect(m_readyBtn, 5, 5);
            p.setFont(QFont("Arial", 8, QFont::Bold));
            p.setPen(locReady ? QColor(40, 230, 115) : QColor(0, 235, 255));
            p.drawText(m_readyBtn, Qt::AlignCenter, locReady ? "STATUS: READY" : "SET READY");

            // Start Race in Center
            int startW = 360;
            m_startMpBtn = QRect(paneX + (paneW - startW) / 2, actY, startW, actH);
            bool canStart = (net.isHost() && totalConnected >= 2);
            QLinearGradient sGrad(m_startMpBtn.topLeft(), m_startMpBtn.bottomRight());
            if (canStart) {
                sGrad.setColorAt(0.0, QColor(0, 210, 115));
                sGrad.setColorAt(1.0, QColor(0, 155, 75));
            } else if (net.isHost()) {
                sGrad.setColorAt(0.0, QColor(0, 145, 195));
                sGrad.setColorAt(1.0, QColor(0, 95, 145));
            } else {
                sGrad.setColorAt(0.0, QColor(26, 38, 54));
                sGrad.setColorAt(1.0, QColor(18, 28, 42));
            }
            p.setBrush(sGrad);
            p.setPen(QPen(canStart ? QColor(90, 255, 170) : (net.isHost() ? QColor(0, 220, 255) : QColor(50, 70, 95)), 1.5));
            p.drawRoundedRect(m_startMpBtn, 5, 5);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(canStart ? QColor(8, 20, 12) : Qt::white);
            QString startText = net.isHost()
                ? (totalConnected >= 2 ? QString("LAUNCH MULTIPLAYER RACE  (%1 DRIVERS)").arg(totalConnected) : "WAITING FOR DRIVERS  (CLICK TO LAUNCH)")
                : "WAITING FOR HOST TO LAUNCH RACE";
            p.drawText(m_startMpBtn, Qt::AlignCenter, startText);

            // Disconnect / Leave Room on Right
            int discW = 160;
            m_disconnectBtn = QRect(paneX + paneW - 16 - discW, actY, discW, actH);
            p.setBrush(QColor(75, 22, 28));
            p.setPen(QPen(QColor(215, 60, 65), 1.2));
            p.drawRoundedRect(m_disconnectBtn, 5, 5);
            p.setFont(QFont("Arial", 8, QFont::Bold));
            p.setPen(QColor(255, 185, 185));
            p.drawText(m_disconnectBtn, Qt::AlignCenter, net.isHost() ? "CLOSE ROOM" : "LEAVE ROOM");

            currentY += lobbyH + 14;
        }

        // Return button at bottom of multiplayer tab
        m_resumeBtn = QRect(paneX + paneW / 2 - 130, currentY, 260, 34);
        p.setBrush(QColor(22, 32, 46));
        p.setPen(QPen(QColor(60, 85, 115), 1.2));
        p.drawRoundedRect(m_resumeBtn, 5, 5);
        p.setFont(QFont("Arial", 9, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(m_resumeBtn, Qt::AlignCenter, "RETURN TO COCKPIT  [ESC / M]");

        p.restore();
        return;
    }

    // -------------------------------------------------------------
    // 1. AI GRID DIFFICULTY SELECTION ROW (EASY, NORMAL, PRO)
    // -------------------------------------------------------------
    int contentW = qMin(1108, w - 72);
    int contentX = (w - contentW) / 2;
    int diffGap = 14;
    int diffW = (contentW - 2 * diffGap) / 3;
    int diffH = 46;
    int diffY = 88;

    struct DiffData {
        GameDifficulty diff;
        QString title;
        QString desc;
        QColor accent;
    } diffs[] = {
        { DIFFICULTY_EASY, "TIER 1  //  ROOKIE GRID", "6-Car Grid  |  Sprint Distance  |  1.0x Score", QColor(40, 220, 100) },
        { DIFFICULTY_NORMAL, "TIER 2  //  PRO CIRCUIT", "10-Car Grid  |  Standard Distance  |  1.5x Score", QColor(255, 190, 35) },
        { DIFFICULTY_PRO, "TIER 3  //  CHAMPIONSHIP", "16-Car Grid  |  Endurance Distance  |  2.5x Score", QColor(255, 55, 75) }
    };

    GameDifficulty curDiff = m_engine.getDifficulty();

    for (int i = 0; i < 3; ++i) {
        m_diffButtons[i] = QRect(contentX + i * (diffW + diffGap), diffY, diffW, diffH);
        bool isCur = (curDiff == diffs[i].diff);

        p.setBrush(isCur ? QColor(18, 26, 40) : QColor(12, 16, 24));
        p.setPen(QPen(isCur ? diffs[i].accent : QColor(36, 46, 62), isCur ? 1.8 : 1.0));
        p.drawRoundedRect(m_diffButtons[i], 5, 5);
        p.fillRect(QRect(m_diffButtons[i].x(), m_diffButtons[i].y() + 6, 3, diffH - 12),
                   isCur ? diffs[i].accent : QColor(50, 62, 80));

        p.setFont(QFont("Arial", 9, QFont::Bold));
        p.setPen(isCur ? diffs[i].accent : Qt::white);
        p.drawText(QRect(m_diffButtons[i].x() + 12, m_diffButtons[i].y() + 6, m_diffButtons[i].width() - 24, 16), Qt::AlignLeft, diffs[i].title);

        if (isCur) {
            QRect activeBadge(m_diffButtons[i].right() - 68, m_diffButtons[i].y() + 6, 58, 15);
            p.setBrush(QColor(diffs[i].accent.red(), diffs[i].accent.green(), diffs[i].accent.blue(), 35));
            p.setPen(QPen(diffs[i].accent, 1));
            p.drawRoundedRect(activeBadge, 3, 3);
            p.setFont(QFont("Arial", 6, QFont::Bold));
            p.setPen(diffs[i].accent);
            p.drawText(activeBadge, Qt::AlignCenter, "SELECTED");
        }

        p.setFont(QFont("Arial", 7, QFont::Normal));
        p.setPen(QColor(145, 165, 188));
        p.drawText(QRect(m_diffButtons[i].x() + 12, m_diffButtons[i].y() + 24, m_diffButtons[i].width() - 24, 16), Qt::AlignLeft, diffs[i].desc);
    }

    // -------------------------------------------------------------
    // 2. ENVIRONMENT LIGHTING & SUPERCAR SPECIFICATION ROW
    // -------------------------------------------------------------
    int todY = 142;
    int todH = 30;
    int todW = (contentW / 3 - 16) / 3;
    TimeOfDay currentTod = m_engine.getTimeOfDay();

    struct TodData {
        TimeOfDay tod;
        QString label;
        QColor activeCol;
    } tods[] = {
        { TIME_DAY, "DAYLIGHT", QColor(30, 145, 240) },
        { TIME_SUNSET, "SUNSET", QColor(235, 115, 35) },
        { TIME_NIGHT, "MIDNIGHT", QColor(135, 55, 235) }
    };

    for (int i = 0; i < 3; ++i) {
        m_todButtons[i] = QRect(contentX + i * (todW + 8), todY, todW, todH);
        bool isCur = (currentTod == tods[i].tod);

        p.setBrush(isCur ? tods[i].activeCol : QColor(14, 19, 28));
        p.setPen(QPen(isCur ? Qt::white : QColor(38, 48, 64), isCur ? 1.5 : 1.0));
        p.drawRoundedRect(m_todButtons[i], 4, 4);

        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(isCur ? Qt::white : QColor(165, 185, 205));
        p.drawText(m_todButtons[i], Qt::AlignCenter, tods[i].label);
    }

    struct CarOption { QString label; QColor col; } carOpts[4] = {
        { "APEX GT-R  [BALANCED]", QColor(0, 215, 255) },
        { "VORTEX V12  [SPEED]", QColor(255, 60, 75) },
        { "DRIFT SPEC-R  [GRIP]", QColor(255, 185, 35) },
        { "TITAN GT  [ARMOR]", QColor(45, 225, 130) }
    };
    int carStartX = m_todButtons[2].right() + 16;
    int remW = contentX + contentW - carStartX;
    int carBtnW = (remW - 3 * 8) / 4;
    int curCar = m_engine.getPlayer().selectedCar;
    for (int c = 0; c < 4; ++c) {
        m_carButtons[c] = QRect(carStartX + c * (carBtnW + 8), todY, carBtnW, todH);
        bool isSel = (curCar == c);
        p.setBrush(isSel ? QColor(20, 28, 42) : QColor(14, 19, 28));
        p.setPen(QPen(isSel ? carOpts[c].col : QColor(38, 48, 64), isSel ? 1.6 : 1.0));
        p.drawRoundedRect(m_carButtons[c], 4, 4);
        if (isSel) {
            p.fillRect(QRect(m_carButtons[c].x() + 8, m_carButtons[c].bottom() - 3, carBtnW - 16, 2), carOpts[c].col);
        }
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(isSel ? carOpts[c].col : QColor(185, 200, 220));
        p.drawText(m_carButtons[c], Qt::AlignCenter, carOpts[c].label);
    }

    // -------------------------------------------------------------
    // 3. 4 CIRCUIT SELECTION CARDS
    // -------------------------------------------------------------
    int cardGap = 14;
    int cardW = (contentW - 3 * cardGap) / 4;
    int cardH = qMin(315, h - 255);
    int cardY = 180;

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
        { THEME_SUNSET_COAST, "Pacific Coast", "Coastal Highway  |  Rolling Sweeps", "COASTAL", 2, 3, 2, QColor(255, 140, 40) },
        { THEME_ALPINE_CLIMB, "Alpine Summit", "Mountain Pass  |  Steep Elevation", "ALPINE", 5, 2, 5, QColor(75, 175, 255) },
        { THEME_NEON_METROPOLIS, "Neo City", "Urban Expressway  |  High Speed", "METROPOLIS", 1, 5, 4, QColor(245, 55, 160) },
        { THEME_RED_ROCK, "Red Rock Canyon", "Desert Speedway  |  Canyon Crests", "CANYON", 3, 3, 3, QColor(240, 100, 50) }
    };

    TrackTheme currentTheme = m_engine.getCurrentTheme();

    for (int i = 0; i < 4; ++i) {
        int cx = contentX + i * (cardW + cardGap);
        m_trackCards[i] = QRect(cx, cardY, cardW, cardH);

        bool isSelected = (cards[i].theme == currentTheme);

        p.setBrush(isSelected ? QColor(20, 28, 42) : QColor(13, 18, 26));
        p.setPen(QPen(isSelected ? cards[i].accent : QColor(36, 46, 62), isSelected ? 2.0 : 1.0));
        p.drawRoundedRect(m_trackCards[i], 6, 6);

        // Preview thumbnail
        const QImage& bgImg = RasterAssets::instance().getBackdrop(cards[i].theme, currentTod);
        QRect thumbRect(cx + 6, cardY + 6, cardW - 12, qMin(104, cardH / 3));
        p.drawImage(thumbRect, bgImg);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 120), 1));
        p.drawRect(thumbRect);

        // Category Tag Badge
        p.setBrush(QColor(10, 14, 22, 220));
        p.setPen(QPen(cards[i].accent, 1));
        QRect tagRect(cx + 12, cardY + 12, 86, 18);
        p.drawRoundedRect(tagRect, 3, 3);
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.setPen(cards[i].accent);
        p.drawText(tagRect, Qt::AlignCenter, cards[i].tag);

        // Card Title & Subtitle
        int textStartY = cardY + thumbRect.height() + 12;
        p.setFont(QFont("Arial", 11, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(QRect(cx + 12, textStartY, cardW - 24, 20), Qt::AlignLeft, cards[i].title);

        p.setFont(QFont("Arial", 7, QFont::Normal));
        p.setPen(QColor(145, 165, 188));
        p.drawText(QRect(cx + 12, textStartY + 20, cardW - 24, 20), Qt::AlignLeft, cards[i].subtitle);

        // Telemetry Spec Bars (Elevation, Density, Technicality)
        auto drawRating = [&](int ry, const QString& label, int rating, const QColor& col) {
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(130, 150, 175));
            p.drawText(cx + 12, ry + 9, label);
            int barStartX = cx + cardW - 82;
            for (int seg = 0; seg < 5; ++seg) {
                p.setBrush((seg < rating) ? col : QColor(28, 36, 48));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(barStartX + seg * 14, ry + 2, 11, 6, 1, 1);
            }
        };

        int ratingBaseY = textStartY + 46;
        drawRating(ratingBaseY, "ELEVATION", cards[i].hills, QColor(0, 215, 255));
        drawRating(ratingBaseY + 18, "DENSITY", cards[i].traffic, QColor(255, 185, 40));
        drawRating(ratingBaseY + 36, "TECHNICALITY", cards[i].difficulty, cards[i].accent);

        // Select Circuit Button
        int btnH = 30;
        QRect btnRect(cx + 12, cardY + cardH - btnH - 10, cardW - 24, btnH);
        p.setBrush(isSelected ? cards[i].accent : QColor(24, 32, 46));
        p.setPen(QPen(isSelected ? cards[i].accent : QColor(48, 62, 82), 1));
        p.drawRoundedRect(btnRect, 4, 4);
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(isSelected ? QColor(8, 12, 18) : QColor(210, 225, 240));
        p.drawText(btnRect, Qt::AlignCenter, isSelected ? "ACTIVE CIRCUIT" : "SELECT CIRCUIT");
    }

    // -------------------------------------------------------------
    // 4. COMMAND BAR (Start/Resume, Repair, Cycle Lighting)
    // -------------------------------------------------------------
    int actW = 230;
    int actH = 36;
    int actGap = 14;
    int actTotalW = 3 * actW + 2 * actGap;
    int actStartX = (w - actTotalW) / 2;
    int actY = cardY + cardH + 12;

    // Start / Resume Button
    m_resumeBtn = QRect(actStartX, actY, actW, actH);
    QLinearGradient resumeGrad(actStartX, actY, actStartX + actW, actY + actH);
    resumeGrad.setColorAt(0.0, QColor(0, 210, 115));
    resumeGrad.setColorAt(1.0, QColor(0, 160, 80));
    p.setBrush(resumeGrad);
    p.setPen(QPen(QColor(90, 255, 175), 1.5));
    p.drawRoundedRect(m_resumeBtn, 5, 5);
    p.setFont(QFont("Arial", 9, QFont::Bold));
    p.setPen(QColor(8, 20, 12));
    p.drawText(m_resumeBtn, Qt::AlignCenter, (m_engine.getPlayer().speed > 0) ? "RESUME RACE  [ESC / M]" : "START RACE  [ESC / M]");

    // Repair Vehicle Button
    m_repairBtn = QRect(actStartX + actW + actGap, actY, actW, actH);
    p.setBrush(QColor(32, 44, 62));
    p.setPen(QPen(QColor(255, 155, 45), 1.2));
    p.drawRoundedRect(m_repairBtn, 5, 5);
    p.setPen(QColor(255, 190, 95));
    p.drawText(m_repairBtn, Qt::AlignCenter, "RESET & REPAIR  [KEY R]");

    // Cycle Lighting Button
    m_lightsBtn = QRect(actStartX + 2 * (actW + actGap), actY, actW, actH);
    p.setBrush(QColor(24, 38, 58));
    p.setPen(QPen(QColor(0, 200, 245), 1.2));
    p.drawRoundedRect(m_lightsBtn, 5, 5);
    p.setPen(QColor(160, 225, 255));
    p.drawText(m_lightsBtn, Qt::AlignCenter, "CYCLE LIGHTING  [KEY N]");

    // -------------------------------------------------------------
    // 5. BOTTOM CONTROLS TELEMETRY FOOTER
    // -------------------------------------------------------------
    int bottomY = h - 28;
    p.setPen(QColor(125, 145, 170));
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.drawText(QRect(0, bottomY, w, 20), Qt::AlignCenter,
               "CONTROLS:  [W/S] THROTTLE & BRAKE   |   [A/D] STEER   |   [C] CAMERA   |   [F1-F4] VEHICLE   |   [O] MULTIPLAYER   |   [M] MENU   |   [R] RESET");

    p.restore();
}
