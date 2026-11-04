# FLFHMCLauncher — 设计与实施计划（PLAN）

> 状态：v0.1 设计稿（仓库初始化阶段）
> 目标：用 C++ / 静态 Qt 实现 FLFHMCL 的原生启动器，支持实例管理、共享库（ichor 风格）、
> 快速启动、在线安装、更新接管与崩溃上报。

---

## 一、调研结论

### 1.1 ichor 是什么

**结论：ichor 是 Lunar Client 私有的类加载 / 模块化运行时，不是公开标准，也不在本机以独立安装形式存在。**

证据（本机 `C:\Users\HiBer2007\.lunarclient`）：

| 证据 | 说明 |
|------|------|
| `offline/multiver/genesis-0.1.0-SNAPSHOT-all.jar` | 内含 `com/moonsworth/lunar/ichor/**` 类（`IchorClassLoader`、`IchorPipeline`、模块/映射相关类，类名混淆） |
| `offline/multiver/.ichor/ichormodule-libs/` | 按 `group_artifact_version[-vendor].jar` 命名的共享库缓存，例如 `net_fabricmc_fabric_loader_0_20_1-lunar-0.1.5.jar` |
| `offline/multiver/.ichor/genesis.log` | `Found classpath URL ... with SHA-1 hash`、`Baking classes... Baked 11607 classes in 27896ms`、`Missing class cache: cache/<hash>/bake.cache` |
| `profiles/1.21/logs/ichor-boot.log` | `IchorPipeline/main` 为 `META_MIXIN`、`MIXIN` 分别加载 Mixin runtime；classloader 名为 `IchorClassLoader(META_MIXIN)`、`GenesisClassLoader(v1_21_11)` |
| `META-INF/services/com.moonsworth.lunar.ichor.*` | ServiceLoader 形式的模块入口 |

ichor 的核心能力（我们从行为上归纳，不复制其实现）：

1. **多层 ClassLoader**：meta / mixin / 游戏层分离，每层独立加载 Mixin runtime；
2. **共享库仓**：`ichormodule-libs` 用 `group_artifact_version` 扁平命名，多个实例/版本共用同一份 jar；
3. **类缓存/烘焙（bake）**：把转换后的类缓存到 `cache/<hash>/bake.cache`，加速二次启动；
4. **模块清单**：模块声明依赖的库和 Mixin 配置，由 pipeline 装配。

FLFHMCLauncher 将实现 **ichor 风格的共享库与模块装配**（下称 "FLF Ichor"）：
共享库仓 + 实例清单 + 自定义 classpath 解析；不依赖、不反编译 Lunar 的私有实现。

### 1.2 HMCL 的下载 / 更新 / 启动接口

| 用途 | HMCL 实现 | 接口 / 地址 |
|------|-----------|-------------|
| 启动器更新检查 | `UpdateChecker` + `RemoteVersion` | `GET https://hmcl.huangyuhui.net/api/update_link?version=<v>&channel=stable\|dev\|nightly[-preview]`，返回 `{version, jar, jarsha1, force}`；可用 `-Dhmcl.update_source.override=<url>` 覆盖 |
| 应用更新 | `UpdateHandler` | 新 jar 以 `--apply-to <dir>` 启动自身完成替换 |
| 关闭自动更新 | 无官方开关 | FLFHMCL agent 增加受管模式（`-Dflfhmcl.update.managed=true`）跳过 `UpdateChecker.init` |
| 原版版本列表 | `GameVersionList` / `MojangDownloadProvider` | `https://piston-meta.mojang.com/mc/game/version_manifest.json`（BMCLAPI 镜像可替换） |
| Fabric Loader | `FabricVersionList` | `https://meta.fabricmc.net/v2/versions/loader`、`.../loader/<game>/<loader>`、`.../loader/<game>/<loader>/profile/json` |
| JavaFX | `SelfDependencyPatcher` | 缺失时从 Maven Central / 腾讯云镜像下载 `org.openjfx:javafx-*:<ver>:win`；`-Dhmcl.openjfx.repo` 可覆盖 |
| Java 运行时 | `JavaManager` + `MojangJavaDownloads` | Mojang java-runtime manifest；本机已装 JRE 优先复用 |
| 直接启动实例 | HMCL 无 CLI 参数 | FLFHMCL agent 增加 `--flf-launch <instance>`，在 HMCL GUI 之前直接组装启动 |

