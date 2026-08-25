#include "freecell.h"
#include <QDebug>
#include <QDateTime>
#include <QHash>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <random>

namespace FCLogic {

bool isRedSuit(int suit) { return suit == 1 || suit == 2; }
bool isRed(const FCard &c) { return isRedSuit(c.suit); }
QChar suitChar(int suit) {
    // 必须用 QChar（Unicode）：QLatin1Char 是单字节，0x2663 会截断成 'c'（实测牌面渲染成字母）
    static const QChar ch[4] = {QChar(0x2663), QChar(0x2666), QChar(0x2665), QChar(0x2660)};  // ♣♦♥♠
    return ch[suit];
}

bool canStack(const FCard &c, const FCard &onto) {
    return isRed(c) != isRed(onto) && c.rank == onto.rank - 1;
}

bool seqOk(const QVector<FCard> &col, int idx) {
    if (idx < 0 || idx >= col.size()) return false;
    for (int i = idx + 1; i < col.size(); ++i)
        if (!canStack(col[i], col[i - 1])) return false;
    return true;
}

int emptyCells(const FCState &s) {
    int n = 0;
    for (const FCard &c : s.cells) if (c.rank == 0) n++;
    return n;
}

int emptyCols(const FCState &s) {
    int n = 0;
    for (const auto &col : s.cols) if (col.isEmpty()) n++;
    return n;
}

int maxMoveable(const FCState &s) {
    return (1 + emptyCols(s)) * (1 + emptyCells(s));
}

QVector<int> msShuffle(int dealNo) {
    quint32 state = (quint32)dealNo;
    auto rand = [&state]() {
        state = (214013u * state + 2531011u) % 2147483648u;
        return (state >> 16) & 0x7fff;
    };
    QVector<int> deck(52);
    for (int i = 0; i < 52; ++i) deck[i] = i;
    for (int i = 0; i < 51; ++i) {
        const int j = i + rand() % (52 - i);
        std::swap(deck[i], deck[j]);
    }
    return deck;
}

int randomDealNo() {
    static std::mt19937 rng((quint32)QDateTime::currentMSecsSinceEpoch());
    int n;
    do { n = 1 + (int)(rng() % 32000u); } while (n == 11982);   // 微软唯一无解局
    return n;
}

static bool colTop(const FCState &s, int i, FCard *out) {
    if (i < 0 || i >= 8 || s.cols[i].isEmpty()) return false;
    *out = s.cols[i].last();
    return true;
}

bool tryMove(FCState &s, int fromZone, int fromIdx, int fromCardIdx,
             int toZone, int toIdx, QString *err) {
    auto fail = [err](const char *m) { if (err) *err = QString::fromLatin1(m); return false; };

    // ---- 取出源序列 ----
    QVector<FCard> moving;
    if (fromZone == 0) {                       // 列
        if (fromIdx < 0 || fromIdx >= 8) return fail("bad col");
        auto &col = s.cols[fromIdx];
        if (fromCardIdx < 0 || fromCardIdx >= col.size()) return fail("bad card");
        if (fromCardIdx < col.size() - 1 && !seqOk(col, fromCardIdx))
            return fail("not a valid sequence");
        moving = col.mid(fromCardIdx);
        if (moving.size() > maxMoveable(s)) return fail("not enough free slots");
    } else if (fromZone == 1) {                // 自由位
        if (fromIdx < 0 || fromIdx >= 4) return fail("bad cell");
        if (s.cells[fromIdx].rank == 0) return fail("empty cell");
        moving.append(s.cells[fromIdx]);
    } else {
        return fail("bad source");
    }
    const FCard head = moving.first();

    // ---- 落点校验 + 执行 ----
    if (toZone == 0) {                         // 列：空列直接放，否则红黑降序
        if (toIdx < 0 || toIdx >= 8 || (fromZone == 0 && toIdx == fromIdx)) return fail("bad target col");
        auto &dst = s.cols[toIdx];
        if (!dst.isEmpty() && !canStack(head, dst.last())) return fail("cannot stack");
        if (fromZone == 0) {
            for (const FCard &c : moving) s.cols[fromIdx].removeLast();
        } else {
            s.cells[fromIdx] = FCard();
        }
        for (const FCard &c : moving) dst.append(c);
    } else if (toZone == 1) {                  // 自由位：仅单张且目标为空
        if (moving.size() != 1) return fail("only single card to free cell");
        if (toIdx < 0 || toIdx >= 4 || s.cells[toIdx].rank != 0) return fail("free cell occupied");
        if (fromZone == 0) s.cols[fromIdx].removeLast();
        else s.cells[fromIdx] = FCard();
        s.cells[toIdx] = head;
    } else if (toZone == 2) {                  // 回收位：仅单张、同花色、恰好下一张
        if (moving.size() != 1) return fail("only single card to foundation");
        if (toIdx < 0 || toIdx >= 4) return fail("bad foundation");
        if (head.joker) {                      // Joker 按第 4 花色（♠）收
            if (toIdx != 3) return fail("joker goes to spade pile");
        } else if (head.suit != toIdx) {
            return fail("wrong suit pile");
        }
        if (head.rank != s.found[toIdx] + 1) return fail("need next rank");
        if (fromZone == 0) s.cols[fromIdx].removeLast();
        else s.cells[fromIdx] = FCard();
        s.found[toIdx] = head.rank;
    } else {
        return fail("bad target zone");
    }

    s.moves++;
    checkWin(s);
    return true;
}

bool checkWin(FCState &s) {
    if (s.won) return true;
    for (int i = 0; i < 4; ++i)
        if (s.found[i] != 13) return false;
    s.won = true;
    return true;
}

}  // namespace FCLogic

