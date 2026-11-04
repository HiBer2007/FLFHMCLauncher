#include "core/quickstart_config.h"

#include <windows.h>

#include <string>

namespace {

std::wstring widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(),
                                         static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                        result.data(), size);
    return result;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    const auto slash = exeDir.find_last_of(L'\\');
    if (slash != std::wstring::npos) {
        exeDir.resize(slash);
    }

    flf::QuickStartConfig config;
    std::string error;
    if (!flf::loadQuickStartConfig(std::filesystem::path(exeDir), config, error)) {
        MessageBoxW(nullptr, widen(error).c_str(), L"FLFQuickStart", MB_ICONERROR);
        return 1;
    }

    // TODO(M4): 组装 java 命令行并启动实例，见 PLAN.md。
    const std::wstring message = L"FLFQuickStart\n\n实例: " + widen(config.instance) +
                                 L"\n根目录: " + config.launcherRoot.wstring();
    MessageBoxW(nullptr, message.c_str(), L"FLFQuickStart", MB_ICONINFORMATION);
    return 0;
}
