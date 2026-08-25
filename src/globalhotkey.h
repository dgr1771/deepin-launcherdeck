// X11 全局热键：独立 xcb 连接 + QSocketNotifier 轮询事件
// （Qt 自身的 X 连接被窗口系统占用，抢 root 键盘事件要用自己的连接才干净）
#pragma once
#include <QObject>
#include <QSocketNotifier>
#include <functional>
struct xcb_connection_t;

class GlobalHotkey : public QObject {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;
    // keysym 用 ASCII：字母传 'j' 这类小写字符，mods 传 X11 修饰掩码（ControlMask 等）
    bool registerKey(quint32 keysym, quint32 mods, std::function<void()> cb);

private:
    void drainEvents();
    xcb_connection_t *m_conn = nullptr;
    quint32 m_keycode = 0;
    quint32 m_mods = 0;
    std::function<void()> m_cb;
    QSocketNotifier *m_notifier = nullptr;
};
