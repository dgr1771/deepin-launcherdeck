#include "tarotpanel.h"
#include "deckcard.h"
#include "flowlayout.h"
#include <algorithm>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QProcess>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

static QSettings *usageStore() {
    static QSettings *s = new QSettings(QStringLiteral("dgr"), QStringLiteral("launcherdeck"));
    return s;
}

TarotPanel::TarotPanel(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("panel"));
    buildUi();
    refresh();
}

void TarotPanel::buildUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 14, 18, 12);
    root->setSpacing(8);

    // 标题区（deepin 原生排版：主标题 + 状态行）
    auto *title = new QLabel(QStringLiteral("✦ 应用牌堆"), this);
    title->setStyleSheet("color: #ffffff; font-size: 19px; font-weight: 700; letter-spacing: 5px; background: transparent;");
    m_subLbl = new QLabel(QStringLiteral("正在召集本机程序…"), this);
    m_subLbl->setStyleSheet("color: rgba(255,255,255,0.5); font-size: 11px; letter-spacing: 1px; background: transparent;");
    root->addWidget(title);
    root->addWidget(m_subLbl);

    // 顶栏：模式切换 + 游戏控件 + 搜索
    auto *top = new QHBoxLayout();
    top->setSpacing(8);
    m_modeBtn = new QPushButton(QStringLiteral("🎮 空当接龙"), this);
    m_modeBtn->setCursor(Qt::PointingHandCursor);
    m_newBtn = new QPushButton(QStringLiteral("🆕 新局"), this);
    m_newBtn->setCursor(Qt::PointingHandCursor);
    m_newBtn->hide();
    m_moveLbl = new QLabel(this);
    m_moveLbl->setStyleSheet("color: rgba(255,255,255,0.65); font-size: 12px;");
    m_moveLbl->hide();
    const QString btnQss = QStringLiteral(
        "QPushButton { background: rgba(255,255,255,0.08); border: 1px solid rgba(255,255,255,0.16);"
        "  border-radius: 14px; padding: 5px 14px; color: #fff; font-size: 12px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.16); }");
    m_modeBtn->setStyleSheet(btnQss);
    m_newBtn->setStyleSheet(btnQss);
    top->addWidget(m_modeBtn);
    top->addWidget(m_newBtn);
    top->addWidget(m_moveLbl);
    top->addStretch();
    connect(m_modeBtn, &QPushButton::clicked, this, [this] { toggleMode(); });
    connect(m_newBtn, &QPushButton::clicked, this, [this] {
        m_board->newGame(FCLogic::randomDealNo(), appNamesByUsage());
        saveGame();
        updateGameChrome();
    });
    root->addLayout(top);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("兜底搜索（支持应用名）…"));
    m_search->setFixedHeight(32);
    m_search->setStyleSheet(
        "QLineEdit { background: rgba(255,255,255,0.07); border: 1px solid rgba(255,255,255,0.14);"
        "  border-radius: 16px; padding: 0 14px; color: #fff; font-size: 13px; }"
        "QLineEdit:focus { border-color: rgba(255,215,130,0.7); }");
    top->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &t) { rebuildGrid(t); });

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // 宽度即换行依据——不许横向滚
    m_scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 8px; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.16); border-radius: 4px; min-height: 30px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }");
    m_gridBox = new QWidget(m_scroll);
    m_gridBox->setStyleSheet("background: transparent;");
    m_grid = new FlowLayout(m_gridBox);
    m_scroll->setWidget(m_gridBox);
    root->addWidget(m_scroll);

    // 空当接龙板（初始隐藏）
    m_board = new FreeCellBoard(this);
    m_board->hide();
    root->addWidget(m_board, 1);
    connect(m_board, &FreeCellBoard::stateChanged, this, [this] {
        saveGame();
        updateGameChrome();
    });
    connect(m_board, &FreeCellBoard::wonSignal, this, [this](int, int) {
        clearSavedGame();   // 通关清档，下次进游戏模式开新局
    });

    // 小屏/高缩放夹紧（Windows 版同款教训：写死 820 在 150% 缩放会裁底）
    QRect av = QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(1280, av.width() - 16), qMin(820, av.height() - 12));
    move(av.center() - rect().center());

    // 尺寸变化 → 防抖重排（自适应牌面尺寸的核心触发器）
    m_refitTimer = new QTimer(this);
    m_refitTimer->setSingleShot(true);
    m_refitTimer->setInterval(120);
    connect(m_refitTimer, &QTimer::timeout, this, [this] {
        if (!gameMode) rebuildGrid(m_search->text());
    });
}

void TarotPanel::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    if (m_refitTimer) m_refitTimer->start();
}

void TarotPanel::refresh() {
    AppScanner scanner;
    m_apps = scanner.scan();
    QSettings *s = usageStore();
    for (AppEntry &a : m_apps)
        a.count = s->value(QStringLiteral("usage/") + a.name).toUInt();
    // 游戏模式不重建牌阵（FCard 存名字符串，重扫不破坏牌局；省一份隐藏网格的 CPU）
    if (!gameMode) rebuildGrid(m_search->text());
    if (m_subLbl)
        m_subLbl->setText(QStringLiteral("本机 %1 款程序入阵 · 常用自动浮前 · 零输入 — 点牌即达")
                              .arg(m_apps.size()));
}

