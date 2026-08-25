#include "tarotpanel.h"
#include "deckcard.h"
#include "flowlayout.h"
#include <algorithm>
#include <QButtonGroup>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
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
#include <QMenu>
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

// ---------------- 内置花色定义（chips/右键菜单/牌意共用） ----------------
struct SuitDef2 { const char *id; const char *name; const char *badge; const char *divination; };
static const SuitDef2 BUILTIN_SUITS[] = {
    {"all",         "全部",     "\xF0\x9F\x83\x8F", ""},
    {"network",     "社交通讯", "♦", "联结之牌——牌面汇聚人脉与讯息，今日宜沟通。一次翻开，四方来仪。"},
    {"audiovideo",  "影音游戏", "♣", "欢愉之牌——此牌主松弛与沉浸，宜以一段光影犒赏自己，劳逸自此调匀。"},
    {"browser",     "浏览器",   "♥", "启程之牌——牌意指向辽阔之境，宜巡游四方、广纳新知，见天地而后见自己。"},
    {"development", "开发工具", "♠", "匠作之牌——此牌主创造与构筑，心流涌动之日，落子成局，代码生辉。"},
    {"utility",     "效率工具", "★", "利器之牌——工欲善其事，必先利其器。此牌一出，事半功倍。"},
    {"system",      "系统组件", "⚙", "基石之牌——无形而承万物。维护得当，根基永固，上层数十应用皆得其庇。"},
    {"other",       "未名之牌", "✦", "未名之牌——不在五行中的存在，静待命名。缘分到时，自见真章。"},
};
static QString suitDivination(const QString &suit) {
    for (const SuitDef2 &d : BUILTIN_SUITS)
        if (QLatin1String(d.id) == suit) return QString::fromUtf8(d.divination);
    return QStringLiteral("自定义之牌——由你亲手加冕，意义自定，其力自显。");
}
static QString suitNameOf(const QString &suit) {
    for (const SuitDef2 &d : BUILTIN_SUITS)
        if (QLatin1String(d.id) == suit) return QString::fromUtf8(d.name);
    return suit;
}
static QChar suitBadgeOf(const QString &suit) {
    for (const SuitDef2 &d : BUILTIN_SUITS)
        if (QLatin1String(d.id) == suit) return QString::fromUtf8(d.badge).at(0);
    return QChar(0x2726);
}


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
    // 今日一抽（金卡弹层，当天固定）
    m_fortuneBtn = new QPushButton(QStringLiteral("🎴 今日一抽"), this);
    m_fortuneBtn->setCursor(Qt::PointingHandCursor);
    m_fortuneBtn->setStyleSheet(QStringLiteral(
        "QPushButton { color: #ffd782; border: 1px solid rgba(255,215,130,0.55);"
        "  border-radius: 14px; padding: 5px 14px; font-size: 12px; background: rgba(255,215,130,0.12); }"
        "QPushButton:hover { background: rgba(255,215,130,0.24); }"));
    connect(m_fortuneBtn, &QPushButton::clicked, this, [this] { showFortune(); });
    top->addWidget(m_fortuneBtn);
    root->addLayout(top);

    // 牌意解读浮层（悬停 450ms 触发）
    m_reading = new QLabel(this);
    m_reading->setStyleSheet(QStringLiteral(
        "QLabel { background: rgba(13, 17, 30, 0.97); border: 1px solid rgba(255, 215, 130, 0.5);"
        "  border-radius: 10px; padding: 10px 12px; color: #e8ecf4; }"));
    m_reading->setAttribute(Qt::WA_TransparentForMouseEvents, true);   // 不拦截 hover 防互闪
    m_reading->hide();
    m_readingTimer = new QTimer(this);
    m_readingTimer->setSingleShot(true);
    m_readingTimer->setInterval(450);
    connect(m_readingTimer, &QTimer::timeout, this, [this] {
        for (const AppEntry &a : std::as_const(m_apps))
            if (a.desktopPath == m_readingPath) { showReading(a, m_readingCardRect); break; }
    });

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("兜底搜索（支持应用名）…"));
    m_search->setFixedHeight(32);
    m_search->setStyleSheet(
        "QLineEdit { background: rgba(255,255,255,0.07); border: 1px solid rgba(255,255,255,0.14);"
        "  border-radius: 16px; padding: 0 14px; color: #fff; font-size: 13px; }"
        "QLineEdit:focus { border-color: rgba(255,215,130,0.7); }");
    top->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &t) { rebuildGrid(t); });

    // 花色过滤行（rebuildChips 按实际数量填充，空花色隐藏）
    m_chipsLayout = new QHBoxLayout();
    m_chipsLayout->setSpacing(6);
    m_chipGroup = new QButtonGroup(this);
    m_chipGroup->setExclusive(true);
    root->addLayout(m_chipsLayout);

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
    rebuildChips();
    if (m_subLbl) {
        quint32 usedN = 0;
        for (const AppEntry &a : std::as_const(m_apps)) if (a.count > 0) usedN++;
        m_subLbl->setText(QStringLiteral("本机 %1 款程序入阵 · 已启用 %2 款 · 常用自动浮前 · 零输入 — 翻牌即达")
                              .arg(m_apps.size()).arg(usedN));
    }
}

