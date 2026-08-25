#include "appscanner.h"
#include <QDir>
#include <QFile>
#include <QHash>
#include <QSettings>

QVector<AppEntry> AppScanner::scan() const {
    // 本地目录放后面：同名 .desktop 本地覆盖系统
    const QStringList dirs = {
        QStringLiteral("/usr/share/applications"),
        QStringLiteral("/usr/local/share/applications"),
        QStringLiteral("/var/lib/flatpak/exports/share/applications"),
        QDir::homePath() + QStringLiteral("/.local/share/applications"),
    };

    QHash<QString, AppEntry> byFile;   // key = 文件名（去重 + 本地覆盖）
    for (const QString &dir : dirs) {
        const auto entries = QDir(dir).entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &fn : entries) {
            AppEntry e;
            if (!parseDesktopFile(dir + QLatin1Char('/') + fn, e)) continue;
            byFile.insert(fn, e);
        }
    }
    return byFile.values().toVector();
}

bool AppScanner::parseDesktopFile(const QString &path, AppEntry &out) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    // 手工解析而不用 QSettings/IniFormat：QSettings 会把键名转小写，Name/Exec 就没了
    bool inEntry = false;
    QString type, name, nameZh, noDisplay, hidden, exec, icon, comment, categories;
    while (!f.atEnd()) {
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        if (line.startsWith(QLatin1Char('['))) { inEntry = (line == QLatin1String("[Desktop Entry]")); continue; }
        if (!inEntry) continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) continue;
        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1).trimmed();
        if      (key == QLatin1String("Type"))          type = val;
        else if (key == QLatin1String("Name"))          name = val;
        else if (key == QLatin1String("Name[zh_CN]"))   nameZh = val;
        else if (key == QLatin1String("Comment"))       comment = val;
        else if (key == QLatin1String("Exec"))          exec = val;
        else if (key == QLatin1String("Icon"))          icon = val;
        else if (key == QLatin1String("Categories"))    categories = val;
        else if (key == QLatin1String("NoDisplay"))     noDisplay = val;
        else if (key == QLatin1String("Hidden"))        hidden = val;
    }

    if (type != QLatin1String("Application")) return false;
    if (noDisplay == QLatin1String("true") || hidden == QLatin1String("true")) return false;
    if (exec.isEmpty() && !path.contains(QLatin1String("gio"))) return false;

    out.name = nameZh.isEmpty() ? name : nameZh;
    if (out.name.isEmpty()) return false;
    out.comment = comment;
    out.icon = icon;
    out.desktopPath = path;
    out.categories = categories;
    return true;
}
