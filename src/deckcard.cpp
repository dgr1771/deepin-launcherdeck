#include "deckcard.h"
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QTextOption>

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
    const qreal dy = -7.0 * lift;                    // 浮起位移
    p.translate(0.0, dy);

    // 悬停投影（越悬越明显）
    if (lift > 0.01) {
        p.setOpacity(0.45 * lift);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 140));
        QPainterPath sh;
        sh.addRoundedRect(QRectF(3, 5, w - 6, h - 8), 11, 11);
        p.drawPath(sh);
        p.setOpacity(1.0);
    }

    // 牌体：星夜渐变（悬停微亮）
    QPainterPath body;
    body.addRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 10, 10);
    QLinearGradient g(0, 0, w, h);
    const qreal hl = 0.5 + 0.5 * lift;
    g.setColorAt(0.0, QColor::fromRgbF(0.145 * (1 + 0.35 * hl), 0.17 * (1 + 0.35 * hl), 0.28 * (1 + 0.35 * hl)));
    g.setColorAt(1.0, QColor::fromRgbF(0.078, 0.094, 0.18));
    p.fillPath(body, g);
    p.setPen(QPen(QColor(255, 255, 255, 40 + 60 * lift), 1));
    p.drawPath(body);

    // 金线内框 + 四角菱形饰
    const qreal inset = 5;
    QPainterPath inner;
    inner.addRoundedRect(QRectF(inset, inset, w - 2 * inset, h - 2 * inset), 7, 7);
    p.setPen(QPen(QColor(255, 215, 130, 55 + 130 * lift), 1));
    p.drawPath(inner);
    p.setPen(QPen(QColor(255, 215, 130, 90 + 120 * lift), 1));
    const qreal d = 3.2;
    const QPointF corners[4] = {{inset + 6, inset + 6}, {w - inset - 6, inset + 6},
                                {inset + 6, h - inset - 6}, {w - inset - 6, h - inset - 6}};
    for (const QPointF &c : corners) {
        QPainterPath dia;
        dia.moveTo(c.x(), c.y() - d);
        dia.lineTo(c.x() + d, c.y());
        dia.lineTo(c.x(), c.y() + d);
        dia.lineTo(c.x() - d, c.y());
        p.drawPath(dia);
    }

    // 图标：柔和光晕底 + 图标本体（上 2/3 区域中央）
    if (!m_icon.isNull()) {
        const int isz = qMin<int>(w * 0.42, h * 0.36);
        const QPointF center(w / 2, h * 0.40);
        QRadialGradient halo(center, isz * 0.9);
        halo.setColorAt(0.0, QColor(255, 255, 255, 26));
        halo.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(center, isz * 0.9, isz * 0.9);
        const QRect iconRect(center.x() - isz / 2, center.y() - isz / 2, isz, isz);
        p.drawPixmap(iconRect, m_icon);
    }

    // 名称：底部两行居中（字号随牌高缩放）
    const int fs = qMax<qreal>(8.5, h / 13.5);
    QFont fnt(QStringLiteral("Noto Sans CJK SC"), fs);
    fnt.setLetterSpacing(QFont::PercentageSpacing, 102);
    p.setFont(fnt);
    p.setPen(QColor(235, 240, 250, 235));
    QTextOption opt;
    opt.setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    opt.setWrapMode(QTextOption::WordWrap);
    QRectF nameRect(8, h * 0.62, w - 16, h * 0.34);
    p.drawText(nameRect, m_app.name, opt);
}
