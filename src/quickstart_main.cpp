#include "core/quickstart_config.h"

#include <windows.h>

#include <string>
#include <vector>

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

    if (config.command.empty()) {
        const std::wstring message = L"quickstart.json 缺少 command：\n\n实例: "
                + widen(config.instance);
        MessageBoxW(nullptr, message.c_str(), L"FLFQuickStart", MB_ICONERROR);
        return 1;
    }

    std::vector<wchar_t> buffer(config.command.begin(), config.command.end());
    buffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const std::wstring workingDir = widen(config.workingDir);
    const BOOL ok = CreateProcessW(
            nullptr,
            buffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            workingDir.empty() ? nullptr : workingDir.c_str(),
            &startup,
            &process);
    if (!ok) {
        MessageBoxW(nullptr, L"启动实例失败（CreateProcessW）", L"FLFQuickStart", MB_ICONERROR);
        return 1;
    }

    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}
