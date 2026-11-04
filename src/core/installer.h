#pragma once

#include "core/workspace.h"

#include <cstdint>
#include <functional>
#include <string>

namespace flf {

using ProgressFn = std::function<void(const std::string& step, int percent)>;

/// 下载/安装 HMCL jar（含 SHA-1 校验）。返回是否成功。
bool downloadHmcl(const std::function<bool(std::int64_t, std::int64_t)>& progress,
                  std::string* error);

/// 确保 Agent jar 存在（从启动器目录/配置路径复制）。
bool ensureAgent(std::string* error);

/// 创建一个可启动的实例：Java、JavaFX、Fabric、HMCL、Agent、目录与清单。
bool installInstance(Instance& instance, const ProgressFn& progress, std::string* error);

/// 首次运行释放 CrashTracker.exe（GitHub 最新 Release），并放到启动器同目录。
bool ensureCrashTracker(const std::function<bool(std::int64_t, std::int64_t)>& progress,
                        std::string* error);

} // namespace flf
