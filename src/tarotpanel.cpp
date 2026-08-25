#include "tarotpanel.h"
#include "flowlayout.h"
#include <algorithm>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QProcess>
#include <QPainter>
#include <QPainterPath>
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

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("兜底搜索（支持应用名）…"));
    m_search->setFixedHeight(32);
    m_search->setStyleSheet(
        "QLineEdit { background: rgba(255,255,255,0.07); border: 1px solid rgba(255,255,255,0.14);"
        "  border-radius: 16px; padding: 0 14px; color: #fff; font-size: 13px; }"
        "QLineEdit:focus { border-color: rgba(255,215,130,0.7); }");
    root->addWidget(m_search);
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &t) { rebuildGrid(t); });

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
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

    // 小屏/高缩放夹紧（Windows 版同款教训：写死 820 在 150% 缩放会裁底）
    QRect av = QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(1280, av.width() - 16), qMin(820, av.height() - 12));
    move(av.center() - rect().center());
}

void TarotPanel::refresh() {
    AppScanner scanner;
    m_apps = scanner.scan();
    QSettings *s = usageStore();
    for (AppEntry &a : m_apps)
        a.count = s->value(QStringLiteral("usage/") + a.name).toUInt();
    rebuildGrid(m_search->text());
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

    const QString cardQss = QStringLiteral(
        "QFrame#tcard { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "  stop:0 #232a45, stop:1 #171d33); border: 1px solid rgba(255,255,255,0.2);"
        "  border-radius: 10px; }"
        "QFrame#tcard:hover { border: 1px solid rgba(255,215,130,0.85);"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #2c3556, stop:1 #1e2540); }");

    for (const AppEntry &a : list) {
        auto *card = new QFrame(m_gridBox);
        card->setObjectName(QStringLiteral("tcard"));
        card->setFixedSize(102, 148);
        card->setStyleSheet(cardQss);
        card->setCursor(Qt::PointingHandCursor);
        card->setToolTip(a.comment.isEmpty() ? a.name : a.comment);
        card->setProperty("deckPath", a.desktopPath);

        auto *v = new QVBoxLayout(card);
        v->setContentsMargins(8, 12, 8, 8);
        v->setSpacing(6);
        auto *iconLbl = new QLabel(card);
        iconLbl->setAlignment(Qt::AlignCenter);
        QIcon ic = a.icon.startsWith(QLatin1Char('/'))
                       ? QIcon(a.icon)
                       : QIcon::fromTheme(a.icon, QIcon::fromTheme(QStringLiteral("application-x-executable")));
        iconLbl->setPixmap(ic.pixmap(52, 52));
        v->addWidget(iconLbl);
        auto *nameLbl = new QLabel(a.name, card);
        nameLbl->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        nameLbl->setWordWrap(true);
        nameLbl->setStyleSheet("color: rgba(255,255,255,0.92); font-size: 11px; background: transparent; border: none;");
        nameLbl->setFixedHeight(30);
        v->addWidget(nameLbl);
        v->addStretch();

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
            auto *card = qobject_cast<QFrame *>(obj);
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
