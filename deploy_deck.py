#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
deepin-launcherdeck（应用牌堆 DTK 版）—— 远程部署/测试助手
用法:
  python deploy_deck.py sync     # 上传源码到远端 ~/deepin-launcherdeck/
  python deploy_deck.py build    # 远端 cmake 编译
  python deploy_deck.py launch   # nohup 后台启动
  python deploy_deck.py shot     # 远端截屏回传 shots/deck-live.png
  python deploy_deck.py status   # 查看进程
  python deploy_deck.py logs     # 拉 /tmp/dtk-deck.log
  python deploy_deck.py kill     # 停止
  python deploy_deck.py probe    # 探测远端环境
"""
import sys, os, argparse

HOST = "192.168.1.54"
USER = "dgr"
PASS = "1"
LOCAL_ROOT = os.path.dirname(os.path.abspath(__file__))
REMOTE_ROOT = "/home/dgr/deepin-launcherdeck"
DISPLAY = ":0"
BIN = "deepin-launcherdeck"
BIN_TRUNC = BIN[:15]          # Linux comm 名 15 字符截断
LOG = "/tmp/dtk-deck.log"

import paramiko

def conn():
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username=USER, password=PASS, timeout=12,
              allow_agent=False, look_for_keys=False)
    return c

def run(c, cmd, timeout=300):
    stdin, stdout, stderr = c.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode("utf-8", "replace")
    err = stderr.read().decode("utf-8", "replace")
    rc = stdout.channel.recv_exit_status()
    return rc, out, err

SKIP_DIRS = {"build", ".git", ".vscode", "__pycache__", "shots"}

def _should_skip(path):
    parts = path.replace("\\", "/").split("/")
    return any(p in SKIP_DIRS for p in parts)

def _ensure_remote_dir(sftp, path):
    try:
        sftp.stat(path)
    except IOError:
        parent = "/".join(path.rstrip("/").split("/")[:-1])
        if parent:
            _ensure_remote_dir(sftp, parent)
        sftp.mkdir(path)

def sync(c):
    sftp = c.open_sftp()
    print("同步源码 %s -> %s" % (LOCAL_ROOT, REMOTE_ROOT))
    _ensure_remote_dir(sftp, REMOTE_ROOT)
    n = 0
    for root, dirs, files in os.walk(LOCAL_ROOT):
        rel = os.path.relpath(root, LOCAL_ROOT).replace("\\", "/")
        if _should_skip(rel):
            dirs[:] = []
            continue
        remote_dir = REMOTE_ROOT if rel == "." else REMOTE_ROOT + "/" + rel
        _ensure_remote_dir(sftp, remote_dir)
        for f in files:
            local_file = os.path.join(root, f)
            rel_file = os.path.relpath(local_file, LOCAL_ROOT).replace("\\", "/")
            if _should_skip(rel_file) or os.path.getsize(local_file) > 5 * 1024 * 1024:
                continue
            sftp.put(local_file, remote_dir + "/" + f)
            n += 1
    sftp.close()
    print("已上传 %d 个文件" % n)

def build(c):
    print("远端 cmake 编译 ...")
    cmd = ("cd %s && rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -4 && "
           "{ cmake --build build -j$(nproc) 2>&1 || echo 'BUILD_FAILED'; } | tail -25" % REMOTE_ROOT)
    rc, out, err = run(c, cmd, timeout=300)
    print(out)
    if err.strip(): print("[stderr]", err[:500])
    ok = "BUILD_FAILED" not in out
    print("[build %s]" % ("OK" if ok else "FAILED"))
    rc2, out2, _ = run(c, "ls -la %s/build/%s 2>&1" % (REMOTE_ROOT, BIN))
    print(out2.strip())
    return 0 if ok else 1

def launch(c, debug=False, show=False):
    print("后台启动 nohup ...")
    run(c, "killall -9 %s %s 2>/dev/null; sleep 1" % (BIN, BIN_TRUNC), timeout=10)
    envs = ""
    if debug: envs += "DECK_DEBUG_SHOW=1 DECK_DEBUG_GAME=1 "
    elif show: envs += "DECK_DEBUG_SHOW=1 "
    cmd = ("cd %s/build && export DISPLAY=%s; "
           "%s nohup ./%s >%s 2>&1 & echo PID=$!; sleep 2; "
           "ps -p $! >/dev/null && echo ALIVE || echo DEAD" % (REMOTE_ROOT, DISPLAY, envs, BIN, LOG))
    rc, out, err = run(c, cmd, timeout=15)
    print(out.strip())
    if err.strip(): print("[stderr]", err[:300])

def shot(c):
    os.makedirs(os.path.join(LOCAL_ROOT, "shots"), exist_ok=True)
    out_file = os.path.join(LOCAL_ROOT, "shots", "deck-live.png")
    rc, out, err = run(c, "export DISPLAY=%s; rm -f /tmp/deck-shot.png; sleep 1; "
                          "scrot /tmp/deck-shot.png && ls -la /tmp/deck-shot.png" % DISPLAY, timeout=40)
    print(out, err[:200] if err else "")
    if "deck-shot.png" in out:
        sftp = c.open_sftp()
        sftp.get("/tmp/deck-shot.png", out_file)
        sftp.close()
        print("已回传:", out_file, os.path.getsize(out_file), "bytes")

def status(c):
    rc, out, _ = run(c, "ps -ef | grep -E '%s' | grep -v grep || echo '(未运行)'" % BIN)
    print(out.strip())

def logs(c):
    rc, out, _ = run(c, "tail -40 %s 2>/dev/null || echo '(无日志)'" % LOG)
    print(out.strip())

def kill(c):
    rc, out, _ = run(c, "killall -9 %s %s 2>/dev/null; pkill -9 -f '/%s/build/%s' 2>/dev/null; "
                        "sleep 1; pgrep -f '%s' || echo 'ALL_KILLED'" % (BIN, BIN_TRUNC, BIN, BIN, BIN), timeout=10)
    print(out.strip())

def probe(c):
    cmds = [
        ("系统", "cat /etc/os-release | head -2"),
        ("Qt6/DTK6", "pkg-config --modversion Qt6Core 2>/dev/null; dpkg -l | grep -E 'libdtk6widget-dev' | awk '{print $2,$3}'"),
        ("X11 头文件", "ls /usr/include/X11/Xlib.h 2>&1"),
        ("scrot", "which scrot"),
        ("应用目录", "ls /usr/share/applications | wc -l"),
    ]
    for name, cmd in cmds:
        rc, out, _ = run(c, cmd)
        print("\n[%s]\n%s" % (name, out.strip() or "(空)"))

def deb(c):
    print("远端 dpkg-buildpackage 打包 ...")
    cmd = ("cd %s && chmod +x debian/rules && rm -rf obj-x86_64-linux-gnu && "
           "dpkg-buildpackage -us -uc -b 2>&1 | tail -12; "
           "ls -lh ../deepin-launcherdeck_*.deb 2>/dev/null" % REMOTE_ROOT)
    rc, out, err = run(c, cmd, timeout=300)
    print(out)
    if err.strip(): print("[stderr]", err[:400])

def install(c):
    print("上传 deb 并安装 ...")
    rc, out, _ = run(c, "ls %s/../deepin-launcherdeck_*.deb 2>/dev/null | sort -V | tail -1" % REMOTE_ROOT)
    deb_path = out.strip()
    if not deb_path or ".deb" not in deb_path:
        print("未找到 deb，先运行: python deploy_deck.py deb"); return
    local_deb = os.path.join(LOCAL_ROOT, os.path.basename(deb_path))
    sftp = c.open_sftp()
    sftp.get(deb_path, local_deb); sftp.close()
    print("下载:", local_deb)
    rc, out, err = run(c, "echo %s | sudo -S -p '' dpkg -i %s 2>&1 | tail -4; "
                          "echo %s | sudo -S -p '' apt-get install -f -y 2>&1 | tail -3"
                          % (PASS, deb_path, PASS), timeout=120)
    print(out)
    if err.strip(): print("[stderr]", err[:300])

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["probe", "sync", "build", "launch", "shot", "status", "logs", "kill", "deb", "install"])
    ap.add_argument("--debug", action="store_true", help="启动即显示面板并进游戏模式（联测）")
    ap.add_argument("--show", action="store_true", help="启动即显示面板（塔罗模式联测）")
    a = ap.parse_args()
    print("连接 %s@%s ..." % (USER, HOST))
    c = conn()
    print("已连接\n")
    {"probe": probe, "sync": sync, "build": build,
     "launch": lambda cc: launch(cc, a.debug, a.show), "shot": shot,
     "status": status, "logs": logs, "kill": kill,
     "deb": deb, "install": install}[a.cmd](c)
    c.close()

if __name__ == "__main__":
    main()
