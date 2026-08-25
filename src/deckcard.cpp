#include "deckcard.h"
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QTextOption>

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

    m_lift = new QVariantAnimation(this);
    m_lift->setDuration(130);
    m_lift->setStartValue(0.0);
    m_lift->setEndValue(1.0);
    connect(m_lift, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_liftVal = v.toReal();
        update();
    });
    m_lift->stop();
    m_liftVal = 0.0;
}

void DeckCard::enterEvent(QEnterEvent *) {
    m_lift->setDirection(QVariantAnimation::Forward);
    m_lift->start();
}

void DeckCard::leaveEvent(QEvent *) {
    m_lift->setDirection(QVariantAnimation::Backward);
    m_lift->start();
}

void DeckCard::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal w = width(), h = height();
    const qreal lift = m_liftVal;                    // 0 静置 → 1 悬停
    const qreal inset = 5;
    p.translate(0.0, -7.0 * lift);

    // 悬停投影 + 金色外发光（越悬越亮）
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

    // 牌体：星夜渐变（悬停微亮）
    QPainterPath body;
    body.addRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 10, 10);
    QLinearGradient g(0, 0, w, h);
    const qreal hl = 0.5 + 0.5 * lift;
    g.setColorAt(0.0, QColor::fromRgbF(0.15 * (1 + 0.4 * hl), 0.18 * (1 + 0.4 * hl), 0.30 * (1 + 0.4 * hl)));
    g.setColorAt(1.0, QColor::fromRgbF(0.075, 0.09, 0.175));
    p.fillPath(body, g);
    p.setPen(QPen(QColor(255, 255, 255, 45 + 70 * lift), 1));
    p.drawPath(body);

    // 星点纹理：按应用名哈希确定性布点（每张牌自己的星空）
    {
        quint32 hsh = (quint32)qHash(m_app.name);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 18; ++i) {
            hsh = hsh * 2654435761u + 1u;
            const qreal x = inset + 4 + (hsh % 1000) / 1000.0 * (w - 2 * inset - 8);
            hsh = hsh * 40503u + 7u;
            const qreal y = inset + 4 + (hsh % 1000) / 1000.0 * (h * 0.55);
            hsh = hsh * 97u + 11u;
            const qreal r = 0.55 + (hsh % 10) / 10.0 * 0.85;
            p.setBrush(QColor(215, 226, 255, 26 + hsh % 42));
            p.drawEllipse(QPointF(x, y), r, r);
        }
    }

    // 金线内框 + 四角菱形饰（悬停时显著提亮）
    QPainterPath inner;
    inner.addRoundedRect(QRectF(inset, inset, w - 2 * inset, h - 2 * inset), 7, 7);
    p.setPen(QPen(QColor(255, 215, 130, 115 + 115 * lift), 1.5));
    p.drawPath(inner);
    p.setPen(QPen(QColor(255, 222, 150, 150 + 105 * lift), 1.4));
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
    {
        const qreal br = qMax<qreal>(7.0, w * 0.085);
        const QPointF bc(w - inset - 5 - br, inset + 5 + br);
        p.setPen(QPen(QColor(255, 235, 180, 200), 1));
        p.setBrush(QColor(255, 200, 92, 205));
        p.drawEllipse(bc, br, br);
        p.setPen(QColor(46, 32, 6, 235));
        QFont bf(QStringLiteral("DejaVu Sans"), qMax<qreal>(6.5, br * 0.95), QFont::Bold);
        p.setFont(bf);
        p.drawText(QRectF(bc.x() - br, bc.y() - br, br * 2, br * 2), Qt::AlignCenter,
                   QString(suitBadgeChar(m_app.suit)));
    }

    // 图标：柔和光晕底 + 图标本体
    if (!m_icon.isNull()) {
        const int isz = qMin<int>(w * 0.42, h * 0.36);
        const QPointF center(w / 2, h * 0.42);
        QRadialGradient halo(center, isz * 0.95);
        halo.setColorAt(0.0, QColor(255, 255, 255, 42));
        halo.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(center, isz * 0.95, isz * 0.95);
        const QRect iconRect(center.x() - isz / 2, center.y() - isz / 2, isz, isz);
        p.drawPixmap(iconRect, m_icon);
    }

    // 名称：底部两行居中（字号随牌高缩放）
    const int fs = qMax<qreal>(8.5, h / 13.5);
    QFont fnt(QStringLiteral("Noto Sans CJK SC"), fs);
    fnt.setLetterSpacing(QFont::PercentageSpacing, 102);
    p.setFont(fnt);
    p.setPen(QColor(240, 244, 252, 240));
    QTextOption opt;
    opt.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    opt.setWrapMode(QTextOption::WordWrap);
    QRectF nameRect(8, h * 0.64, w - 16, h * 0.32);
    p.drawText(nameRect, m_app.name, opt);
}
