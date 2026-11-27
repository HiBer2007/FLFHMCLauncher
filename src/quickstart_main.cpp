#include "core/quickstart_config.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
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

/// 引号化单个 Windows argv 元素。
std::wstring quoteArg(const std::wstring& value) {
    if (value.empty()) {
        return L"\"\"";
    }
    bool needQuote = false;
    for (wchar_t c : value) {
        if (c == L' ' || c == L'\t' || c == L'"') {
            needQuote = true;
            break;
        }
    }
    if (!needQuote) {
        return value;
    }
    std::wstring result = L"\"";
    std::size_t backslashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
            backslashes = 0;
            continue;
        }
        result.append(backslashes, L'\\');
        backslashes = 0;
        result.push_back(c);
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

/// 每次启动时按当前模组目录重建 -Dfabric.addMods，不依赖创建快速启动器时的快照。
std::wstring refreshMods(const flf::QuickStartConfig& config,
                         const std::wstring& originalCommand) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(originalCommand.c_str(), &argc);
    if (argv == nullptr || argc <= 0) {
        return originalCommand;
    }
    const std::wstring prefix = L"-Dfabric.addMods=";
    bool changed = false;
    std::wstring replacement;
    for (int i = 0; i < argc; ++i) {
        std::wstring argument = argv[i];
        if (argument.rfind(prefix, 0) != 0) {
            continue;
        }
        const std::wstring oldValue = argument.substr(prefix.size());
        const std::size_t separator = oldValue.find(L';');
        const std::wstring agent = separator == std::wstring::npos
                ? oldValue : oldValue.substr(0, separator);
        if (agent.empty()) {
            break;
        }

        const std::filesystem::path modsDir = !config.modsDir.empty()
                ? config.modsDir
                : config.launcherRoot / L".flfhmcl" / L"mods" / widen(config.instance);
        std::vector<std::filesystem::path> jars;
        std::error_code ec;
        if (std::filesystem::is_directory(modsDir, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(modsDir, ec)) {
                if (!entry.is_regular_file(ec)) {
                    continue;
                }
                if (entry.path().extension() == L".jar") {
                    jars.push_back(entry.path());
                }
            }
        }
        std::sort(jars.begin(), jars.end());

        std::wstring value = agent;
        for (const auto& jar : jars) {
            value += L";";
            value += jar.wstring();
        }
        replacement = prefix + value;
        argv[i] = replacement.data();
        changed = true;
        break;
    }

    std::wstring command;
    for (int i = 0; i < argc; ++i) {
        if (!command.empty()) {
            command.push_back(L' ');
        }
        command += quoteArg(argv[i] == nullptr ? L"" : argv[i]);
    }
    LocalFree(argv);
    return changed ? command : originalCommand;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    flf::QuickStartConfig config;
    std::string error;
    if (!flf::loadQuickStartConfig(std::filesystem::path(exePath), config, error)) {
        MessageBoxW(nullptr, widen(error).c_str(), L"FLFQuickStart", MB_ICONERROR);
        return 1;
    }

    if (config.command.empty()) {
        const std::wstring message = L"快速启动配置缺少 command：\n\n实例: "
                + widen(config.instance);
        MessageBoxW(nullptr, message.c_str(), L"FLFQuickStart", MB_ICONERROR);
        return 1;
    }

    std::wstring commandLine = refreshMods(config, widen(config.command));
    std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
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
