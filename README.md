# FLFHMCLauncher

FLFHMCL 的原生启动器（C++ / 静态 Qt）。

- 默认非快速启动：打开 GUI，管理实例、安装、更新、模组；
- 支持为指定实例释放小型快速启动 EXE；
- 共享库 / 分层隔离（FLF Ichor，参考 Lunar Client 的 ichor 思路）；
- 集成日志（CommonLoggerCPP）、崩溃捕获（CrashTrackerHandleLibCPP）、GUI 组件（HiBerGUILibCPP）。

设计、调研与里程碑见 [PLAN.md](PLAN.md)，ichor 调研见 [docs/ICHOR.md](docs/ICHOR.md)。

## 构建

```powershell
call "F:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

cmake -B build -S . -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="H:/Qt-static-rt/6.11.1/msvc2022_64" `
  -DCMAKE_TOOLCHAIN_FILE="H:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static

cmake --build build --config Release
```

> 当前 vcpkg 仅安装 `x64-windows`（动态 CRT）。要么安装 `x64-windows-static`
> 三元组的 `spdlog` / `nlohmann-json` / `webview2` 并使用 `Qt-static-rt`（`/MT`），
> 要么改用 `H:\Qt-static`（`/MD`）+ `x64-windows`。

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
