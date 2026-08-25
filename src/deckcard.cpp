#include "deckcard.h"
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QTextOption>
#include <cmath>

// 花色徽章字符（DejaVu/Symbols 字体链可渲染）
static QChar suitBadgeChar(const QString &suit) {
    if (suit == QLatin1String("network"))      return QChar(0x2666);  // ♦
    if (suit == QLatin1String("audiovideo"))   return QChar(0x2663);  // ♣
    if (suit == QLatin1String("browser"))      return QChar(0x2665);  // ♥
    if (suit == QLatin1String("development"))  return QChar(0x2660);  // ♠
    if (suit == QLatin1String("utility"))      return QChar(0x2605);  // ★
    if (suit == QLatin1String("system"))       return QChar(0x2699);  // ⚙
    return QChar(0x2726);                                                     // ✦
}

DeckCard::DeckCard(const AppEntry &a, QWidget *parent) : QWidget(parent), m_app(a) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(a.comment.isEmpty() ? a.name : a.comment);
    setProperty("deckPath", a.desktopPath);

    auto mk = [](int dur) {
        auto *an = new QVariantAnimation(nullptr);
        an->setDuration(dur);
        an->setStartValue(0.0);
        an->setEndValue(1.0);
        an->setEasingCurve(QEasingCurve::InOutCubic);
        return an;
    };
    m_lift = mk(130);
    m_flip = mk(420);   // win 版 .5s 弹性翻面的近似
    connect(m_lift, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_liftVal = v.toReal();
        update();
    });
    connect(m_flip, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_flipVal = v.toReal();
        update();
    });
}

void DeckCard::enterEvent(QEnterEvent *) {
    m_lift->setDirection(QVariantAnimation::Forward);
    m_flip->setDirection(QVariantAnimation::Forward);
    m_lift->start();
    m_flip->start();
}

void DeckCard::leaveEvent(QEvent *) {
    m_lift->setDirection(QVariantAnimation::Backward);
    m_flip->setDirection(QVariantAnimation::Backward);
    m_lift->start();
    m_flip->start();
}

void DeckCard::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal w = width(), h = height();
    const qreal lift = m_liftVal;

    p.translate(0.0, -7.0 * lift);

    // 悬停投影 + 金色外发光
    if (lift > 0.01) {
        p.setOpacity(0.5 * lift);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 150));
        QPainterPath sh;
        sh.addRoundedRect(QRectF(3, 6, w - 6, h - 8), 11, 11);
        p.drawPath(sh);
        p.setOpacity(0.8 * lift);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 205, 105, 60), 4));
        QPainterPath glow;
        glow.addRoundedRect(QRectF(1.5, 1.5, w - 3, h - 3), 11, 11);
        p.drawPath(glow);
        p.setOpacity(1.0);
    }

    // 3D 翻面：宽度按 |cos| 缩放，半程切换牌背/牌面（win 版 rotateY 的 2D 等效）
    const qreal prog = m_flipVal;
    const qreal sx = qMax(std::abs(std::cos(prog * M_PI)), 0.04);
    p.translate(w / 2.0, 0.0);
    p.scale(sx, 1.0);
    p.translate(-w / 2.0, 0.0);

    if (prog < 0.5) drawTarotBack(p);
    else drawFace(p);
}

