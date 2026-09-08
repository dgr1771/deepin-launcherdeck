【deepin插件开发活动】唤启 Launcher Deck —— 一屏全览 · 一唤即启的 DTK 原生应用启动器

## 项目简介

**唤启 Launcher Deck** 是一个基于 DTK6 的 deepin 原生应用启动器。按下 `Ctrl+J`，本机所有已安装应用以精美卡片形式一屏铺开——悬停 3D 翻面看详情、花色分类、常用自动浮前、点击即启。同时内置一局完整的空当接龙游戏。

- **源码仓库**：https://github.com/dgr1771/deepin-launcherdeck
- **下载安装（deb）**：https://github.com/dgr1771/deepin-launcherdeck/releases/download/v0.9.5/deepin-launcherdeck_0.9.5-1_amd64.deb
- **开源协议**：GPL-3.0
- **开发方向**：DTK 原生应用（deepin 25 / amd64）
- **开发者**：隔壁村布布（dgr1771）

## 功能特性

### 应用一屏模式
- 自动扫描 .desktop 条目（含 zh_CN 本地化与本地覆盖，回收站也纳入牌堆）
- 卡片网格自适应缩放：34~100+ 款应用全部完整可见
- 花色归类（♦♣♥♠★⚙✦ 七花色）+ 自建分类 + 右键归类
- 悬停 3D 翻面 + 牌意解读浮层
- 常用自动浮前（次数×时间衰减）+ 手动置顶
- 拼音首字母搜索 · 全键盘导航（方向键选牌 + Enter 启动）

### 空当接龙游戏模式
- 微软原版发牌算法（LCG，1-32000 局全可通关）
- 标准规则：红黑交替降序、序列移动、自由位/回收位
- 发牌动画：三段抖洗 → 逐张飞出 → 落地翻面
- 进度自动保存 · 难度选择（初级/中级/经典）

### 系统集成
- **DBlurEffectWidget 系统级真毛玻璃**：桌面壁纸透过面板实时模糊，deepin-skills dtk-development 指引集成
- 托盘常驻 + Ctrl+J 全局热键（XGrabKey 独立 xcb 连接，与输入法无冲突）
- 外观主题四套预设 + 透明度调节
- 「关于唤启」标准关于弹窗（DAboutDialog）：开发者署名 + GPL-3.0 + 仓库直达
- 单实例锁 · 日志轮转 · 渲染错误上报

## 截图

1. 应用一屏主界面（唤启面板 + 七花色分类 + 38 应用卡片）
2. 空当接龙游戏模式（发牌完毕 + 应用名牌面）
3. 拼音搜索
4. 关于弹窗（开发者署名 + GPL-3.0）
5. AI 编程工具调用 deepin Skills 对话记录（见「开发过程说明」）

## 安装方式

### deb 包（推荐）
```bash
sudo dpkg -i deepin-launcherdeck_0.9.5-1_amd64.deb && sudo apt-get install -f
```
deb 已通过 Debian 官方打包规范检查器 `lintian --pedantic` **零警告**验证；安装、卸载（`sudo dpkg -r deepin-launcherdeck`）、重装、启动器/任务栏图标一致均实机验证通过。

### 从源码构建
```bash
sudo apt install cmake pkg-config qt6-base-dev qt6-base-dev-tools \
  libdtk6core-dev libdtk6gui-dev libdtk6widget-dev libx11-dev libxcb1-dev
git clone https://github.com/dgr1771/deepin-launcherdeck.git
cd deepin-launcherdeck
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/deepin-launcherdeck
```

## 开发过程说明（deepin Skills 辅助开发）

本项目全程采用 AI 结对开发模式，AI 编程工具为 ZCode 与 Codex（双 AI 并行，git patch 合流）。

### deepin Skills 的使用（附调用对话记录截图）

使用了 [linuxdeepin/deepin-skills](https://github.com/linuxdeepin/deepin-skills) 仓库中的 **dtk-development** Skill，重点应用了以下模块：

1. **DBlurEffectWidget 集成**（references/widgets 索引）
   通过 Skill 指引将面板背景从自绘半透明底改为 `DBlurEffectWidget` + `BehindWindowBlend` + `AutoColor` 遮罩，实现系统级真毛玻璃，deepin 25 桌面壁纸透过面板呈现模糊质感

2. **DTK6 构建约束**（SKILL.md 关键约束）
   遵循"先确定 Qt/DTK 主版本再选包名"的要求，全程使用 DTK6/Qt 6 链路 `find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)`，未混用 DTK5/Qt 5

3. **DAboutDialog 标准关于框**（references/widgets 控件决策树）
   按 Skill 控件选型指引使用 DApplication + DAboutDialog；开发中发现 `DApplication::aboutDialog()` 在未 `setAboutDialog()` 前返回空指针直接 show() 会段错误，正确姿势是 `new DAboutDialog()` → `app.setAboutDialog(dlg)` → 配置字段（gdb 崩溃栈定位）

4. **gotchas 排查**（references/gotchas.md）
   面板失焦收起与弹窗焦点冲突问题，通过 gotchas 章节定位并修复

### AI 辅助开发亮点

- **双 AI 并行**：ZCode 与 Codex 在两台机器上独立迭代，通过 git patch 合流（1022 行补丁零冲突）
- **像素级验收**：所有 UI 改动通过远程截屏 + 像素采样（彩色像素计数/坐标偏移/中心对称性测量）验证，不依赖肉眼
- **真实 bug 发现**：AI 错误上报机制自动捕获了 QLatin1Char 截断 bug（0x2663 → 'c'）和 QFile /proc 读取 atEnd() 恒真 bug（deepin 25 + Qt 6.8 特有）
- **自动化联测**：deploy_deck.py 一键完成 sync→build→launch→shot→kill 全链路，`DECK_SELFTEST=1` 自测 24 条逻辑断言全过
- **打包规范零警告**：lintian --pedantic 从 6 Error + 2 Warning 修到零警告（补 debian/copyright、man 手册页、修 Maintainer 邮箱域名、.desktop 行尾）

### 踩坑记录

| 坑 | 根因 | 修法 |
|----|------|------|
| QDesktopServices 开 vim | deepin 上对 .desktop 文件当文本打开 | 改用 gio launch |
| QFile /proc 读不到 | Qt6.8 atEnd() 对 /proc 恒真 | 改 readAll() 解析 |
| QLatin1Char 截断 | 0x2663 & 0xFF = 'c' | 用 QChar(0x2663) |
| 任务栏图标不一致 | dde-shell 按图标名缓存旧渲染 | .desktop Icon 用绝对路径 |
| aboutDialog() 段错误 | 未 setAboutDialog 前返回 nullptr | 先 new 再 setAboutDialog |
| lintian bogus-mail-host | Maintainer 邮箱域名无效 | 改 GitHub noreply 邮箱 |

## 开源协议

GPL-3.0 —— 任何人可以自由使用、修改和分发，衍生作品须同样以 GPL-3.0 开源并署名原作者。
