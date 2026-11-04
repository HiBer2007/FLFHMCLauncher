#pragma once

#include "core/workspace.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace flf {

struct LaunchPlan {
    std::string program;
    std::vector<std::string> arguments;
    fs::path workingDir;
    /// 实际使用的启动协议（legacy-jar / knot），便于诊断。
    std::string provider;
};

LaunchPlan buildLaunchPlan(const Instance& instance, bool quickStart);

bool launchInstance(const Instance& instance, std::string* error, bool quickStart = false);
bool validateInstanceFiles(const Instance& instance, std::string* error);

/// 持有子进程：读取合并后的 stdout/stderr，退出时回调；不随启动器退出而终止。
bool launchInstanceManaged(const Instance& instance,
                           const std::function<void(const std::string&)>& onOutput,
                           const std::function<void(int, const std::string&)>& onExit,
                           std::string* error);

/// 释放快速启动器：复制 FLFQuickStart.exe + quickstart.json 到 outDir。
bool writeQuickStart(const Instance& instance, const fs::path& outDir, std::string* error);

} // namespace flf