// ---------------- 模式切换 / 进度存取 ----------------

QStringList TarotPanel::appNamesByUsage() const {
    QVector<AppEntry> list = m_apps;
    std::sort(list.begin(), list.end(), [](const AppEntry &x, const AppEntry &y) {
        if (x.count != y.count) return x.count > y.count;
        return x.name.localeAwareCompare(y.name) < 0;
    });
    QStringList names;
    for (const AppEntry &a : list) names.append(a.name);
    return names;
}

void TarotPanel::toggleMode() {
    gameMode = !gameMode;
    if (gameMode) {
        ensureGame();
        enterGameUi();
    } else {
        m_board->hide();
        m_scroll->show();
        m_search->show();
        rebuildGrid(m_search->text());
    }
    updateGameChrome();
}

void TarotPanel::enterGameUi() {
    m_scroll->hide();
    m_search->hide();
    m_board->show();
    m_board->setFocus();
}

void TarotPanel::ensureGame() {
    loadGame();
    if (m_board->state().dealNo == 0) {
        m_board->newGame(FCLogic::randomDealNo(), appNamesByUsage());
        saveGame();
    }
    updateGameChrome();
}

void TarotPanel::updateGameChrome() {
    m_modeBtn->setText(gameMode ? QStringLiteral("🃏 塔罗牌阵") : QStringLiteral("🎮 空当接龙"));
    m_newBtn->setVisible(gameMode);
    m_moveLbl->setVisible(gameMode);
    if (gameMode) {
        const FCState &s = m_board->state();
        m_moveLbl->setText(QStringLiteral("第 %1 局 · %2 步").arg(s.dealNo).arg(s.moves));
    }
}

void TarotPanel::saveGame() {
    const FCState &s = m_board->state();
    if (s.dealNo == 0) return;
    QJsonObject o;
    o[QStringLiteral("deal")] = s.dealNo;
    o[QStringLiteral("moves")] = s.moves;
    o[QStringLiteral("won")] = s.won;
    QJsonArray found;
    for (int i = 0; i < 4; ++i) found.append(s.found[i]);
    o[QStringLiteral("found")] = found;
    QJsonArray cols;
    for (const auto &col : s.cols) {
        QJsonArray jc;
        for (const FCard &c : col)
            jc.append(QJsonObject{{"s", c.suit}, {"r", c.rank}, {"n", c.appName}, {"j", c.joker}});
        cols.append(jc);
    }
    o[QStringLiteral("cols")] = cols;
    QJsonArray cells;
    for (const FCard &c : s.cells)
        cells.append(c.rank == 0 ? QJsonObject() : QJsonObject{{"s", c.suit}, {"r", c.rank}, {"n", c.appName}, {"j", c.joker}});
    o[QStringLiteral("cells")] = cells;
    QSettings *st = usageStore();
    st->setValue(QStringLiteral("game"), QJsonDocument(o).toJson(QJsonDocument::Compact));
    st->sync();   // 强制落盘：kill/崩溃时进度不丢（QSettings 平时异步写，实测 kill -9 丢档）
}

void TarotPanel::loadGame() {
    QSettings *st = usageStore();
    const QByteArray raw = st->value(QStringLiteral("game")).toByteArray();
    if (raw.isEmpty()) return;
    QJsonObject o = QJsonDocument::fromJson(raw).object();
    if (o.value(QStringLiteral("deal")).toInt() <= 0) return;
    FCState s;
    s.dealNo = o.value(QStringLiteral("deal")).toInt();
    s.moves = o.value(QStringLiteral("moves")).toInt();
    s.won = o.value(QStringLiteral("won")).toBool();
    const QJsonArray found = o.value(QStringLiteral("found")).toArray();
    for (int i = 0; i < 4 && i < found.size(); ++i) s.found[i] = found[i].toInt();
    const QJsonArray cols = o.value(QStringLiteral("cols")).toArray();
    for (int i = 0; i < 8 && i < cols.size(); ++i) {
        s.cols[i].clear();
        for (const auto &v : cols[i].toArray()) {
            const QJsonObject c = v.toObject();
            s.cols[i].append(FCard{c.value("s").toInt(), c.value("r").toInt(),
                                   c.value("n").toString(), c.value("j").toBool()});
        }
    }
    const QJsonArray cells = o.value(QStringLiteral("cells")).toArray();
    for (int i = 0; i < 4 && i < cells.size(); ++i) {
        const QJsonObject c = cells[i].toObject();
        s.cells[i] = c.isEmpty() ? FCard()
                                 : FCard{c.value("s").toInt(), c.value("r").toInt(),
                                         c.value("n").toString(), c.value("j").toBool()};
    }
    if (s.won) return;   // 通关档不恢复
    m_board->restore(s);
}

