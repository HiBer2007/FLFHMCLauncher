#include "core/app_bootstrap.h"

#include "core/flf_paths.h"

#include <crash_reporter.h>
#include <logger.h>

#include <string>

namespace flf {

void bootstrap() {
    const auto root = ensureLayout();

    CLogger::Init(logFile().string(), "flfhmlauncher");
    // CommonLoggerCPP 的模板 API 与 spdlog 2.x 的编译期格式串不兼容，
    // 这里通过 Get() 直接使用 spdlog；见 PLAN.md「已知问题」。
    if (auto logger = CLogger::Get()) {
        logger->info("FLFHMCLauncher starting, home={}", root.string());
    }

    HiBerCTM::InstallCrashHandler(root.string());
    HiBerCTM::SetCrashAppName("FLFHMCLauncher");
    HiBerCTM::SetCrashHelpText(
        "请把 crash-report 目录打包反馈到 FLFHMCL 仓库 Issues，并附上 launcher.log。");
    HiBerCTM::InstallCrtReportHook();
}

void shutdown() {
    if (auto logger = CLogger::Get()) {
        logger->info("FLFHMCLauncher exiting");
    }
}

} // namespace flf
