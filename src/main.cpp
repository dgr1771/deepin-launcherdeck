#include <DApplication>
#include <QFile>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QSystemTrayIcon>
#include <X11/X.h>

#include "globalhotkey.h"
#include "tarotpanel.h"

DWIDGET_USE_NAMESPACE

static QIcon drawTrayIcon() {
    // 程序化画托盘图标：深底圆角 + 金色 ✦（无外部资源依赖）
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath r;
    r.addRoundedRect(4, 4, 56, 56, 14, 14);
    p.fillPath(r, QColor(23, 29, 51));
    p.setPen(QPen(QColor(255, 215, 130, 200), 2));
    p.drawPath(r);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 215, 130));
    // 四角星 ✦（两条二次贝塞尔拼菱形星）
    QPainterPath star;
    star.moveTo(32, 14);
    star.quadTo(36, 28, 50, 32);
    star.quadTo(36, 36, 32, 50);
    star.quadTo(28, 36, 14, 32);
    star.quadTo(28, 28, 32, 14);
    p.drawPath(star);
    return QIcon(pm);
}

int main(int argc, char *argv[]) {
    // DDE 25 可能默认 wayland：全局热键 XGrabKey 与牌阵置顶都依赖 X11（看板同款决策）
    qputenv("QT_QPA_PLATFORM", "xcb");
    qputenv("DSG_APP_ID", "org.dgr.launcherdeck");

    DApplication app(argc, argv);
    app.setOrganizationName("dgr");
    app.setApplicationName("deepin-launcherdeck");
    app.setApplicationVersion("0.1.0");
    app.setProductName(QStringLiteral("应用牌堆"));
    app.setApplicationDescription(QStringLiteral("托盘常驻 + 全局热键的塔罗牌阵应用启动器（DTK 原生版）"));
    app.loadTranslator();
    if (!app.setSingleInstance(QStringLiteral("deepin-launcherdeck"))) return 0;

    TarotPanel panel;

    // 托盘：左键/菜单展开
    auto *tray = new QSystemTrayIcon(drawTrayIcon(), &app);
    tray->setToolTip(QStringLiteral("应用牌堆 · Ctrl+J 唤起"));
    auto *menu = new QMenu();
    menu->addAction(QStringLiteral("展开牌堆（Ctrl+J）"), &panel, [this_ = &panel] { this_->toggle(); });
    menu->addSeparator();
    menu->addAction(QStringLiteral("退出"), &app, &QApplication::quit);
    tray->setContextMenu(menu);
    QObject::connect(tray, &QSystemTrayIcon::activated, &app,
                     [this_ = &panel](QSystemTrayIcon::ActivationReason r) {
                         if (r == QSystemTrayIcon::Trigger) this_->toggle();
                     });
    tray->show();

    // 全局热键 Ctrl+J
    GlobalHotkey hotkey;
    const bool hkOk = hotkey.registerKey('j', ControlMask, [&panel] { panel.toggle(); });
    if (!hkOk) qWarning("global hotkey Ctrl+J register FAILED (tray click still works)");

    return app.exec();
}