using namespace FCLogic;

// ---------------- FreeCellBoard ----------------

static const int CW = 92, CH = 128, GAP = 10, TOP_Y = 8, COL_Y = 150, STACK = 24;

FreeCellBoard::FreeCellBoard(QWidget *parent) : QWidget(parent) {
    setStyleSheet("background: transparent;");
    setMouseTracking(false);
}

// ---------------- 发牌动画（win 版逐帧参数复刻）----------------
// 洗牌：每张牌独立 左甩→右甩→归中（900ms，逐张 6ms 错峰、随机旋转、0.55 缩放、淡入）
// 发牌：1300ms 后从顶部牌堆逐张飞出（18ms 错峰 + 160ms easeOutCubic + 0.55→1 放大），落地翻面
static const int WASH_DUR = 900, WASH_STAG = 6, DEAL_T0 = 1300, STAGGER_MS = 18, FLIGHT_MS = 160;

void FreeCellBoard::startDealAnimation() {
    m_animating = true;
    m_animT0 = QDateTime::currentMSecsSinceEpoch();
    if (!m_animTimer) {
        m_animTimer = new QTimer(this);
        m_animTimer->setInterval(33);
        connect(m_animTimer, &QTimer::timeout, this, [this] {
            if (!m_animating) { m_animTimer->stop(); return; }
            const qint64 t = QDateTime::currentMSecsSinceEpoch() - m_animT0;
            if (t > DEAL_T0 + 51 * STAGGER_MS + FLIGHT_MS + 80) {
                m_animating = false;
                m_animTimer->stop();
                update();
                return;
            }
            update();
        });
    }
    m_animTimer->start();
    update();
}

void FreeCellBoard::drawBack(QPainter &p, const QRect &r) const {
    QPainterPath rr;
    rr.addRoundedRect(QRectF(r), 8, 8);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0.0, QColor(43, 45, 49));              // #2B2D31
    g.setColorAt(1.0, QColor(30, 31, 34));              // #1E1F22
    p.fillPath(rr, g);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(88, 101, 242, 165), 1.4));     // Blurple 边
    p.drawPath(rr);
    p.setFont(QFont(QStringLiteral("DejaVu Sans"), qMax<qreal>(9, r.height() / 6.0)));
    p.setPen(QColor(254, 231, 92, 200));                // Discord 黄 ✦
    p.drawText(r, Qt::AlignCenter, QString(QChar(0x2726)));
}

