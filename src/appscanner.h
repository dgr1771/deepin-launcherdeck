// 扫描本机 .desktop 应用条目（Linux 侧比 Windows 注册表干净得多：一个目录读全）
#pragma once
#include <QString>
#include <QVector>

struct AppEntry {
    QString name;        // 显示名（优先 zh_CN 本地化）
    QString comment;     // 描述（tooltip）
    QString icon;        // 主题名或绝对路径
    QString desktopPath; // .desktop 文件路径（启动入口）
    QString categories;  // 原始 Categories（花色归类依据）
    QString suit;        // 花色 id：network/audiovideo/browser/development/utility/system/other
    quint32 count = 0;   // 启动次数（常用浮前）
};

class AppScanner {
public:
    QVector<AppEntry> scan() const;

private:
    static bool parseDesktopFile(const QString &path, AppEntry &out);
};
