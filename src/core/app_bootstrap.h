#pragma once

namespace flf {

/// 初始化日志、崩溃处理、`.flfhmcl` 目录骨架。
/// 必须在 main 的最开始调用一次。
void bootstrap();

/// 关闭日志（进程退出前调用）。
void shutdown();

} // namespace flf
