# FLFHMCL 2:30 宣传片 · 分镜

- 片长：150 秒
- 画幅：1920×1080 / 30fps
- 配色：亮色蓝紫
- 音乐：Dragonflame — Kirara Magic（15s–165s）
- 旁白：无
- 主角：**FLFHMCL —— 给 HMCL 提供 Fabric 运行时**
- 配角：FLF API（HMCL 的 Fabric API 式扩展层）、mclhr（底层库解析技术手段）
- FLFHMCLauncher：仅作为**外壳**出现一小段，不抢主体

---

## 01 启幕｜0:00–0:10

**画面**
- FLFHMCL 图标 + 呼吸圆环；
- 「FLFHMCL」；
- 「为 HMCL 提供 Fabric 运行时」；
- 「HMCL 原生没有扩展接口；FLFHMCL 用 Fabric 补上运行时」；
- 标签：Fabric 运行时 / FLF API / 跨版本维护。

**动效**
- Logo 弹性缩放；
- 圆环扩散；
- 文字遮罩揭示；
- 标签逐拍弹入。

---

## 02 问题｜0:10–0:26

**画面**
- 「为什么 HMCL 版本一换，组件就崩？」；
- 三张卡：
  - 版本分叉：3.14 / 3.15 / 3.16 / 3.17 内部结构不同
  - 组件易碎：为 3.16 编译，换 3.17 就启动失败
  - 安装繁琐：Java、Fabric、依赖、资源
- 结语：「FLFHMCL 先给 HMCL 一个 Fabric 运行时，跨版本才成为可维护的问题」。

---

## 03 FLFHMCL 主体｜0:26–0:44

**画面**
- 真实 HMCL 3.16.4 界面，标题栏带 `FLFHMCL已修改`；
- 标题「给 HMCL 一个 Fabric 运行时」；
- 副标题「让 Fabric mod 加载、用 Mixin 介入，并通过 FLF API 提供扩展接口」；
- 右侧三点：
  - **Fabric 运行时**：HMCL 成为 Fabric `GameProvider`，mod 正常加载
  - **FLF API**：类似 Fabric API 的 HMCL 扩展层
  - **跨版本维护**：抽象层隔离 HMCL 内部变化，不是目标

**动效**
- HMCL 截图从右侧滑入；
- 三点卡片逐个浮入；
- 截图带阴影、圆角、蓝色描边。

---

## 04 跨版本｜0:44–1:02

**画面**
- 「一个 FLFHMCL，四个 HMCL 大版本」；
- 四张版本卡：3.14 / 3.15 / 3.16 / 3.17；
- 显示补丁版本与「已适配」；
- 底部蓝色徽章：FLFHMCL 2.1.6；
- 副标题明确：「跨版本兼容只是维护手段，不是 FLFHMCL 的目的」。

**动效**
- 版本卡回弹入场；
- 适配标签点亮；
- 徽章随节拍呼吸。

---

## 05 FLF API｜1:02–1:20

**画面**
- 「FLF API：HMCL 的扩展层」；
- 副标题「类似 Fabric API：HMCL 本身没有扩展接口，FLF API 补上事件、注册表与抽象层」；
- 六张组件卡：
  - FLF API：事件总线 / 注册表
  - UI API：界面扩展
  - Account：账号接入
  - Download：下载源
  - Modpack：整合包
  - flf-title：标题标识
- 底部：「让 mod 不必绑死 HMCL 内部类，跨版本为 HMCL 提供服务」。

**动效**
- 卡片网格逐张浮入；
- 顶部色条生长；
- 底部文案随拍出现。

---

## 06 自带运行时｜1:20–1:36

**画面**
- 「自带运行时，下载即用」；
- 四层：Java 25 / JavaFX / Fabric Loader / Libraries；
- 底部：自动检测 / 自动下载 / 自动解析 / 自动修复。

**动效**
- 分层卡片依次落下；
- 色条脉冲；
- 四个自动能力逐拍点亮。

---

## 07 mclhr｜1:36–1:52

**画面**
- 「mclhr」；
- 「一份库，多个实例共用」；
- 三份库：JavaFX / Fabric / 依赖库；
- 三个实例节点：实例 A / B / C；
- 连线与流光；
- 底部：「自动解析 · 共享缓存 · 跨实例复用」。

**说明**
- mclhr 是底层技术手段，不是产品核心；它为 FLFHMCL / FLFHMCLauncher 提供库解析与共享能力。

**动效**
- 库卡片逐个落下；
- 连线生长；
- 光点沿连线移动；
- 实例节点依次亮起。

---

## 08 崩溃接管｜1:52–2:06

**画面**
- 「崩溃了，也看得懂」；
- 异常窗口：HMCL 异常退出、重复 Fabric Loader、退出代码 1；
- 建议更新 FLFHMCL 2.1.6；
- 按钮：修复实例 / 查看报告；
- 右侧 crash-report 卡片与一键反馈。

**动效**
- 窗口上浮；
- 报告卡滑入；
- 状态标签依次出现。

---

## 09 FLFHMCLauncher（外壳，小段）｜2:06–2:18

**画面**
- 「FLFHMCLauncher：外壳，不是核心」；
- 「环境配置 · 快速启动 · 版本隔离 · 缓存」；
- 启动器截图；
- 三点：
  - 环境配置：Java、HMCL、FLFHMCL 统一准备
  - 快速启动：桌面快捷方式，双击即玩
  - 版本隔离 / 缓存：实例独立，共享库复用
- 副标题：「不用它也能直接启动 FLFHMCL；它只是把常用操作包了一层」。

---

## 10 开源与参考｜2:18–2:30

**画面**
- 「开源与参考」；
- 副标题：「HMCL 原生没有扩展接口；FLFHMCL 用 Fabric 补上运行时，这些项目提供了基础」；
- 列表：
  - FLFHMCL — github.com/HiBer2007/FLFHMCL — LGPL-2.1
  - mclhr — github.com/HiBer2007/mclhr — LGPL-2.1
  - HMCL — github.com/HMCL-dev/HMCL — GPL-3.0
  - Fabric Loader — fabricmc.net — Apache-2.0
  - Metadata — metadata.hibernet.top
  - CrashTracker — github.com/HiBer2007/CrashTracker
- 底部音乐署名：Dragonflame — Kirara Magic。

---

## 节奏

- BPM 约 107.67，每拍约 0.557 秒；
- 场景切换点：9.8 / 25.8 / 43.8 / 62.0 / 80.2 / 96.0 / 112.0 / 126.0 / 138.0 秒；
- 白色短闪 + 每拍微闪；
- 卡片/徽章按拍脉冲。

> 全片只有 2:06–2:18 这 12 秒属于 FLFHMCLauncher，且明确标注“外壳 / 可选”。
