// 应用升级管理（deepin）：apt 数据源 + pkexec 静默批量升级
// 实现注记：UOS eagle 的 DTK5/Qt5.11 环境下 QProcess 异步信号偶发不触发
// （finished 不回调、事件循环无感知），故全部 I/O 走 popen + 后台线程 +
// QMetaObject::invokeMethod 队列回主线程，不依赖任何信号机制。
#pragma once

#include <QObject>
#include <QList>

struct UpgradeItem {
    QString package;      // apt 包名
    QString newVersion;   // 仓库里的新版本
    QString oldVersion;   // 当前安装版本
    QString desktopFile;  // 包内 .desktop 路径（空 = 未匹配到本机应用卡）
};

class UpgradeManager : public QObject {
    Q_OBJECT
public:
    explicit UpgradeManager(QObject *parent = nullptr);

    // 查询可升级列表（force=false 时 6 小时缓存内直接返回上次结果）
    void checkUpgrades(bool force = false);
    // 批量升级（pkexec apt-get install -y --only-upgrade …，一次授权带全部包）
    void upgradePackages(const QStringList &packages);
    bool busy() const { return m_busy; }
    const QList<UpgradeItem> &items() const { return m_items; }

signals:
    void checked(const QList<UpgradeItem> &items);
    void checkFailed(const QString &err);
    void upgradeProgress(const QString &line);
    void upgradeFinished(bool ok, const QString &detail);

private:
    QList<UpgradeItem> m_items;
    qint64 m_lastCheckMs = 0;
    bool m_busy = false;
};
