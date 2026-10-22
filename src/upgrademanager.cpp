// 应用升级管理（deepin）：apt 数据源 + pkexec 静默批量升级
// I/O 全走 popen + std::thread 后台执行，结果经队列回主线程。
#include "upgrademanager.h"

#include <QDateTime>
#include <QMetaObject>
#include <QRegularExpression>
#include <cstdio>
#include <thread>

namespace {

// popen 读全部输出（/bin/sh -c，无信号/事件循环依赖）
QString popenRead(const QString &cmd) {
    QString out;
    FILE *fp = ::popen(cmd.toUtf8().constData(), "r");
    if (!fp) return out;
    char buf[4096];
    while (fgets(buf, sizeof buf, fp)) out += QString::fromUtf8(buf);
    ::pclose(fp);
    return out;
}

struct ParsedItem {
    QString package;
    QString newVersion;
    QString oldVersion;
};

// 行格式兼容英文 [upgradable from: X] 与中文 [可从该版本升级：X]
ParsedItem parseLine(const QString &raw) {
    static const QRegularExpression re(
        QStringLiteral("^([^/\\s]+)/\\S+\\s+(\\S+)\\s+\\S+\\s+\\[[^\\]]*[:：]\\s*([^\\]]+)\\]\\s*$"));
    ParsedItem it;
    const auto m = re.match(raw.trimmed());
    if (m.hasMatch()) {
        it.package = m.captured(1);
        it.newVersion = m.captured(2);
        it.oldVersion = m.captured(3);
    }
    return it;
}

}  // namespace

UpgradeManager::UpgradeManager(QObject *parent) : QObject(parent) {}

void UpgradeManager::checkUpgrades(bool force) {
    if (m_busy) {
        fprintf(stderr, "[UM] skip busy
");
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!force && m_lastCheckMs && now - m_lastCheckMs < 6 * 3600 * 1000) {
        emit checked(m_items);
        return;   // 缓存期内直接回上次结果
    }
    m_busy = true;
    fprintf(stderr, "[UM] check called, running apt in background thread
");
    std::thread([this] {
        const QString out = popenRead(QStringLiteral("apt list --upgradable 2>/dev/null"));
        QList<UpgradeItem> list;
        for (const QString &raw : out.split(QLatin1Char('\n'))) {
            const ParsedItem it = parseLine(raw);
            if (!it.package.isEmpty()) {
                UpgradeItem full;
                full.package = it.package;
                full.newVersion = it.newVersion;
                full.oldVersion = it.oldVersion;
                list.append(full);
            }
        }
        fprintf(stderr, "[UM] apt parsed: %d
", (int)list.size());

        // 二段：批量查每个包提供的 .desktop（一次 bash 调用）
        if (!list.isEmpty()) {
            QStringList pkgs;
            for (const auto &it : list) pkgs << it.package;
            const QString dOut = popenRead(
                QStringLiteral("for p in %1; do d=$(dpkg -L \"$p\" 2>/dev/null | grep -m1 '/usr/share/applications/.*\\.desktop$'); echo \"$p|$d\"; done")
                    .arg(pkgs.join(QLatin1Char(' '))));
            QHash<QString, QString> df;
            for (const QString &line : dOut.split(QLatin1Char('\n'))) {
                const int pos = line.indexOf(QLatin1Char('|'));
                if (pos > 0) df.insert(line.left(pos), line.mid(pos + 1));
            }
            for (auto &it : list) it.desktopFile = df.value(it.package);
        }

        QMetaObject::invokeMethod(this, [this, list] {
            m_items = list;
            m_lastCheckMs = QDateTime::currentMSecsSinceEpoch();
            m_busy = false;
            fprintf(stderr, "[UM] resolved: %d items
", (int)m_items.size());
            emit checked(m_items);
        }, Qt::QueuedConnection);
    }).detach();
}

void UpgradeManager::upgradePackages(const QStringList &packages) {
    if (m_busy || packages.isEmpty()) return;
    m_busy = true;
    emit upgradeProgress(QStringLiteral("等待授权…（请输入用户密码并点确认）"));
    std::thread([this, packages] {
        // pkexec 弹系统授权框；DEBIAN_FRONTEND 防交互卡住
        const QString cmd = QStringLiteral(
            "pkexec env DEBIAN_FRONTEND=noninteractive apt-get install -y --only-upgrade %1")
            .arg(packages.join(QLatin1Char(' ')));
        FILE *fp = ::popen(cmd.toUtf8().constData(), "r");
        QString out;
        char buf[4096];
        while (fp && fgets(buf, sizeof buf, fp)) out += QString::fromUtf8(buf);
        const int code = fp ? ::pclose(fp) : -1;
        fprintf(stderr, "[UM] upgrade pclose code: %d
", code);

        // 退出码非零≠失败（下载波折也可能非零）——复核清单定性：包消失 = 实际已升级
        bool ok = (code == 0);
        if (!ok) {
            const QString recheck = popenRead(QStringLiteral("apt list --upgradable 2>/dev/null"));
            bool still = false;
            for (const QString &pkg : packages) {
                if (recheck.contains(QLatin1Char('\n') + pkg + QLatin1Char('/'))) { still = true; break; }
            }
            if (!still) ok = true;   // 全部从待升清单消失 = 成功
        }
        const QString detail = ok ? QStringLiteral("升级完成")
                                  : QStringLiteral("升级未完成，请关闭相关应用后重试");
        QMetaObject::invokeMethod(this, [this, ok, detail] {
            m_busy = false;
            m_lastCheckMs = 0;   // 强制下次重查
            emit upgradeFinished(ok, detail);
        }, Qt::QueuedConnection);
    }).detach();
}