QSize FreeCellBoard::boardSize() const {
    return QSize(CW * 8 + GAP * 7 + 8, COL_Y + STACK * 18 + CH + 8);
}

static int colX(int i, int w, int cw) {
    const int span = w - cw;
    return i * span / 7;
}

QRect FreeCellBoard::cardRect(int zone, int i, int idx) const {
    const int w = width();
    if (zone == 1) return QRect(colX(i, w, m_cw), TOP_Y, m_cw, m_ch);
    if (zone == 2) return QRect(colX(i + 4, w, m_cw), TOP_Y, m_cw, m_ch);
    return QRect(colX(i, w, m_cw), COL_Y + idx * STACK, m_cw, m_ch);
}

void FreeCellBoard::newGame(int dealNo) {
    m_s = FCState();
    m_s.dealNo = dealNo;
    m_selZone = -1;
    const QVector<int> order = msShuffle(dealNo);
    for (int i = 0; i < 52; ++i) {
        const int msIdx = order[i];
        FCard c;
        c.suit = msIdx % 4;
        c.rank = msIdx / 4 + 1;
        c.joker = (msIdx >= 48);
        if (c.joker) { c.suit = 3; c.rank = 13; }
        m_s.cols[i % 8].append(c);
    }
    // 洗牌随机旋转按局号做种（重绘不抖动，同局观感一致）
    {
        std::mt19937 rng((quint32)dealNo);
        std::uniform_real_distribution<qreal> rot(6.0, 14.0);
        m_washRotL.resize(52);
        m_washRotR.resize(52);
        for (int k = 0; k < 52; ++k) { m_washRotL[k] = -rot(rng); m_washRotR[k] = rot(rng); }
    }
    startDealAnimation();
    update();
    emit stateChanged();
}

void FreeCellBoard::newGame(int dealNo, const QStringList &appNames) {
    newGame(dealNo);
    // 应用名注入：msIdx<48 的牌按 k/4+1=排名（A=最常用 4 个）、k%4=花色；发牌序 msIdx→牌面
    const QVector<int> order = msShuffle(dealNo);
    QHash<int, QString> byMsIdx;
    for (int k = 0; k < 48 && k < appNames.size(); ++k)
        byMsIdx.insert(k, appNames[k]);
    for (auto &col : m_s.cols)
        for (FCard &c : col) {
            // 反查这张牌的 msIdx：suit/rank 已定，遍历 order 找同面值且 <48 的索引
            if (c.joker || !c.appName.isEmpty()) continue;
            for (int k = 0; k < 52; ++k) {
                const int ms = order[k];
                if (ms < 48 && ms % 4 == c.suit && ms / 4 + 1 == c.rank) {
                    c.appName = byMsIdx.value(ms);
                    break;
                }
            }
        }
    update();
}

void FreeCellBoard::restore(const FCState &s) {
    m_s = s;
    m_selZone = -1;
    update();
    emit stateChanged();
}

FreeCellBoard::Hit FreeCellBoard::hitTest(const QPoint &pos) const {
    Hit h;
    const int w = width();
    for (int i = 0; i < 4; ++i)
        if (cardRect(1, i, 0).contains(pos)) { h.zone = 1; h.i = i; return h; }
    for (int i = 0; i < 4; ++i)
        if (cardRect(2, i, 0).contains(pos)) { h.zone = 2; h.i = i; return h; }
    if (pos.y() >= COL_Y - 20) {
        for (int i = 0; i < 8; ++i) {
            const int x0 = colX(i, w, m_cw), x1 = x0 + m_cw;
            if (pos.x() >= x0 - 6 && pos.x() <= x1 + 6) {
                h.zone = 0; h.i = i;
                const auto &col = m_s.cols[i];
                int idx = col.isEmpty() ? -1
                    : qBound(0, (pos.y() - COL_Y) / STACK, col.size() - 1);
                h.idx = idx;
                return h;
            }
        }
    }
    return h;
}