// ---------------- 花色过滤行 ----------------

void TarotPanel::rebuildChips() {
    if (!m_chipsLayout) return;
    while (QLayoutItem *it = m_chipsLayout->takeAt(0)) {
        if (QWidget *bw = it->widget()) bw->deleteLater();
        delete it;
    }
    // 计数按实际归属（自定义优先于自动）
    QMap<QString, int> cnt;
    for (const AppEntry &a : m_apps) cnt[suitOf(a)]++;
    const QString chipQss = QStringLiteral(
        "QPushButton { border-radius: 13px; padding: 4px 12px; font-size: 11.5px;"
        "  color: rgba(255,255,255,0.78); background: rgba(255,255,255,0.06);"
        "  border: 1px solid rgba(255,255,255,0.13); }"
        "QPushButton:hover { background: rgba(255,255,255,0.12); }"
        "QPushButton:checked { color: #ffd782; border-color: rgba(255,215,130,0.75);"
        "  background: rgba(255,215,130,0.13); }");

    auto addChip = [this, &cnt, &chipQss](const QString &id, const QString &label,
                                          bool custom, const QString &customId) {
        const int n = (id == QLatin1String("all")) ? m_apps.size() : cnt.value(id, 0);
        if (id != QLatin1String("all") && n == 0) return;
        auto *b = new QPushButton(QStringLiteral("%1 %2").arg(label).arg(n), this);
        b->setCheckable(true);
        b->setChecked(m_filterSuit == id);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(chipQss);
        connect(b, &QPushButton::clicked, this, [this, id] {
            m_filterSuit = id;
            rebuildGrid(m_search->text());
        });
        if (custom) {   // 自建类别右键删除（其下程序恢复自动）
            b->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(b, &QPushButton::customContextMenuRequested, this,
                    [this, customId, label](const QPoint &) {
                QMenu m;
                QAction *del = m.addAction(QStringLiteral("删除分类「%1」（其下程序恢复自动归类）").arg(label));
                if (m.exec(QCursor::pos()) == del) {
                    removeCustomCat(customId);
                    if (m_filterSuit == customId) m_filterSuit = QStringLiteral("all");
                    refresh();
                }
            });
        }
        m_chipGroup->addButton(b);
        m_chipsLayout->addWidget(b);
    };

    for (const SuitDef2 &d : BUILTIN_SUITS) {
        if (QLatin1String(d.id) == QLatin1String("all"))
            addChip(QStringLiteral("all"), QStringLiteral("\xF0\x9F\x83\x8F 全部"), false, QString());
        else
            addChip(QString::fromLatin1(d.id),
                    QStringLiteral("%1 %2").arg(QString::fromUtf8(d.badge), QString::fromUtf8(d.name)),
                    false, QString());
    }
    // 自建类别
    for (const QString &c : customCats()) {
        const QStringList parts = c.split(QLatin1Char('|'));
        if (parts.size() != 3) continue;
        addChip(parts[0], QStringLiteral("%1 %2").arg(parts[2], parts[1]), true, parts[0]);
    }
    // ＋新建分类
    auto *add = new QPushButton(QStringLiteral("＋ 新建分类"), this);
    add->setCursor(Qt::PointingHandCursor);
    add->setStyleSheet(QStringLiteral(
        "QPushButton { border-radius: 13px; padding: 4px 12px; font-size: 11.5px; color: #ffd782;"
        "  background: rgba(255,255,255,0.05); border: 1px dashed rgba(255,215,130,0.45); }"
        "QPushButton:hover { background: rgba(255,215,130,0.14); }"));
    connect(add, &QPushButton::clicked, this, [this] {
        QDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("新建分类"));
        auto *v = new QVBoxLayout(&dlg);
        auto *inp = new QLineEdit(&dlg);
        inp->setPlaceholderText(QStringLiteral("分类名称（如：写作工具）"));
        v->addWidget(inp);
        // 16 个候选徽章单选
        const QString badges = QStringLiteral("♦♣♥♠★⚙✦🌙⚡🔥🎮🎨💰📚🧭🔮");
        auto *grid = new QHBoxLayout();
        QList<QPushButton *> opts;
        const QString optQss = QStringLiteral(
            "QPushButton { font-size: 16px; padding: 4px 8px; border-radius: 8px;"
            "  background: rgba(255,255,255,0.06); border: 1px solid transparent; }"
            "QPushButton:checked { border-color: rgba(255,215,130,0.8); background: rgba(255,215,130,0.15); }");
        for (QChar ch : badges) {
            auto *b = new QPushButton(QString(ch), &dlg);
            b->setCheckable(true);
            b->setStyleSheet(optQss);
            grid->addWidget(b);
            opts.append(b);
        }
        opts[0]->setChecked(true);
        v->addLayout(grid);
        auto *btns = new QHBoxLayout();
        auto *ok = new QPushButton(QStringLiteral("创建"), &dlg);
        auto *cancel = new QPushButton(QStringLiteral("取消"), &dlg);
        btns->addStretch();
        btns->addWidget(cancel);
        btns->addWidget(ok);
        v->addLayout(btns);
        connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
        connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
        if (dlg.exec() == QDialog::Accepted && !inp->text().trimmed().isEmpty()) {
            QChar badge = opts[0]->text().at(0);
            for (QPushButton *b : opts)
                if (b->isChecked()) { badge = b->text().at(0); break; }
            addCustomCat(inp->text().trimmed(), badge);
            refresh();
        }
    });
    m_chipsLayout->addWidget(add);
    m_chipsLayout->addStretch();
}

