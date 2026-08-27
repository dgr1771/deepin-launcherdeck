// 塔罗牌阵面板：无边框半透明圆角窗 + 搜索 + 牌阵网格 + 失焦自动收起
#pragma once
#include <QWidget>
#include "appscanner.h"
#include "freecell.h"
#include "slotmachine.h"

class QLineEdit;
class QScrollArea;
class QVBoxLayout;
class QHBoxLayout;
class FlowLayout;
class QPushButton;
class QLabel;
class QButtonGroup;
class QComboBox;

class TarotPanel : public QWidget {
    Q_OBJECT
public:
    explicit TarotPanel(QWidget *parent = nullptr);
    void toggle();     // 热键/托盘：显示↔收起
    void toggleMode(); // 塔罗牌阵 ↔ 空当接龙（公开：DECK_DEBUG_GAME 联测用）
    void showFortune();  // 今日一抽（公开：DECK_DEBUG_FORTUNE 联测用）
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
    void rebuildChips();     // 花色过滤行（按各花色实际数量生成，空花色隐藏）
    QStringList appNamesByUsage() const;   // 按使用排名的应用名（前 48 注入牌面）
    QString suitOf(const AppEntry &a) const;          // 归属：自定义优先，否则 Categories 自动
    QStringList customCats() const;                    // 自建类别 "id|名称|徽章"
    void addCustomCat(const QString &name, const QChar &badge);
    void removeCustomCat(const QString &id);
    void showReading(const AppEntry &a, const QRect &cardRect);   // 牌意解读浮层
    void hideReading();
    void showCatMenu(const AppEntry &a, const QPoint &globalPos); // 右键归类菜单
    void toggleSlotMode();
    void enterSlotUi();
    void loadAppearance();
    void showAppearanceDialog();
    void applyAppearance();

    QVector<AppEntry> m_apps;
    QLineEdit *m_search = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_gridBox = nullptr;
    FlowLayout *m_grid = nullptr;
    FreeCellBoard *m_board = nullptr;
    SlotMachineBoard *m_slot = nullptr;
    QPushButton *m_modeBtn = nullptr;
    QPushButton *m_newBtn = nullptr;
    QPushButton *m_slotBtn = nullptr;
    QComboBox *m_difficultyBox = nullptr;
    QLabel *m_moveLbl = nullptr;
    QLabel *m_subLbl = nullptr;
    QTimer *m_refitTimer = nullptr;
    QHBoxLayout *m_chipsLayout = nullptr;
    QButtonGroup *m_chipGroup = nullptr;
    QString m_filterSuit = QStringLiteral("all");
    QLabel *m_reading = nullptr;            // 牌意解读浮层
    QTimer *m_readingTimer = nullptr;       // 悬停 450ms 延时
    QString m_readingPath;                  // 待解读的应用 desktopPath
    QRect m_readingCardRect;                // 触发悬停的牌矩形（浮层定位用）
    QPushButton *m_fortuneBtn = nullptr;    // 今日一抽
    QPushButton *m_appearanceBtn = nullptr;  // 外观设置
    int m_themeId = 0;
    int m_panelOpacity = 88;
    bool gameMode = false;
    bool slotMode = false;
    int m_difficulty = 1;
};
