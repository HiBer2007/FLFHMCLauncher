#pragma once

#include "core/workspace.h"

#include <filesystem>
#include <string>
#include <vector>

namespace flf {

struct LaunchPlan {
    std::string program;
    std::vector<std::string> arguments;
    fs::path workingDir;
};

LaunchPlan buildLaunchPlan(const Instance& instance, bool quickStart);

bool launchInstance(const Instance& instance, std::string* error, bool quickStart = false);

/// 释放快速启动器：复制 FLFQuickStart.exe + quickstart.json 到 outDir。
bool writeQuickStart(const Instance& instance, const fs::path& outDir, std::string* error);

} // namespace flf
