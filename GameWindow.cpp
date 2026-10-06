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

    // Initialize procedural raster assets
    RasterAssets::instance().initialize();

    // Initialize audio synthesizer
    m_audio.initialize();

    // Connect signals
    connect(&m_engine, &GameEngine::collisionOccurred, this, [this]() {
        m_audio.playCrash();
    });

    // 60 FPS Game Loop
    connect(&m_loopTimer, &QTimer::timeout, this, &GameWindow::onGameLoop);
    m_frameTimer.start();
    m_loopTimer.start(16); // ~60 FPS
}

void GameWindow::onGameLoop() {
    qint64 elapsedMs = m_frameTimer.restart();
    qreal dt = elapsedMs / 1000.0;
    if (dt > 0.05) dt = 0.05;

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

    // Render pure raster 3D game scene
    m_renderer.render(painter, m_engine, width(), height());

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
            m_engine.setThrottle(true);
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
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
            m_engine.setInfotainmentMode(INFO_MODE_WEB);
            break;
        case Qt::Key_2:
            m_engine.setInfotainmentMode(INFO_MODE_MUSIC);
            break;
        case Qt::Key_3:
            m_engine.setInfotainmentMode(INFO_MODE_RADAR);
            break;
        case Qt::Key_4:
            m_engine.setInfotainmentMode(INFO_MODE_CAR);
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
            m_engine.setThrottle(false);
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            m_engine.setBrake(false);
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

void GameWindow::mousePressEvent(QMouseEvent *event) {
    QPoint pos = event->pos();

    if (m_showMainMenu) {
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

    // Touchscreen Dashboard Center Console Interaction (Web browser, Music player, GPS radar)
    int w = width();
    int h = height();
    const PlayerCar& player = m_engine.getPlayer();
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

    if (screenRect.contains(pos)) {
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

    QWidget::mousePressEvent(event);
}

void GameWindow::selectTrack(TrackTheme theme) {
    m_engine.setTrackTheme(theme);
}

void GameWindow::renderMainMenuOverlay(QPainter& p) {
    int w = width();
    int h = height();

    // Dark frosted glass overlay
    p.fillRect(0, 0, w, h, QColor(8, 12, 18, 244));

    // Top Header Badge & Title
    p.setBrush(QColor(0, 240, 255, 35));
    p.setPen(QPen(QColor(0, 240, 255, 160), 1.5));
    QRect headerPill(w / 2 - 130, 12, 260, 22);
    p.drawRoundedRect(headerPill, 11, 11);
    QFont pf("Arial", 8, QFont::Bold);
    p.setFont(pf);
    p.setPen(QColor(0, 240, 255));
    p.drawText(headerPill, Qt::AlignCenter, "★ APEX VELOCITY DASHBOARD ★");

    QFont titleFont("Arial", 18, QFont::Black);
    p.setFont(titleFont);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 36, w, 28), Qt::AlignCenter, "MAIN CONTROL CENTER & RACING SETTINGS");

    QFont subFont("Arial", 9, QFont::Normal);
    p.setFont(subFont);
    p.setPen(QColor(165, 185, 210));
    p.drawText(QRect(0, 64, w, 18), Qt::AlignCenter, "Press [M] or [ESC] to Resume Race • Choose Difficulty, Track, & Environment");

    // -------------------------------------------------------------
    // 1. DIFFICULTY SELECTION ROW (EASY, NORMAL, PRO)
    // -------------------------------------------------------------
    int diffW = qMin(340, (w - 100) / 3);
    int diffH = 50;
    int diffTotalW = 3 * diffW + 2 * 14;
    int diffStartX = (w - diffTotalW) / 2;
    int diffY = 88;

    struct DiffData {
        GameDifficulty diff;
        QString title;
        QString desc;
        QColor accent;
    } diffs[] = {
        { DIFFICULTY_EASY, "🟢 EASY / CASUAL", "18 Cars • 0.4x Damage • High Grip • 1.0x Score", QColor(40, 225, 90) },
        { DIFFICULTY_NORMAL, "🟡 NORMAL / HIGHWAY", "24 Cars • Standard Damage • Authentic Flow • 1.5x Score", QColor(255, 195, 30) },
        { DIFFICULTY_PRO, "🔴 PRO / INSANE", "28 Cars • Heavy Impacts • Fast Traffic • 2.5x SCORE", QColor(255, 45, 65) }
    };

    GameDifficulty curDiff = m_engine.getDifficulty();

    for (int i = 0; i < 3; ++i) {
        m_diffButtons[i] = QRect(diffStartX + i * (diffW + 14), diffY, diffW, diffH);
        bool isCur = (curDiff == diffs[i].diff);

        p.setBrush(isCur ? QColor(24, 32, 45) : QColor(16, 20, 28));
        p.setPen(QPen(isCur ? diffs[i].accent : QColor(48, 56, 72), isCur ? 2.5 : 1));
        p.drawRoundedRect(m_diffButtons[i], 8, 8);

        // Header Title
        QFont dtf("Arial", 10, QFont::Bold);
        p.setFont(dtf);
        p.setPen(isCur ? diffs[i].accent : Qt::white);
        p.drawText(QRect(m_diffButtons[i].x() + 8, m_diffButtons[i].y() + 6, m_diffButtons[i].width() - 16, 20), Qt::AlignLeft, diffs[i].title);

        if (isCur) {
            p.setPen(diffs[i].accent);
            p.drawText(QRect(m_diffButtons[i].x() + m_diffButtons[i].width() - 85, m_diffButtons[i].y() + 6, 75, 20), Qt::AlignRight, "ACTIVE ✓");
        }

        // Subtitle description
        QFont ddf("Arial", 8, QFont::Normal);
        p.setFont(ddf);
        p.setPen(QColor(150, 165, 185));
        p.drawText(QRect(m_diffButtons[i].x() + 8, m_diffButtons[i].y() + 27, m_diffButtons[i].width() - 16, 18), Qt::AlignLeft, diffs[i].desc);
    }

    // -------------------------------------------------------------
    // 2. TIME OF DAY SELECTION ROW
    // -------------------------------------------------------------
    int todW = 140;
    int todH = 32;
    int todStartX = (w - (3 * todW + 2 * 14)) / 2;
    int todY = 146;
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

        p.setBrush(isCur ? tods[i].activeCol : QColor(20, 24, 34));
        p.setPen(QPen(isCur ? Qt::white : QColor(50, 60, 78), isCur ? 2 : 1));
        p.drawRoundedRect(m_todButtons[i], 6, 6);

        QFont todf("Arial", 9, QFont::Bold);
        p.setFont(todf);
        p.setPen(Qt::white);
        p.drawText(m_todButtons[i], Qt::AlignCenter, tods[i].label);
    }

    // -------------------------------------------------------------
    // 3. 4 TRACK CARDS
    // -------------------------------------------------------------
    int cardW = qMin(265, (w - 110) / 4);
    int cardH = 325;
    int totalCardsW = 4 * cardW + 3 * 16;
    int startX = (w - totalCardsW) / 2;
    int cardY = 188;

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

        p.setBrush(isSelected ? QColor(26, 34, 48) : QColor(16, 20, 28));
        p.setPen(QPen(isSelected ? cards[i].accent : QColor(42, 50, 68), isSelected ? 3 : 1));
        p.drawRoundedRect(m_trackCards[i], 10, 10);

        // Preview thumbnail
        const QImage& bgImg = RasterAssets::instance().getBackdrop(cards[i].theme, currentTod);
        QRect thumbRect(cx + 6, cardY + 6, cardW - 12, 105);
        p.drawImage(thumbRect, bgImg);

        // Tag pill
        p.setBrush(cards[i].accent);
        p.setPen(Qt::NoPen);
        QRect tagRect(cx + 12, cardY + 12, 85, 18);
        p.drawRoundedRect(tagRect, 4, 4);
        QFont tf("Arial", 8, QFont::Bold);
        p.setFont(tf);
        p.setPen(Qt::white);
        p.drawText(tagRect, Qt::AlignCenter, cards[i].tag);

        // Card Title
        QFont ctf("Arial", 12, QFont::Bold);
        p.setFont(ctf);
        p.setPen(Qt::white);
        p.drawText(QRect(cx + 10, cardY + 118, cardW - 20, 22), Qt::AlignLeft, cards[i].title);

        // Subtitle
        QFont csf("Arial", 8, QFont::Normal);
        p.setFont(csf);
        p.setPen(QColor(155, 170, 190));
        p.drawText(QRect(cx + 10, cardY + 140, cardW - 20, 32), Qt::AlignLeft, cards[i].subtitle);

        // Rating bars (Hills, Traffic, Difficulty)
        auto drawRating = [&](int ry, const QString& label, int rating, const QColor& col) {
            QFont rf("Arial", 8, QFont::Bold);
            p.setFont(rf);
            p.setPen(QColor(135, 150, 175));
            p.drawText(cx + 12, ry + 10, label);
            for (int dot = 0; dot < 5; ++dot) {
                p.setBrush((dot < rating) ? col : QColor(36, 44, 56));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(cx + cardW - 75 + dot * 11, ry + 2, 7, 9, 2, 2);
            }
        };

        drawRating(cardY + 180, "HILLS", cards[i].hills, QColor(0, 220, 255));
        drawRating(cardY + 204, "TRAFFIC", cards[i].traffic, QColor(255, 180, 40));
        drawRating(cardY + 228, "DIFFICULTY", cards[i].difficulty, cards[i].accent);

        // Select button
        QRect btnRect(cx + 12, cardY + 275, cardW - 24, 36);
        p.setBrush(isSelected ? cards[i].accent : QColor(32, 42, 58));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(btnRect, 6, 6);
        QFont bf("Arial", 9, QFont::Bold);
        p.setFont(bf);
        p.setPen(Qt::white);
        p.drawText(btnRect, Qt::AlignCenter, isSelected ? "CURRENT TRACK" : "SELECT TRACK");
    }

    // -------------------------------------------------------------
    // 4. QUICK ACTION BUTTONS ROW (Resume, Repair, Lights)
    // -------------------------------------------------------------
    int actW = 210;
    int actH = 38;
    int actTotalW = 3 * actW + 2 * 16;
    int actStartX = (w - actTotalW) / 2;
    int actY = 525;

    m_resumeBtn = QRect(actStartX, actY, actW, actH);
    p.setBrush(QColor(35, 180, 85));
    p.setPen(QPen(QColor(60, 230, 120), 2));
    p.drawRoundedRect(m_resumeBtn, 8, 8);
    QFont actF("Arial", 10, QFont::Bold);
    p.setFont(actF);
    p.setPen(Qt::white);
    p.drawText(m_resumeBtn, Qt::AlignCenter, "▶️ RESUME RACE (M / ESC)");

    m_repairBtn = QRect(actStartX + actW + 16, actY, actW, actH);
    p.setBrush(QColor(220, 110, 30));
    p.setPen(QPen(QColor(255, 160, 60), 2));
    p.drawRoundedRect(m_repairBtn, 8, 8);
    p.drawText(m_repairBtn, Qt::AlignCenter, "🔄 REPAIR CAR (KEY R)");

    m_lightsBtn = QRect(actStartX + 2 * (actW + 16), actY, actW, actH);
    p.setBrush(QColor(30, 100, 180));
    p.setPen(QPen(QColor(60, 160, 255), 2));
    p.drawRoundedRect(m_lightsBtn, 8, 8);
    p.drawText(m_lightsBtn, Qt::AlignCenter, "💡 CYCLE LIGHTS (KEY N)");

    // -------------------------------------------------------------
    // 5. BOTTOM CONTROLS CHEATSHEET BANNER
    // -------------------------------------------------------------
    QFont hf("Arial", 9, QFont::Normal);
    p.setFont(hf);
    p.setPen(QColor(140, 160, 185));
    p.drawText(QRect(0, h - 38, w, 24), Qt::AlignCenter, "Controls: W,A,S,D: Drive/Brake | Q/E: Side POV Glance | B: Rear Camera | X: Wipers | M: Menu");
}
