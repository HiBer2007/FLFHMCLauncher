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
        "Please attach this crash-report directory and launcher.log when filing an issue at the FLFHMCL repository.");
    HiBerCTM::InstallCrtReportHook();
}

void shutdown() {
    CLogger::Info("FLFHMCLauncher exiting");
}

} // namespace flf