void FreeCellBoard::mousePressEvent(QMouseEvent *e) {
    if (m_animating || m_s.won || e->button() != Qt::LeftButton) return;
    const Hit h = hitTest(e->pos());
    const quint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool dbl = (now - m_lastClickMs < 400) && h.zone == m_lastClickZone &&
                     h.i == m_lastClickI && h.idx == m_lastClickIdx;
    m_lastClickMs = now; m_lastClickZone = h.zone; m_lastClickI = h.i; m_lastClickIdx = h.idx;

    if (!h.valid()) { m_selZone = -1; update(); return; }

    if (m_selZone >= 0) {   // 已有选中 → 尝试移动
        QString err;
        const int sz = m_selZone == 0 ? m_s.cols[m_selI].size() - m_selIdx : 1;
        Q_UNUSED(sz);
        if (tryMove(m_s, m_selZone, m_selI, m_selIdx, h.zone, h.i, &err)) {
            m_selZone = -1;
            update();
            emit stateChanged();
            if (m_s.won) emit wonSignal(m_s.dealNo, m_s.moves);
            return;
        }
        // 失败 → 若目标可选则换选，否则清选
    }

    // 选中逻辑：列（合法序列头或顶牌）/ 自由位
    if (h.zone == 0) {
        const auto &col = m_s.cols[h.i];
        if (col.isEmpty()) { m_selZone = -1; update(); return; }
        int idx = h.idx;
        if (!seqOk(col, idx)) idx = col.size() - 1;   // 非法序列点中间 → 只选顶牌
        m_selZone = 0; m_selI = h.i; m_selIdx = idx;
    } else if (h.zone == 1) {
        if (m_s.cells[h.i].rank == 0) { m_selZone = -1; update(); return; }
        m_selZone = 1; m_selI = h.i; m_selIdx = 0;
    } else {                // 回收位不可选
        m_selZone = -1;
    }
    update();
}

void FreeCellBoard::mouseDoubleClickEvent(QMouseEvent *e) {
    if (m_animating || m_s.won || e->button() != Qt::LeftButton) return;
    const Hit h = hitTest(e->pos());
    if (h.zone != 0 && h.zone != 1) return;
    FCard top;
    if (h.zone == 0) {
        const auto &col = m_s.cols[h.i];
        if (col.isEmpty() || h.idx != col.size() - 1) return;   // 只认顶牌
        top = col.last();
    } else {
        if (m_s.cells[h.i].rank == 0) return;
        top = m_s.cells[h.i];
    }
    const int fi = top.joker ? 3 : top.suit;
    QString err;
    if (tryMove(m_s, h.zone, h.i, h.zone == 0 ? m_s.cols[h.i].size() - 1 : 0, 2, fi, &err)) {
        m_selZone = -1;
        update();
        emit stateChanged();
        if (m_s.won) emit wonSignal(m_s.dealNo, m_s.moves);
    } else {
        update();   // 可选：失败提示
    }
}

