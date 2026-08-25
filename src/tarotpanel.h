// 塔罗牌阵面板：无边框半透明圆角窗 + 搜索 + 牌阵网格 + 失焦自动收起
#pragma once
#include <QWidget>
#include "appscanner.h"

class QLineEdit;
class QScrollArea;
class QVBoxLayout;
class FlowLayout;

class TarotPanel : public QWidget {
    Q_OBJECT
public:
    explicit TarotPanel(QWidget *parent = nullptr);
    void toggle();     // 热键/托盘：显示↔收起
    void refresh();    // 重扫应用 + 重建牌阵

protected:
    bool event(QEvent *e) override;          // 失焦 → 延迟收起
    bool eventFilter(QObject *obj, QEvent *e) override; // 牌的点击 → 启动
    void paintEvent(QPaintEvent *e) override; // 圆角毛玻璃底
    void mousePressEvent(QMouseEvent *e) override;

private:
    void buildUi();
    void rebuildGrid(const QString &filter);
    void launchApp(const AppEntry &a);
    QVector<AppEntry> m_apps;
    QLineEdit *m_search = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_gridBox = nullptr;
    FlowLayout *m_grid = nullptr;
};
