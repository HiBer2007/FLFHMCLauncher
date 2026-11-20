# FLFHMCLauncher

FLFHMCL 的**外壳 / 高阶入口**（C++ / 静态 Qt），不是 FLFHMCL 的核心。

- FLFHMCL 负责给 HMCL 提供 Fabric 运行时；本启动器负责把运行环境、实例和启动流程包装起来；
- 提供环境配置、快速启动、版本隔离、共享库缓存等高层能力；
- 默认打开 GUI 管理实例、安装、更新、模组；模组支持多选和批量拖入；
- 支持 `.flfhmclpack` 整合包：拖拽导入/导出实例的 `.minecraft`、`.hmcl`、模组与运行时元数据；
- 可为指定实例导出带图标的快速启动 EXE：默认名 `launch-<实例名>.exe`，保存时可改名、选择保存位置；
- 共享库 / 分层隔离由 mclhr 实现（参考 Lunar Client 的 ichor 思路；<https://github.com/HiBer2007/mclhr>）；
- 集成日志（CommonLoggerCPP）、崩溃捕获（CrashTrackerHandleLibCPP）、GUI 组件（HiBerGUILibCPP）。

> 即使不使用 FLFHMCLauncher，也可以直接通过 Agent 启动 FLFHMCL；本启动器是可选外壳。

设计、调研与里程碑见 [PLAN.md](PLAN.md)，ichor 调研见 [docs/ICHOR.md](docs/ICHOR.md)，整合包格式见 [docs/MODPACK.md](docs/MODPACK.md)。

## 构建

```powershell
pwsh -NoProfile -File scripts\build.ps1
```

`scripts/build.ps1` 会构建 GUI、快速启动器，并调用 `package-release` 生成自包含的
`build/FLFHMCLauncher.exe`：

- 发布版只有一个 EXE，**不需要随包放 spdlog / VC++ runtime**；
- 首次运行把 GUI、`FLFQuickStart.exe`、spdlog 和 VC runtime 释放到
  `<启动器目录>/.flfhmcl/bin`，之后再启动 GUI；
- 引导器本身使用 `/MT`，只依赖 Windows 系统 DLL。

手动构建：

```powershell
call "F:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

cmake -B build -S . -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="H:/Qt-static/6.11.1/msvc2022_64" `
  -DCMAKE_TOOLCHAIN_FILE="H:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build --config Release
cmake --build build --target package-release
```

## 子模块

```powershell
git submodule update --init --recursive
```

- `third_party/HiBerGUILibCPP`
- `third_party/CommonLoggerCPP`
- `third_party/CrashTrackerHandleLibCPP`

CrashTracker 分析工具（`CrashTracker.exe`）在首次运行时从
[HiBer2007/CrashTracker](https://github.com/HiBer2007/CrashTracker) 的最新 Release 拉取，
释放到 `.flfhmcl/tools/`。