> HMCL 主类路径：`org.jackhuang.hmcl.Main`（HMCLBoot）→ `EntryPoint` → `Launcher`。
> FLFHMCL agent 已能拦截 `Main`，因此快速启动参数由 agent 处理最自然。

### 1.3 本机工具链

| 项 | 值 |
|----|----|
| Qt（静态 + 静态 CRT） | `H:\Qt-static-rt\6.11.1\msvc2022_64`（`QT_CONFIG += static static_runtime`，推荐） |
| Qt（静态 + 动态 CRT） | `H:\Qt-static\6.11.1\msvc2022_64`（`static`，`/MD`） |
| MSVC | `F:\Program Files\Microsoft Visual Studio\18\Community`（`vcvars64.bat` 存在） |
| CMake | 4.4.3 |
| vcpkg | `H:\vcpkg`，当前仅有 `x64-windows`（spdlog、nlohmann-json 已装；**webview2 未装**） |

> 自包含目标优先 `Qt-static-rt`（`/MT`）。vcpkg 依赖需补 `x64-windows-static` 三元组
> （`spdlog`、`nlohmann-json`、`webview2`），否则与 `/MT` 混链会 CRT 冲突。

---

## 二、需求分解

| 需求 | 设计 |
|------|------|
| C++ + 静态 Qt | Qt 6.11.1 static，MSVC 2022 x64；`Qt-static-rt` 优先 |
| 子模块引入三个库 | `third_party/HiBerGUILibCPP`、`CommonLoggerCPP`、`CrashTrackerHandleLibCPP` |
| CrashTracker 自动拉取最新 Release | 启动时访问 GitHub `releases/latest`，下载 `CrashTracker.exe` 到 `.flfhmcl/tools/`，并用硬链接放到 EXE 同目录（库按同目录查找） |
| 自包含、首次运行释放 | 首次运行创建 `.flfhmcl/` 全量目录；所有配置/库/缓存都在其中；EXE 目录只放启动器与 CrashTracker 硬链接 |
| 共享库（ichor 风格） | `.flfhmcl/libraries/<group>_<artifact>_<version>[_classifier].jar` + 实例清单引用；FLFHMCL agent 解析共享路径 |
| 默认非快速启动 | `FLFHMCLauncher.exe` 打开 GUI，可配置/安装/管理实例 |
| 快速启动小 EXE | 每个实例可"释放快速启动器"：生成 `FLFQuickStart-<id>.exe` + 同目录 `quickstart.json`，直接组装启动 |
| 创建实例 | 向导：选版本 → 选加载器 → Java（扫描/下载）→ JavaFX → Fabric → HMCL/Agent → 生成实例 |
| 更新由 FLFHMCLauncher 接管 | 启动器检查 HMCL/FLFHMCL 更新并下载替换；HMCL 侧传 `-Dflfhmcl.update.managed=true` 关闭自更新 |
| 更新提示 | 启动器 GUI 内提示（有更新/更新中/失败重试） |
| 模组管理 | 实例级 mods 启用/禁用、导入、从共享库引用；`.minecraft/mods` 可为共享/私有 |
| 实例隔离（细分） | 见 §4：按 Java/JavaFX/Fabric/HMCL/库/mods/.minecraft/.hmcl 分层配置 |
| FLFHMCL 支持 | agent 增加 ichor 库解析、`--flf-launch`、受管更新开关、实例上下文 |

---

## 三、总体架构

