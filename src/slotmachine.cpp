#include "slotmachine.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QDateTime>
#include <QTimer>

SlotMachineBoard::SlotMachineBoard(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    m_symbols = {QStringLiteral("🍒"), QStringLiteral("7"), QStringLiteral("♦"),
                 QStringLiteral("BAR"), QStringLiteral("★"), QStringLiteral("♠")};
    m_reels = {m_symbols[0], m_symbols[1], m_symbols[2], m_symbols[3], m_symbols[4]};
    m_winCols = QVector<bool>(5, false);
    m_timer = new QTimer(this);
    m_timer->setInterval(72);
    connect(m_timer, &QTimer::timeout, this, [this] {
        for (QString &reel : m_reels)
            reel = m_symbols.at(QRandomGenerator::global()->bounded(m_symbols.size()));
        if (++m_ticks >= 14) finishSpin();
        update();
    });
    auto *clock = new QTimer(this);
    clock->setInterval(1000);
    connect(clock, &QTimer::timeout, this, [this] { updateCooldown(); });
    clock->start();
    m_fxTimer = new QTimer(this);
    m_fxTimer->setInterval(80);
    connect(m_fxTimer, &QTimer::timeout, this, [this] {
        if (m_fxTicks <= 0) { m_jackpot = false; m_fxTimer->stop(); }
        else --m_fxTicks;
        update();
    });
}

void SlotMachineBoard::spin() {
    if (m_spinning) return;
    if (m_resetAt > 0) {
        updateCooldown();
        return;
    }
    if (m_coins < SPIN_COST || m_spinsLeft <= 0) {
        m_resetAt = QDateTime::currentSecsSinceEpoch() + 3600;
        m_message = QStringLiteral("本小时体验额度已用完，休息 1 小时后恢复 1000 金币");
        update();
        return;
    }
    m_coins -= SPIN_COST;
    --m_spinsLeft;
    m_spinning = true;
    m_ticks = 0;
    m_message = QStringLiteral("好运正在滚动……");
    m_winTitle.clear();
    m_winCols.fill(false);
    m_jackpot = false;
    m_timer->start();
    update();
}

void SlotMachineBoard::finishSpin() {
    m_timer->stop();
    m_spinning = false;
    const int prize = QRandomGenerator::global()->bounded(60, 121);
    if (QRandomGenerator::global()->bounded(100) >= 45) {
        for (QString &reel : m_reels)
            reel = m_symbols.at(QRandomGenerator::global()->bounded(m_symbols.size()));
        m_winCols.fill(false);
        m_message = QStringLiteral("这轮没有连线 · 再攒一点好运");
        emit resultChanged(m_message, m_wins);
        update();
        return;
    }
    const int pattern = QRandomGenerator::global()->bounded(5);
    m_winCols.fill(true);
    // 纯娱乐展示：每次都中奖，不连接真实货币或支付系统。
    if (pattern == 0) {
        const QString symbol = QStringLiteral("7");
        m_reels = {symbol, symbol, symbol, symbol, symbol};
        m_winTitle = QStringLiteral("MEGA WIN");
        const int jackpot = 1000;
        m_coins = qMin(1000, m_coins + jackpot);
        m_jackpot = true;
        m_fxTicks = 28;
        m_fxTimer->start();
        m_winTitle = QStringLiteral("✦ SUPER JACKPOT · MEGA WIN ✦");
        m_message = QStringLiteral("🎉 五连大奖！+%1 金币 · JACKPOT").arg(jackpot);
    } else if (pattern == 1) {
        m_reels = {QStringLiteral("BAR"), QStringLiteral("BAR"), QStringLiteral("BAR"), QStringLiteral("BAR"), QStringLiteral("★")};
        m_winTitle = QStringLiteral("BIG WIN");
        m_coins = qMin(1000, m_coins + 300);
        m_message = QStringLiteral("💎 四连 BAR！+300 金币 · 华丽收官");
    } else if (pattern == 2) {
        m_reels = {QStringLiteral("🔔"), QStringLiteral("🍋"), QStringLiteral("🔔"), QStringLiteral("🍒"), QStringLiteral("🔔")};
        m_winTitle = QStringLiteral("SCATTER BONUS");
        m_coins = qMin(1000, m_coins + 220);
        m_message = QStringLiteral("🔔 三枚铃铛触发奖励！+220 金币");
    } else if (pattern == 3) {
        m_reels = {QStringLiteral("♠"), QStringLiteral("♦"), QStringLiteral("7"), QStringLiteral("♦"), QStringLiteral("♠")};
        m_winTitle = QStringLiteral("LUCKY LINE");
        m_coins = qMin(1000, m_coins + 150);
        m_message = QStringLiteral("✨ 幸运中线命中！+150 金币 · 好运连成线");
    } else {
        m_reels = {QStringLiteral("🍒"), QStringLiteral("🍒"), QStringLiteral("★"), QStringLiteral("🍒"), QStringLiteral("🍒")};
        m_winTitle = QStringLiteral("FRUIT FESTIVAL");
        m_coins = qMin(1000, m_coins + prize);
        m_message = QStringLiteral("🍒 水果连线中奖！+%1 金币 · 再来一把").arg(prize);
    }
    ++m_wins;
    emit resultChanged(m_message, m_wins);
    update();
}

void SlotMachineBoard::updateCooldown() {
    if (m_resetAt <= 0) return;
    const qint64 left = m_resetAt - QDateTime::currentSecsSinceEpoch();
    if (left <= 0) {
        m_resetAt = 0;
        m_coins = 1000;
        m_spinsLeft = 20;
        m_message = QStringLiteral("金币已恢复 1000，欢迎回来");
    } else {
        m_message = QStringLiteral("金币已用光 · %1 分钟后恢复 1000 金币").arg((left + 59) / 60);
    }
    update();
}