// ---------------- 牌背：星夜 + 金框 + 角饰 + 花色徽章 ----------------
void DeckCard::drawTarotBack(QPainter &p) {
    const qreal w = width(), h = height();
    const qreal lift = m_liftVal;
    const qreal inset = 5;

    QPainterPath body;
    body.addRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 10, 10);
    // win 版玻璃系牌背：深青底 + 蒂芙尼→粉 150° 渐变（rgba(10,186,181,.55)→rgba(255,123,172,.42)）
    p.fillPath(body, QColor(9, 34, 38));
    QLinearGradient g(0, 0, w, h * 0.95);
    const qreal hl = m_liftVal;
    g.setColorAt(0.0, QColor(10, 186, 181, 135 + 35 * hl));
    g.setColorAt(1.0, QColor(255, 123, 172, 100 + 30 * hl));
    p.fillPath(body, g);
    p.setPen(QPen(QColor(255, 255, 255, 55 + 70 * hl), 1));
    p.drawPath(body);

    // 星点纹理：按应用名哈希确定性布点
    quint32 hsh = (quint32)qHash(m_app.name);
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 22; ++i) {
        hsh = hsh * 2654435761u + 1u;
        const qreal x = inset + 4 + (hsh % 1000) / 1000.0 * (w - 2 * inset - 8);
        hsh = hsh * 40503u + 7u;
        const qreal y = inset + 4 + (hsh % 1000) / 1000.0 * (h - 2 * inset - 8);
        hsh = hsh * 97u + 11u;
        const qreal r = 0.55 + (hsh % 10) / 10.0 * 0.85;
        p.setBrush(QColor(215, 226, 255, 26 + hsh % 42));
        p.drawEllipse(QPointF(x, y), r, r);
    }

    // 图标 + 名称直接在牌背上可见（win 版如此：星夜是底纹不是遮罩——牌必须一眼可辨识）
    if (!m_icon.isNull()) {
        const int isz = qMin<int>(w * 0.42, h * 0.36);
        const QPointF center(w / 2, h * 0.40);
        QRadialGradient halo(center, isz * 0.95);
        halo.setColorAt(0.0, QColor(255, 255, 255, 42));
        halo.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(center, isz * 0.95, isz * 0.95);
        p.drawPixmap(QRect(center.x() - isz / 2, center.y() - isz / 2, isz, isz), m_icon);
    } else {
        // 主题+兜底主题都找不到：名字首字金徽（任何牌都不会空白）
        const qreal br = qMin(w, h) * 0.19;
        const QPointF c(w / 2, h * 0.40);
        p.setPen(QPen(QColor(255, 222, 150, 190), 1.5));
        p.setBrush(QColor(255, 200, 92, 40));
        p.drawEllipse(c, br, br);
        p.setPen(QColor(255, 230, 170, 235));
        p.setFont(QFont(QStringLiteral("Noto Sans CJK SC"), qMax<qreal>(12, br * 0.9), QFont::Bold));
        p.drawText(QRectF(c.x() - br, c.y() - br, br * 2, br * 2), Qt::AlignCenter,
                   m_app.name.left(1));
    }
    p.setFont(QFont(QStringLiteral("Noto Sans CJK SC"), qMax<qreal>(8.5, h / 13.5)));
    p.setPen(QColor(240, 244, 252, 240));
    QTextOption nopt;
    nopt.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    nopt.setWrapMode(QTextOption::WordWrap);
    p.drawText(QRectF(8, h * 0.64, w - 16, h * 0.32), m_app.name, nopt);

    // 内框 + 四角菱形饰（白系——win 玻璃背无金框，悬停提亮）
    QPainterPath inner;
    inner.addRoundedRect(QRectF(inset, inset, w - 2 * inset, h - 2 * inset), 7, 7);
    p.setPen(QPen(QColor(255, 255, 255, 85 + 115 * lift), 1.5));
    p.drawPath(inner);
    p.setPen(QPen(QColor(255, 255, 255, 120 + 110 * lift), 1.4));
    const qreal d = 4.0;
    const QPointF corners[4] = {{inset + 7, inset + 7}, {w - inset - 7, inset + 7},
                                {inset + 7, h - inset - 7}, {w - inset - 7, h - inset - 7}};
    for (const QPointF &c : corners) {
        QPainterPath dia;
        dia.moveTo(c.x(), c.y() - d);
        dia.lineTo(c.x() + d, c.y());
        dia.lineTo(c.x(), c.y() + d);
        dia.lineTo(c.x() - d, c.y());
        p.drawPath(dia);
    }

    // 花色徽章：右上金底圆章
    const qreal br = qMax<qreal>(7.0, w * 0.085);
    const QPointF bc(w - inset - 5 - br, inset + 5 + br);
    p.setPen(QPen(QColor(255, 235, 180, 200), 1));
    p.setBrush(QColor(255, 200, 92, 205));
    p.drawEllipse(bc, br, br);
    p.setPen(QColor(46, 32, 6, 235));
    p.setFont(QFont(QStringLiteral("DejaVu Sans"), qMax<qreal>(6.5, br * 0.95), QFont::Bold));
    p.drawText(QRectF(bc.x() - br, bc.y() - br, br * 2, br * 2), Qt::AlignCenter,
               QString(suitBadgeChar(m_app.suit)));
}

// ---------------- 牌面：浅底 + 图标 + 名称 + 描述 ----------------
void DeckCard::drawFace(QPainter &p) {
    const qreal w = width(), h = height();
    const qreal inset = 5;

    // 浅色羊皮纸面（与牌背强反差，翻转瞬间可感知）
    QPainterPath body;
    body.addRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 10, 10);
    QLinearGradient g(0, 0, 0, h);
    g.setColorAt(0.0, QColor(249, 250, 252));
    g.setColorAt(1.0, QColor(236, 240, 246));
    p.fillPath(body, g);
    p.setPen(QPen(QColor(148, 163, 184, 190), 1));
    p.drawPath(body);
    // 内侧金线呼应牌背
    QPainterPath inner;
    inner.addRoundedRect(QRectF(inset, inset, w - 2 * inset, h - 2 * inset), 7, 7);
    p.setPen(QPen(QColor(196, 155, 60, 130), 1));
    p.drawPath(inner);

    if (!m_icon.isNull()) {
        const int isz = qMin<int>(w * 0.42, h * 0.34);
        const QPointF center(w / 2, h * 0.36);
        p.drawPixmap(QRect(center.x() - isz / 2, center.y() - isz / 2, isz, isz), m_icon);
    }

    // 名称（深字）
    p.setFont(QFont(QStringLiteral("Noto Sans CJK SC"), qMax<qreal>(8.5, h / 14.0), QFont::DemiBold));
    p.setPen(QColor(30, 41, 59, 245));
    QTextOption opt;
    opt.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    opt.setWrapMode(QTextOption::WordWrap);
    p.drawText(QRectF(8, h * 0.56, w - 16, h * 0.2), m_app.name, opt);

    // 描述（灰字两行）
    if (!m_app.comment.isEmpty()) {
        p.setFont(QFont(QStringLiteral("Noto Sans CJK SC"), qMax<qreal>(6.5, h / 20.0)));
        p.setPen(QColor(100, 116, 139, 220));
        p.drawText(QRectF(8, h * 0.74, w - 16, h * 0.22), m_app.comment, opt);
    }
}