```
FLFHMCLauncher.exe (Qt GUI)
├─ AppBootstrap       初始化 .flfhmcl、日志、崩溃处理、单实例锁
├─ ConfigStore        launcher.json / instances.json / mirrors.json
├─ InstanceManager    实例 CRUD、隔离/共享解析
├─ LibraryStore       FLF Ichor：共享库索引、哈希校验、引用计数
├─ DownloadManager    多镜像、断点续传、SHA-1/SHA-256 校验
├─ InstallWizard      版本/加载器/Java/JavaFX/HMCL/Agent 安装
├─ UpdateManager      HMCL + FLFHMCL + 自身更新
├─ LaunchAssembler    生成 java 命令行、classpath、JVM 参数
├─ QuickStartWriter   释放小型快速启动 EXE
└─ CrashIntegration   CrashTrackerHandleLib + CrashTracker.exe

FLFQuickStart-<id>.exe (Win32, 无 Qt，< 200 KB)
└─ 读取同目录 quickstart.json → 定位根目录 → 调用 LaunchAssembler 同款逻辑
```

### 3.1 进程启动链

非快速启动：

```
FLFHMCLauncher.exe
  └─ java -javaagent:FLFHMCL-agent.jar
          -Dflf.libraries.dir=.flfhmcl/libraries
          -Dflfhmcl.update.managed=true
          -Dflf.instance=<id>
          -jar .flfhmcl/hmcl/<ver>/HMCL.jar [--flf-launch <id>?]
```

- 普通启动：只带实例上下文，HMCL 打开 GUI（用户自己点启动）；
- 快捷启动：加 `--flf-launch <id>`，FLFHMCL agent 在 GUI 之前直接启动该实例。

快速启动 EXE：读 `quickstart.json`（记录根目录、实例 id、Java 路径），调用同一套
`LaunchAssembler`，不加载 Qt/不打开 GUI。

---

## 四、实例、隔离与共享（FLF Ichor）

### 4.1 分层

| 层 | 内容 | 可共享 |
|----|------|--------|
| `JAVA` | JRE/JDK 目录 | ✅ 全局/组 |
| `JAVAFX` | JavaFX jar + native | ✅ |
| `FABRIC` | Fabric loader jar/profile | ✅ |
| `HMCL` | HMCL.jar + FLFHMCL agent.jar | ✅ |
| `LIBRARIES` | 依赖库 jar（Maven 坐标扁平命名） | ✅ |
| `MODS` | 模组 jar | ✅ 全局/组/实例私有 |
| `MINECRAFT` | `.minecraft`（assets、versions、saves、config） | ✅ 全局/组/私有 |
| `HMCL_HOME` | HMCL 配置目录（`.hmcl` / user home） | ✅ 全局/组/私有 |

### 4.2 共享模式

```json
{
  "layers": {
    "java":      {"mode": "shared",  "group": "default"},
    "javafx":    {"mode": "shared"},
    "fabric":    {"mode": "shared"},
    "hmcl":      {"mode": "shared"},
    "libraries": {"mode": "shared"},
    "mods":      {"mode": "group", "group": "1.21-mods"},
    "minecraft": {"mode": "private"},
    "hmclHome":  {"mode": "group", "group": "1.21-config"}
  }
}
```

模式：`private` / `shared`（全局）/ `group`（命名组）/ `inherit`（继承另一实例）。
解析顺序：实例覆盖 → 组 → 全局 → 默认私有。

### 4.3 FLF Ichor 库解析

库文件采用 Maven 坐标扁平命名（与 Lunar 的 `ichormodule-libs` 一致）：

```
.flfhmcl/libraries/
  net_fabricmc_fabric_loader_0.19.3.jar
  org_openjfx_javafx-base_17.0.13-win.jar
  org_ow2_asm_asm_9.10.1.jar
```

实例清单只保存逻辑坐标：

```json
{"libraries": [
  {"group": "net.fabricmc", "artifact": "fabric-loader", "version": "0.19.3", "sha1": "..."},
  {"group": "org.openjfx", "artifact": "javafx-base", "version": "17.0.13", "classifier": "win"}
]}
```

FLFHMCL agent 读取清单 → 在共享库仓查找 → 校验 → 拼 classpath；同一文件被多个实例
引用只存一份。缺失时由启动器补齐（下载到共享库仓，而不是实例目录）。

---

## 五、目录布局（`.flfhmcl`）

