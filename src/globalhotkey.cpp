#include "globalhotkey.h"
#include <QTimer>
#include <X11/Xlib.h>
#include <xcb/xcb.h>

GlobalHotkey::GlobalHotkey(QObject *parent) : QObject(parent) {}

GlobalHotkey::~GlobalHotkey() {
    if (m_notifier) m_notifier->setEnabled(false);
    if (m_conn) xcb_disconnect(m_conn);
}

bool GlobalHotkey::registerKey(quint32 keysym, quint32 mods, std::function<void()> cb) {
    // keysym → keycode：借 Xlib 查一次（xcb-keysyms 不想多拉一个依赖）
    Display *x = XOpenDisplay(nullptr);
    if (!x) return false;
    KeyCode kc = XKeysymToKeycode(x, keysym);
    XCloseDisplay(x);
    if (!kc) return false;

    m_conn = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(m_conn)) { m_conn = nullptr; return false; }
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(m_conn)).data;
    if (!screen) return false;

    m_keycode = kc;
    m_mods = mods;
    m_cb = std::move(cb);

    // NumLock/CapsLock 开着时修饰掩码会带上额外位——四种组合全抓
    const quint32 lockMasks[] = {0, Mod2Mask, LockMask, Mod2Mask | LockMask};
    for (quint32 m : lockMasks) {
        xcb_grab_key(m_conn, 1, screen->root, mods | m, kc,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
    xcb_flush(m_conn);

    int fd = xcb_get_file_descriptor(m_conn);
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] { drainEvents(); });
    return true;
}

void GlobalHotkey::drainEvents() {
    xcb_generic_event_t *e = nullptr;
    while ((e = xcb_poll_for_event(m_conn))) {
        const quint8 type = e->response_type & 0x7f;
        if (type == XCB_KEY_PRESS) {
            auto *kp = reinterpret_cast<xcb_key_press_event_t *>(e);
            // 掩掉锁位后比对修饰键（抓的时候按组合抓，这里宽松匹配主修饰位）
            if (kp->detail == m_keycode && (kp->state & m_mods) == m_mods && m_cb) {
                m_cb();
            }
        }
        free(e);
    }
}
