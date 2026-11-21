#include "GameWindow.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFont>
#include <QRandomGenerator>
#include <QInputDialog>
#include <QtMath>

GameWindow::GameWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("APEX VELOCITY - First Person Qt Racing Simulator");
    resize(1280, 720);
    setMinimumSize(960, 540);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
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

    // Initialize SQLite User Account Login state
    m_previewCarIdx = m_engine.getPlayer().selectedCar;
    m_previewEngineIdx = m_engine.getPlayer().selectedEngine;
    m_showAccountModal = !m_engine.getProfile().isLoggedIn;
    QStringList existingUsers = m_engine.getProfile().listRegisteredUsernames();
    if (existingUsers.isEmpty()) {
        m_loginRegisterMode = true; // Default to Create Account if DB has no users yet
    } else {
        m_loginRegisterMode = false;
        m_loginUsername = existingUsers.first();
        m_loginFocusField = 1; // Focus password box for quick sign-in
    }
    // Multiplayer Network Signals
    NetworkManager& net = NetworkManager::instance();
    if (!m_engine.getProfile().driverName.isEmpty()) {
        net.setLocalPlayerName(m_engine.getProfile().driverName);
    }
    if (!m_engine.getProfile().relayServer.isEmpty()) {
        net.setRelayServerEndpoint(m_engine.getProfile().relayServer);
    }
    net.notifyCarChange(m_engine.getPlayer().selectedCar);
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
        startTransitionToRace(true);
        update();
    });
    connect(&net, &NetworkManager::raceStarted, this, [this]() {
        startTransitionToRace(false);
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

void GameWindow::startTransitionToRace(bool resetPlayerFirst) {
    if (resetPlayerFirst) {
        m_engine.resetPlayer();
        if (NetworkManager::instance().isConnected()) {
            NetworkManager::instance().resetPeersToStartingGrid(m_engine);
        }
    }
    if (m_showMainMenu) {
        m_isTransitioningToRace = true;
        if (m_menuOpacity < 0.2) {
            m_menuOpacity = 1.0;
        }
        m_raceTransitionFlash = 1.0;
        if (m_engine.getPlayer().announcementTimer <= 0.1) {
            m_engine.getPlayerMutable().lastAnnouncement =
                QString("IGNITION SEQUENCE // %1").arg(m_engine.getTrack().getInfo().name.toUpper());
            m_engine.getPlayerMutable().announcementTimer = 2.2;
        }
    } else {
        m_isTransitioningToRace = false;
        m_menuOpacity = 0.0;
        m_raceTransitionFlash = 0.65;
    }
}

void GameWindow::onGameLoop() {
    qint64 elapsedMs = m_frameTimer.restart();
    qreal rawDt = elapsedMs / 1000.0;
    if (rawDt > 0.05) rawDt = 0.05;

    if (m_authErrorTimer > 0) {
        m_authErrorTimer -= rawDt;
    }
    if (m_garageBannerTimer > 0) {
        m_garageBannerTimer -= rawDt;
    }

    // High-framerate responsive delta filter
    if (m_smoothDt <= 0.001) m_smoothDt = 0.0166;
    m_smoothDt = m_smoothDt * 0.35 + qBound(0.004, rawDt, 0.040) * 0.65;
    qreal dt = m_smoothDt;
    m_lastDt = dt;
    m_menuAnimTime += dt;

    // Smooth Menu-to-Race Fade & Ignition Transition
    if (m_isTransitioningToRace) {
        m_menuOpacity -= dt * 1.85; // ~0.54s smooth cinematic fade-out to race view
        if (m_menuOpacity <= 0.0) {
            m_menuOpacity = 0.0;
            m_isTransitioningToRace = false;
            m_showMainMenu = false;
        }
    } else if (m_showMainMenu) {
        m_menuOpacity = qMin(1.0, m_menuOpacity + dt * 4.5);
    } else {
        m_menuOpacity = qMax(0.0, m_menuOpacity - dt * 4.5);
    }

    if (m_raceTransitionFlash > 0.0) {
        m_raceTransitionFlash = qMax(0.0, m_raceTransitionFlash - dt * 1.35);
    }

    NetworkManager& net = NetworkManager::instance();
    bool activeGameplay = (!m_showMainMenu || m_isTransitioningToRace);

    // Hold vehicles on starting grid during pre-race 3-2-1 countdown
    bool countdownLocked = net.isConnected() && (net.getCountdownTimer() > 0.5);
    if (countdownLocked) {
        m_engine.setThrottle(false);
        m_engine.setBrake(false);
        m_engine.getPlayerMutable().speed = 0.0;
    } else if (activeGameplay) {
        m_engine.setThrottle(m_keyGas || m_mouseGas);
        m_engine.setBrake(m_keyBrake || m_mouseBrake);
    }

    // Network Telemetry & Sync Update
    net.update(dt, m_engine);
    if (net.isConnected()) {
        net.sendTelemetry(m_engine.getPlayer());
    }

    if (activeGameplay) {
        if (!countdownLocked) {
            m_engine.update(dt);
        }

        const PlayerCar& p = m_engine.getPlayer();
        m_audio.updateEngine(p.rpm, p.isAccelerating, p.isBraking, p.speed);
        m_audio.setHorn(p.hornActive);
    } else {
        m_audio.updateEngine(850, false, false, 0); // Subtle showroom idle purr in menu
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
    if (pSize > 1 && (!m_showMainMenu || m_isTransitioningToRace)) {
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

    // Render Main Menu & Difficulty Overlay with smooth cubic fade opacity
    if (m_showMainMenu || m_menuOpacity > 0.005) {
        painter.save();
        qreal clamped = qBound(0.0, m_menuOpacity, 1.0);
        qreal easedOpacity = clamped * clamped * (3.0 - 2.0 * clamped);
        painter.setOpacity(easedOpacity);
        renderMainMenuOverlay(painter);
        painter.restore();
    }

    // Render cinematic Menu-to-Race Ignition & Visor Fade Transition Overlay
    if (m_isTransitioningToRace || m_raceTransitionFlash > 0.005) {
        renderRaceTransitionOverlay(painter);
    }
}

void GameWindow::renderRaceTransitionOverlay(QPainter& p) {
    int w = width();
    int h = height();
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    qreal flash = qBound(0.0, m_raceTransitionFlash, 1.0);
    // Smooth bell curve peaking mid-transition when menu crossfades into cockpit
    qreal bell = qSin(flash * M_PI);

    // 1. Deep Visor Vignette & Cinema Shutter Bars that retract smoothly
    int shutterH = qRound(bell * 34.0);
    if (shutterH > 0) {
        p.fillRect(0, 0, w, shutterH, QColor(4, 7, 12, qRound(220 * bell)));
        p.fillRect(0, h - shutterH, w, shutterH, QColor(4, 7, 12, qRound(220 * bell)));
        p.fillRect(0, shutterH, w, 2, QColor(0, 230, 255, qRound(190 * bell)));
        p.fillRect(0, h - shutterH - 2, w, 2, QColor(40, 235, 130, qRound(190 * bell)));
    }

    // 2. Subtle Cyan-Emerald Ignition Flash Veil
    QRadialGradient veilGrad(w * 0.5, h * 0.5, qMax(w, h) * 0.65);
    veilGrad.setColorAt(0.0, QColor(0, 225, 255, qRound(38 * bell)));
    veilGrad.setColorAt(0.6, QColor(8, 16, 28, qRound(85 * bell)));
    veilGrad.setColorAt(1.0, QColor(3, 6, 12, qRound(165 * bell)));
    p.fillRect(0, 0, w, h, veilGrad);

    // 3. Telemetry Ignition HUD Banner
    if (flash > 0.05) {
        int bannerW = qMin(560, w - 80);
        int bannerH = 44;
        int bannerX = (w - bannerW) / 2;
        int bannerY = shutterH + 18;

        p.setOpacity(qBound(0.0, flash * 1.25, 1.0));
        QRect bannerRect(bannerX, bannerY, bannerW, bannerH);
        p.setBrush(QColor(8, 14, 24, 225));
        p.setPen(QPen(QColor(0, 230, 255, 190), 1.5));
        p.drawRoundedRect(bannerRect, 6, 6);

        // Animated progress sweep bar at bottom of banner
        qreal prog = 1.0 - flash;
        int fillW = qRound((bannerW - 16) * qBound(0.0, prog, 1.0));
        p.fillRect(QRect(bannerX + 8, bannerY + bannerH - 6, bannerW - 16, 3), QColor(24, 38, 56));
        p.fillRect(QRect(bannerX + 8, bannerY + bannerH - 6, fillW, 3), QColor(40, 235, 135));

        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(QColor(40, 235, 135));
        p.drawText(bannerRect.adjusted(14, 6, -14, -18), Qt::AlignLeft | Qt::AlignVCenter,
                   "IGNITION SEQUENCE  //  ENTERING LIVE COCKPIT");

        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(bannerRect.adjusted(14, 6, -14, -18), Qt::AlignRight | Qt::AlignVCenter,
                   QString("%1  @  %2")
                       .arg(m_engine.getPlayer().carModelName)
                       .arg(m_engine.getTrack().getInfo().name.toUpper()));
    }

    p.restore();
}

void GameWindow::keyPressEvent(QKeyEvent *event) {
    if (m_showMainMenu && (m_showAccountModal || !m_engine.getProfile().isLoggedIn)) {
        int k = event->key();
        if (k == Qt::Key_Tab || k == Qt::Key_Down || k == Qt::Key_Up) {
            m_loginFocusField = (m_loginFocusField == 0) ? 1 : 0;
            update();
            return;
        }
        if (k == Qt::Key_Return || k == Qt::Key_Enter) {
            submitAccountLoginOrRegister();
            update();
            return;
        }
        if (k == Qt::Key_Escape && m_engine.getProfile().isLoggedIn) {
            m_showAccountModal = false;
            update();
            return;
        }
        if (k == Qt::Key_Backspace) {
            if (m_loginFocusField == 0 && !m_loginUsername.isEmpty()) {
                m_loginUsername.chop(1);
            } else if (m_loginFocusField == 1 && !m_loginPassword.isEmpty()) {
                m_loginPassword.chop(1);
            }
            update();
            return;
        }
        QString txt = event->text();
        if (!txt.isEmpty() && txt[0].isPrint() && k != Qt::Key_Tab && k != Qt::Key_Return && k != Qt::Key_Enter) {
            if (m_loginFocusField == 0 && m_loginUsername.length() < 24) {
                m_loginUsername += txt;
            } else if (m_loginFocusField == 1 && m_loginPassword.length() < 32) {
                m_loginPassword += txt;
            }
            update();
            return;
        }
        return;
    }

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
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_showMainMenu && !m_isTransitioningToRace) {
                startTransitionToRace(false);
                update();
            }
            break;
        case Qt::Key_M:
        case Qt::Key_T:
        case Qt::Key_Escape:
            if (m_showMainMenu && !m_isTransitioningToRace) {
                startTransitionToRace(false);
            } else {
                m_isTransitioningToRace = false;
                m_showMainMenu = true;
            }
            update();
            break;
        case Qt::Key_O:
            m_menuTab = 2;
            m_showMultiplayerTab = true;
            m_isTransitioningToRace = false;
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
            handleCarSelectionOrPurchase(0);
            break;
        case Qt::Key_F2:
            handleCarSelectionOrPurchase(1);
            break;
        case Qt::Key_F3:
            handleCarSelectionOrPurchase(2);
            break;
        case Qt::Key_F4:
            handleCarSelectionOrPurchase(3);
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
    m_mousePos = pos;

    if (m_showMainMenu && !m_isTransitioningToRace) {
        // If Account Login / Register Modal is active, handle its interactions exclusively
        if (m_showAccountModal || !m_engine.getProfile().isLoggedIn) {
            if (m_loginSignInTabRect.contains(pos)) {
                m_loginRegisterMode = false;
                m_loginStatusMsg.clear();
                update();
                return;
            }
            if (m_loginRegisterTabRect.contains(pos)) {
                m_loginRegisterMode = true;
                m_loginStatusMsg.clear();
                update();
                return;
            }
            if (m_loginUserBoxRect.contains(pos)) {
                m_loginFocusField = 0;
                bool ok = false;
                QString text = QInputDialog::getText(
                    this,
                    m_loginRegisterMode ? "Create Account - Username" : "Sign In - Username",
                    "Enter Driver Account Username:",
                    QLineEdit::Normal,
                    m_loginUsername,
                    &ok);
                if (ok) {
                    m_loginUsername = text.trimmed();
                    m_loginFocusField = 1;
                }
                update();
                return;
            }
            if (m_loginPassBoxRect.contains(pos)) {
                m_loginFocusField = 1;
                bool ok = false;
                QString text = QInputDialog::getText(
                    this,
                    m_loginRegisterMode ? "Create Account - Password" : "Sign In - Password",
                    "Enter Account Password (min 4 chars):",
                    m_showPasswordPlain ? QLineEdit::Normal : QLineEdit::Password,
                    m_loginPassword,
                    &ok);
                if (ok) {
                    m_loginPassword = text;
                }
                update();
                return;
            }
            if (m_loginShowPassRect.contains(pos)) {
                m_showPasswordPlain = !m_showPasswordPlain;
                update();
                return;
            }
            if (m_loginSubmitBtnRect.contains(pos)) {
                m_loginRegisterMode = false;
                submitAccountLoginOrRegister();
                update();
                return;
            }
            if (m_registerDirectBtnRect.contains(pos)) {
                m_loginRegisterMode = true;
                submitAccountLoginOrRegister();
                update();
                return;
            }
            if (m_engine.getProfile().isLoggedIn && m_loginCloseBtnRect.contains(pos)) {
                m_showAccountModal = false;
                update();
                return;
            }
            for (const auto& chip : m_savedUserChips) {
                if (chip.first.contains(pos)) {
                    m_loginUsername = chip.second;
                    m_loginPassword.clear();
                    m_loginRegisterMode = false;
                    m_loginFocusField = 1;
                    m_loginStatusMsg = QString("Selected account '%1'. Enter password to sign in.").arg(chip.second);
                    m_loginStatusIsError = false;
                    update();
                    return;
                }
            }
            return;
        }

        // Top-Left Driver Account Badge click -> Open Switch Account / Logout Portal
        if (m_accountBadgeRect.contains(pos)) {
            m_showAccountModal = true;
            m_loginPassword.clear();
            m_loginStatusMsg = QString("Currently signed in as '%1'. Sign into another account or create a new one.")
                .arg(m_engine.getProfile().username);
            m_loginStatusIsError = false;
            update();
            return;
        }

        if (m_soloTabBtn.contains(pos)) {
            m_menuTab = 0;
            m_showMultiplayerTab = false;
            update();
            return;
        }
        if (m_garageTabBtn.contains(pos)) {
            m_menuTab = 1;
            m_showMultiplayerTab = false;
            m_previewCarIdx = m_engine.getPlayer().selectedCar;
            m_previewEngineIdx = m_engine.getPlayer().selectedEngine;
            update();
            return;
        }
        if (m_mpTabBtn.contains(pos)) {
            m_menuTab = 2;
            m_showMultiplayerTab = true;
            update();
            return;
        }

        // TAB 1: GARAGE (CARS & ENGINES UNLOCK / TUNING SHOP)
        if (m_menuTab == 1) {
            for (int i = 0; i < kCarCount; ++i) {
                if (m_garageCarCards[i].contains(pos)) {
                    int carIdx = kCarDisplayOrder[i];
                    handleCarSelectionOrPurchase(carIdx);
                    return;
                }
            }
            for (int e = 0; e < kEngineCount; ++e) {
                if (m_garageEngineCards[e].contains(pos)) {
                    handleEngineSelectionOrPurchase(e);
                    return;
                }
            }
            if (m_resumeBtn.contains(pos)) {
                startTransitionToRace(false);
                update();
                return;
            }
            if (m_repairBtn.contains(pos)) {
                m_menuTab = 0;
                m_showMultiplayerTab = false;
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

        if (m_showMultiplayerTab || m_menuTab == 2) {
            NetworkManager& net = NetworkManager::instance();

            if (m_editNameBtn.contains(pos)) {
                bool ok = false;
                QString text = QInputDialog::getText(this, "Driver Callsign", "Enter your Driver Callsign:", QLineEdit::Normal, net.getLocalPlayerName(), &ok);
                if (ok && !text.trimmed().isEmpty()) {
                    net.setLocalPlayerName(text.trimmed());
                    m_engine.getProfileMutable().driverName = text.trimmed();
                    m_engine.getProfileMutable().save();
                    update();
                }
                return;
            }

            if (m_relayServerBtn.contains(pos)) {
                bool ok = false;
                QString curRelay = net.getRelayServerEndpoint().isEmpty() ? "127.0.0.1:7779" : net.getRelayServerEndpoint();
                QString text = QInputDialog::getText(
                    this,
                    "Cloud / Cross-LAN Relay Server",
                    "Enter Relay Server host:port to connect across any two LANs (or leave blank for LAN-only):",
                    QLineEdit::Normal,
                    curRelay,
                    &ok);
                if (ok) {
                    net.setRelayServerEndpoint(text.trimmed());
                    m_engine.getProfileMutable().relayServer = net.getRelayServerEndpoint();
                    m_engine.getProfileMutable().save();
                    update();
                }
                return;
            }

            if (m_remoteHostBtn.contains(pos)) {
                bool ok = false;
                QString text = QInputDialog::getText(
                    this,
                    "Direct Remote Host IP / Domain",
                    "Enter Remote Host IP:port or Tunnel (e.g. 203.0.113.5:7777 or AUTO):",
                    QLineEdit::Normal,
                    net.getRemoteHostEndpoint(),
                    &ok);
                if (ok) {
                    net.setRemoteHostEndpoint(text.trimmed().isEmpty() ? "AUTO" : text.trimmed());
                    update();
                }
                return;
            }

            // Multiplayer Lobby Supercar Selector (respects unlocked cars)
            for (int c = 0; c < 4; ++c) {
                if (m_mpCarButtons[c].contains(pos)) {
                    handleCarSelectionOrPurchase(c);
                    return;
                }
            }

            // Multiplayer Lobby Circuit Selector (available pre-connection or to Host)
            for (int t = 0; t < 4; ++t) {
                if (m_mpTrackButtons[t].contains(pos)) {
                    if (!net.isConnected() || net.isHost()) {
                        selectTrack(static_cast<TrackTheme>(t));
                        update();
                    }
                    return;
                }
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
                startTransitionToRace(false);
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

        // 2b. Supercar Selection Buttons (4 Selectable Supercars + Garage Button)
        for (int i = 0; i < 4; ++i) {
            if (m_carButtons[i].contains(pos)) {
                int carIdx = kCarDisplayOrder[i];
                handleCarSelectionOrPurchase(carIdx);
                return;
            }
        }
        if (m_openGarageQuickBtn.contains(pos)) {
            m_menuTab = 1;
            m_showMultiplayerTab = false;
            m_previewCarIdx = m_engine.getPlayer().selectedCar;
            m_previewEngineIdx = m_engine.getPlayer().selectedEngine;
            update();
            return;
        }

        // 3. Track Cards — Select Track & Smooth Fade Transition into Race
        for (int i = 0; i < 4; ++i) {
            if (m_trackCards[i].contains(pos)) {
                selectTrack(static_cast<TrackTheme>(i));
                startTransitionToRace(false);
                update();
                return;
            }
        }

        // 4. Quick Action Buttons
        if (m_resumeBtn.contains(pos)) {
            startTransitionToRace(false);
            update();
            return;
        }
        if (m_repairBtn.contains(pos)) {
            startTransitionToRace(true);
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
        int bw = qMin(660, w - 40);
        int bh = 408;
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
    QRect camSwitchBtn(292, 6, 148, 22);
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
        int btnY = 56;
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
    m_mousePos = event->pos();
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
    } else {
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void GameWindow::submitAccountLoginOrRegister() {
    PlayerProfile& prof = m_engine.getProfileMutable();
    QString err;
    bool ok = false;
    if (m_loginRegisterMode) {
        ok = prof.registerAccount(m_loginUsername, m_loginPassword, err);
    } else {
        ok = prof.loginAccount(m_loginUsername, m_loginPassword, err);
    }

    if (!ok) {
        m_loginStatusMsg = err;
        m_loginStatusIsError = true;
        return;
    }

    // Account authenticated! Sync equipped car, engine, callsign, and online server
    m_showAccountModal = false;
    m_loginPassword.clear();
    m_loginStatusMsg.clear();
    m_loginStatusIsError = false;

    m_engine.setSelectedCar(prof.selectedCar);
    m_engine.setSelectedEngine(prof.selectedEngine);
    m_previewCarIdx = prof.selectedCar;
    m_previewEngineIdx = prof.selectedEngine;

    NetworkManager& net = NetworkManager::instance();
    net.setLocalPlayerName(prof.username);
    net.notifyCarChange(prof.selectedCar);
    if (!prof.relayServer.isEmpty()) {
        net.setRelayServerEndpoint(prof.relayServer);
    }

    m_garageBannerText = QString("WELCOME BACK, %1!   •   %2 COINS AVAILABLE   •   %3 CAREER WINS")
        .arg(prof.username.toUpper())
        .arg(prof.coins)
        .arg(prof.wins);
    m_garageBannerIsError = false;
    m_garageBannerTimer = 5.0;
}

void GameWindow::renderAccountLoginOverlay(QPainter& p, int w, int h) {
    m_savedUserChips.clear();

    // 1. Velvet Obsidian Showroom Backdrop with Dual Studio Spotlights
    QLinearGradient veil(0, 0, 0, h);
    veil.setColorAt(0.0, QColor(4, 7, 14, 228));
    veil.setColorAt(0.5, QColor(7, 11, 22, 215));
    veil.setColorAt(1.0, QColor(3, 5, 10, 238));
    p.fillRect(0, 0, w, h, veil);

    qreal pulse = 0.5 + 0.5 * qSin(m_menuAnimTime * 2.2);
    QRadialGradient goldGlow(w * 0.38, h * 0.28, w * 0.42);
    goldGlow.setColorAt(0.0, QColor(255, 195, 55, qRound(26 + 10 * pulse)));
    goldGlow.setColorAt(1.0, QColor(255, 195, 55, 0));
    p.fillRect(0, 0, w, h, goldGlow);

    QRadialGradient cyanGlow(w * 0.62, h * 0.72, w * 0.42);
    cyanGlow.setColorAt(0.0, QColor(0, 215, 255, qRound(24 + 8 * pulse)));
    cyanGlow.setColorAt(1.0, QColor(0, 215, 255, 0));
    p.fillRect(0, 0, w, h, cyanGlow);

    int cardW = qMin(590, w - 56);
    int cardH = 466;
    int cx = (w - cardW) / 2;
    int cy = (h - cardH) / 2;
    QRect card(cx, cy, cardW, cardH);

    // Soft Outer Drop Halo
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 130));
    p.drawRoundedRect(card.adjusted(-6, -4, 6, 8), 16, 16);

    // Main Frosted Glass Card Body
    QLinearGradient bg(card.topLeft(), card.bottomRight());
    bg.setColorAt(0.0, QColor(15, 23, 38, 250));
    bg.setColorAt(0.45, QColor(10, 16, 28, 252));
    bg.setColorAt(1.0, QColor(7, 11, 20, 254));
    p.setBrush(bg);
    p.setPen(QPen(QColor(255, 210, 75, 175), 1.8));
    p.drawRoundedRect(card, 14, 14);

    // Inner Glass Rim Frame
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 28), 1.0));
    p.drawRoundedRect(card.adjusted(2, 2, -2, -2), 12, 12);

    // Top Champagne-Gold & Cyan Horizon Ribbon
    QLinearGradient topRibbon(cx + 16, cy, cx + cardW - 16, cy);
    topRibbon.setColorAt(0.0, QColor(0, 215, 255, 0));
    topRibbon.setColorAt(0.25, QColor(0, 225, 255, 220));
    topRibbon.setColorAt(0.5, QColor(255, 215, 75, 255));
    topRibbon.setColorAt(0.75, QColor(0, 225, 255, 220));
    topRibbon.setColorAt(1.0, QColor(0, 215, 255, 0));
    p.fillRect(QRect(cx + 16, cy + 1, cardW - 32, 3), topRibbon);

    // 2. Hero Supercar Showroom Banner inside Login Card
    QRect heroBanner(cx + 18, cy + 14, cardW - 36, 78);
    QLinearGradient heroGrad(heroBanner.topLeft(), heroBanner.bottomRight());
    heroGrad.setColorAt(0.0, QColor(22, 34, 54, 230));
    heroGrad.setColorAt(0.5, QColor(14, 22, 36, 235));
    heroGrad.setColorAt(1.0, QColor(26, 22, 16, 230));
    p.setBrush(heroGrad);
    p.setPen(QPen(QColor(55, 78, 110, 160), 1.0));
    p.drawRoundedRect(heroBanner, 9, 9);

    // Left & Right Hero Supercars flanking the Apex Crest
    const QImage& leftCar = RasterAssets::instance().getVehicleSide(VEHICLE_SPORTS_RED, false);
    const QImage& rightCar = RasterAssets::instance().getVehicle(VEHICLE_SEDAN_SILVER, true, true, false, false);
    p.drawImage(QRect(heroBanner.x() + 10, heroBanner.y() + 14, 138, 52), leftCar);
    p.drawImage(QRect(heroBanner.right() - 104, heroBanner.y() + 8, 92, 62), rightCar);

    // Center Championship Title inside Hero Banner
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.setPen(QColor(255, 210, 75));
    p.drawText(QRect(heroBanner.x(), heroBanner.y() + 10, heroBanner.width(), 14), Qt::AlignCenter,
               "APEX VELOCITY   •   WORLD TOUR");

    p.setFont(QFont("Arial", 15, QFont::Black));
    p.setPen(Qt::white);
    p.drawText(QRect(heroBanner.x(), heroBanner.y() + 26, heroBanner.width(), 24), Qt::AlignCenter,
               "DRIVER VIP LOUNGE");

    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.setPen(QColor(135, 225, 255));
    p.drawText(QRect(heroBanner.x(), heroBanner.y() + 52, heroBanner.width(), 16), Qt::AlignCenter,
               "PERSONAL GARAGE   •   COIN VAULT   •   CAREER LICENSE");

    if (m_engine.getProfile().isLoggedIn) {
        m_loginCloseBtnRect = QRect(heroBanner.right() - 74, heroBanner.y() + 6, 66, 22);
        p.setBrush(QColor(24, 34, 50, 230));
        p.setPen(QPen(QColor(130, 160, 195), 1.0));
        p.drawRoundedRect(m_loginCloseBtnRect, 4, 4);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(m_loginCloseBtnRect, Qt::AlignCenter, "✕ BACK");
    } else {
        m_loginCloseBtnRect = QRect();
    }

    // 3. Mode Switcher Pills: [ SIGN IN ] | [ NEW DRIVER (+500 COINS) ]
    int tabY = heroBanner.bottom() + 12;
    int tabW = (cardW - 46) / 2;
    m_loginSignInTabRect   = QRect(cx + 18, tabY, tabW, 32);
    m_loginRegisterTabRect = QRect(cx + 28 + tabW, tabY, tabW, 32);

    bool signAct = !m_loginRegisterMode;
    QLinearGradient sgGrad(m_loginSignInTabRect.topLeft(), m_loginSignInTabRect.bottomLeft());
    sgGrad.setColorAt(0.0, signAct ? QColor(0, 195, 245, 75) : QColor(18, 26, 40));
    sgGrad.setColorAt(1.0, signAct ? QColor(0, 120, 180, 45) : QColor(13, 19, 30));
    p.setBrush(sgGrad);
    p.setPen(QPen(signAct ? QColor(0, 235, 255) : QColor(45, 62, 86), signAct ? 1.8 : 1.0));
    p.drawRoundedRect(m_loginSignInTabRect, 6, 6);
    p.setFont(QFont("Arial", 8, QFont::Black));
    p.setPen(signAct ? QColor(120, 248, 255) : QColor(155, 175, 200));
    p.drawText(m_loginSignInTabRect, Qt::AlignCenter, "SIGN IN TO LICENSE");

    bool regAct = m_loginRegisterMode;
    QLinearGradient rgGrad(m_loginRegisterTabRect.topLeft(), m_loginRegisterTabRect.bottomLeft());
    rgGrad.setColorAt(0.0, regAct ? QColor(255, 195, 45, 80) : QColor(18, 26, 40));
    rgGrad.setColorAt(1.0, regAct ? QColor(195, 125, 15, 45) : QColor(13, 19, 30));
    p.setBrush(rgGrad);
    p.setPen(QPen(regAct ? QColor(255, 215, 65) : QColor(45, 62, 86), regAct ? 1.8 : 1.0));
    p.drawRoundedRect(m_loginRegisterTabRect, 6, 6);
    p.setPen(regAct ? QColor(255, 230, 100) : QColor(155, 175, 200));
    p.drawText(m_loginRegisterTabRect, Qt::AlignCenter, "NEW DRIVER  (+500 COINS 🪙)");

    // 4. Status / Welcome Banner
    int formY = tabY + 40;
    QRect statusRect(cx + 18, formY, cardW - 36, 28);
    if (!m_loginStatusMsg.isEmpty()) {
        p.setBrush(m_loginStatusIsError ? QColor(115, 20, 32, 235) : QColor(14, 52, 72, 235));
        p.setPen(QPen(m_loginStatusIsError ? QColor(255, 95, 105) : QColor(0, 225, 255), 1.3));
        p.drawRoundedRect(statusRect, 5, 5);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(statusRect, Qt::AlignCenter, m_loginStatusMsg);
    } else {
        p.setBrush(QColor(14, 21, 34, 210));
        p.setPen(QPen(QColor(38, 54, 76), 1.0));
        p.drawRoundedRect(statusRect, 5, 5);
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(m_loginRegisterMode ? QColor(255, 220, 95) : QColor(175, 205, 235));
        p.drawText(statusRect, Qt::AlignCenter,
                   m_loginRegisterMode
                       ? "Create your Driver License — receive 500 Starting Coins & the APEX GT-R!"
                       : "Sign in to access your accumulated Coins, Supercars, and Custom Engines.");
    }

    bool cursorOn = (qSin(m_menuAnimTime * 6.5) >= 0.0);

    // 5. Driver Name Input Field with Emblem Badge
    int uY = formY + 34;
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.setPen(QColor(175, 200, 228));
    p.drawText(cx + 20, uY + 11, "DRIVER CALLSIGN");
    m_loginUserBoxRect = QRect(cx + 18, uY + 16, cardW - 36, 38);
    bool uFocus = (m_loginFocusField == 0);
    p.setBrush(uFocus ? QColor(20, 32, 50) : QColor(13, 20, 32));
    p.setPen(QPen(uFocus ? QColor(0, 235, 255) : QColor(52, 72, 98), uFocus ? 1.8 : 1.1));
    p.drawRoundedRect(m_loginUserBoxRect, 6, 6);

    // Left Icon Pill inside Username Box
    QRect uIcon(m_loginUserBoxRect.x() + 6, m_loginUserBoxRect.y() + 6, 26, 26);
    p.setBrush(uFocus ? QColor(0, 210, 255, 55) : QColor(22, 34, 52));
    p.setPen(QPen(uFocus ? QColor(0, 235, 255) : QColor(65, 90, 120), 1.0));
    p.drawRoundedRect(uIcon, 4, 4);
    p.setFont(QFont("Arial", 8, QFont::Black));
    p.setPen(uFocus ? QColor(110, 245, 255) : QColor(145, 170, 200));
    p.drawText(uIcon, Qt::AlignCenter, "ID");

    p.setFont(QFont("Arial", 10, QFont::Bold));
    p.setPen(m_loginUsername.isEmpty() ? QColor(95, 118, 145) : Qt::white);
    QString uCursor = (uFocus && cursorOn) ? " ▋" : "";
    QString uDisp = m_loginUsername.isEmpty()
        ? (uFocus ? "Type your driver name..." : "Click or type driver name...")
        : (m_loginUsername + uCursor);
    p.drawText(m_loginUserBoxRect.adjusted(40, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, uDisp);

    // 6. Password Input Field with Key Emblem & [VIEW / HIDE] Button
    int pY = uY + 60;
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.setPen(QColor(175, 200, 228));
    p.drawText(cx + 20, pY + 11, "SECURITY PASSKEY");
    int showBtnW = 78;
    m_loginPassBoxRect  = QRect(cx + 18, pY + 16, cardW - 36 - showBtnW - 8, 38);
    m_loginShowPassRect = QRect(card.right() - 18 - showBtnW, pY + 16, showBtnW, 38);

    bool pFocus = (m_loginFocusField == 1);
    p.setBrush(pFocus ? QColor(20, 32, 50) : QColor(13, 20, 32));
    p.setPen(QPen(pFocus ? QColor(255, 210, 60) : QColor(52, 72, 98), pFocus ? 1.8 : 1.1));
    p.drawRoundedRect(m_loginPassBoxRect, 6, 6);

    QRect pIcon(m_loginPassBoxRect.x() + 6, m_loginPassBoxRect.y() + 6, 26, 26);
    p.setBrush(pFocus ? QColor(255, 195, 45, 55) : QColor(22, 34, 52));
    p.setPen(QPen(pFocus ? QColor(255, 215, 65) : QColor(65, 90, 120), 1.0));
    p.drawRoundedRect(pIcon, 4, 4);
    p.setFont(QFont("Arial", 7, QFont::Black));
    p.setPen(pFocus ? QColor(255, 225, 90) : QColor(145, 170, 200));
    p.drawText(pIcon, Qt::AlignCenter, "KEY");

    QString maskedPass = m_showPasswordPlain ? m_loginPassword : QString(m_loginPassword.length(), QChar(0x2022));
    p.setFont(QFont("Arial", 10, QFont::Bold));
    p.setPen(m_loginPassword.isEmpty() ? QColor(95, 118, 145) : QColor(255, 225, 90));
    QString pCursor = (pFocus && cursorOn) ? " ▋" : "";
    QString pDisp = m_loginPassword.isEmpty()
        ? (pFocus ? "Type your passkey..." : "Click or type passkey...")
        : (maskedPass + pCursor);
    p.drawText(m_loginPassBoxRect.adjusted(40, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, pDisp);

    bool showHov = m_loginShowPassRect.contains(m_mousePos);
    p.setBrush(m_showPasswordPlain ? QColor(36, 54, 78) : (showHov ? QColor(26, 38, 56) : QColor(18, 26, 40)));
    p.setPen(QPen(m_showPasswordPlain ? QColor(0, 225, 255) : QColor(68, 92, 124), 1.1));
    p.drawRoundedRect(m_loginShowPassRect, 6, 6);
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.setPen(QColor(205, 225, 248));
    p.drawText(m_loginShowPassRect, Qt::AlignCenter, m_showPasswordPlain ? "HIDE KEY" : "VIEW KEY");

    // 7. Side-by-Side Luxury Action Buttons: [ ▶ LOGIN TO GARAGE ] and [ ✚ REGISTER (+500 COINS) ]
    int subY = pY + 64;
    int btnGap = 12;
    int halfBtnW = (cardW - 36 - btnGap) / 2;
    m_loginSubmitBtnRect    = QRect(cx + 18, subY, halfBtnW, 42);
    m_registerDirectBtnRect = QRect(cx + 18 + halfBtnW + btnGap, subY, halfBtnW, 42);

    // Button 1: LOGIN
    bool logHov = m_loginSubmitBtnRect.contains(m_mousePos);
    QLinearGradient logGrad(m_loginSubmitBtnRect.topLeft(), m_loginSubmitBtnRect.bottomRight());
    logGrad.setColorAt(0.0, logHov ? QColor(35, 245, 150) : QColor(10, 215, 125));
    logGrad.setColorAt(1.0, logHov ? QColor(0, 180, 95) : QColor(0, 145, 75));
    p.setBrush(logGrad);
    p.setPen(QPen(!m_loginRegisterMode ? Qt::white : QColor(110, 255, 185), (!m_loginRegisterMode || logHov) ? 2.0 : 1.2));
    p.drawRoundedRect(m_loginSubmitBtnRect, 7, 7);
    p.setFont(QFont("Arial", 9, QFont::Black));
    p.setPen(QColor(5, 18, 10));
    p.drawText(m_loginSubmitBtnRect, Qt::AlignCenter, "▶  SIGN IN & ENTER");

    // Button 2: REGISTER
    bool regHov = m_registerDirectBtnRect.contains(m_mousePos);
    QLinearGradient regGrad(m_registerDirectBtnRect.topLeft(), m_registerDirectBtnRect.bottomRight());
    regGrad.setColorAt(0.0, regHov ? QColor(255, 225, 85) : QColor(245, 192, 42));
    regGrad.setColorAt(1.0, regHov ? QColor(220, 150, 18) : QColor(185, 120, 10));
    p.setBrush(regGrad);
    p.setPen(QPen(m_loginRegisterMode ? Qt::white : QColor(255, 235, 140), (m_loginRegisterMode || regHov) ? 2.0 : 1.2));
    p.drawRoundedRect(m_registerDirectBtnRect, 7, 7);
    p.setFont(QFont("Arial", 9, QFont::Black));
    p.setPen(QColor(24, 14, 4));
    p.drawText(m_registerDirectBtnRect, Qt::AlignCenter, "✚  REGISTER (+500 🪙)");

    // 8. Saved Driver Profiles Bar at Bottom
    int chipY = subY + 52;
    QRect chipBar(cx + 18, chipY, cardW - 36, 46);
    p.setBrush(QColor(10, 15, 25, 225));
    p.setPen(QPen(QColor(36, 52, 74), 1.0));
    p.drawRoundedRect(chipBar, 6, 6);

    QStringList savedUsers = m_engine.getProfile().listRegisteredUsernames();
    p.setFont(QFont("Arial", 6, QFont::Bold));
    p.setPen(QColor(150, 178, 208));
    if (savedUsers.isEmpty()) {
        p.drawText(chipBar, Qt::AlignCenter,
                   "NEW DRIVER BONUS: Register your Driver Callsign above to claim 500 Starting Coins!");
    } else {
        p.drawText(QRect(chipBar.x() + 12, chipBar.y() + 4, chipBar.width() - 24, 13), Qt::AlignLeft,
                   "REGISTERED DRIVERS  (CLICK TO SELECT PROFILE):");
        int curChipX = chipBar.x() + 12;
        for (const QString& uName : savedUsers) {
            int cw = qMax(74, int(uName.length()) * 7 + 26);
            if (curChipX + cw > chipBar.right() - 8) break;
            QRect cr(curChipX, chipBar.y() + 20, cw, 21);
            bool sel = (m_loginUsername.compare(uName, Qt::CaseInsensitive) == 0);
            bool chHov = cr.contains(m_mousePos);
            p.setBrush(sel ? QColor(0, 215, 255, 60) : (chHov ? QColor(28, 42, 62) : QColor(18, 28, 44)));
            p.setPen(QPen(sel ? QColor(0, 235, 255) : QColor(62, 88, 120), 1.1));
            p.drawRoundedRect(cr, 4, 4);
            p.setPen(sel ? QColor(130, 248, 255) : Qt::white);
            p.drawText(cr, Qt::AlignCenter, uName);
            m_savedUserChips.append(qMakePair(cr, uName));
            curChipX += cw + 6;
        }
    }
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

void GameWindow::handleCarSelectionOrPurchase(int carIdx) {
    carIdx = qBound(0, carIdx, kCarCount - 1);
    m_previewCarIdx = carIdx;
    PlayerProfile& prof = m_engine.getProfileMutable();
    const CarSpec& spec = kCars[carIdx];

    if (prof.ownsCar(carIdx)) {
        m_engine.setSelectedCar(carIdx);
        NetworkManager::instance().notifyCarChange(carIdx);
        m_garageBannerText = QString("CHASSIS EQUIPPED: %1  —  %2").arg(spec.name).arg(spec.feature);
        m_garageBannerIsError = false;
        m_garageBannerTimer = 4.0;
    } else if (prof.buyCar(carIdx)) {
        m_engine.setSelectedCar(carIdx);
        NetworkManager::instance().notifyCarChange(carIdx);
        m_garageBannerText = QString("UNLOCKED & EQUIPPED: %1 (-%2 COINS)!  %3")
            .arg(spec.name).arg(spec.price).arg(spec.feature);
        m_garageBannerIsError = false;
        m_garageBannerTimer = 5.0;
    } else {
        int needed = spec.price - prof.coins;
        m_garageBannerText = QString("LOCKED SUPERCAR: %1 REQUIRES %2 COINS (NEED %3 MORE COINS — RACE TO EARN!)")
            .arg(spec.name).arg(spec.price).arg(needed);
        m_garageBannerIsError = true;
        m_garageBannerTimer = 4.5;
    }
    update();
}

void GameWindow::handleEngineSelectionOrPurchase(int engineIdx) {
    engineIdx = qBound(0, engineIdx, kEngineCount - 1);
    m_previewEngineIdx = engineIdx;
    PlayerProfile& prof = m_engine.getProfileMutable();
    const EngineSpec& eng = kEngines[engineIdx];

    if (prof.ownsEngine(engineIdx)) {
        m_engine.setSelectedEngine(engineIdx);
        m_garageBannerText = QString("POWERTRAIN INSTALLED: %1 (%2 HP)  —  %3")
            .arg(eng.name).arg(eng.horsepower).arg(eng.feature);
        m_garageBannerIsError = false;
        m_garageBannerTimer = 4.0;
    } else if (prof.buyEngine(engineIdx)) {
        m_engine.setSelectedEngine(engineIdx);
        m_garageBannerText = QString("UNLOCKED & INSTALLED: %1 (%2 HP, -%3 COINS)!  %4")
            .arg(eng.name).arg(eng.horsepower).arg(eng.price).arg(eng.feature);
        m_garageBannerIsError = false;
        m_garageBannerTimer = 5.0;
    } else {
        int needed = eng.price - prof.coins;
        m_garageBannerText = QString("LOCKED POWERTRAIN: %1 REQUIRES %2 COINS (NEED %3 MORE COINS!)")
            .arg(eng.name).arg(eng.price).arg(needed);
        m_garageBannerIsError = true;
        m_garageBannerTimer = 4.5;
    }
    update();
}

void GameWindow::renderGarageTab(QPainter& p, int contentX, int contentW, int topY, int h) {
    const PlayerProfile& prof = m_engine.getProfile();
    const PlayerCar& player = m_engine.getPlayer();

    int curY = topY;
    // Status / Purchase Feedback Banner
    QRect bannerRect(contentX, curY, contentW, 28);
    if (m_garageBannerTimer > 0.0 && !m_garageBannerText.isEmpty()) {
        p.setBrush(m_garageBannerIsError ? QColor(125, 22, 32, 235) : QColor(14, 58, 42, 235));
        p.setPen(QPen(m_garageBannerIsError ? QColor(255, 85, 95) : QColor(45, 240, 135), 1.5));
        p.drawRoundedRect(bannerRect, 6, 6);
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(bannerRect, Qt::AlignCenter, m_garageBannerText);
    } else {
        p.setBrush(QColor(13, 20, 32, 230));
        p.setPen(QPen(QColor(46, 64, 90), 1.0));
        p.drawRoundedRect(bannerRect, 6, 6);
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(QColor(255, 218, 85));
        p.drawText(bannerRect, Qt::AlignCenter,
                   "CUSTOMIZATION SHOWROOM: Select any Owned Car or Engine to Equip, or click any Locked item to Unlock with Coins!");
    }
    curY += 34;

    int colGap = 12;
    int leftW = qRound(contentW * 0.34);
    int rightW = qRound(contentW * 0.34);
    int centerW = contentW - leftW - rightW - 2 * colGap;
    int panelH = qMin(430, h - curY - 76);

    // =============================================================
    // LEFT COLUMN: 4 SUPERCAR CHASSIS CARDS (WITH LOCKS & FEATURES)
    // =============================================================
    QRect leftHdr(contentX, curY, leftW, 22);
    p.setFont(QFont("Arial", 8, QFont::Black));
    p.setPen(QColor(0, 230, 255));
    p.drawText(leftHdr, Qt::AlignLeft | Qt::AlignVCenter, "01  •  SUPERCAR LINEUP (4 CHASSIS)");

    int carCardY = curY + 24;
    int carGap = 7;
    int carCardH = (panelH - 24 - 3 * carGap) / 4;

    for (int i = 0; i < kCarCount; ++i) {
        int cIdx = kCarDisplayOrder[i];
        const CarSpec& cs = kCars[cIdx];
        QColor accent(cs.accentRgb);
        bool owned = prof.ownsCar(cIdx);
        bool equipped = (player.selectedCar == cIdx);
        bool previewed = (m_previewCarIdx == cIdx);
        bool canAfford = (prof.coins >= cs.price);

        QRect card(contentX, carCardY + i * (carCardH + carGap), leftW, carCardH);
        m_garageCarCards[i] = card;
        bool hov = card.contains(m_mousePos);

        QLinearGradient cg(card.topLeft(), card.bottomRight());
        if (equipped) {
            cg.setColorAt(0.0, QColor(22, 38, 60, 248));
            cg.setColorAt(1.0, QColor(12, 22, 36, 248));
        } else if (owned) {
            cg.setColorAt(0.0, hov ? QColor(22, 34, 52, 242) : QColor(14, 21, 32, 235));
            cg.setColorAt(1.0, QColor(10, 15, 24, 235));
        } else {
            cg.setColorAt(0.0, hov ? QColor(28, 24, 34, 242) : QColor(14, 15, 22, 230));
            cg.setColorAt(1.0, QColor(9, 10, 16, 235));
        }
        p.setBrush(cg);
        p.setPen(QPen(equipped ? accent : (previewed ? QColor(255, 215, 75) : (hov ? accent.lighter(115) : QColor(40, 56, 78))),
                      equipped ? 2.0 : 1.2));
        p.drawRoundedRect(card, 7, 7);
        p.fillRect(QRect(card.x(), card.y() + 6, 4, card.height() - 12), owned ? accent : QColor(110, 120, 138));

        // Car miniature sprite
        const QImage& spr = RasterAssets::instance().getVehicle(cs.sprite, true, false, false, false);
        QRect sprRect(card.x() + 10, card.y() + (card.height() - 44) / 2, 58, 44);
        p.drawImage(sprRect, spr);

        // Car Name & Tagline
        int tx = card.x() + 74;
        int tw = card.width() - 82;
        p.setFont(QFont("Arial", 9, QFont::Black));
        p.setPen(owned ? Qt::white : QColor(195, 205, 220));
        p.drawText(QRect(tx, card.y() + 5, tw - 92, 16), Qt::AlignLeft | Qt::AlignVCenter, cs.name);

        // Status / Price Pill on top-right of card
        QRect badge(card.right() - 94, card.y() + 5, 88, 17);
        if (equipped) {
            p.setBrush(QColor(40, 235, 130, 50));
            p.setPen(QPen(QColor(40, 235, 130), 1.2));
            p.drawRoundedRect(badge, 4, 4);
            p.setFont(QFont("Arial", 6, QFont::Black));
            p.setPen(QColor(60, 250, 145));
            p.drawText(badge, Qt::AlignCenter, "✓ EQUIPPED");
        } else if (owned) {
            p.setBrush(QColor(0, 210, 255, 35));
            p.setPen(QPen(QColor(0, 215, 255, 160), 1.0));
            p.drawRoundedRect(badge, 4, 4);
            p.setFont(QFont("Arial", 6, QFont::Bold));
            p.setPen(QColor(130, 235, 255));
            p.drawText(badge, Qt::AlignCenter, "OWNED • EQUIP");
        } else {
            p.setBrush(canAfford ? QColor(255, 195, 35, 55) : QColor(45, 30, 36, 180));
            p.setPen(QPen(canAfford ? QColor(255, 210, 60) : QColor(165, 75, 85), 1.2));
            p.drawRoundedRect(badge, 4, 4);
            p.setFont(QFont("Arial", 6, QFont::Black));
            p.setPen(canAfford ? QColor(255, 225, 85) : QColor(255, 130, 135));
            p.drawText(badge, Qt::AlignCenter, QString("🔒 %1 🪙").arg(cs.price));
        }

        // Base specs row
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(accent);
        p.drawText(QRect(tx, card.y() + 22, tw, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   QString("%1 KM/H BASE  •  GRIP %2%  •  ARMOR %3%")
                       .arg((int)cs.maxSpeed)
                       .arg(qRound(cs.grip * 100))
                       .arg(qRound(cs.armor * 100)));

        // Unique Feature description
        p.setFont(QFont("Arial", 6, QFont::Normal));
        p.setPen(QColor(160, 180, 205));
        p.drawText(QRect(tx, card.y() + 37, tw, card.height() - 40), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   cs.feature);
    }

    // =============================================================
    // CENTER COLUMN: SHOWROOM TURNTABLE & COMBINED PERFORMANCE SPECS
    // =============================================================
    int centerX = contentX + leftW + colGap;
    QRect centerCard(centerX, curY, centerW, panelH);
    p.setBrush(QColor(11, 16, 26, 246));
    p.setPen(QPen(QColor(0, 220, 255, 150), 1.5));
    p.drawRoundedRect(centerCard, 8, 8);
    p.fillRect(QRect(centerX, curY, centerW, 3), QColor(255, 210, 65));

    int pCarIdx = qBound(0, m_previewCarIdx, kCarCount - 1);
    int pEngIdx = qBound(0, m_previewEngineIdx, kEngineCount - 1);
    const CarSpec& cSpec = kCars[pCarIdx];
    const EngineSpec& eSpec = kEngines[pEngIdx];
    QColor cAccent(cSpec.accentRgb);

    p.setFont(QFont("Arial", 8, QFont::Black));
    p.setPen(QColor(255, 220, 80));
    p.drawText(QRect(centerX + 12, curY + 8, centerW - 24, 16), Qt::AlignCenter, "SHOWROOM STAGE  •  LIVE DYNO SPECS");

    // Showroom Stage Pedestal & Dual Car Views
    QRect stageRect(centerX + 12, curY + 28, centerW - 24, 118);
    QRadialGradient stageGlow(stageRect.center().x(), stageRect.bottom() - 18, stageRect.width() * 0.54);
    stageGlow.setColorAt(0.0, QColor(cAccent.red(), cAccent.green(), cAccent.blue(), 82));
    stageGlow.setColorAt(0.6, QColor(0, 195, 255, 24));
    stageGlow.setColorAt(1.0, QColor(8, 12, 20, 0));
    p.setBrush(stageGlow);
    p.setPen(QPen(QColor(42, 60, 86), 1));
    p.drawRoundedRect(stageRect, 7, 7);

    // Overhead Studio Softbox Light Bars
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 38));
    p.drawRoundedRect(QRect(stageRect.x() + 24, stageRect.y() + 5, stageRect.width() - 48, 3), 1, 1);

    // Glowing Double-Ring Turntable Floor
    p.setBrush(QColor(14, 24, 38, 195));
    p.setPen(QPen(cAccent, 1.8));
    p.drawEllipse(QPointF(stageRect.center().x(), stageRect.bottom() - 18), stageRect.width() * 0.42, 15);
    p.setPen(QPen(QColor(255, 220, 85, 130), 1.0));
    p.drawEllipse(QPointF(stageRect.center().x(), stageRect.bottom() - 18), stageRect.width() * 0.34, 10);

    const QImage& sideImg = RasterAssets::instance().getVehicleSide(cSpec.sprite, true);
    const QImage& rearImg = RasterAssets::instance().getVehicle(cSpec.sprite, true, true, false, false);
    p.drawImage(QRect(stageRect.x() + 10, stageRect.y() + 18, stageRect.width() * 0.56, 76), sideImg);
    p.drawImage(QRect(stageRect.right() - int(stageRect.width() * 0.40) - 8, stageRect.y() + 14, int(stageRect.width() * 0.40), 82), rearImg);

    // Combined Title & Horsepower Badge
    int specY = stageRect.bottom() + 8;
    p.setFont(QFont("Arial", 10, QFont::Black));
    p.setPen(Qt::white);
    p.drawText(QRect(centerX + 12, specY, centerW - 24, 18), Qt::AlignCenter,
               QString("%1  +  %2").arg(cSpec.name).arg(eSpec.name));

    p.setFont(QFont("Arial", 8, QFont::Bold));
    p.setPen(QColor(255, 215, 70));
    qreal combSpeed = cSpec.maxSpeed + eSpec.topSpeedBonus;
    qreal combAccel = cSpec.accel + eSpec.accelBonus + (eSpec.lowSpeedTorque + eSpec.highSpeedBoost) * 0.35;
    qreal combGrip  = cSpec.grip * eSpec.gripMod;
    qreal combArmor = cSpec.armor * eSpec.armorMod;
    p.drawText(QRect(centerX + 12, specY + 18, centerW - 24, 16), Qt::AlignCenter,
               QString("POWER: %1 HP   •   MAX SPEED: %2 KM/H").arg(eSpec.horsepower).arg((int)combSpeed));

    // 4 Animated Dyno Telemetry Bars
    auto drawDynoBar = [&](int by, const QString& label, const QString& valText, qreal baseFrac, qreal totalFrac, const QColor& col) {
        int bx = centerX + 14;
        int bw = centerW - 28;
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(QColor(175, 195, 220));
        p.drawText(QRect(bx, by, bw, 14), Qt::AlignLeft | Qt::AlignVCenter, label);
        p.setPen(col);
        p.drawText(QRect(bx, by, bw, 14), Qt::AlignRight | Qt::AlignVCenter, valText);

        QRect track(bx, by + 15, bw, 8);
        p.fillRect(track, QColor(20, 28, 42));
        int baseW = qRound(bw * qBound(0.05, baseFrac, 1.0));
        int totW  = qRound(bw * qBound(0.05, totalFrac, 1.0));
        p.fillRect(QRect(bx, by + 15, qMin(baseW, totW), 8), col);
        if (totW > baseW) {
            p.fillRect(QRect(bx + baseW, by + 15, totW - baseW, 8), QColor(45, 245, 135));
        }
    };

    int barY = specY + 38;
    drawDynoBar(barY,       "TOP SPEED",        QString("%1 KM/H (+%2)").arg((int)combSpeed).arg((int)eSpec.topSpeedBonus),
                (cSpec.maxSpeed - 180.0) / 60.0, (combSpeed - 180.0) / 60.0, QColor(0, 225, 255));
    drawDynoBar(barY + 28,  "ACCELERATION",     QString("%1 RATING (%2 HP)").arg(qRound(combAccel)).arg(eSpec.horsepower),
                (cSpec.accel - 75.0) / 65.0, (combAccel - 75.0) / 65.0, QColor(255, 195, 45));
    drawDynoBar(barY + 56,  "CORNERING GRIP",   QString("%1%").arg(qRound(combGrip * 100)),
                cSpec.grip / 1.40, combGrip / 1.40, QColor(175, 125, 255));
    drawDynoBar(barY + 84,  "ARMOR DURABILITY", QString("%1%").arg(qRound(combArmor * 100)),
                cSpec.armor / 1.65, combArmor / 1.65, QColor(50, 235, 140));

    // Coin Prize Purse Guide Box at bottom of Center Card
    QRect earnBox(centerX + 12, centerCard.bottom() - 62, centerW - 24, 52);
    p.setBrush(QColor(16, 24, 36, 225));
    p.setPen(QPen(QColor(255, 205, 60, 145), 1));
    p.drawRoundedRect(earnBox, 6, 6);
    p.setFont(QFont("Arial", 7, QFont::Black));
    p.setPen(QColor(255, 220, 80));
    p.drawText(earnBox.adjusted(8, 4, -8, -28), Qt::AlignCenter, "CHAMPIONSHIP PRIZE PURSE");
    p.setFont(QFont("Arial", 6, QFont::Bold));
    p.setPen(QColor(195, 215, 235));
    p.drawText(earnBox.adjusted(8, 20, -8, -4), Qt::AlignCenter | Qt::TextWordWrap,
               "1st: 500 COINS  •  2nd: 350 COINS  •  3rd: 250 COINS  •  Clean Race: +100 COINS\n"
               "Difficulty Multiplier: Rookie 1.0x  |  Pro Circuit 1.5x  |  Championship 2.5x!");

    // =============================================================
    // RIGHT COLUMN: 5 ENGINE POWERTRAIN UPGRADES (WITH LOCKS & HP)
    // =============================================================
    int rightX = centerX + centerW + colGap;
    QRect rightHdr(rightX, curY, rightW, 22);
    p.setFont(QFont("Arial", 8, QFont::Black));
    p.setPen(QColor(255, 205, 55));
    p.drawText(rightHdr, Qt::AlignLeft | Qt::AlignVCenter, "02  •  ENGINE POWERTRAINS (5 BLOCKS)");

    int engCardY = curY + 24;
    int engGap = 6;
    int engCardH = (panelH - 24 - 4 * engGap) / 5;

    for (int e = 0; e < kEngineCount; ++e) {
        const EngineSpec& es = kEngines[e];
        QColor eCol(es.accentRgb);
        bool owned = prof.ownsEngine(e);
        bool equipped = (player.selectedEngine == e);
        bool previewed = (m_previewEngineIdx == e);
        bool canAfford = (prof.coins >= es.price);

        QRect card(rightX, engCardY + e * (engCardH + engGap), rightW, engCardH);
        m_garageEngineCards[e] = card;
        bool hov = card.contains(m_mousePos);

        p.setBrush(equipped ? QColor(22, 36, 54, 248) : (hov ? QColor(22, 30, 46, 240) : QColor(13, 19, 30, 235)));
        p.setPen(QPen(equipped ? eCol : (previewed ? QColor(255, 215, 75) : (hov ? eCol.lighter(115) : QColor(40, 56, 78))),
                      equipped ? 2.0 : 1.1));
        p.drawRoundedRect(card, 7, 7);
        p.fillRect(QRect(card.x(), card.y() + 5, 4, card.height() - 10), owned ? eCol : QColor(110, 120, 138));

        // Engine HP Icon Badge on Left
        QRect hpBox(card.x() + 10, card.y() + 7, 54, card.height() - 14);
        p.setBrush(QColor(18, 26, 40));
        p.setPen(QPen(eCol, 1.2));
        p.drawRoundedRect(hpBox, 4, 4);
        p.setFont(QFont("Arial", 9, QFont::Black));
        p.setPen(eCol);
        p.drawText(hpBox.adjusted(0, 4, 0, -16), Qt::AlignCenter, QString::number(es.horsepower));
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.setPen(QColor(185, 205, 225));
        p.drawText(hpBox.adjusted(0, 20, 0, -2), Qt::AlignCenter, "HP");

        int tx = card.x() + 72;
        int tw = card.width() - 80;
        p.setFont(QFont("Arial", 8, QFont::Black));
        p.setPen(owned ? Qt::white : QColor(195, 205, 220));
        p.drawText(QRect(tx, card.y() + 4, tw - 90, 15), Qt::AlignLeft | Qt::AlignVCenter, es.name);

        // Lock / Price / Equipped Pill on top-right
        QRect badge(card.right() - 92, card.y() + 4, 86, 16);
        if (equipped) {
            p.setBrush(QColor(40, 235, 130, 50));
            p.setPen(QPen(QColor(40, 235, 130), 1.2));
            p.drawRoundedRect(badge, 4, 4);
            p.setFont(QFont("Arial", 6, QFont::Black));
            p.setPen(QColor(60, 250, 145));
            p.drawText(badge, Qt::AlignCenter, "✓ INSTALLED");
        } else if (owned) {
            p.setBrush(QColor(0, 210, 255, 35));
            p.setPen(QPen(QColor(0, 215, 255, 160), 1.0));
            p.drawRoundedRect(badge, 4, 4);
            p.setFont(QFont("Arial", 6, QFont::Bold));
            p.setPen(QColor(130, 235, 255));
            p.drawText(badge, Qt::AlignCenter, "OWNED • INSTALL");
        } else {
            p.setBrush(canAfford ? QColor(255, 195, 35, 55) : QColor(45, 30, 36, 180));
            p.setPen(QPen(canAfford ? QColor(255, 210, 60) : QColor(165, 75, 85), 1.2));
            p.drawRoundedRect(badge, 4, 4);
            p.setFont(QFont("Arial", 6, QFont::Black));
            p.setPen(canAfford ? QColor(255, 225, 85) : QColor(255, 130, 135));
            p.drawText(badge, Qt::AlignCenter, QString("🔒 %1 🪙").arg(es.price));
        }

        // Spec Bonus Line
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.setPen(eCol);
        p.drawText(QRect(tx, card.y() + 20, tw, 13), Qt::AlignLeft | Qt::AlignVCenter,
                   QString("%1  •  +%2 KM/H  •  +%3 ACCEL")
                       .arg(es.layout)
                       .arg((int)es.topSpeedBonus)
                       .arg((int)es.accelBonus));

        // Unique Feature Line
        p.setFont(QFont("Arial", 6, QFont::Normal));
        p.setPen(QColor(155, 175, 200));
        p.drawText(QRect(tx, card.y() + 34, tw, card.height() - 36), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   es.feature);
    }

    // Bottom Command Dock for Garage Tab
    int actY = curY + panelH + 10;
    int primaryW = 310;
    int secondaryW = 225;
    int actH = 38;
    int actGap = 14;
    int actTotalW = primaryW + 2 * secondaryW + 2 * actGap;
    int actStartX = contentX + (contentW - actTotalW) / 2;

    m_resumeBtn = QRect(actStartX, actY, primaryW, actH);
    bool startHov = m_resumeBtn.contains(m_mousePos);
    QLinearGradient resumeGrad(m_resumeBtn.topLeft(), m_resumeBtn.bottomRight());
    resumeGrad.setColorAt(0.0, startHov ? QColor(20, 240, 140) : QColor(0, 215, 120));
    resumeGrad.setColorAt(1.0, startHov ? QColor(0, 185, 95) : QColor(0, 160, 80));
    p.setBrush(resumeGrad);
    p.setPen(QPen(QColor(130, 255, 195), startHov ? 2.2 : 1.6));
    p.drawRoundedRect(m_resumeBtn, 6, 6);
    p.setFont(QFont("Arial", 10, QFont::Black));
    p.setPen(QColor(6, 18, 10));
    p.drawText(m_resumeBtn, Qt::AlignCenter, "▶  RACE WITH THIS BUILD  [ENTER]");

    m_repairBtn = QRect(actStartX + primaryW + actGap, actY, secondaryW, actH);
    bool backHov = m_repairBtn.contains(m_mousePos);
    p.setBrush(backHov ? QColor(34, 48, 68) : QColor(22, 32, 48));
    p.setPen(QPen(QColor(0, 215, 255), 1.4));
    p.drawRoundedRect(m_repairBtn, 6, 6);
    p.setFont(QFont("Arial", 8, QFont::Bold));
    p.setPen(Qt::white);
    p.drawText(m_repairBtn, Qt::AlignCenter, "◀  BACK TO CIRCUITS");

    m_lightsBtn = QRect(actStartX + primaryW + secondaryW + 2 * actGap, actY, secondaryW, actH);
    bool lgtHov = m_lightsBtn.contains(m_mousePos);
    p.setBrush(lgtHov ? QColor(32, 48, 72) : QColor(22, 34, 52));
    p.setPen(QPen(QColor(255, 195, 55), 1.4));
    p.drawRoundedRect(m_lightsBtn, 6, 6);
    p.setPen(QColor(255, 220, 110));
    p.drawText(m_lightsBtn, Qt::AlignCenter, "SHOWROOM AMBIENCE [N]");
}

void GameWindow::renderMainMenuOverlay(QPainter& p) {
    int w = width();
    int h = height();

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 1. Luxury Obsidian-Glass Showroom Backdrop
    QLinearGradient bgGrad(0, 0, 0, h);
    bgGrad.setColorAt(0.0, QColor(5, 9, 17, 236));
    bgGrad.setColorAt(0.45, QColor(9, 14, 26, 216));
    bgGrad.setColorAt(1.0, QColor(4, 7, 14, 242));
    p.fillRect(0, 0, w, h, bgGrad);

    // Subtle animated diagonal showroom light beams
    int gridShift = qRound(m_menuAnimTime * 18.0) % 64;
    p.setPen(QPen(QColor(0, 215, 255, 9), 1));
    for (int gx = -h + gridShift; gx < w + h; gx += 64) {
        p.drawLine(gx, 0, gx + h, h);
    }

    // Dual warm-gold & cyan ambient studio spotlights
    qreal pulse = 0.5 + 0.5 * qSin(m_menuAnimTime * 2.4);
    QRadialGradient spotGrad(w * 0.5, h * 0.14, w * 0.48);
    spotGrad.setColorAt(0.0, QColor(0, 210, 255, qRound(24 + 8 * pulse)));
    spotGrad.setColorAt(0.55, QColor(255, 195, 55, qRound(10 + 4 * pulse)));
    spotGrad.setColorAt(1.0, QColor(0, 210, 255, 0));
    p.fillRect(0, 0, w, h, spotGrad);

    // Floating Golden & Cyan Showroom Bokeh Motes
    p.setPen(Qt::NoPen);
    for (int m = 0; m < 14; ++m) {
        qreal mx = fmod((m * 137.0) + m_menuAnimTime * (12.0 + (m % 4) * 5.0), (qreal)w);
        qreal my = fmod((m * 89.0) - m_menuAnimTime * (10.0 + (m % 3) * 4.0) + h * 4.0, (qreal)h);
        qreal mRad = 1.6 + (m % 3) * 1.1;
        QColor mc = (m % 2 == 0) ? QColor(255, 215, 90, 38) : QColor(80, 230, 255, 34);
        p.setBrush(mc);
        p.drawEllipse(QPointF(mx, my), mRad, mRad);
    }

    // Top & Bottom Accent Horizon Laser Lines
    QLinearGradient topBarGrad(0, 0, w, 0);
    topBarGrad.setColorAt(0.0, QColor(0, 210, 255, 0));
    topBarGrad.setColorAt(0.25, QColor(0, 210, 255, 220));
    topBarGrad.setColorAt(0.5, QColor(255, 215, 70, 255));
    topBarGrad.setColorAt(0.75, QColor(0, 210, 255, 220));
    topBarGrad.setColorAt(1.0, QColor(0, 210, 255, 0));
    p.fillRect(0, 0, w, 3, topBarGrad);
    p.fillRect(0, h - 2, w, 2, topBarGrad);

    // Top Motorsport Header Bar
    int contentW = qMin(1160, w - 48);
    int contentX = (w - contentW) / 2;
    const PlayerProfile& prof = m_engine.getProfile();
    const PlayerCar& player = m_engine.getPlayer();

    // Left Driver License & Career Record Card (Click to Switch Driver / Sign Out)
    m_accountBadgeRect = QRect(contentX, 8, 308, 40);
    bool accHov = m_accountBadgeRect.contains(m_mousePos);
    QLinearGradient accGrad(m_accountBadgeRect.topLeft(), m_accountBadgeRect.bottomRight());
    accGrad.setColorAt(0.0, accHov ? QColor(20, 34, 52, 242) : QColor(13, 21, 34, 235));
    accGrad.setColorAt(1.0, QColor(10, 16, 26, 235));
    p.setBrush(accGrad);
    p.setPen(QPen(prof.isLoggedIn ? QColor(45, 240, 135, 180) : QColor(255, 205, 55, 180), accHov ? 1.8 : 1.2));
    p.drawRoundedRect(m_accountBadgeRect, 7, 7);

    // Driver Crest Circle
    QRect drvCircle(m_accountBadgeRect.x() + 9, m_accountBadgeRect.y() + 8, 24, 24);
    p.setBrush(prof.isLoggedIn ? QColor(20, 68, 46) : QColor(58, 44, 14));
    p.setPen(QPen(prof.isLoggedIn ? QColor(55, 245, 145) : QColor(255, 210, 65), 1.2));
    p.drawEllipse(drvCircle);
    p.setFont(QFont("Arial", 8, QFont::Black));
    p.setPen(Qt::white);
    QString initial = (!prof.username.isEmpty()) ? prof.username.left(1).toUpper() : QStringLiteral("D");
    p.drawText(drvCircle, Qt::AlignCenter, initial);

    p.setFont(QFont("Arial", 8, QFont::Black));
    p.setPen(prof.isLoggedIn ? QColor(85, 250, 160) : QColor(255, 220, 75));
    QString accHeader = prof.isLoggedIn
        ? QString("DRIVER: %1   •   SWITCH").arg(prof.username.toUpper())
        : QString("GUEST DRIVER   •   SIGN IN");
    p.drawText(m_accountBadgeRect.adjusted(40, 4, -8, -20), Qt::AlignLeft | Qt::AlignVCenter, accHeader);
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.setPen(QColor(185, 220, 250));
    p.drawText(m_accountBadgeRect.adjusted(40, 21, -8, -3), Qt::AlignLeft | Qt::AlignVCenter,
               QString("CAREER: %1 WINS  •  %2 PODIUMS  •  %3 RACES")
                   .arg(prof.wins).arg(prof.podiums).arg(prof.racesCompleted));

    // Center Main Championship Title
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.setPen(QColor(255, 212, 75));
    p.drawText(QRect(0, 7, w, 14), Qt::AlignCenter, "WORLD SERIES CHAMPIONSHIP   •   GRAND PRIX EDITION");

    p.setFont(QFont("Arial", 16, QFont::Black));
    p.setPen(Qt::white);
    p.drawText(QRect(0, 21, w, 26), Qt::AlignCenter, "A P E X   V E L O C I T Y");

    // Right Golden Coin Vault & Active Build Card
    QRect walletBadge(contentX + contentW - 312, 8, 312, 40);
    QLinearGradient wg(walletBadge.topLeft(), walletBadge.bottomRight());
    wg.setColorAt(0.0, QColor(46, 35, 10, 238));
    wg.setColorAt(0.55, QColor(18, 25, 38, 238));
    wg.setColorAt(1.0, QColor(12, 18, 28, 238));
    p.setBrush(wg);
    p.setPen(QPen(QColor(255, 208, 60, 205), 1.5));
    p.drawRoundedRect(walletBadge, 7, 7);
    p.fillRect(QRect(walletBadge.right() - 3, walletBadge.y() + 6, 3, 28), QColor(255, 210, 55));

    // Golden Coin Emblem
    QRect coinCircle(walletBadge.x() + 10, walletBadge.y() + 8, 24, 24);
    QRadialGradient cg(coinCircle.center(), 12);
    cg.setColorAt(0.0, QColor(255, 245, 150));
    cg.setColorAt(0.6, QColor(245, 185, 30));
    cg.setColorAt(1.0, QColor(175, 110, 10));
    p.setBrush(cg);
    p.setPen(QPen(QColor(255, 235, 120), 1.2));
    p.drawEllipse(coinCircle);
    p.setFont(QFont("Arial", 8, QFont::Black));
    p.setPen(QColor(45, 28, 4));
    p.drawText(coinCircle, Qt::AlignCenter, "$");

    p.setFont(QFont("Arial", 9, QFont::Black));
    p.setPen(QColor(255, 225, 80));
    QString coinStr = prof.isLoggedIn
        ? QString("%1  •  %2 COINS").arg(prof.username.toUpper()).arg(prof.coins)
        : QString("VAULT: %1 COINS").arg(prof.coins);
    if (player.lastRaceCoinsEarned > 0) {
        coinStr += QString("  (+%1)").arg(player.lastRaceCoinsEarned);
    }
    p.drawText(walletBadge.adjusted(40, 4, -12, -20), Qt::AlignLeft | Qt::AlignVCenter, coinStr);

    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.setPen(QColor(175, 230, 255));
    p.drawText(walletBadge.adjusted(40, 21, -12, -3), Qt::AlignLeft | Qt::AlignVCenter,
               QString("%1  •  %2 (%3 HP)").arg(player.carModelName).arg(player.engineModelName).arg(player.horsepower));

    // =============================================================
    // 3-TAB NAVIGATION BAR: [ CIRCUITS & RACE ] | [ GARAGE & ENGINES ] | [ ONLINE MULTIPLAYER ]
    // =============================================================
    int tabW = 286;
    int tabH = 28;
    int tabGap = 10;
    int totalTabsW = 3 * tabW + 2 * tabGap;
    int tabsStartX = (w - totalTabsW) / 2;
    m_soloTabBtn   = QRect(tabsStartX, 53, tabW, tabH);
    m_garageTabBtn = QRect(tabsStartX + tabW + tabGap, 53, tabW, tabH);
    m_mpTabBtn     = QRect(tabsStartX + 2 * (tabW + tabGap), 53, tabW, tabH);

    // Tab 0: Solo / Circuits
    bool soloActive = (m_menuTab == 0 && !m_showMultiplayerTab);
    bool soloHover = m_soloTabBtn.contains(m_mousePos);
    p.setBrush(soloActive ? QColor(0, 200, 245, 56) : (soloHover ? QColor(22, 32, 48, 230) : QColor(14, 20, 30, 215)));
    p.setPen(QPen(soloActive ? QColor(0, 235, 255) : (soloHover ? QColor(0, 195, 240) : QColor(44, 58, 78)), soloActive ? 1.8 : 1.2));
    p.drawRoundedRect(m_soloTabBtn, 6, 6);
    p.setFont(QFont("Arial", 8, QFont::Black));
    p.setPen(soloActive ? QColor(110, 245, 255) : (soloHover ? Qt::white : QColor(160, 180, 205)));
    p.drawText(m_soloTabBtn, Qt::AlignCenter, "01  •  WORLD TOUR CIRCUITS");

    // Tab 1: Garage (Cars & Engines Unlock / Upgrade Shop)
    bool garActive = (m_menuTab == 1 && !m_showMultiplayerTab);
    bool garHover = m_garageTabBtn.contains(m_mousePos);
    p.setBrush(garActive ? QColor(255, 195, 40, 58) : (garHover ? QColor(36, 30, 18, 230) : QColor(14, 20, 30, 215)));
    p.setPen(QPen(garActive ? QColor(255, 215, 65) : (garHover ? QColor(255, 195, 45) : QColor(44, 58, 78)), garActive ? 1.8 : 1.2));
    p.drawRoundedRect(m_garageTabBtn, 6, 6);
    p.setPen(garActive ? QColor(255, 228, 90) : (garHover ? Qt::white : QColor(215, 195, 145)));
    p.drawText(m_garageTabBtn, Qt::AlignCenter, "02  •  GARAGE: CARS & ENGINES 🪙");

    // Tab 2: Multiplayer
    bool hasPeer = NetworkManager::instance().isConnected();
    bool mpActive = (m_menuTab == 2 || m_showMultiplayerTab);
    bool mpHover = m_mpTabBtn.contains(m_mousePos);
    QColor mpAccent = hasPeer ? QColor(40, 230, 115) : QColor(0, 225, 255);
    p.setBrush(mpActive ? QColor(mpAccent.red(), mpAccent.green(), mpAccent.blue(), 52)
                        : (mpHover ? QColor(26, 34, 48, 230) : QColor(14, 20, 30, 215)));
    p.setPen(QPen(mpActive ? mpAccent : (mpHover ? mpAccent : QColor(44, 58, 78)), mpActive ? 1.8 : 1.2));
    p.drawRoundedRect(m_mpTabBtn, 6, 6);
    p.setPen(mpActive ? mpAccent : (hasPeer ? QColor(40, 230, 115) : (mpHover ? Qt::white : QColor(160, 180, 205))));
    p.drawText(m_mpTabBtn, Qt::AlignCenter, hasPeer ? "03  •  MULTIPLAYER [LIVE]" : "03  •  ONLINE MULTIPLAYER [O]");

    // Render Driver Account Login & Registration Lounge if not signed in or switching profile
    if (m_showAccountModal || !prof.isLoggedIn) {
        renderAccountLoginOverlay(p, w, h);
        p.restore();
        return;
    }

    // Render Garage Tab if selected
    if (m_menuTab == 1 && !m_showMultiplayerTab) {
        renderGarageTab(p, contentX, contentW, 86, h);
        int bottomY = h - 24;
        p.setPen(QColor(145, 168, 195));
        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.drawText(QRect(0, bottomY, w, 18), Qt::AlignCenter,
                   "SHOWROOM:  Click any Supercar or Engine to Equip or Unlock with Coins   •   Win races on higher tiers for up to 2.5x Coin rewards!");
        p.restore();
        return;
    }

    if (m_showMultiplayerTab || m_menuTab == 2) {
        NetworkManager& net = NetworkManager::instance();
        m_discoveredRoomChips.clear();

        int paneW = qMin(1080, w - 56);
        int paneX = (w - paneW) / 2;
        int paneY = 86;

        // 1. Driver Profile & Online Server Bar
        QRect idCard(paneX, paneY, paneW, 50);
        p.setBrush(QColor(12, 18, 28, 238));
        p.setPen(QPen(QColor(40, 56, 80), 1.2));
        p.drawRoundedRect(idCard, 6, 6);
        p.fillRect(QRect(paneX, paneY + 8, 3, 34), QColor(0, 220, 255));

        // Driver Callsign (clickable to edit)
        m_editNameBtn = QRect(paneX + 10, paneY + 7, 210, 36);
        bool nameHover = m_editNameBtn.contains(m_mousePos);
        p.setBrush(nameHover ? QColor(24, 36, 54) : QColor(18, 26, 38));
        p.setPen(QPen(nameHover ? QColor(0, 235, 255) : QColor(0, 200, 240, 120), 1.2));
        p.drawRoundedRect(m_editNameBtn, 4, 4);
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.setPen(QColor(0, 220, 255));
        p.drawText(m_editNameBtn.adjusted(8, 3, -8, -19), Qt::AlignLeft, "DRIVER CALLSIGN [CLICK TO EDIT]");
        p.setFont(QFont("Arial", 9, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(m_editNameBtn.adjusted(8, 17, -8, -2), Qt::AlignLeft, net.getLocalPlayerName());

        // Online Global Server Button (connects players across any two different networks!)
        m_relayServerBtn = QRect(paneX + 228, paneY + 7, 285, 36);
        bool relHover = m_relayServerBtn.contains(m_mousePos);
        bool relOnline = net.isRelayConnected();
        p.setBrush(relHover ? QColor(24, 38, 56) : QColor(16, 24, 36));
        p.setPen(QPen(relOnline ? QColor(45, 240, 130) : (relHover ? QColor(255, 210, 65) : QColor(65, 95, 130)), 1.2));
        p.drawRoundedRect(m_relayServerBtn, 4, 4);
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.setPen(relOnline ? QColor(50, 245, 135) : QColor(255, 210, 65));
        p.drawText(m_relayServerBtn.adjusted(8, 3, -8, -19), Qt::AlignLeft,
                   relOnline ? "ONLINE SERVER [CONNECTED • CLICK TO CHANGE]" : "ONLINE SERVER [CLICK TO SET ADDRESS]");
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(Qt::white);
        QString relayDisp = net.getRelayServerEndpoint().isEmpty()
            ? "LOCAL NETWORK (CLICK FOR GLOBAL ONLINE)"
            : net.getRelayServerEndpoint();
        p.drawText(m_relayServerBtn.adjusted(8, 17, -8, -2), Qt::AlignLeft, relayDisp);

        // Direct Host Address Button
        m_remoteHostBtn = QRect(paneX + 521, paneY + 7, 265, 36);
        bool remHover = m_remoteHostBtn.contains(m_mousePos);
        p.setBrush(remHover ? QColor(24, 38, 56) : QColor(16, 24, 36));
        p.setPen(QPen(remHover ? QColor(0, 230, 255) : QColor(65, 95, 130), 1.2));
        p.drawRoundedRect(m_remoteHostBtn, 4, 4);
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.setPen(QColor(0, 220, 255));
        p.drawText(m_remoteHostBtn.adjusted(8, 3, -8, -19), Qt::AlignLeft, "DIRECT HOST ADDRESS [CLICK TO SET]");
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(Qt::white);
        QString remDisp = (net.getRemoteHostEndpoint().isEmpty() || net.getRemoteHostEndpoint() == "AUTO")
            ? QString("AUTO-DETECT (%1)").arg(net.getLocalIpAddress())
            : net.getRemoteHostEndpoint();
        p.drawText(m_remoteHostBtn.adjusted(8, 17, -8, -2), Qt::AlignLeft, remDisp);

        // Right Status Indicator
        QRect statusBox(paneX + paneW - 280, paneY + 7, 270, 36);
        p.setBrush(QColor(16, 23, 34));
        p.setPen(QPen(hasPeer ? QColor(40, 230, 115, 160) : QColor(45, 62, 86), 1.2));
        p.drawRoundedRect(statusBox, 4, 4);
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(hasPeer ? QColor(40, 230, 115) : QColor(0, 220, 255));
        p.drawText(statusBox.adjusted(10, 4, -10, -16), Qt::AlignRight,
                   hasPeer ? QString("ROOM ACTIVE: %1").arg(net.getRoomId()) : "MULTIPLAYER LOBBY READY");
        p.setFont(QFont("Arial", 7, QFont::Normal));
        p.setPen(QColor(145, 165, 190));
        int discCount = net.getDiscoveredHosts().size();
        p.drawText(statusBox.adjusted(10, 20, -10, -2), Qt::AlignRight,
                   hasPeer ? QString("CONNECTED DRIVERS: %1 / 8").arg(net.getConnectedPlayerCount())
                           : (discCount > 0 ? QString("%1 LIVE ROOM(S) AVAILABLE").arg(discCount) : "CREATE OR JOIN AN 8-CAR LOBBY"));

        int currentY = paneY + 56;

        // 1b. Interactive Multiplayer Loadout Selector (4 Supercars + 4 Circuits)
        QRect loadoutBar(paneX, currentY, paneW, 42);
        p.setBrush(QColor(11, 16, 25, 235));
        p.setPen(QPen(QColor(36, 50, 72), 1));
        p.drawRoundedRect(loadoutBar, 5, 5);

        int curCar = m_engine.getPlayer().selectedCar;
        int halfW = (paneW - 28) / 2;
        int mpCarBtnW = (halfW - 3 * 6) / 4;
        for (int c = 0; c < 4; ++c) {
            const CarSpec& cs = kCars[c];
            QColor col(cs.accentRgb);
            bool owned = prof.ownsCar(c);
            m_mpCarButtons[c] = QRect(paneX + 8 + c * (mpCarBtnW + 6), currentY + 6, mpCarBtnW, 30);
            bool isSel = (curCar == c);
            bool isHov = m_mpCarButtons[c].contains(m_mousePos);
            p.setBrush(isSel ? QColor(20, 30, 46) : (isHov ? QColor(22, 30, 44) : QColor(15, 21, 32)));
            p.setPen(QPen(isSel ? col : (isHov ? col.darker(120) : QColor(42, 54, 72)), isSel ? 1.6 : 1.0));
            p.drawRoundedRect(m_mpCarButtons[c], 4, 4);

            const QImage& vImg = RasterAssets::instance().getVehicle(cs.sprite, true, false, false, false);
            QRect iconR(m_mpCarButtons[c].x() + 4, m_mpCarButtons[c].y() + 4, 26, 20);
            p.drawImage(iconR, vImg);

            p.setFont(QFont("Arial", 6, QFont::Bold));
            p.setPen(isSel ? col : (owned ? Qt::white : QColor(255, 205, 65)));
            QString label = owned ? QString::fromLatin1(cs.name) : QString("🔒 %1").arg(cs.name);
            p.drawText(m_mpCarButtons[c].adjusted(32, 0, -3, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
        }

        const char* mpTrackNames[4] = { "PACIFIC COAST", "ALPINE SUMMIT", "NEO CITY", "RED ROCK" };
        QColor mpTrackCols[4] = { QColor(255, 145, 45), QColor(75, 180, 255), QColor(245, 65, 165), QColor(240, 105, 55) };
        int trackStartX = paneX + 18 + halfW;
        int mpTrkBtnW = (halfW - 3 * 6) / 4;
        int curTrk = static_cast<int>(m_engine.getCurrentTheme());
        bool canPickTrack = (!net.isConnected() || net.isHost());
        for (int t = 0; t < 4; ++t) {
            m_mpTrackButtons[t] = QRect(trackStartX + t * (mpTrkBtnW + 6), currentY + 6, mpTrkBtnW, 30);
            bool isSel = (curTrk == t);
            bool isHov = canPickTrack && m_mpTrackButtons[t].contains(m_mousePos);
            p.setBrush(isSel ? QColor(22, 32, 48) : (isHov ? QColor(22, 30, 44) : QColor(15, 21, 32)));
            p.setPen(QPen(isSel ? mpTrackCols[t] : (isHov ? mpTrackCols[t].darker(120) : QColor(42, 54, 72)), isSel ? 1.6 : 1.0));
            p.drawRoundedRect(m_mpTrackButtons[t], 4, 4);
            p.setFont(QFont("Arial", 6, QFont::Bold));
            p.setPen(isSel ? mpTrackCols[t] : (canPickTrack ? QColor(195, 210, 230) : QColor(110, 125, 145)));
            p.drawText(m_mpTrackButtons[t], Qt::AlignCenter, mpTrackNames[t]);
        }

        currentY += 48;

        // 2. Authentication Error or Connecting Banner
        if (m_authErrorTimer > 0.0 && !m_lastAuthError.isEmpty()) {
            QRect errRect(paneX, currentY, paneW, 32);
            p.setBrush(QColor(150, 22, 32, 235));
            p.setPen(QPen(QColor(255, 80, 90), 1.5));
            p.drawRoundedRect(errRect, 5, 5);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(errRect, Qt::AlignCenter, QString("UNABLE TO JOIN ROOM: %1").arg(m_lastAuthError));
            currentY += 38;
        } else if (net.isSearchingRoom()) {
            QRect searchRect(paneX, currentY, paneW, 32);
            p.setBrush(QColor(16, 48, 72, 235));
            p.setPen(QPen(QColor(0, 220, 255), 1.5));
            p.drawRoundedRect(searchRect, 5, 5);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(QColor(0, 235, 255));
            p.drawText(searchRect, Qt::AlignCenter, QString("CONNECTING TO ROOM [%1]...").arg(m_joinRoomId));
            currentY += 38;
        }

        // 3. Room Creation & Join Panels (when not connected)
        if (!net.isConnected()) {
            int cardGap = 16;
            int cardW = (paneW - cardGap) / 2;
            int cardH = qMin(256, h - currentY - 82);

            // --- LEFT CARD: CREATE & HOST ROOM ---
            QRect hostCard(paneX, currentY, cardW, cardH);
            p.setBrush(QColor(13, 19, 29, 238));
            p.setPen(QPen(QColor(42, 58, 82), 1.5));
            p.drawRoundedRect(hostCard, 7, 7);
            p.fillRect(QRect(hostCard.x(), hostCard.y(), cardW, 3), QColor(0, 220, 255));

            p.setFont(QFont("Arial", 11, QFont::Bold));
            p.setPen(QColor(0, 225, 255));
            p.drawText(hostCard.x() + 18, currentY + 24, "01  •  HOST MULTIPLAYER ROOM");
            p.setFont(QFont("Arial", 8, QFont::Normal));
            p.setPen(QColor(145, 165, 190));
            p.drawText(hostCard.x() + 18, currentY + 41, "Create an 8-driver championship lobby with Room ID & Passkey.");

            // Host Room ID Field
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(175, 195, 220));
            p.drawText(hostCard.x() + 18, currentY + 62, "ROOM ID  (CLICK TO EDIT)");
            int idBtnW = 110;
            m_hostRoomIdBox = QRect(hostCard.x() + 18, currentY + 68, cardW - 36 - idBtnW - 8, 30);
            p.setBrush(m_hostRoomIdBox.contains(m_mousePos) ? QColor(24, 34, 50) : QColor(19, 27, 40));
            p.setPen(QPen(QColor(0, 200, 240, 160), 1));
            p.drawRoundedRect(m_hostRoomIdBox, 4, 4);
            p.setFont(QFont("Arial", 10, QFont::Bold));
            p.setPen(QColor(0, 235, 255));
            p.drawText(m_hostRoomIdBox.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, m_hostRoomId);

            // Generate Random Room ID Button
            m_hostRandomIdBtn = QRect(hostCard.x() + cardW - 18 - idBtnW, currentY + 68, idBtnW, 30);
            p.setBrush(m_hostRandomIdBtn.contains(m_mousePos) ? QColor(32, 48, 72) : QColor(24, 36, 54));
            p.setPen(QPen(QColor(65, 100, 145), 1));
            p.drawRoundedRect(m_hostRandomIdBtn, 4, 4);
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(195, 225, 255));
            p.drawText(m_hostRandomIdBtn, Qt::AlignCenter, "NEW CODE");

            // Host Room Password Field
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(175, 195, 220));
            p.drawText(hostCard.x() + 18, currentY + 114, "ROOM PASSKEY  (CLICK TO EDIT  |  BLANK = OPEN LOBBY)");
            m_hostPassBox = QRect(hostCard.x() + 18, currentY + 120, cardW - 36, 30);
            p.setBrush(m_hostPassBox.contains(m_mousePos) ? QColor(24, 34, 50) : QColor(19, 27, 40));
            p.setPen(QPen(QColor(0, 200, 240, 160), 1));
            p.drawRoundedRect(m_hostPassBox, 4, 4);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(m_hostRoomPassword.isEmpty() ? QColor(110, 135, 165) : QColor(255, 205, 55));
            p.drawText(m_hostPassBox.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       m_hostRoomPassword.isEmpty() ? "OPEN LOBBY (NO PASSKEY)" : QString("PASSKEY: %1").arg(m_hostRoomPassword));

            // Info Box
            QRect infoBox(hostCard.x() + 18, currentY + 158, cardW - 36, 32);
            p.setBrush(QColor(10, 15, 24, 190));
            p.setPen(QPen(QColor(30, 42, 60), 1));
            p.drawRoundedRect(infoBox, 4, 4);
            p.setFont(QFont("Arial", 7, QFont::Normal));
            p.setPen(QColor(135, 160, 190));
            p.drawText(infoBox.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       QString("Share Room ID [%1] with friends to race together on local or online networks.").arg(m_hostRoomId));

            // Create & Host Button
            m_createRoomBtn = QRect(hostCard.x() + 18, currentY + cardH - 44, cardW - 36, 34);
            bool hostBtnHov = m_createRoomBtn.contains(m_mousePos);
            QLinearGradient hostGrad(m_createRoomBtn.topLeft(), m_createRoomBtn.bottomRight());
            hostGrad.setColorAt(0.0, hostBtnHov ? QColor(20, 210, 255) : QColor(0, 185, 230));
            hostGrad.setColorAt(1.0, hostBtnHov ? QColor(0, 145, 215) : QColor(0, 120, 185));
            p.setBrush(hostGrad);
            p.setPen(QPen(QColor(110, 245, 255), hostBtnHov ? 1.8 : 1.2));
            p.drawRoundedRect(m_createRoomBtn, 5, 5);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(m_createRoomBtn, Qt::AlignCenter, "CREATE & HOST ROOM");

            // --- RIGHT CARD: JOIN ROOM BY ID & PASSWORD ---
            QRect joinCard(paneX + cardW + cardGap, currentY, cardW, cardH);
            p.setBrush(QColor(13, 19, 29, 238));
            p.setPen(QPen(QColor(42, 58, 82), 1.5));
            p.drawRoundedRect(joinCard, 7, 7);
            p.fillRect(QRect(joinCard.x(), joinCard.y(), cardW, 3), QColor(255, 195, 40));

            p.setFont(QFont("Arial", 11, QFont::Bold));
            p.setPen(QColor(255, 205, 55));
            p.drawText(joinCard.x() + 18, currentY + 24, "02  •  JOIN MULTIPLAYER ROOM");
            p.setFont(QFont("Arial", 8, QFont::Normal));
            p.setPen(QColor(145, 165, 190));
            p.drawText(joinCard.x() + 18, currentY + 41, "Enter a Room ID & Passkey or click any active lobby below.");

            // Target Room ID Input
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(175, 195, 220));
            p.drawText(joinCard.x() + 18, currentY + 62, "ROOM ID  (CLICK TO EDIT)");
            m_joinRoomIdBox = QRect(joinCard.x() + 18, currentY + 68, cardW - 36, 30);
            p.setBrush(m_joinRoomIdBox.contains(m_mousePos) ? QColor(24, 34, 50) : QColor(19, 27, 40));
            p.setPen(QPen(QColor(255, 195, 40, 160), 1));
            p.drawRoundedRect(m_joinRoomIdBox, 4, 4);
            p.setFont(QFont("Arial", 10, QFont::Bold));
            p.setPen(QColor(255, 210, 65));
            p.drawText(m_joinRoomIdBox.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft, m_joinRoomId);

            // Target Room Password Input
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(175, 195, 220));
            p.drawText(joinCard.x() + 18, currentY + 114, "ROOM PASSKEY  (CLICK TO EDIT)");
            m_joinPassBox = QRect(joinCard.x() + 18, currentY + 120, cardW - 36, 30);
            p.setBrush(m_joinPassBox.contains(m_mousePos) ? QColor(24, 34, 50) : QColor(19, 27, 40));
            p.setPen(QPen(QColor(255, 195, 40, 160), 1));
            p.drawRoundedRect(m_joinPassBox, 4, 4);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(m_joinRoomPassword.isEmpty() ? QColor(110, 135, 165) : QColor(255, 210, 65));
            p.drawText(m_joinPassBox.adjusted(12, 0, -12, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       m_joinRoomPassword.isEmpty() ? "NO PASSKEY" : QString("PASSKEY: %1").arg(m_joinRoomPassword));

            // Active Rooms Detected Bar
            QRect discBox(joinCard.x() + 18, currentY + 158, cardW - 36, 32);
            p.setBrush(QColor(10, 15, 24, 190));
            p.setPen(QPen(QColor(30, 42, 60), 1));
            p.drawRoundedRect(discBox, 4, 4);

            const auto& hosts = net.getDiscoveredHosts();
            if (hosts.isEmpty()) {
                p.setFont(QFont("Arial", 7, QFont::Normal));
                p.setPen(QColor(135, 160, 190));
                p.drawText(discBox.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                           "Automatically discovers active rooms on your network and online server.");
            } else {
                p.setFont(QFont("Arial", 7, QFont::Bold));
                p.setPen(QColor(40, 230, 115));
                p.drawText(discBox.x() + 10, discBox.y() + 20, "LIVE ROOMS:");
                int chipX = discBox.x() + 86;
                for (int hi = 0; hi < qMin(3, (int)hosts.size()); ++hi) {
                    QString rId = hosts[hi].roomId;
                    QString dispChip = hosts[hi].isWanRelay ? QString("%1 [ONLINE]").arg(rId) : rId;
                    int cw = qMax(76, dispChip.length() * 7 + 18);
                    if (chipX + cw > discBox.right() - 6) break;
                    QRect chipRect(chipX, discBox.y() + 4, cw, 24);
                    bool isSelectedRoom = (m_joinRoomId.compare(rId, Qt::CaseInsensitive) == 0);
                    p.setBrush(isSelectedRoom ? QColor(40, 230, 115, 55) : QColor(24, 36, 52));
                    p.setPen(QPen(isSelectedRoom ? QColor(40, 230, 115) : QColor(65, 95, 130), 1));
                    p.drawRoundedRect(chipRect, 3, 3);
                    p.setPen(Qt::white);
                    p.drawText(chipRect, Qt::AlignCenter, dispChip);
                    m_discoveredRoomChips.append(qMakePair(chipRect, rId));
                    chipX += cw + 6;
                }
            }

            // Enter & Join Button
            m_enterRoomBtn = QRect(joinCard.x() + 18, currentY + cardH - 44, cardW - 36, 34);
            bool joinBtnHov = m_enterRoomBtn.contains(m_mousePos);
            QLinearGradient joinGrad(m_enterRoomBtn.topLeft(), m_enterRoomBtn.bottomRight());
            joinGrad.setColorAt(0.0, joinBtnHov ? QColor(255, 175, 35) : QColor(235, 155, 20));
            joinGrad.setColorAt(1.0, joinBtnHov ? QColor(205, 120, 15) : QColor(180, 100, 10));
            p.setBrush(joinGrad);
            p.setPen(QPen(QColor(255, 220, 85), joinBtnHov ? 1.8 : 1.2));
            p.drawRoundedRect(m_enterRoomBtn, 5, 5);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(Qt::white);
            p.drawText(m_enterRoomBtn, Qt::AlignCenter, net.isSearchingRoom() ? "CONNECTING..." : "JOIN ROOM");

            currentY += cardH + 10;

            // Guide Footer Banner
            QRect tipCard(paneX, currentY, paneW, 28);
            p.setBrush(QColor(12, 16, 24, 210));
            p.setPen(QPen(QColor(32, 44, 60), 1));
            p.drawRoundedRect(tipCard, 5, 5);
            p.setFont(QFont("Arial", 7, QFont::Normal));
            p.setPen(QColor(155, 185, 215));
            p.drawText(tipCard, Qt::AlignCenter,
                       "ONLINE PLAY: Connect across different networks by setting the same [ONLINE SERVER] address, or race instantly on local Wi-Fi/LAN!");

            currentY += 36;
        } else {
            // 4. ACTIVE ROOM LOBBY ROSTER & CONTROLS (when connected)
            int lobbyH = 248;
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
            p.drawText(paneX + 240, currentY + 29, QString("GRID: %1 / 8 DRIVERS   •   ROLE: %2   •   1.25x MULTIPLAYER COIN BONUS")
                .arg(totalConnected)
                .arg(net.isHost() ? "ROOM HOST" : QString("DRIVER P%1").arg(net.getMyPeerId() + 1)));

            p.setFont(QFont("Arial", 8, QFont::Bold));
            p.setPen(QColor(0, 220, 255));
            p.drawText(QRect(paneX + paneW - 310, currentY + 16, 294, 18), Qt::AlignRight,
                QString("PING: %1 MS   •   SYNC: LIVE").arg(net.getAveragePingMs()));

            // 8-Player Slot Grid (2 rows x 4 columns) with Supercar Thumbnails
            int slotCols = 4;
            int slotGap = 8;
            int slotW = (paneW - 32 - (slotCols - 1) * slotGap) / slotCols;
            int slotH = 54;
            const auto& peers = net.getPeers();

            for (int slot = 0; slot < 8; ++slot) {
                int col = slot % slotCols;
                int row = slot / slotCols;
                int sx = paneX + 16 + col * (slotW + slotGap);
                int sy = currentY + 46 + row * (slotH + 8);
                QRect sRect(sx, sy, slotW, slotH);

                if (slot == net.getMyPeerId()) {
                    p.setBrush(QColor(18, 34, 52));
                    p.setPen(QPen(QColor(0, 230, 255), 1.5));
                    p.drawRoundedRect(sRect, 5, 5);
                    p.fillRect(QRect(sx, sy + 6, 3, slotH - 12), QColor(0, 230, 255));

                    int myCarIdx = qBound(0, m_engine.getPlayer().selectedCar, 3);
                    const QImage& carImg = RasterAssets::instance().getVehicle(kCars[myCarIdx].sprite, true, false, false, false);
                    p.drawImage(QRect(sx + 8, sy + 8, 44, 34), carImg);

                    p.setFont(QFont("Arial", 8, QFont::Bold));
                    p.setPen(QColor(0, 235, 255));
                    p.drawText(sx + 58, sy + 20, QString("[P%1] %2 (YOU)").arg(slot + 1).arg(net.getLocalPlayerName()));
                    p.setFont(QFont("Arial", 7, QFont::Normal));
                    p.setPen(QColor(175, 200, 225));
                    QString readyStr = (net.isHost() || net.isLocalReady()) ? "READY" : "STANDBY";
                    p.drawText(sx + 58, sy + 38, QString("%1  |  %2").arg(m_engine.getPlayer().carModelName).arg(readyStr));
                } else if (peers.contains(slot)) {
                    const auto& pr = peers[slot];
                    p.setBrush(QColor(20, 28, 40));
                    p.setPen(QPen(QColor(255, 200, 50), 1.2));
                    p.drawRoundedRect(sRect, 5, 5);
                    p.fillRect(QRect(sx, sy + 6, 3, slotH - 12), QColor(255, 200, 50));

                    int peerCarIdx = qBound(0, pr.carModel, 3);
                    const QImage& carImg = RasterAssets::instance().getVehicle(kCars[peerCarIdx].sprite, true, false, false, false);
                    p.drawImage(QRect(sx + 8, sy + 8, 44, 34), carImg);

                    p.setFont(QFont("Arial", 8, QFont::Bold));
                    p.setPen(QColor(255, 210, 65));
                    p.drawText(sx + 58, sy + 20, QString("[P%1] %2").arg(slot + 1).arg(pr.name));
                    p.setFont(QFont("Arial", 7, QFont::Normal));
                    p.setPen(QColor(165, 185, 210));
                    QString cName = kCars[peerCarIdx].name;
                    QString statusStr = (slot == 0) ? "HOST" : (pr.isReady ? "READY" : "STANDBY");
                    p.drawText(sx + 58, sy + 38, QString("%1  |  %2 ms  |  %3").arg(cName).arg(pr.pingMs).arg(statusStr));
                } else {
                    p.setBrush(QColor(14, 18, 25, 150));
                    p.setPen(QPen(QColor(34, 44, 60), 1));
                    p.drawRoundedRect(sRect, 5, 5);
                    p.setFont(QFont("Arial", 7, QFont::Bold));
                    p.setPen(QColor(75, 95, 120));
                    p.drawText(sRect, Qt::AlignCenter, QString("GRID SLOT P%1  •  OPEN").arg(slot + 1));
                }
            }

            // Action Controls Bar
            int actY = currentY + lobbyH - 46;
            int actH = 34;

            m_readyBtn = QRect(paneX + 16, actY, 150, actH);
            bool locReady = net.isLocalReady();
            p.setBrush(locReady ? QColor(20, 55, 38) : QColor(22, 34, 50));
            p.setPen(QPen(locReady ? QColor(40, 230, 115) : QColor(0, 220, 255), 1.2));
            p.drawRoundedRect(m_readyBtn, 5, 5);
            p.setFont(QFont("Arial", 8, QFont::Bold));
            p.setPen(locReady ? QColor(40, 230, 115) : QColor(0, 235, 255));
            p.drawText(m_readyBtn, Qt::AlignCenter, locReady ? "STATUS: READY" : "SET READY");

            int startW = 380;
            m_startMpBtn = QRect(paneX + (paneW - startW) / 2, actY, startW, actH);
            bool canStart = (net.isHost() && totalConnected >= 2);
            QLinearGradient sGrad(m_startMpBtn.topLeft(), m_startMpBtn.bottomRight());
            if (canStart) {
                sGrad.setColorAt(0.0, QColor(0, 220, 120));
                sGrad.setColorAt(1.0, QColor(0, 160, 80));
            } else if (net.isHost()) {
                sGrad.setColorAt(0.0, QColor(0, 155, 205));
                sGrad.setColorAt(1.0, QColor(0, 100, 155));
            } else {
                sGrad.setColorAt(0.0, QColor(26, 38, 54));
                sGrad.setColorAt(1.0, QColor(18, 28, 42));
            }
            p.setBrush(sGrad);
            p.setPen(QPen(canStart ? QColor(110, 255, 180) : (net.isHost() ? QColor(0, 220, 255) : QColor(50, 70, 95)), 1.5));
            p.drawRoundedRect(m_startMpBtn, 5, 5);
            p.setFont(QFont("Arial", 9, QFont::Bold));
            p.setPen(canStart ? QColor(8, 20, 12) : Qt::white);
            QString startText = net.isHost()
                ? (totalConnected >= 2 ? QString("LAUNCH MULTIPLAYER RACE  (%1 DRIVERS)").arg(totalConnected) : "LAUNCH PRACTICE OR WAIT FOR DRIVERS")
                : "WAITING FOR HOST TO LAUNCH RACE";
            p.drawText(m_startMpBtn, Qt::AlignCenter, startText);

            int discW = 160;
            m_disconnectBtn = QRect(paneX + paneW - 16 - discW, actY, discW, actH);
            p.setBrush(QColor(75, 22, 28));
            p.setPen(QPen(QColor(215, 60, 65), 1.2));
            p.drawRoundedRect(m_disconnectBtn, 5, 5);
            p.setFont(QFont("Arial", 8, QFont::Bold));
            p.setPen(QColor(255, 185, 185));
            p.drawText(m_disconnectBtn, Qt::AlignCenter, net.isHost() ? "CLOSE ROOM" : "LEAVE ROOM");

            currentY += lobbyH + 12;
        }

        // Return button at bottom of multiplayer tab
        m_resumeBtn = QRect(paneX + paneW / 2 - 140, currentY, 280, 34);
        bool retHov = m_resumeBtn.contains(m_mousePos);
        p.setBrush(retHov ? QColor(30, 44, 64) : QColor(22, 32, 46));
        p.setPen(QPen(retHov ? QColor(0, 220, 255) : QColor(60, 85, 115), 1.2));
        p.drawRoundedRect(m_resumeBtn, 5, 5);
        p.setFont(QFont("Arial", 9, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(m_resumeBtn, Qt::AlignCenter, "ENTER COCKPIT VIEW  [ESC / M]");

        p.restore();
        return;
    }

    // -------------------------------------------------------------
    // 1. AI GRID DIFFICULTY & COIN MULTIPLIER SELECTION ROW
    // -------------------------------------------------------------
    int diffGap = 14;
    int diffW = (contentW - 2 * diffGap) / 3;
    int diffH = 44;
    int diffY = 86;

    struct DiffData {
        GameDifficulty diff;
        QString title;
        QString desc;
        QColor accent;
    } diffs[] = {
        { DIFFICULTY_EASY,   "TIER 1  •  ROOKIE SERIES",     "6-Car Grid   |   1.0x Coin Rewards (1st: 500 🪙)",   QColor(40, 225, 110) },
        { DIFFICULTY_NORMAL, "TIER 2  •  PRO CIRCUIT",       "10-Car Grid  |   1.5x Coin Rewards (1st: 750 🪙)",   QColor(255, 195, 40) },
        { DIFFICULTY_PRO,    "TIER 3  •  WORLD CHAMPIONSHIP","16-Car Grid  |   2.5x Coin Rewards (1st: 1,250 🪙)", QColor(255, 60, 80) }
    };

    GameDifficulty curDiff = m_engine.getDifficulty();

    for (int i = 0; i < 3; ++i) {
        m_diffButtons[i] = QRect(contentX + i * (diffW + diffGap), diffY, diffW, diffH);
        bool isCur = (curDiff == diffs[i].diff);
        bool isHov = m_diffButtons[i].contains(m_mousePos);

        p.setBrush(isCur ? QColor(18, 27, 42, 240) : (isHov ? QColor(18, 25, 38, 230) : QColor(12, 17, 26, 225)));
        p.setPen(QPen(isCur ? diffs[i].accent : (isHov ? diffs[i].accent.darker(125) : QColor(38, 50, 68)), isCur ? 1.8 : 1.1));
        p.drawRoundedRect(m_diffButtons[i], 6, 6);
        p.fillRect(QRect(m_diffButtons[i].x(), m_diffButtons[i].y() + 6, 3, diffH - 12),
                   isCur ? diffs[i].accent : QColor(50, 62, 80));

        p.setFont(QFont("Arial", 9, QFont::Bold));
        p.setPen(isCur ? diffs[i].accent : Qt::white);
        p.drawText(QRect(m_diffButtons[i].x() + 12, m_diffButtons[i].y() + 5, m_diffButtons[i].width() - 24, 16), Qt::AlignLeft, diffs[i].title);

        if (isCur) {
            QRect activeBadge(m_diffButtons[i].right() - 68, m_diffButtons[i].y() + 6, 58, 15);
            p.setBrush(QColor(diffs[i].accent.red(), diffs[i].accent.green(), diffs[i].accent.blue(), 40));
            p.setPen(QPen(diffs[i].accent, 1));
            p.drawRoundedRect(activeBadge, 3, 3);
            p.setFont(QFont("Arial", 6, QFont::Bold));
            p.setPen(diffs[i].accent);
            p.drawText(activeBadge, Qt::AlignCenter, "ACTIVE");
        }

        p.setFont(QFont("Arial", 7, QFont::Normal));
        p.setPen(QColor(165, 185, 210));
        p.drawText(QRect(m_diffButtons[i].x() + 12, m_diffButtons[i].y() + 23, m_diffButtons[i].width() - 24, 16), Qt::AlignLeft, diffs[i].desc);
    }

    // -------------------------------------------------------------
    // 2. ENVIRONMENT LIGHTING, SUPERCAR QUICK-SELECT & ENGINE TUNING BUTTON
    // -------------------------------------------------------------
    int todY = 136;
    int todH = 38;
    int todW = 74;
    TimeOfDay currentTod = m_engine.getTimeOfDay();

    struct TodData {
        TimeOfDay tod;
        QString label;
        QColor activeCol;
    } tods[] = {
        { TIME_DAY, "DAYLIGHT", QColor(30, 150, 245) },
        { TIME_SUNSET, "SUNSET", QColor(240, 120, 35) },
        { TIME_NIGHT, "MIDNIGHT", QColor(140, 60, 240) }
    };

    for (int i = 0; i < 3; ++i) {
        m_todButtons[i] = QRect(contentX + i * (todW + 6), todY, todW, todH);
        bool isCur = (currentTod == tods[i].tod);
        bool isHov = m_todButtons[i].contains(m_mousePos);

        p.setBrush(isCur ? tods[i].activeCol : (isHov ? QColor(22, 30, 44) : QColor(14, 19, 28, 225)));
        p.setPen(QPen(isCur ? Qt::white : (isHov ? tods[i].activeCol : QColor(40, 52, 70)), isCur ? 1.5 : 1.0));
        p.drawRoundedRect(m_todButtons[i], 5, 5);

        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(isCur ? Qt::white : QColor(175, 195, 215));
        p.drawText(m_todButtons[i], Qt::AlignCenter, tods[i].label);
    }

    int garageQuickW = 178;
    m_openGarageQuickBtn = QRect(contentX + contentW - garageQuickW, todY, garageQuickW, todH);
    bool gqHov = m_openGarageQuickBtn.contains(m_mousePos);
    QLinearGradient gqGrad(m_openGarageQuickBtn.topLeft(), m_openGarageQuickBtn.bottomRight());
    gqGrad.setColorAt(0.0, gqHov ? QColor(75, 58, 16, 245) : QColor(46, 36, 12, 235));
    gqGrad.setColorAt(1.0, QColor(22, 28, 42, 235));
    p.setBrush(gqGrad);
    p.setPen(QPen(QColor(255, 210, 60), gqHov ? 1.8 : 1.3));
    p.drawRoundedRect(m_openGarageQuickBtn, 5, 5);
    p.setFont(QFont("Arial", 7, QFont::Black));
    p.setPen(QColor(255, 225, 80));
    p.drawText(m_openGarageQuickBtn.adjusted(8, 3, -8, -18), Qt::AlignCenter, "⚙ TUNE ENGINE & GARAGE");
    p.setFont(QFont("Arial", 6, QFont::Bold));
    p.setPen(QColor(185, 225, 255));
    p.drawText(m_openGarageQuickBtn.adjusted(6, 18, -6, -3), Qt::AlignCenter,
               QString("%1 (%2 HP)").arg(player.engineModelName).arg(player.horsepower));

    int carStartX = m_todButtons[2].right() + 12;
    int remW = m_openGarageQuickBtn.left() - 12 - carStartX;
    int carBtnW = (remW - 3 * 8) / 4;
    int curCar = m_engine.getPlayer().selectedCar;
    for (int i = 0; i < 4; ++i) {
        int cIdx = kCarDisplayOrder[i];
        const CarSpec& cs = kCars[cIdx];
        QColor col(cs.accentRgb);
        bool owned = prof.ownsCar(cIdx);
        m_carButtons[i] = QRect(carStartX + i * (carBtnW + 8), todY, carBtnW, todH);
        bool isSel = (curCar == cIdx);
        bool isHov = m_carButtons[i].contains(m_mousePos);
        p.setBrush(isSel ? QColor(20, 30, 46, 245) : (isHov ? QColor(20, 28, 42, 235) : QColor(13, 18, 28, 225)));
        p.setPen(QPen(isSel ? col : (isHov ? col.darker(115) : QColor(40, 52, 70)), isSel ? 1.8 : 1.0));
        p.drawRoundedRect(m_carButtons[i], 5, 5);
        if (isSel) {
            p.fillRect(QRect(m_carButtons[i].x() + 8, m_carButtons[i].bottom() - 3, carBtnW - 16, 2), col);
        }

        const QImage& carSprite = RasterAssets::instance().getVehicle(cs.sprite, true, false, false, false);
        QRect spriteRect(m_carButtons[i].x() + 5, m_carButtons[i].y() + 5, 36, 27);
        p.drawImage(spriteRect, carSprite);

        p.setFont(QFont("Arial", 7, QFont::Bold));
        p.setPen(isSel ? col : (owned ? Qt::white : QColor(255, 210, 65)));
        p.drawText(m_carButtons[i].adjusted(44, 3, -4, -18), Qt::AlignLeft | Qt::AlignVCenter, cs.name);

        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.setPen(owned ? QColor(145, 170, 195) : QColor(255, 195, 55));
        QString subInfo = owned
            ? QString("%1 KM/H • OWNED").arg((int)(cs.maxSpeed + kEngines[player.selectedEngine].topSpeedBonus))
            : QString("🔒 UNLOCK: %1 🪙").arg(cs.price);
        p.drawText(m_carButtons[i].adjusted(44, 19, -4, -3), Qt::AlignLeft | Qt::AlignVCenter, subInfo);
    }

    // -------------------------------------------------------------
    // 3. 4 GRAND PRIX CIRCUIT SELECTION CARDS
    // -------------------------------------------------------------
    int cardGap = 14;
    int cardW = (contentW - 3 * cardGap) / 4;
    int cardH = qMin(312, h - 256);
    int cardY = 182;

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
        { THEME_SUNSET_COAST, "Pacific Coast", "Coastal Highway  •  Ocean Sweeps", "COASTAL GP", 2, 3, 2, QColor(255, 145, 45) },
        { THEME_ALPINE_CLIMB, "Alpine Summit", "Mountain Pass  •  High Elevation", "ALPINE PASS", 5, 2, 5, QColor(75, 180, 255) },
        { THEME_NEON_METROPOLIS, "Neo Metropolis", "Night Expressway  •  High Speed", "METRO CIRCUIT", 1, 5, 4, QColor(245, 65, 165) },
        { THEME_RED_ROCK, "Red Rock Canyon", "Desert Speedway  •  Canyon Crests", "CANYON RUN", 3, 3, 3, QColor(240, 105, 55) }
    };

    TrackTheme currentTheme = m_engine.getCurrentTheme();
    int p1Purse = qRound(500 * difficultyCoinMultiplier(static_cast<int>(curDiff)));

    for (int i = 0; i < 4; ++i) {
        int cx = contentX + i * (cardW + cardGap);
        m_trackCards[i] = QRect(cx, cardY, cardW, cardH);

        bool isSelected = (cards[i].theme == currentTheme);
        bool isHovered = m_trackCards[i].contains(m_mousePos);

        p.setBrush(isSelected ? QColor(20, 29, 44, 246) : (isHovered ? QColor(18, 25, 38, 240) : QColor(12, 17, 26, 232)));
        p.setPen(QPen(isSelected ? cards[i].accent : (isHovered ? cards[i].accent.lighter(115) : QColor(38, 50, 68)),
                      isSelected ? 2.2 : (isHovered ? 1.6 : 1.0)));
        p.drawRoundedRect(m_trackCards[i], 7, 7);

        // Preview thumbnail with bottom gradient vignette
        const QImage& bgImg = RasterAssets::instance().getBackdrop(cards[i].theme, currentTod);
        QRect thumbRect(cx + 6, cardY + 6, cardW - 12, qMin(104, cardH / 3));
        p.drawImage(thumbRect, bgImg);

        QLinearGradient thumbVig(0, thumbRect.top(), 0, thumbRect.bottom());
        thumbVig.setColorAt(0.0, QColor(0, 0, 0, 35));
        thumbVig.setColorAt(0.65, QColor(0, 0, 0, 0));
        thumbVig.setColorAt(1.0, QColor(8, 12, 20, 185));
        p.fillRect(thumbRect, thumbVig);

        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(isSelected ? cards[i].accent : QColor(0, 0, 0, 140), 1));
        p.drawRect(thumbRect);

        // Category Tag Badge
        p.setBrush(QColor(8, 12, 20, 225));
        p.setPen(QPen(cards[i].accent, 1));
        QRect tagRect(cx + 12, cardY + 12, 88, 18);
        p.drawRoundedRect(tagRect, 3, 3);
        p.setFont(QFont("Arial", 6, QFont::Bold));
        p.setPen(cards[i].accent);
        p.drawText(tagRect, Qt::AlignCenter, cards[i].tag);

        // Coin Purse Badge on Top-Right of Thumbnail
        QRect purseRect(cx + cardW - 102, cardY + 12, 90, 18);
        p.setBrush(QColor(28, 22, 6, 230));
        p.setPen(QPen(QColor(255, 205, 55), 1));
        p.drawRoundedRect(purseRect, 3, 3);
        p.setFont(QFont("Arial", 6, QFont::Black));
        p.setPen(QColor(255, 225, 75));
        p.drawText(purseRect, Qt::AlignCenter, QString("1ST: +%1 🪙").arg(p1Purse));

        // Card Title & Subtitle
        int textStartY = cardY + thumbRect.height() + 12;
        p.setFont(QFont("Arial", 11, QFont::Bold));
        p.setPen(Qt::white);
        p.drawText(QRect(cx + 12, textStartY, cardW - 24, 20), Qt::AlignLeft, cards[i].title);

        p.setFont(QFont("Arial", 7, QFont::Normal));
        p.setPen(QColor(150, 170, 195));
        p.drawText(QRect(cx + 12, textStartY + 19, cardW - 24, 18), Qt::AlignLeft, cards[i].subtitle);

        // Track Rating Bars (Elevation, Density, Challenge)
        auto drawRating = [&](int ry, const QString& label, int rating, const QColor& col) {
            p.setFont(QFont("Arial", 7, QFont::Bold));
            p.setPen(QColor(135, 155, 180));
            p.drawText(cx + 12, ry + 9, label);
            int barStartX = cx + cardW - 82;
            for (int seg = 0; seg < 5; ++seg) {
                p.setBrush((seg < rating) ? col : QColor(26, 34, 46));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(barStartX + seg * 14, ry + 2, 11, 6, 1, 1);
            }
        };

        int ratingBaseY = textStartY + 43;
        drawRating(ratingBaseY, "ELEVATION", cards[i].hills, QColor(0, 215, 255));
        drawRating(ratingBaseY + 18, "TRAFFIC", cards[i].traffic, QColor(255, 185, 40));
        drawRating(ratingBaseY + 36, "CHALLENGE", cards[i].difficulty, cards[i].accent);

        // Launch / Active Circuit Button inside card
        int btnH = 30;
        QRect btnRect(cx + 12, cardY + cardH - btnH - 10, cardW - 24, btnH);
        p.setBrush(isSelected ? cards[i].accent : (isHovered ? QColor(34, 46, 66) : QColor(22, 30, 44)));
        p.setPen(QPen(isSelected ? cards[i].accent.lighter(125) : (isHovered ? cards[i].accent : QColor(48, 62, 82)), 1.2));
        p.drawRoundedRect(btnRect, 5, 5);
        p.setFont(QFont("Arial", 8, QFont::Bold));
        p.setPen(isSelected ? QColor(8, 12, 18) : Qt::white);
        p.drawText(btnRect, Qt::AlignCenter, isSelected ? "SELECTED CIRCUIT  •  RACE ▶" : (isHovered ? "LAUNCH CIRCUIT ▶" : "SELECT CIRCUIT"));
    }

    // -------------------------------------------------------------
    // 4. COMMAND DOCK (Prominent Start Race CTA, Repair, Cycle Lighting)
    // -------------------------------------------------------------
    int primaryW = 310;
    int secondaryW = 215;
    int actH = 40;
    int actGap = 14;
    int actTotalW = primaryW + 2 * secondaryW + 2 * actGap;
    int actStartX = (w - actTotalW) / 2;
    int actY = cardY + cardH + 12;

    // Primary Start / Resume Race CTA Button with animated glow
    m_resumeBtn = QRect(actStartX, actY, primaryW, actH);
    bool startHov = m_resumeBtn.contains(m_mousePos);
    QLinearGradient resumeGrad(m_resumeBtn.topLeft(), m_resumeBtn.bottomRight());
    resumeGrad.setColorAt(0.0, startHov ? QColor(20, 240, 140) : QColor(0, 215, 120));
    resumeGrad.setColorAt(1.0, startHov ? QColor(0, 185, 95) : QColor(0, 160, 80));
    p.setBrush(resumeGrad);
    p.setPen(QPen(QColor(130, 255, 195), startHov ? 2.2 : 1.6));
    p.drawRoundedRect(m_resumeBtn, 6, 6);
    p.setFont(QFont("Arial", 10, QFont::Black));
    p.setPen(QColor(6, 18, 10));
    p.drawText(m_resumeBtn, Qt::AlignCenter,
               (m_engine.getPlayer().speed > 1.0) ? "▶  RESUME RACE  [ESC]" : "▶  START RACE  [ENTER / CLICK]");

    // Repair & Reset Grid Button
    m_repairBtn = QRect(actStartX + primaryW + actGap, actY, secondaryW, actH);
    bool repHov = m_repairBtn.contains(m_mousePos);
    p.setBrush(repHov ? QColor(42, 54, 74) : QColor(26, 36, 52));
    p.setPen(QPen(QColor(255, 165, 50), repHov ? 1.8 : 1.2));
    p.drawRoundedRect(m_repairBtn, 6, 6);
    p.setFont(QFont("Arial", 8, QFont::Bold));
    p.setPen(QColor(255, 200, 105));
    p.drawText(m_repairBtn, Qt::AlignCenter, "FRESH GRID START  [KEY R]");

    // Cycle Lighting Button
    m_lightsBtn = QRect(actStartX + primaryW + secondaryW + 2 * actGap, actY, secondaryW, actH);
    bool lgtHov = m_lightsBtn.contains(m_mousePos);
    p.setBrush(lgtHov ? QColor(32, 48, 72) : QColor(22, 34, 52));
    p.setPen(QPen(QColor(0, 210, 255), lgtHov ? 1.8 : 1.2));
    p.drawRoundedRect(m_lightsBtn, 6, 6);
    p.setPen(QColor(165, 230, 255));
    p.drawText(m_lightsBtn, Qt::AlignCenter, "TIME OF DAY  [KEY N]");

    // -------------------------------------------------------------
    // 5. BOTTOM CONTROLS FOOTER
    // -------------------------------------------------------------
    int bottomY = h - 24;
    p.setPen(QColor(135, 155, 182));
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.drawText(QRect(0, bottomY, w, 18), Qt::AlignCenter,
               "CONTROLS:  [W/S] THROTTLE & BRAKE   •   [A/D] STEER   •   [C] CAMERA VIEW   •   [F1-F4] SUPERCAR   •   [O] MULTIPLAYER   •   [M/ESC] MENU");

    p.restore();
}
