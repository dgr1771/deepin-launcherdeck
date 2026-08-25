// 空当接龙：纯逻辑（FCState/FCLogic）+ 单控件绘制板（FreeCellBoard）
// 设计要点：FCard 存花色/点数/应用名字符串而非对象引用——重扫应用不破坏牌局（Windows 版教训）
#pragma once
#include <QWidget>
#include <QVector>

struct FCard {
    int suit = 0;        // 0♣ 1♦ 2♥ 3♠（红=1,2）
    int rank = 0;        // 1..13，0=空位
    QString appName;     // 空 = Joker
    bool joker = false;
};

struct FCState {
    QVector<QVector<FCard>> cols;   // 8 列
    QVector<FCard> cells;           // 4 自由位（rank==0 为空）
    int found[4] = {0, 0, 0, 0};    // 每花色已收张数（A=1..K=13）
    int dealNo = 0;
    int moves = 0;
    bool won = false;
    FCState() : cols(8), cells(4) {}
};

namespace FCLogic {
    bool isRed(const FCard &c);
    QChar suitChar(int suit);
    bool isRedSuit(int suit);
    bool canStack(const FCard &c, const FCard &onto);   // 红黑交替降序
    bool seqOk(const QVector<FCard> &col, int idx);     // idx..末尾是否合法序列
    int emptyCells(const FCState &s);
    int emptyCols(const FCState &s);
    int maxMoveable(const FCState &s);                  // (1+空列)*(1+空自由位)
    QVector<int> msShuffle(int dealNo);                 // 微软发牌序（0..51，i%4=花色 i/4+1=点数）
    int randomDealNo();                                 // 1..32000，避开 11982 无解局
    // from/to: zone 0=列 1=自由位 2=回收位；fromCardIdx 仅列有效（序列头）
    bool tryMove(FCState &s, int fromZone, int fromIdx, int fromCardIdx, int toZone, int toIdx, QString *err);
    bool checkWin(FCState &s);
    int runSelfTest();                                  // DECK_SELFTEST=1 时跑逻辑断言，0/1 退出码
}

class QTimer;

// 单控件自绘板：所有牌画在一个 widget 里（无子控件，命中测试按几何算）
class FreeCellBoard : public QWidget {
    Q_OBJECT
public:
    explicit FreeCellBoard(QWidget *parent = nullptr);
    QSize boardSize() const;                 // 建议尺寸（随牌面常量）
    void newGame(int dealNo);                // 纯牌局（自测用，无应用名）
    void newGame(int dealNo, const QStringList &appNames);  // 前 48 张按使用排名注入应用名（A=最常用）
    void restore(const FCState &s);          // 恢复存档（不带发牌动画）
    const FCState &state() const { return m_s; }

signals:
    void stateChanged();                     // 每次成功走子后（存进度+步数标签）
    void wonSignal(int dealNo, int moves);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private:
    struct Hit { int zone = -1, i = -1, idx = -1; bool valid() const { return zone >= 0; } };
    Hit hitTest(const QPoint &pos) const;
    QRect cardRect(int zone, int i, int idx) const;
    void drawCard(QPainter &p, const QRect &r, const FCard &c, bool selected) const;
    void drawBack(QPainter &p, const QRect &r) const;   // 牌背（洗牌/发牌飞行期）
    void startDealAnimation();                          // 中央堆三段抖洗 → 逐张发牌

    FCState m_s;
    int m_selZone = -1, m_selI = -1, m_selIdx = -1;   // 当前选中（-1 无）
    quint64 m_lastClickMs = 0;                         // 双击判定
    int m_lastClickZone = -1, m_lastClickI = -1, m_lastClickIdx = -1;
    int m_cw = 92, m_ch = 128;                         // 牌面尺寸（随板宽缩放）
    QTimer *m_animTimer = nullptr;
    qint64 m_animT0 = 0;                               // 动画起始时刻（ms）
    bool m_animating = false;
};
