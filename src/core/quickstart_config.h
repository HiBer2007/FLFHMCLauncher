#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace flf {

/// 快速启动配置。优先从 EXE 末尾的内嵌载荷读取，旧文件仍兼容同目录 quickstart.json。
struct QuickStartConfig {
    std::filesystem::path launcherRoot;   // 启动器根目录（含 .flfhmcl）
    std::string instance;                 // 实例 id
    std::string java;                     // 可选：java.exe
    std::string command;                  // 完整命令行（由启动器生成）
    std::string workingDir;               // 工作目录
    std::vector<std::string> extraArgs;   // 可选：附加 JVM 参数
};

/// 把 JSON 载荷追加到 EXE 末尾；覆盖已有的 FLFQS01 载荷。
bool appendQuickStartConfig(const std::filesystem::path& exePath,
                            const std::string& json,
                            std::string& error);

/// 从指定 EXE 读取内嵌配置；没有载荷时回退同目录 quickstart.json。
bool loadQuickStartConfig(const std::filesystem::path& exePath,
                          QuickStartConfig& out,
                          std::string& error);

} // namespace flf