```
.flfhmcl/
  config/
    launcher.json          # 全局设置、镜像、Java 偏好
    instances.json         # 实例索引
    mirrors.json           # BMCLAPI / 腾讯 / ghfast 等镜像
  libraries/               # FLF Ichor 共享库仓
  java/<vendor>-<version>/ # 共享 Java 运行时
  javafx/<version>/        # 共享 JavaFX
  fabric/<game>/<loader>/  # Fabric profile
  hmcl/<version>/          # HMCL.jar、FLFHMCL-agent.jar
  tools/
    CrashTracker/CrashTracker.exe
  instances/<id>/
    instance.json          # 版本、加载器、隔离配置、库清单
    minecraft/             # 仅 private 模式
    hmcl-home/             # 仅 private 模式
    mods/                  # 仅 private 模式
  cache/
    downloads/             # 断点续传临时文件
    meta/                  # 版本列表缓存（ETag/Last-Modified）
  logs/
  crash-report/
```

EXE 同目录仅保留：`FLFHMCLauncher.exe`、`FLFQuickStart-*.exe`、`CrashTracker.exe`（硬链接）。

---

## 六、创建实例向导

1. **选择版本**：拉取原版 `version_manifest.json`（或镜像），过滤 release/snapshot；
2. **选择加载器**：Fabric（Loader + Installer 版本列表）、Vanilla；
3. **Java**：扫描本机 Java（注册表/常见目录/HMCL 缓存）→ 不足则从 Mojang java-runtime 或
   Adoptium 下载匹配主版本（17/21）；
4. **JavaFX**：下载 `org.openjfx:javafx-{base,graphics,controls}:<ver>:win` 到共享库仓；
5. **HMCL + Agent**：下载 HMCL jar（更新接口或 GitHub Release）与 FLFHMCL agent jar；
6. **实例配置**：命名、隔离/共享模式、初始模组目录；
7. **写入** `.flfhmcl/instances/<id>/instance.json` 并注册到 `instances.json`。

所有下载支持：多镜像回退、断点续传、SHA-1/256 校验、失败重试与进度事件。

---

## 七、更新流程

| 对象 | 检查 | 下载 | 应用 |
|------|------|------|------|
| HMCL | `api/update_link` JSON | jar + sha1 | 写入 `hmcl/<version>/`，切换实例引用 |
| FLFHMCL | GitHub Release / 自定义 JSON | agent jar | 同上 |
| FLFHMCLauncher 自身 | GitHub Release（或 gitee 镜像） | 新 EXE | 退出后 helper 替换，失败回滚 |

- 启动器负责提示与执行；
- 启动 HMCL 时始终传 `-Dflfhmcl.update.managed=true`，FLFHMCL agent 跳过 HMCL 自更新；
- 支持"忽略此版本""稍后提醒"。

---

## 八、FLFHMCL（Java 侧）需要新增的能力

1. **受管更新开关**：`flfhmcl.update.managed=true` 时 mixin 跳过 `UpdateChecker.init()`；
2. **`--flf-launch <instance>`**：agent 在 `Main` 拦截，读取实例配置并直接调用 HMCL 的启动逻辑；
3. **FLF Ichor 库解析**：`-Dflf.libraries.dir` + 实例清单 → 组合 classpath；
4. **实例上下文**：`-Dflf.instance=<id>`，用于选择 `.minecraft` / `.hmcl` 目录；
5. 与启动器共享 `instance.json` schema（版本、加载器、库坐标、隔离模式）。

---

## 九、构建与工具链

```powershell
call "F:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

cmake -B build -S . -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="H:/Qt-static-rt/6.11.1/msvc2022_64" `
  -DCMAKE_TOOLCHAIN_FILE="H:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --config Release
