【deepin插件开发活动】唤启 HuanQi Launcher Deck —— 一屏全览 · 一唤即启的 DTK 原生应用启动器

## 项目简介

**唤启 HuanQi Launcher Deck** 是一个基于 DTK6 的 deepin 原生应用启动器。按下 `Ctrl+J`，本机所有已安装应用以塔罗牌卡片形式一屏铺开——悬停翻面看详情、花色分类、常用自动浮前、点击启动。同时内置一局完整的空当接龙游戏和幸运老虎机。

- **源码仓库**：https://github.com/dgr1771/deepin-launcherdeck
- **开源协议**：GPL-3.0
- **开发方向**：DTK 原生应用
- **开发者**：隔壁村布布（dgr1771）

## 功能特性

### 应用一屏模式
- 自动扫描 .desktop 条目（含 zh_CN 本地化与本地覆盖）
- 卡片网格自适应缩放：34~100+ 款应用全部完整可见
- 花色归类（♦♣♥♠★⚙✦ 七花色）+ 自建分类 + 拖拽归类
- 悬停 3D 翻面 + 像素级牌意解读浮层
- 常用自动浮前（次数×时间衰减）+ 手动置顶
- 拼音首字母搜索 · 全键盘导航（方向键选牌+Enter 启动）

### 空当接龙游戏模式
- 微软原版发牌算法（LCG，1-32000 局全可通关）
- 标准规则：红黑交替降序、序列移动、自由位/回收位
- 发牌动画：三段抖洗→逐张飞出→落地翻面（win 版逐帧复刻）
- 进度自动保存 · 难度选择（初级/中级/经典）

### 幸运老虎机
- 五轴卷轴 · 金币系统 · 头奖庆祝 · 每小时限玩

### 其他
- 四套配色预设 + 透明度滑条 + 接龙牌背主题
- 托盘常驻 + Ctrl+J 全局热键（XGrabKey 独立 xcb 连接）
- DBlurEffectWidget 系统级真毛玻璃
- 渲染错误上报 · 日志轮转 · 单实例锁

## 截图

（见帖子附件或下方链接）
1. 应用一屏主界面（塔罗卡阵+花色分类）
2. 空当接龙游戏模式（发牌完毕+应用名牌面）
3. 外观设置弹窗（四套预设+透明度滑条）
4. 拼音搜索（输入 db 出豆包）

## 安装方式

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

### deb 包
从 [Releases](https://github.com/dgr1771/deepin-launcherdeck/releases) 下载后 `sudo dpkg -i *.deb && sudo apt-get install -f`

## 开发过程说明（deepin Skills 辅助开发）

本项目全程采用 AI 结对开发模式，AI 编程工具为 ZCode（GLM-4）与 Codex。

### deepin Skills 的使用

使用了 [linuxdeepin/deepin-skills](https://github.com/linuxdeepin/deepin-skills) 仓库中的 **dtk-development** Skill，重点应用了以下模块：

1. **DBlurEffectWidget 集成**（references/widgets/blur-effect.md）
   通过 Skill 指引将面板背景从自绘半透明底改为 `DBlurEffectWidget` + `BehindWindowBlend` + `AutoColor` 遮罩，实现系统级真毛玻璃效果，Deepin 25 桌面壁纸透过面板呈现模糊质感

2. **DTK6 构建约束**（SKILL.md 关键约束）
   遵循"先确定 Qt/DTK 主版本再选包名"的要求，全程使用 DTK6/Qt 6 链路 `find_package(Dtk6 REQUIRED COMPONENTS Core Gui Widget)`，未混用 DTK5/Qt 5

3. **gotchas 排查**（references/gotchas.md）
   面板失焦收起与弹窗焦点冲突问题，通过 gotchas 的"QEvent::WindowDeactivate 与模态弹窗共存"章节定位并修复

### AI 辅助开发亮点

- **双 AI 并行**：ZCode（GLM-4）与 Codex 在两台机器上独立迭代，通过 git patch 合流（1022 行补丁零冲突）
- **像素级验收**：所有 UI 改动通过 CDP 远程截屏 + PowerShell 像素采样（彩色像素计数/坐标偏移/中心对称性测量）验证，不依赖肉眼
- **真实 bug 发现**：AI 错误上报机制自动捕获了 QLatin1Char 截断 bug（0x2663 → 'c'）和 QFile /proc 读取 atEnd() 恒真 bug（Deepin 25 + Qt 6.8 特有）
- **自动化联测**：deploy_deck.py 一键完成 sync→build→launch→shot→kill 全链路，自测脚本 DECK_SELFTEST=1 跑 24 条逻辑断言

### 踩坑记录

| 坑 | 根因 | 修法 |
|----|------|------|
| QDesktopServices 开 vim | deepin 上 QDesktopServices 对 .desktop 文件当文本 | 改用 gio launch |
| QFile /proc 读不到 | Qt6.8 atEnd() 对 /proc 恒真 | 改 readAll() 解析 |
| QLatin1Char 截断 | 0x2663 & 0xFF = 'c' | 用 QChar(0x2663) |
| 拼包 EBUSY | 工作区文件监视器锁新写文件 | 输出到工作区外 |
| image2 输入不限帧数 | -frames:v 放错位置 | 紧贴 -i 之前 |
| yuvj420p concat 报错 | JPEG 全量程与常规不一致 | 归一化 setsar=1,format=yuv420p |

## 开源协议

GPL-3.0 —— 任何人可以自由使用、修改和分发，衍生作品须同样以 GPL-3.0 开源并署名原作者。