// ---------------- 归类体系（自定义优先，Categories 自动兜底） ----------------

QString TarotPanel::suitOf(const AppEntry &a) const {
    const QString c = usageStore()->value(QStringLiteral("custom/") + a.name).toString();
    return c.isEmpty() ? a.suit : c;
}

QStringList TarotPanel::customCats() const {
    return usageStore()->value(QStringLiteral("customCats")).toStringList();
}

void TarotPanel::addCustomCat(const QString &name, const QChar &badge) {
    QStringList list = customCats();
    const QString id = QStringLiteral("cat%1").arg(QDateTime::currentMSecsSinceEpoch());
    list.append(QStringLiteral("%1|%2|%3").arg(id, name, QString(badge)));
    usageStore()->setValue(QStringLiteral("customCats"), list);
}

void TarotPanel::removeCustomCat(const QString &id) {
    QStringList list = customCats();
    for (int i = 0; i < list.size(); ++i)
        if (list[i].startsWith(id + QLatin1Char('|'))) { list.removeAt(i); break; }
    usageStore()->setValue(QStringLiteral("customCats"), list);
    usageStore()->remove(QStringLiteral("customCat/") + id);
    // 其下程序恢复自动
    const QStringList keys = usageStore()->allKeys();
    for (const QString &k : keys) {
        if (k.startsWith(QStringLiteral("custom/")) &&
            usageStore()->value(k).toString() == id)
            usageStore()->remove(k);
    }
}

