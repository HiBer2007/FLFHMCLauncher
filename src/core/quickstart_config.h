#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace flf {

/// 快速启动 EXE 同目录的 quickstart.json。
struct QuickStartConfig {
    std::filesystem::path launcherRoot;   // 启动器根目录（含 .flfhmcl）
    std::string instance;                 // 实例 id
    std::string java;                     // 可选：java.exe
    std::string command;                  // 完整命令行（由启动器生成）
    std::string workingDir;               // 工作目录
    std::vector<std::string> extraArgs;   // 可选：附加 JVM 参数
};

/// 读取指定 EXE 同目录的 quickstart.json；失败返回 false 并写入 error。
bool loadQuickStartConfig(const std::filesystem::path& exeDir,
                          QuickStartConfig& out,
                          std::string& error);

} // namespace flf
