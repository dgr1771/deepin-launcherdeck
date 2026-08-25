// 塔罗牌阵面板：无边框半透明圆角窗 + 搜索 + 牌阵网格 + 失焦自动收起
#pragma once
#include <QWidget>
#include "appscanner.h"
#include "freecell.h"

class QLineEdit;
class QScrollArea;
class QVBoxLayout;
class FlowLayout;
class QPushButton;
class QLabel;

class TarotPanel : public QWidget {
    Q_OBJECT
public:
    explicit TarotPanel(QWidget *parent = nullptr);
    void toggle();     // 热键/托盘：显示↔收起
    void toggleMode(); // 塔罗牌阵 ↔ 空当接龙（公开：DECK_DEBUG_GAME 联测用）
    void refresh();    // 重扫应用 + 重建牌阵

protected:
    bool event(QEvent *e) override;          // 失焦 → 延迟收起
    bool eventFilter(QObject *obj, QEvent *e) override; // 牌的点击 → 启动
    void paintEvent(QPaintEvent *e) override; // 圆角毛玻璃底
    void mousePressEvent(QMouseEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;   // 尺寸变化 → 防抖重排牌阵

private:
    void buildUi();
    void rebuildGrid(const QString &filter);
    void launchApp(const AppEntry &a);
    void enterGameUi();
    void ensureGame();       // 有存档恢复，否则新局
    void saveGame();
    void loadGame();
    void clearSavedGame();
    void updateGameChrome();
    QStringList appNamesByUsage() const;   // 按使用排名的应用名（前 48 注入牌面）

    QVector<AppEntry> m_apps;
    QLineEdit *m_search = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_gridBox = nullptr;
    FlowLayout *m_grid = nullptr;
    FreeCellBoard *m_board = nullptr;
    QPushButton *m_modeBtn = nullptr;
    QPushButton *m_newBtn = nullptr;
    QLabel *m_moveLbl = nullptr;
    QLabel *m_subLbl = nullptr;
    QTimer *m_refitTimer = nullptr;
    bool gameMode = false;
};