void TarotPanel::showCatMenu(const AppEntry &a, const QPoint &globalPos) {
    QMenu m(this);
    m.setStyleSheet(QStringLiteral(
        "QMenu { background: rgba(20, 25, 40, 0.98); color: #e8ecf4; border: 1px solid rgba(255,255,255,0.15);"
        "  border-radius: 8px; padding: 5px; }"
        "QMenu::item { padding: 5px 22px; border-radius: 5px; }"
        "QMenu::item:selected { background: rgba(255,255,255,0.1); }"));
    const QString cur = suitOf(a);
    for (const SuitDef2 &d : BUILTIN_SUITS) {
        if (QLatin1String(d.id) == QLatin1String("all")) continue;
        const QString id = QString::fromLatin1(d.id);
        QAction *act = m.addAction(QStringLiteral("%1 %2%3")
            .arg(QString::fromUtf8(d.badge), QString::fromUtf8(d.name),
                 cur == id ? (id == a.suit ? QStringLiteral("　● 自动") : QStringLiteral("　◈ 自定义")) : QString()));
        act->setData(id);
    }
    for (const QString &c : customCats()) {
        const QStringList parts = c.split(QLatin1Char('|'));
        if (parts.size() != 3) continue;
        QAction *act = m.addAction(QStringLiteral("%1 %2%3")
            .arg(parts[2], parts[1],
                 cur == parts[0] ? QStringLiteral("　◈ 自定义") : QString()));
        act->setData(parts[0]);
    }
    m.addSeparator();
    QAction *autoAct = m.addAction(QStringLiteral("↩ 恢复自动归类"));
    autoAct->setData(QStringLiteral("__auto__"));
    m.addSeparator();
    QAction *launch = m.addAction(QStringLiteral("🚀 立即启动"));

    QAction *sel = m.exec(globalPos);
    if (!sel) return;
    if (sel == launch) { launchApp(a); return; }
    if (sel->data().toString() == QLatin1String("__auto__"))
        usageStore()->remove(QStringLiteral("custom/") + a.name);
    else
        usageStore()->setValue(QStringLiteral("custom/") + a.name, sel->data().toString());
    refresh();
}

// ---------------- 牌意解读浮层 ----------------

void TarotPanel::showReading(const AppEntry &a, const QRect &cardRect) {
    if (!m_reading || gameMode) return;
    const QString suit = suitOf(a);
    const QString div = suitDivination(suit);
    m_reading->setText(QStringLiteral(
        "<div style='color:#ffd782; font-size:12px; letter-spacing:1px;'>%1 %2 · 塔罗解读</div>"
        "<div style='font-size:15px; font-weight:600; margin:4px 0 2px;'>%3</div>"
        "<div style='font-size:11px; color:rgba(255,255,255,0.55);'>%4</div>"
        "<div style='font-size:12px; color:rgba(255,255,255,0.85); margin-top:6px; width: 220px;'>✦ %5</div>")
        .arg(QString(suitBadgeOf(suit)), suitNameOf(suit), a.name,
             a.comment.isEmpty() ? QStringLiteral("—") : a.comment, div));
    m_reading->adjustSize();
    // 定位：牌右侧，贴右则放左侧
    QPoint pos = mapFromGlobal(cardRect.topLeft());
    int x = pos.x() + cardRect.width() + 10;
    if (x + m_reading->width() > width() - 10) x = qMax(10, pos.x() - m_reading->width() - 10);
    int y = qBound(10, pos.y(), qMax(10, height() - m_reading->height() - 10));
    m_reading->move(x, y);
    m_reading->show();
    m_reading->raise();
}

void TarotPanel::hideReading() {
    if (m_reading) m_reading->hide();
}

// ---------------- 今日一抽 ----------------