void FreeCellBoard::drawCard(QPainter &p, const QRect &r, const FCard &c, bool selected) const {
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath rr;
    rr.addRoundedRect(r, 8, 8);
    // Joker：白底金边；普通：白底细边
    p.fillPath(rr, QColor(248, 250, 252));
    p.setPen(QPen(selected ? QColor(88, 101, 242) : QColor(148, 163, 184), selected ? 3 : 1));   // 选中=Discord Blurple
    p.drawPath(rr);

    const QColor suitColor = c.joker ? QColor(254, 231, 92)                          // Discord 黄 #FEE75C
                                     : (isRed(c) ? QColor(237, 66, 69) : QColor(219, 222, 225));  // #ED4245 / #DBDEE1
    const QString rankTxt = c.joker ? QStringLiteral("K")
        : (c.rank == 1 ? QStringLiteral("A") : c.rank == 11 ? QStringLiteral("J")
        : c.rank == 12 ? QStringLiteral("Q") : c.rank == 13 ? QStringLiteral("K")
        : QString::number(c.rank));

    // 角标：点数 + 花色（字号随牌高缩放）
    p.setPen(suitColor);
    p.setFont(QFont(QStringLiteral("DejaVu Sans"), qMax<qreal>(8, r.height() / 10.5), QFont::Bold));
    p.drawText(r.adjusted(6, 4, -4, 0), Qt::AlignLeft | Qt::AlignTop, rankTxt);
    p.drawText(r.adjusted(-4, 4, -6, 0), Qt::AlignRight | Qt::AlignTop, QString(suitChar(c.suit)));

    // 中央：大花色 + 应用名（有则两行小字）
    p.setFont(QFont(QStringLiteral("DejaVu Sans"), qMax<qreal>(12, r.height() / 4.9)));
    p.drawText(r, Qt::AlignCenter, QString(suitChar(c.suit)));
    if (!c.appName.isEmpty()) {
        p.setFont(QFont(QStringLiteral("DejaVu Sans"), qMax<qreal>(6, r.height() / 17)));
        p.setPen(QColor(71, 85, 105));
        QRect tr = r.adjusted(6, r.height() / 2, -6, -6);
        p.drawText(tr, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, c.appName);
    }
}