void TarotPanel::clearSavedGame() {
    usageStore()->remove(QStringLiteral("game"));
}

void TarotPanel::rebuildGrid(const QString &filter) {
    // 清空旧牌
    while (QLayoutItem *it = m_grid->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }

    QVector<AppEntry> list = m_apps;
    if (!filter.isEmpty()) {
        const QString q = filter.toLower();
        list.erase(std::remove_if(list.begin(), list.end(), [&](const AppEntry &a) {
                       return !a.name.toLower().contains(q);
                   }), list.end());
    }
    std::sort(list.begin(), list.end(), [](const AppEntry &x, const AppEntry &y) {
        if (x.count != y.count) return x.count > y.count;      // 常用浮前
        return x.name.localeAwareCompare(y.name) < 0;
    });

    // 自适应牌面尺寸：在可用区找能整排放下全部牌的最大尺寸（全牌可见优先，不靠滚动）
    const int availW = m_scroll->viewport()->width() - 12;
    const int availH = m_scroll->viewport()->height() - 22;
    QSize cs(102, 148);
    {
        const int GX = 13, GY = 14;
        const double R = 102.0 / 148.0;
        if (list.size() > 0 && availW > 200 && availH > 160) {
            for (int hgt = 148; hgt >= 60; hgt -= 2) {
                const int wdt = qRound(hgt * R);
                const int perRow = (availW + GX) / (wdt + GX);
                if (perRow <= 0) continue;
                const int rows = (list.size() + perRow - 1) / perRow;
                if (rows * hgt + (rows - 1) * GY + 18 <= availH) { cs = QSize(wdt, hgt); break; }
            }
        }
    }

    for (const AppEntry &a : list) {
        auto *card = new DeckCard(a, m_gridBox);
        card->setFixedSize(cs);
        QIcon ic = a.icon.startsWith(QLatin1Char('/'))
                       ? QIcon(a.icon)
                       : QIcon::fromTheme(a.icon, QIcon::fromTheme(QStringLiteral("application-x-executable")));
        if (ic.isNull()) ic = QIcon::fromTheme(QStringLiteral("application-x-executable"));
        card->setIconPixmap(ic.pixmap(qMin<int>(cs.width() * 0.5, 72), qMin<int>(cs.width() * 0.5, 72)));
        card->installEventFilter(this);   // 点击 → 启动（eventFilter 统一接管）
        m_grid->addWidget(card);
    }
}

bool TarotPanel::event(QEvent *e) {
    if (e->type() == QEvent::WindowDeactivate) {
        // 失焦收牌（延迟防瞬时焦点抖动），与 Windows 版同节奏
        QTimer::singleShot(220, this, [this] { if (!isActiveWindow()) hide(); });
    }
    return QWidget::event(e);
}

bool TarotPanel::eventFilter(QObject *obj, QEvent *e) {
    // 牌的点击启动：MouseButtonRelease 且落在牌内
    if (e->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::LeftButton) {
            auto *card = qobject_cast<QWidget *>(obj);
            if (card && card->rect().contains(me->pos())) {
                const QString path = card->property("deckPath").toString();
                for (const AppEntry &a : std::as_const(m_apps)) {
                    if (a.desktopPath == path) { launchApp(a); break; }
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, e);
}

void TarotPanel::launchApp(const AppEntry &a) {
    // 实测 deepin 上 QDesktopServices::openUrl(.desktop) 会被当文本用 vim 打开——
    // 标准启动方式是 gio launch（GLib 解析 Exec 并以正确环境拉起），dex 兜底
    if (!QProcess::startDetached(QStringLiteral("gio"), {QStringLiteral("launch"), a.desktopPath})) {
        if (!QProcess::startDetached(QStringLiteral("dex"), {a.desktopPath})) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(a.desktopPath));   // 末级兜底
        }
    }
    QSettings *s = usageStore();
    const QString key = QStringLiteral("usage/") + a.name;
    s->setValue(key, s->value(key).toUInt() + 1);
    for (AppEntry &x : m_apps)
        if (x.desktopPath == a.desktopPath) x.count++;
    hide();   // 点按即收牌（Windows 版 v0.2.1 的感知优化同款）
}

void TarotPanel::toggle() {
    if (isVisible()) { hide(); return; }
    QRect av = QGuiApplication::primaryScreen()->availableGeometry();
    move(av.center() - rect().center());
    show();
    raise();
    activateWindow();
    setFocus();
}

void TarotPanel::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect().adjusted(0, 0, -1, -1), 14, 14);
    // 深色亚克力底（浅色壁纸可读性——Windows 版 v0.1.1 的教训直接带过来）
    p.fillPath(path, QColor(10, 14, 24, 235));
    p.setPen(QPen(QColor(255, 255, 255, 90), 1));
    p.drawPath(path);
}

void TarotPanel::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::RightButton) close();   // 右键收起（占位：后续挂设置菜单）
    QWidget::mousePressEvent(e);
}