void TarotPanel::showFortune() {
    if (m_apps.isEmpty()) return;
    // 按名稳定排序 + 日期哈希选牌（当天固定，换日换牌）
    QVector<AppEntry> list = m_apps;
    std::sort(list.begin(), list.end(), [](const AppEntry &x, const AppEntry &y) {
        return x.name.localeAwareCompare(y.name) < 0;
    });
    const qint64 seed = QDate::currentDate().toString(QStringLiteral("yyyyMMdd")).toLongLong();
    const AppEntry a = list[(int)(seed % list.size())];
    static const char *FORTUNES[] = {
        "今日星象明亮，宜顺势而为——此牌助你事半功倍。",
        "运势沉潜之日，宜整备收纳、处理积压，此牌为舟。",
        "灵感涌动的日子，宜创作与输出，此牌点睛。",
        "宜静不宜动，喝口热茶，用此牌从容推进手头一事。",
        "人际星闪耀，宜沟通协作，此牌为你牵线。",
        "宜专注攻坚，切断杂音，此牌护你心流。",
        "变化之风吹过，宜随机应变，此牌是定盘星。",
        "丰收之相，宜复盘收尾，此牌封存今日战果。",
    };
    const QString txt = QString::fromUtf8(FORTUNES[seed % 8]);

    QDialog pop(this, Qt::Dialog | Qt::FramelessWindowHint);
    pop.setModal(true);
    pop.setAttribute(Qt::WA_TranslucentBackground);
    auto *card = new QFrame(&pop);
    card->setStyleSheet(QStringLiteral(
        "QFrame { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "  stop:0 #3a2f14, stop:1 #241d0c); border: 1px solid rgba(255,215,130,0.55);"
        "  border-radius: 16px; }"));
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(28, 24, 28, 20);
    v->setSpacing(6);
    auto *title = new QLabel(QStringLiteral("今日宜开"), card);
    title->setStyleSheet("color:#ffd782; font-size:13px; letter-spacing:3px; background:transparent;");
    title->setAlignment(Qt::AlignHCenter);
    v->addWidget(title);
    QIcon ic = a.icon.startsWith(QLatin1Char('/'))
                   ? QIcon(a.icon)
                   : QIcon::fromTheme(a.icon, QIcon::fromTheme(QStringLiteral("application-x-executable")));
    auto *icon = new QLabel(card);
    icon->setPixmap(ic.pixmap(64, 64));
    icon->setAlignment(Qt::AlignHCenter);
    v->addWidget(icon);
    auto *name = new QLabel(a.name, card);
    name->setStyleSheet("color:#fff; font-size:20px; font-weight:700; background:transparent;");
    name->setAlignment(Qt::AlignHCenter);
    v->addWidget(name);
    auto *suitLine = new QLabel(QStringLiteral("%1 %2").arg(QString(suitBadgeOf(suitOf(a))), suitNameOf(suitOf(a))), card);
    suitLine->setStyleSheet("color:rgba(255,233,184,0.7); font-size:11px; background:transparent;");
    suitLine->setAlignment(Qt::AlignHCenter);
    v->addWidget(suitLine);
    auto *txtL = new QLabel(txt, card);
    txtL->setStyleSheet("color:rgba(255,255,255,0.88); font-size:13px; background:transparent;");
    txtL->setWordWrap(true);
    txtL->setAlignment(Qt::AlignHCenter);
    txtL->setFixedWidth(240);
    v->addWidget(txtL);
    auto *btns = new QHBoxLayout();
    auto *go = new QPushButton(QStringLiteral("立即启动"), card);
    auto *close = new QPushButton(QStringLiteral("收下"), card);
    const QString fbQss = QStringLiteral(
        "QPushButton { border-radius: 9px; padding: 8px 18px; font-size: 13px;"
        "  border: 1px solid rgba(255,215,130,0.5); background: rgba(255,215,130,0.14); color: #ffd782; }"
        "QPushButton:hover { background: rgba(255,215,130,0.28); }");
    const QString plainQss = QStringLiteral(
        "QPushButton { border-radius: 9px; padding: 8px 18px; font-size: 13px;"
        "  border: 1px solid rgba(255,255,255,0.2); background: rgba(255,255,255,0.08);"
        "  color: rgba(255,255,255,0.75); }");
    go->setStyleSheet(fbQss);
    close->setStyleSheet(plainQss);
    btns->addStretch();
    btns->addWidget(close);
    btns->addWidget(go);
    btns->addStretch();
    v->addLayout(btns);
    auto *wrap = new QVBoxLayout(&pop);
    wrap->setContentsMargins(0, 0, 0, 0);
    wrap->addWidget(card);
    connect(close, &QPushButton::clicked, &pop, &QDialog::reject);
    connect(go, &QPushButton::clicked, &pop, &QDialog::accept);
    const bool launchIt = pop.exec() == QDialog::Accepted;
    if (launchIt) launchApp(a);
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
    if (m_filterSuit != QLatin1String("all"))
        list.erase(std::remove_if(list.begin(), list.end(), [&](const AppEntry &a) {
                       return suitOf(a) != m_filterSuit;
                   }), list.end());
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
    // 牌的交互：点击启动 / 右键归类 / 悬停 450ms 牌意浮层
    auto *card = qobject_cast<QWidget *>(obj);
    if (!card) return QWidget::eventFilter(obj, e);
    const QString path = card->property("deckPath").toString();

    if (e->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::LeftButton && card->rect().contains(me->pos())) {
            for (const AppEntry &a : std::as_const(m_apps)) {
                if (a.desktopPath == path) { launchApp(a); break; }
            }
            return true;
        }
    } else if (e->type() == QEvent::ContextMenu) {
        auto *ce = static_cast<QContextMenuEvent *>(e);
        for (const AppEntry &a : std::as_const(m_apps)) {
            if (a.desktopPath == path) {
                hideReading();
                showCatMenu(a, ce->globalPos());
                break;
            }
        }
        return true;
    } else if (e->type() == QEvent::Enter) {
        m_readingPath = path;
        m_readingCardRect = card->geometry();
        m_readingTimer->start();
    } else if (e->type() == QEvent::Leave) {
        m_readingTimer->stop();
        hideReading();
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