```

- 目标：`FLFHMCore`（静态库）、`FLFHMCLauncher`（WIN32 GUI）、`FLFQuickStart`（Win32，无 Qt）；
- 静态 Qt 插件：`qt_import_plugins(... INCLUDE Qt::QWindowsIntegrationPlugin)`；
- HiBerGUILibCPP 的 WebView2 部分依赖 vcpkg `webview2`，未安装时用
  `FLF_ENABLE_WEBVIEW2=OFF` 只构建 `HiBerGUILibrary`；
- 产物清单：EXE + `.flfhmcl`（首次运行生成）。

---

## 十、里程碑

| 阶段 | 内容 | 验收 |
|------|------|------|
| M0 | 仓库、子模块、CMake 骨架、日志/崩溃接入 | `FLFHMCLauncher.exe` 可启动并写日志 |
| M1 | `.flfhmcl` 初始化、配置/实例存储、FLF Ichor 库索引 | 单实例配置可保存/加载 |
| M2 | 下载器 + 创建实例向导（版本/加载器/Java/JavaFX/HMCL） | 全新目录可在线装出一个可启动实例 |
| M3 | 启动装配 + FLFHMCL `--flf-launch`、受管更新 | 双击实例直接进游戏 |
| M4 | 快速启动 EXE 释放 | 生成的小 EXE 可独立启动指定实例 |
| M5 | 更新管理（HMCL/Agent/自身） | 有更新时提示并成功替换 |
| M6 | 模组管理 + 分层隔离/共享 UI | 两个实例可共享库、隔离存档 |
| M7 | 打磨、崩溃上报、文档、安装包 | 发布可用版本 |

---

## 十一、风险与决策

| 风险 | 影响 | 应对 |
|------|------|------|
| 静态 Qt 与 vcpkg 依赖 CRT 不一致 | 链接失败/运行崩溃 | 统一 `/MT`（`Qt-static-rt` + `x64-windows-static`） |
| WebView2 依赖 DLL | 破坏"单 EXE" | 默认关闭 WebView2 组件，按需启用并随包分发 loader |
| HMCL 无 CLI 启动实例 | 快速启动无法直接进游戏 | FLFHMCL agent 增加 `--flf-launch` |
| HMCL 自更新与启动器冲突 | 双更新源 | `flfhmcl.update.managed` 受管模式 |
| Lunar ichor 为闭源 | 不能直接复用 | 实现等价的共享库 + 模块装配（FLF Ichor） |
| Java/Fabric 元数据变更 | 安装失败 | 版本列表缓存 + 镜像回退 + 明确错误提示 |

---

## 十二、当前状态

- [x] 调研 ichor、HMCL 更新/下载/启动接口、本机 Qt/MSVC/vcpkg
- [x] 创建仓库与 PLAN
- [x] 子模块接入（`third_party/HiBerGUILibCPP`、`CommonLoggerCPP`、`CrashTrackerHandleLibCPP`）
- [x] M0 骨架：`.flfhmcl` 初始化、日志、崩溃处理、Qt 窗口
- [x] 首次构建通过（`H:\Qt-static` + vcpkg `x64-windows`，产物 21 MB 静态 Qt EXE + 64 KB QuickStart）
- [ ] 静态 CRT 版本（`Qt-static-rt` + `x64-windows-static`）
- [ ] CrashTracker.exe 自动拉取与硬链接
- [ ] M1 起（配置/实例/下载/启动/更新）
- [ ] FLFHMCL 侧 `--flf-launch` / 受管更新 / 共享库解析

## 十三、已知问题

| 问题 | 影响 | 处理 |
|------|------|------|
| `CommonLoggerCPP` 的模板 API 与 spdlog 2.x（`SPDLOG_USE_STD_FORMAT`）不兼容：`Info(fmt, args...)` 把运行期 `const char*` 传给 `std::format_string` | 调用 `CLogger::Info(...)` 编译失败 | 骨架里改用 `CLogger::Get()->info("字面量", args...)`；推动上游用 `SPDLOG_FMT_RUNTIME` 包一层，或固定 spdlog 1.x |
| vcpkg 当前只有 `x64-windows`（动态 CRT） | 与 `Qt-static-rt`（`/MT`）混链会 CRT 冲突；spdlog 以 DLL 形式部署 | 安装 `x64-windows-static` 的 `spdlog`/`nlohmann-json`/`webview2`，改用 `Qt-static-rt` |
| vcpkg 未安装 `webview2` | `HiBerGUIWebView2` 暂不构建 | `FLF_ENABLE_WEBVIEW2=OFF`（默认），需要时安装后开启 |
| 快速启动 EXE 目前只弹窗显示配置 | M4 前不可真正启动 | M4 接入 LaunchAssembler |
| 仓库尚未配置远端 | 无法 push | 建 GitHub 仓库后 `git remote add origin ...` |
