# 唤启 HuanQi Launcher Deck

> 所有应用，一屏全览，一唤即启。deepin / UOS 双平台原生应用启动器（DTK6/Qt6 · DTK5/Qt5）。

![唤启主界面](contest-shots/1-main.png)

![应用升级中心](docs/screenshot-upgrade.png)
*⬆ 应用升级中心：可升级应用红点提醒，勾选后一键全部静默升级（不启动程序）*

## 功能介绍

**唤启**是一个托盘常驻的桌面应用启动器。按下 `Ctrl+J`，所有已安装的应用以卡片形式一屏铺开——悬停翻面看详情、花色分类、常用自动浮前、点击启动。

### 应用一屏模式

- 自动扫描 `/usr/share/applications` 等目录下的全部 .desktop 条目（含本地覆盖与 zh_CN 本地化）
- 卡片网格自适应缩放：无论 34 款还是 100+ 款，全部完整可见于同一屏
- 花色分类：♦社交通讯 / ♣影音游戏 / ♥浏览器 / ♠开发工具 / ★效率工具 / ⚙系统组件 / ✦未名之牌，支持自建分类
- 悬停翻面查看应用详情与描述
- 常用自动浮前 + 手动置顶
- 全键盘导航：方向键选牌 · Enter 启动

### 应用升级中心（⬆ 新增）

- 自动检测本机可升级的应用：deepin 25 走 apt 数据源（兼容中文本地化输出），Windows 版走 winget
- 可升级应用**卡片红点提醒** + 工具栏「⬆ 升级 N」数量角标
- 升级中心列表勾选，**一键全部静默升级——不启动任何程序**（deepin 走 pkexec 授权，一次密码带全部包）
- 「忽略此版本」不再提醒；升级完成红点自动消失
- 平台细节：winget 退出码非零时自动复核清单定性；deepin 版 apt 定义为空的裁剪环境自动降级

### 空当接龙游戏模式

- 微软原版发牌算法（1-32000 局，局局可通关）
- 标准规则：红黑交替降序、序列移动、自由位/回收位
- 进度自动保存，重启不丢局
- 发牌动画：三段抖洗 + 逐张飞出 + 落地翻面
- 难度选择：初级（快速局）/ 中级（练习局）/ 经典（随机局）

### 其他特性

- 托盘常驻 + `Ctrl+J` 全局热键唤起
- 四套配色预设 + 透明度滑条 + 接龙牌背主题
- 老虎机小游戏（金币系统 / 头奖庆祝 / 每小时限玩）
- 系统级真毛玻璃（DBlurEffectWidget BehindWindowBlend）
- 「关于唤启」标准关于弹窗（DAboutDialog，开发者署名）
- 单实例 / 渲染错误上报 / 日志自动轮转

## 双平台支持

| 平台 | 桌面环境 | Qt/DTK | 升级数据源 | 状态 |
|------|---------|--------|-----------|------|
| deepin 25 | DDE | Qt6 / DTK6 | apt | ✅ 主力平台 |
| UOS 专业版 20（eagle） | DDE | Qt5 / DTK5 | apt | ✅ 已验证（企业授权环境实测） |

CMake 自动探测 Qt6/DTK6，缺失时回退 Qt5/DTK5（pkg-config 变量链接），一份代码双端构建。

## 安装方式

### 从源码构建

**deepin 25（Qt6/DTK6）：**

```bash
sudo apt install cmake pkg-config qt6-base-dev qt6-base-dev-tools \
  libdtk6core-dev libdtk6gui-dev libdtk6widget-dev libx11-dev libxcb1-dev
git clone https://github.com/dgr1771/deepin-launcherdeck.git
cd deepin-launcherdeck
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cp build/deepin-launcherdeck /usr/bin/
```

**UOS 专业版 20 / eagle（Qt5/DTK5）：**

```bash
sudo apt install cmake pkg-config qtbase5-dev \
  libdtkcore-dev libdtkgui-dev libdtkwidget-dev libx11-dev libxcb1-dev
git clone https://github.com/dgr1771/deepin-launcherdeck.git
cd deepin-launcherdeck
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cp build/deepin-launcherdeck /usr/bin/
```

CMake 自动探测：有 Qt6/DTK6 用 Qt6/DTK6，否则回退 Qt5/DTK5。

### deb 包安装

