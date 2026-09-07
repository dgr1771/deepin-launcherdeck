#include <DApplication>
#include <QFile>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QSystemTrayIcon>
#include <QTimer>
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
    // 逻辑自测模式：DECK_SELFTEST=1 ./deepin-launcherdeck → 断言结果进日志，0/1 退出码
    if (qEnvironmentVariableIsSet("DECK_SELFTEST")) {
        return FCLogic::runSelfTest();
    }

    // DDE 25 可能默认 wayland：全局热键 XGrabKey 与窗口置顶都依赖 X11（看板同款决策）
    qputenv("QT_QPA_PLATFORM", "xcb");
    qputenv("DSG_APP_ID", "org.dgr.launcherdeck");

    DApplication app(argc, argv);
    app.setOrganizationName("dgr");
    app.setApplicationName("deepin-launcherdeck");
    app.setApplicationVersion("0.6.2");
    // 图标主题兜底：bloom 缺系统图标（如 user-trash 只在 hazy-color），缺名时回退查 hazy-color
    QIcon::setFallbackThemeName(QStringLiteral("hazy-color"));
    app.setProductName(QStringLiteral("唤启"));
    app.setApplicationDescription(QStringLiteral("托盘常驻 + 全局热键的应用一屏启动器（DTK 原生版）"));
    app.loadTranslator();
    if (!app.setSingleInstance(QStringLiteral("deepin-launcherdeck"))) return 0;

    TarotPanel panel;
    // 联测钩子：DECK_DEBUG_SHOW=1 启动即显示面板；DECK_DEBUG_GAME=1 直接进游戏模式（免模拟点击）
    if (qEnvironmentVariableIsSet("DECK_DEBUG_SHOW")) panel.show();
    if (qEnvironmentVariableIsSet("DECK_DEBUG_GAME")) panel.toggleMode();
    if (qEnvironmentVariableIsSet("DECK_DEBUG_FORTUNE"))   // 今日一抽弹层联测
        QTimer::singleShot(400, &panel, [&panel] { panel.show(); panel.showFortune(); });

    // 托盘：左键/菜单展开
    auto *tray = new QSystemTrayIcon(drawTrayIcon(), &app);
    tray->setToolTip(QStringLiteral("唤启 · Ctrl+J 唤起"));
    auto *menu = new QMenu();
    menu->addAction(QStringLiteral("展开应用一屏（Ctrl+J）"), &panel, [this_ = &panel] { this_->toggle(); });
    menu->addAction(QStringLiteral("重新扫描本机应用"), &panel, [this_ = &panel, tray_ = tray] {
        this_->refresh();
        tray_->showMessage(QStringLiteral("唤启"), QStringLiteral("已重新扫描本机应用"),
                            QSystemTrayIcon::Information, 1500);
    });
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