void SlotMachineBoard::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && e->pos().y() > height() - 94)
        spin();
    QWidget::mousePressEvent(e);
}

void SlotMachineBoard::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(rect()).adjusted(5, 5, -5, -5);
    QPainterPath panel;
    panel.addRoundedRect(area, 24, 24);
    QLinearGradient bg(area.topLeft(), area.bottomRight());
    bg.setColorAt(0.0, QColor(35, 12, 68, 248));
    bg.setColorAt(0.5, QColor(78, 18, 91, 245));
    bg.setColorAt(1.0, QColor(12, 23, 66, 248));
    p.fillPath(panel, bg);

    QRadialGradient glow(QPointF(width() * 0.5, height() * 0.18), width() * 0.65);
    glow.setColorAt(0.0, QColor(255, 210, 88, 85));
    glow.setColorAt(1.0, QColor(255, 210, 88, 0));
    p.fillPath(panel, glow);
    if (m_jackpot && m_fxTicks > 0) {
        const qreal pulse = 0.5 + 0.5 * qSin(m_fxTicks * 0.75);
        p.setPen(QPen(QColor(255, 224, 108, 90 + int(90 * pulse)), 5 + int(4 * pulse)));
        p.drawRoundedRect(area.adjusted(12, 12, -12, -12), 20, 20);
    }
    p.setPen(QPen(QColor(255, 214, 96, 190), 2));
    p.drawPath(panel);

    p.setPen(QColor(255, 238, 178));
    p.setFont(QFont(QStringLiteral("DejaVu Sans"), 27, QFont::Bold));
    p.drawText(QRectF(0, 28, width(), 46), Qt::AlignCenter, QStringLiteral("✦ LUCKY 777 ✦"));
    p.setPen(QColor(255, 255, 255, 175));
    p.setFont(QFont(QStringLiteral("DejaVu Sans"), 11));
    p.drawText(QRectF(0, 72, width(), 24), Qt::AlignCenter, QStringLiteral("无限金币 · 每一把都有好运 · 纯娱乐体验"));

    const int reelW = qMin(142, (width() - 150) / 5);
    const int reelH = 172;
    const int gap = 18;
    const int startX = (width() - reelW * 5 - gap * 4) / 2;
    const int y = 120;
    for (int i = 0; i < 5; ++i) {
        QRectF rr(startX + i * (reelW + gap), y, reelW, reelH);
        const bool win = i < m_winCols.size() && m_winCols[i];
        p.setPen(QPen(win ? QColor(255, 238, 132, 255) : QColor(255, 220, 120, 230), win ? 4 : 3));
        p.setBrush(win ? QColor(76, 42, 92, 245) : QColor(12, 17, 48, 230));
        p.drawRoundedRect(rr, 15, 15);
        p.setPen(QColor(255, 255, 255, 40));
        p.drawRoundedRect(rr.adjusted(8, 8, -8, -8), 10, 10);
        p.setPen(i == 1 ? QColor(255, 224, 120) : QColor(255, 255, 255));
        p.setFont(QFont(QStringLiteral("DejaVu Sans"), m_reels[i] == QStringLiteral("BAR") ? 24 : 56, QFont::Bold));
        p.drawText(rr, Qt::AlignCenter, m_reels[i]);
    }

    if (!m_winTitle.isEmpty()) {
        p.setPen(QPen(QColor(255, 230, 125, 210), 4, Qt::SolidLine, Qt::RoundCap));
        const qreal lineY = y + reelH * 0.5;
        p.drawLine(QPointF(startX + 16, lineY), QPointF(startX + reelW * 5 + gap * 4 - 16, lineY));
        p.setPen(QColor(255, 248, 196));
        p.setFont(QFont(QStringLiteral("DejaVu Sans"), 18, QFont::Bold));
        p.drawText(QRectF(0, 98, width(), 26), Qt::AlignCenter, m_winTitle);
    }

    p.setPen(QColor(255, 255, 255, 210));
    p.setFont(QFont(QStringLiteral("DejaVu Sans"), 13, QFont::Bold));
    p.drawText(QRectF(20, 310, width() - 40, 28), Qt::AlignCenter, m_message);
    p.setPen(QColor(255, 220, 140, 190));
    p.setFont(QFont(QStringLiteral("DejaVu Sans"), 11));
    p.drawText(QRectF(20, 340, width() - 40, 24), Qt::AlignCenter,
               QStringLiteral("累计中奖 %1 次 · 金币：%2 / 1000 · 剩余 %3 轮 · 每次消耗 %4")
                   .arg(m_wins).arg(m_coins).arg(m_spinsLeft).arg(SPIN_COST));

    QRectF button((width() - 190) / 2.0, height() - 78, 190, 48);
    QLinearGradient buttonG(button.topLeft(), button.bottomRight());
    buttonG.setColorAt(0.0, QColor(255, 194, 74));
    buttonG.setColorAt(1.0, QColor(226, 72, 123));
    p.setPen(QPen(QColor(255, 244, 190), 2));
    p.setBrush(buttonG);
    p.drawRoundedRect(button, 22, 22);
    p.setPen(Qt::white);
    p.setFont(QFont(QStringLiteral("DejaVu Sans"), 15, QFont::Bold));
    p.drawText(button, Qt::AlignCenter, m_spinning ? QStringLiteral("滚动中…") : QStringLiteral("🎰 立即开奖"));
}