从 [Releases](https://github.com/dgr1771/deepin-launcherdeck/releases) 下载 .deb 后：

```bash
sudo dpkg -i deepin-launcherdeck_*.deb
sudo apt-get install -f
```

> UOS 专业版若开了「应用签名校验」，未签名 deb 会被拒装——到 安全中心 → 安全工具 → 应用安全 调整后再装。

## 使用说明

| 操作 | 效果 |
|------|------|
| `Ctrl+J` | 唤起 / 收起应用一屏 |
| 悬停卡片 | 翻面查看详情 |
| 单击卡片 | 启动应用 |
| 右键卡片 | 归类 / 置顶 / 升级（有新版本时）/ 立即启动 |
| 方向键 + 回车 | 键盘选牌启动 |
| `⬆ 升级 N` 按钮 | 打开应用升级中心，勾选一键升级 |
| `🎮` 按钮 | 切换空当接龙游戏模式 |
| `⚙` 按钮 | 外观设置（配色/透明度/牌背） |
| 托盘右键 | 展开一屏 / 重新扫描 / 检查应用升级 / 退出 |

## 开发过程说明

本项目全程采用 AI 结对开发模式。AI 编程工具为 ZCode（GLM）与 Codex，开发过程如下：

### deepin Skills 的使用

开发过程中使用了 [linuxdeepin/deepin-skills](https://github.com/linuxdeepin/deepin-skills) 仓库中的 **dtk-development** Skill：

- **DBlurEffectWidget 集成**：通过 Skill 中 `references/widgets/blur-effect.md` 的指引，将面板背景从自绘半透明底改为 `DBlurEffectWidget` + `BehindWindowBlend` + `AutoColor` 遮罩，实现系统级真毛玻璃效果
- **DTK 主版本约束**：遵循 Skill 中"先确定 Qt/DTK 主版本再选包名"的约束；后为兼容 UOS 专业版扩展为双平台自动探测（Qt6/DTK6 优先，Qt5/DTK5 回退）
- **DAboutDialog 标准关于框**：按 Skill 控件选型使用 DAboutDialog（含 aboutDialog() 空指针段错误的定位与正确用法）
- **gotchas 排查**：面板失焦收起与弹窗焦点冲突问题，通过 Skill `references/gotchas.md` 的"QEvent::WindowDeactivate 与模态弹窗共存"章节定位并修复

### AI 辅助开发要点

- **双 AI 并行**：ZCode（GLM）负责核心功能开发；Codex 在另一台机器上独立迭代，通过 git 补丁方式合流（1022 行补丁零冲突）
- **逐帧动画复刻**：AI 逐帧分析 Windows 原版的洗牌/发牌动画时序参数，精确复刻为 Qt QVariantAnimation + QTimer 33ms 时间轴
- **像素级验收**：所有 UI 改动通过远程截屏 + 像素采样验证，不依赖肉眼
- **远程联测**：`deploy_deck.py` 一键完成 sync→build→launch→shot→kill 全链路，无需手动 SSH
- **打包规范**：deb 通过 `lintian --pedantic` 零警告；安装/卸载/升级全链路实机验证

### 踩坑记录

| 坑 | 根因 | 修法 |
|----|------|------|
| QDesktopServices 开 vim | deepin 上 QDesktopServices 对 .desktop 文件当文本打开 | 改用 gio launch |
| /proc readLine 读不到 | Qt6.8 下 QFile atEnd() 对 /proc 恒真 | 改 readAll() 解析 |
| QLatin1Char 截断 | 0x2663 & 0xFF = 'c' | 用 QChar(0x2663) |
| 任务栏图标不一致 | dde-shell 按图标名缓存旧渲染 | .desktop Icon 用绝对路径 |
| aboutDialog() 段错误 | 未 setAboutDialog 前返回 nullptr | 先 new 再 setAboutDialog |
| lintian bogus-mail-host | Maintainer 邮箱域名无效 | 改 GitHub noreply 邮箱 |
| UOS Qt5.11 无导入目标 | 裁剪版 Qt 不生成 Qt5::Core 等 cmake target | pkg-config 变量链接 |
| QProcess finished 不回调 | UOS DTK5 环境下异步信号偶发不触发 | popen + 后台线程 + 队列回主线程 |
| apt 中文输出解析失败 | UOS apt 本地化为"可从该版本升级：" | 正则兼容中英文措辞 |

## 开源协议

本项目采用 **GPL-3.0** 协议开源。任何人可以自由使用、修改和分发，衍生作品须同样以 GPL-3.0 开源并署名原作者。

## 开发人员

**隔壁村布布**（GitHub: [dgr1771](https://github.com/dgr1771)）
