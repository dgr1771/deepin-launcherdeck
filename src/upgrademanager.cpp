#include "upgrademanager.h"

#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

UpgradeManager::UpgradeManager(QObject *parent) : QObject(parent) {}

void UpgradeManager::checkUpgrades(bool force) {
    if (m_busy) { qWarning() << "[UM] skip busy"; return; }
    qWarning() << "[UM] check called force=" << force;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!force && m_lastCheckMs && now - m_lastCheckMs < 6 * 3600 * 1000) {
        emit checked(m_items);
        return;   // 缓存期内直接回上次结果
    }
    runAptList(force);
}

void UpgradeManager::runAptList(bool force) {
    Q_UNUSED(force);
    m_busy = true;
    qWarning() << "[UM] runAptList starting";
    m_proc = new QProcess(this);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(m_proc->readAllStandardOutput());
        m_proc->deleteLater(); m_proc = nullptr;
        if (code != 0) { m_busy = false; emit checkFailed(QStringLiteral("apt 查询失败（代码 %1）").arg(code)); return; }

        // 行格式：package/repo  newVersion  arch  [可从该版本升级：X]（兼容英文 upgradable from）
        static const QRegularExpression re(
            QStringLiteral("^([^/\\s]+)/\\S+\\s+(\\S+)\\s+\\S+\\s+\\[^\]]*[:：]\\s*([^\\]]+)\\]\\s*$"));
        m_pending.clear();
        for (const QString &raw : out.split(QLatin1Char('\n'))) {
            const auto m = re.match(raw.trimmed());
            if (!m.hasMatch()) continue;
            UpgradeItem it;
            it.package = m.captured(1);
            it.newVersion = m.captured(2);
            it.oldVersion = m.captured(3);
            m_pending.append(it);
        }
        m_lastCheckMs = QDateTime::currentMSecsSinceEpoch();
        if (m_pending.isEmpty()) { m_items.clear(); m_busy = false; emit checked(m_items); return; }

        // 二段：批量查每个包提供的 .desktop（一次 bash 调用，避免逐包起进程）
        QStringList pkgs;
        for (const auto &it : m_pending) pkgs << it.package;
        m_proc = new QProcess(this);
        connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
            const QString out = QString::fromUtf8(m_proc->readAllStandardOutput());
            m_proc->deleteLater(); m_proc = nullptr;
            QHash<QString, QString> df;
            for (const QString &line : out.split(QLatin1Char('\n'))) {
                const int pos = line.indexOf(QLatin1Char('|'));
                if (pos > 0) df.insert(line.left(pos), line.mid(pos + 1));
            }
            for (auto &it : m_pending) it.desktopFile = df.value(it.package);
            m_items = m_pending;
            m_pending.clear();
            m_busy = false;
            emit checked(m_items);
        });
        m_proc->start(QStringLiteral("bash"), QStringList{QStringLiteral("-c"),
            QStringLiteral("for p in %1; do d=$(dpkg -L \"$p\" 2>/dev/null | grep -m1 '/usr/share/applications/.*\\.desktop$'); echo \"$p|$d\"; done")
                .arg(pkgs.join(QLatin1Char(' ')))});
    });
    m_proc->start(QStringLiteral("bash"), QStringList{QStringLiteral("-c"),
        QStringLiteral("apt list --upgradable 2>/dev/null")});
}

void UpgradeManager::upgradePackages(const QStringList &packages) {
    if (m_busy || packages.isEmpty()) return;
    m_busy = true;
    emit upgradeProgress(QStringLiteral("等待授权…（请输入用户密码并点确认）"));
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this] {
        const QString tail = QString::fromUtf8(m_proc->readAllStandardOutput()).simplified();
        if (!tail.isEmpty()) emit upgradeProgress(tail.right(120));
    });
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, packages](int code, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(m_proc->readAllStandardOutput()).simplified();
        m_proc->deleteLater(); m_proc = nullptr;
        m_busy = false;
        m_lastCheckMs = 0;                       // 升级后强制重查
        const bool ok = (code == 0);
        emit upgradeFinished(ok, ok
            ? QStringLiteral("升级完成")
            : QStringLiteral("升级未完成（代码 %1）：%2").arg(code).arg(out.right(200)));
        checkUpgrades(true);                     // 重查 → 红点刷新
    });
    emit upgradeProgress(QStringLiteral("开始升级 %1 个包…").arg(packages.size()));
    m_proc->start(QStringLiteral("pkexec"),
                  QStringList{QStringLiteral("env"), QStringLiteral("DEBIAN_FRONTEND=noninteractive"),
                              QStringLiteral("apt-get"), QStringLiteral("install"), QStringLiteral("-y"),
                              QStringLiteral("--only-upgrade")} + packages);
}
