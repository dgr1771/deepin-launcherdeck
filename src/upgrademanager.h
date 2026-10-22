// 应用升级管理（deepin）：apt 数据源 + pkexec 静默批量升级
// 对应 Windows 版的 winget 集成——deepin 上更新通道是 apt 仓库，
// 升级需 root，走 pkexec 弹系统授权框（一次授权可带一串包）
#pragma once

#include <QObject>
#include <QProcess>
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
    void runAptList(bool force);
    void resolveDesktopFiles();

    QProcess *m_proc = nullptr;
    QList<UpgradeItem> m_items;     // 已解析 desktopFile 的最终列表
    QList<UpgradeItem> m_pending;   // apt 解析完、待匹配 desktop 的中间态
    qint64 m_lastCheckMs = 0;
    bool m_busy = false;
};
