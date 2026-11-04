#pragma once

#include <filesystem>

namespace flf {

/// 启动器 EXE 所在目录。
std::filesystem::path executableDirectory();

/// `.flfhmcl` 根目录（EXE 同目录下），不存在时返回期望路径。
std::filesystem::path flfHome();

/// 首次运行创建 `.flfhmcl` 目录骨架，返回根目录。
std::filesystem::path ensureLayout();

/// 日志目录 / 日志文件路径。
std::filesystem::path logFile();

} // namespace flf
