#include "core/app_bootstrap.h"

#include "core/flf_paths.h"

#include <crash_reporter.h>
#include <logger.h>

#include <string>

namespace flf {

void bootstrap() {
    const auto root = ensureLayout();

    CLogger::Init(logFile().string(), "flfhmlauncher");
    CLogger::Info("FLFHMCLauncher starting, home={}", root.string());

    HiBerCTM::InstallCrashHandler(root.string());
    HiBerCTM::SetCrashAppName("FLFHMCLauncher");
    HiBerCTM::SetCrashHelpText(
        "请把 crash-report 目录打包反馈到 FLFHMCL 仓库 Issues，并附上 launcher.log。");
    HiBerCTM::InstallCrtReportHook();
}

void shutdown() {
    CLogger::Info("FLFHMCLauncher exiting");
}

} // namespace flf