void FreeCellBoard::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 自由位/回收位空槽
    QFont f = QFont(QStringLiteral("DejaVu Sans"), 10);
    for (int i = 0; i < 4; ++i) {
        p.setPen(QPen(QColor(255, 255, 255, 60), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(cardRect(1, i, 0), 8, 8);
        p.setPen(QColor(255, 255, 255, 90));
        p.setFont(f);
        p.drawText(cardRect(1, i, 0), Qt::AlignCenter, QStringLiteral("◇"));
    }
    const QChar foundCh[4] = {QChar(0x2663), QChar(0x2666), QChar(0x2665), QChar(0x2660)};
    for (int i = 0; i < 4; ++i) {
        p.setPen(QPen(QColor(255, 215, 130, 110), 1, Qt::DashLine));
        p.drawRoundedRect(cardRect(2, i, 0), 8, 8);
        p.setPen(QColor(255, 215, 130, 150));
        p.drawText(cardRect(2, i, 0), Qt::AlignCenter,
                   QStringLiteral("%1 A→K").arg(foundCh[i]));
        if (m_s.found[i] > 0) {   // 已收：画堆顶
            FCard top; top.suit = i; top.rank = m_s.found[i];
            drawCard(p, cardRect(2, i, 0), top, false);
        }
    }

    // 列牌（win 版洗牌逐帧复刻：顶部小牌堆 0.55 缩放，每张独立 左甩→右甩→归中
    // 900ms + 6ms 错峰 + 随机旋转 ±6-14° + 淡入；1300ms 后逐张飞出放大落位翻面）
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 t = m_animating ? nowMs - m_animT0 : INT64_MAX;
    const QPointF pileC(width() / 2.0, TOP_Y + m_ch * 0.55 / 2.0);   // 顶部发牌位（缩放牌堆的中心）
    auto washPose = [&](int k, qreal &px, qreal &py, qreal &rot, qreal &alpha) {
        px = pileC.x(); py = pileC.y(); rot = 0.0; alpha = 1.0;
        const qint64 at = t - qint64(k) * WASH_STAG;
        if (at < 0) { alpha = 0.0; return; }
        alpha = qMin<qreal>(1.0, at / 140.0);   // 快速淡入
        const qreal pr = qMin<qreal>(1.0, at / qreal(WASH_DUR));
        auto lerp = [](qreal a, qreal b, qreal u) { return a + (b - a) * u; };
        if (pr < 0.32) {
            const qreal u = pr / 0.32;
            px = lerp(pileC.x(), pileC.x() - 60, u); py = lerp(pileC.y(), pileC.y() + 8, u);
            rot = lerp(0.0, m_washRotL[k], u);
        } else if (pr < 0.68) {
            const qreal u = (pr - 0.32) / 0.36;
            px = lerp(pileC.x() - 60, pileC.x() + 60, u); py = lerp(pileC.y() + 8, pileC.y() + 4, u);
            rot = lerp(m_washRotL[k], m_washRotR[k], u);
        } else {
            const qreal u = (pr - 0.68) / 0.32;
            px = lerp(pileC.x() + 60, pileC.x(), u); py = lerp(pileC.y() + 4, pileC.y(), u);
            rot = lerp(m_washRotR[k], 0.0, u);
        }
    };
    for (int i = 0; i < 8; ++i) {
        const auto &col = m_s.cols[i];
        for (int idx = 0; idx < col.size(); ++idx) {
            const int k = idx * 8 + i;                    // 发牌序（i%8 轮流发）
            const qint64 flyAt = DEAL_T0 + qint64(k) * STAGGER_MS;
            const bool sel = (m_selZone == 0 && m_selI == i && idx >= m_selIdx);
            if (t < flyAt + FLIGHT_MS) {
                if (t < flyAt) {                          // 洗牌期：顶部小牌堆甩动
                    qreal px, py, rot, alpha;
                    washPose(k, px, py, rot, alpha);
                    if (alpha > 0.01) {
                        p.save();
                        p.setOpacity(alpha);
                        p.translate(px, py);
                        p.rotate(rot);
                        p.scale(0.55, 0.55);
                        drawBack(p, QRect(-m_cw / 2, -m_ch / 2, m_cw, m_ch));
                        p.restore();
                    }
                } else {                                  // 飞行：堆位→落点，0.55→1 放大
                    const qreal pr = (t - flyAt) / qreal(FLIGHT_MS);
                    const qreal e = 1.0 - std::pow(1.0 - pr, 3.0);   // easeOutCubic
                    const QRect fin = cardRect(0, i, idx);
                    const qreal fx = pileC.x() + (fin.center().x() - pileC.x()) * e;
                    const qreal fy = pileC.y() + (fin.center().y() - pileC.y()) * e;
                    const qreal sc = 0.55 + 0.45 * e;
                    p.save();
                    p.translate(fx, fy);
                    p.scale(sc, sc);
                    drawBack(p, QRect(-m_cw / 2, -m_ch / 2, m_cw, m_ch));
                    p.restore();
                }
                continue;
            }
            drawCard(p, cardRect(0, i, idx), col[idx], sel);
        }
        if (col.isEmpty()) {
            p.setPen(QPen(QColor(255, 255, 255, 35), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(cardRect(0, i, 0), 8, 8);
        }
    }

    // 自由位上的牌
    for (int i = 0; i < 4; ++i)
        if (m_s.cells[i].rank != 0)
            drawCard(p, cardRect(1, i, 0), m_s.cells[i], m_selZone == 1 && m_selI == i);

    // 胜利横幅
    if (m_s.won) {
        p.fillRect(rect(), QColor(10, 14, 24, 190));
        p.setPen(QColor(255, 215, 130));
        p.setFont(QFont(QStringLiteral("DejaVu Sans"), 30, QFont::Bold));
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("🎉 通关！第 %1 局 · %2 步\n点「新局」再战").arg(m_s.dealNo).arg(m_s.moves));
    }
}

void FreeCellBoard::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    // 牌面随板宽缩放（8 列 + 7 间距），字号随牌高——窄面板不再挤压重叠
    m_cw = qBound(56, (width() - GAP * 7 - 8) / 8, 110);
    m_ch = qRound(m_cw * 128.0 / 92.0);
    update();
}

// ---------------- 逻辑自测（DECK_SELFTEST=1 时 main 调用，远程联测可验） ----------------
namespace FCLogic {

int runSelfTest() {
    int pass = 0, fail = 0;
    auto check = [&](bool ok, const char *name) {
        if (ok) { ++pass; qInfo() << "[selftest] PASS" << name; }
        else    { ++fail; qInfo() << "[selftest] FAIL" << name; }
    };

    // 发牌：确定性 / 完整性 / 排列
    const auto d1 = msShuffle(1);
    check(d1 == msShuffle(1), "deal deterministic");
    check(d1.size() == 52, "deal size 52");
    {
        QVector<int> sorted = d1;
        std::sort(sorted.begin(), sorted.end());
        bool perm = true;
        for (int i = 0; i < 52; ++i) if (sorted[i] != i) perm = false;
        check(perm, "deal is permutation of 0..51");
    }
    // 花色颜色
    check(isRedSuit(1) && isRedSuit(2) && !isRedSuit(0) && !isRedSuit(3), "suit colors");
    const FCard d6{1, 6}, s7{3, 7}, h7{2, 7}, d8{1, 8};
    check(canStack(d6, s7), "red on black ok");
    check(!canStack(d6, h7), "red on red rejected");
    check(!canStack(d8, d6), "same rank rejected");
    // 序列
    const QVector<FCard> col = {{FCard{3, 9}, FCard{2, 9}, FCard{3, 7}, FCard{1, 6}}};
    check(!seqOk(col, 0) && !seqOk(col, 1), "broken sequence detected");
    check(seqOk(col, 2), "valid tail sequence");
    // 可移张数（注意 FCState 构造时 8 列全空——先填 7 列再断言）
    FCState st;
    for (int i = 0; i < 8; ++i) st.cols[i].append(FCard{3, 10});
    st.cells = {FCard(), FCard(), FCard{3, 5}, FCard{1, 2}};
    check(maxMoveable(st) == 3, "maxMoveable (0 empty col, 2 free) = 3");
    st.cols[3].clear();
    check(maxMoveable(st) == 6, "maxMoveable (1 empty col, 2 free) = 6");
    // 走子：合法 / 非法 / 自由位 / 回收位 / 胜利
    FCState t;
    t.cols[0] = {FCard{3, 10}, FCard{2, 9}};   // ♠10 ♥9（合法序列）
    t.cols[1] = {FCard{0, 10}};                 // ♣10
    QString err;
    check(tryMove(t, 0, 0, 1, 0, 1, &err), "move h9 onto c10");
    check(t.cols[1].size() == 2 && t.cols[0].size() == 1, "move executed");
    check(!tryMove(t, 0, 0, 0, 0, 1, &err), "s10 onto h9 rejected (wrong order)");
    t.cells.resize(4);
    check(tryMove(t, 0, 0, 0, 1, 0, &err), "s10 to free cell");
    check(t.cells[0].rank == 10 && t.cols[0].isEmpty(), "free cell holds card");
    t.cells[1] = FCard{0, 5};
    check(!tryMove(t, 1, 0, 0, 1, 1, &err), "occupied free cell rejected");
    check(tryMove(t, 1, 0, 0, 0, 0, &err), "free cell back to empty col");
    const FCard dA{1, 1}, d2{1, 2}, d3{1, 3};
    t.cols[2] = {dA};
    check(tryMove(t, 0, 2, 0, 2, 1, &err), "diamond A to foundation");
    check(t.found[1] == 1, "foundation advanced");
    t.cols[2] = {d3};
    check(!tryMove(t, 0, 2, 0, 2, 1, &err), "d3 before d2 rejected");
    t.cols[2] = {d2};
    check(tryMove(t, 0, 2, 0, 2, 1, &err), "d2 after dA accepted");
    // 胜利判定
    FCState w;
    for (int i = 0; i < 4; ++i) w.found[i] = 13;
    check(checkWin(w) && w.won, "win detected");
    // 随机局号避开 11982
    bool hit11982 = false;
    for (int i = 0; i < 500; ++i) if (randomDealNo() == 11982) hit11982 = true;
    check(!hit11982, "randomDealNo avoids 11982");

    qInfo() << "[selftest] SUMMARY" << pass << "pass" << fail << "fail";
    return fail == 0 ? 0 : 1;
}

}  // namespace FCLogic
