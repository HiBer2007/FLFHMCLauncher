#include "core/quickstart_config.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
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

std::wstring readTextFile(const std::filesystem::path& path, std::size_t maxBytes) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return {};
    }
    const std::streamoff end = in.tellg();
    if (end <= 0) {
        return {};
    }
    const std::size_t size = static_cast<std::size_t>(std::min<std::streamoff>(
            end, static_cast<std::streamoff>(maxBytes)));
    in.seekg(end - static_cast<std::streamoff>(size));
    std::string bytes(size, '\0');
    in.read(bytes.data(), static_cast<std::streamsize>(size));
    bytes.resize(static_cast<std::size_t>(in.gcount()));
    return widen(bytes);
}

std::wstring tailLines(const std::wstring& text, std::size_t maxLines) {
    std::vector<std::wstring> lines;
    std::wstring current;
    for (wchar_t c : text) {
        if (c == L'\n') {
            if (!current.empty() && current.back() == L'\r') {
                current.pop_back();
            }
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    const std::size_t begin = lines.size() > maxLines ? lines.size() - maxLines : 0;
    std::wstring result;
    for (std::size_t i = begin; i < lines.size(); ++i) {
        result += lines[i];
        result += L"\r\n";
    }
    return result;
}

std::filesystem::path firstExistingLog(const flf::QuickStartConfig& config) {
    const std::filesystem::path working = config.workingDir.empty()
            ? std::filesystem::path() : std::filesystem::path(config.workingDir);
    const std::vector<std::filesystem::path> candidates = {
            config.launcherRoot / L"logs" / L"latest.log",
            config.launcherRoot / L".hmcl" / L"logs" / L"latest.log",
            working / L"logs" / L"latest.log",
            working / L".hmcl" / L"logs" / L"latest.log",
    };
    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

std::filesystem::path newestCrashReport(const flf::QuickStartConfig& config) {
    const std::filesystem::path working = config.workingDir.empty()
            ? std::filesystem::path() : std::filesystem::path(config.workingDir);
    std::filesystem::path newest;
    std::filesystem::file_time_type newestTime{};
    for (const auto& dir : {config.launcherRoot / L"crash-report",
                            working / L"crash-report",
                            working / L".hmcl" / L"crash-report"}) {
        std::error_code ec;
        if (!std::filesystem::is_directory(dir, ec)) {
            continue;
        }
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) {
                continue;
            }
            const auto time = entry.last_write_time(ec);
            if (ec) {
                continue;
            }
            if (newest.empty() || time > newestTime) {
                newest = entry.path();
                newestTime = time;
            }
        }
    }
    return newest;
}

std::wstring classifyFailure(const std::wstring& text) {
    if (text.find(L"Incompatible mods found") != std::wstring::npos) {
        return L"Fabric 报告模组不兼容：请检查 -Dfabric.addMods 是否只包含 HMCL 自身的 FLF 模组，"
               L"不要把 .minecraft/mods 中的游戏模组传给 HMCL。";
    }
    if (text.find(L"NoSuchMethodError") != std::wstring::npos) {
        return L"出现 NoSuchMethodError：通常是 HMCL、FLFHMCL Agent、FLF API 版本不匹配。";
    }
    if (text.find(L"UnsupportedClassVersionError") != std::wstring::npos) {
        return L"Java 版本过低：HMCL / FLFHMCL 需要 JDK 17 或更高版本。";
    }
    if (text.find(L"OutOfMemoryError") != std::wstring::npos) {
        return L"Java 堆内存不足：请在实例设置中提高内存，或关闭占用内存的程序。";
    }
    if (text.find(L"AccessDeniedException") != std::wstring::npos ||
        text.find(L"Access is denied") != std::wstring::npos) {
        return L"文件访问被拒绝：请检查实例目录权限、杀毒软件拦截或文件是否被占用。";
    }
    if (text.find(L"Could not find or load main class") != std::wstring::npos) {
        return L"Java 找不到主类：请检查 HMCL jar、Agent jar 和工作目录是否完整。";
    }
    if (text.find(L"@Mixin target") != std::wstring::npos) {
        return L"Mixin 目标缺失：通常是 HMCL 版本与 FLFHMCL / FLF API 不匹配。";
    }
    return L"未识别到明确的已知错误模式，请查看完整日志和崩溃报告。";
}

bool copyTextToClipboard(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) {
        return false;
    }
    EmptyClipboard();
    const std::size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
        CloseClipboard();
        return false;
    }
    void* target = GlobalLock(memory);
    if (target == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    std::memcpy(target, text.c_str(), bytes);
    GlobalUnlock(memory);
    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

void openPath(const std::filesystem::path& path, bool selectFile) {
    if (path.empty()) {
        return;
    }
    std::wstring argument;
    if (selectFile) {
        argument = L"/select,\"" + path.wstring() + L"\"";
    } else {
        argument = L"\"" + path.wstring() + L"\"";
    }
    ShellExecuteW(nullptr, L"open", L"explorer.exe", argument.c_str(), nullptr, SW_SHOWNORMAL);
}

void showLaunchFailure(const flf::QuickStartConfig& config,
                       const std::wstring& commandLine,
                       DWORD exitCode,
                       DWORD createError) {
    const std::filesystem::path log = firstExistingLog(config);
    const std::filesystem::path crash = newestCrashReport(config);
    std::wstring logText;
    if (!log.empty()) {
        logText = readTextFile(log, 512 * 1024);
    }
    std::wstring crashText;
    if (!crash.empty()) {
        crashText = readTextFile(crash, 128 * 1024);
    }

    std::wostringstream details;
    details << L"实例: " << widen(config.instance) << L"\r\n";
    if (createError != 0) {
        details << L"错误: CreateProcessW 失败，Win32 错误码 " << createError << L"\r\n";
    } else {
        details << L"退出码: " << exitCode << L" (0x" << std::hex << exitCode << std::dec << L")\r\n";
    }
    details << L"工作目录: " << widen(config.workingDir) << L"\r\n";
    details << L"命令: " << commandLine << L"\r\n";
    details << L"日志: " << (log.empty() ? L"未找到" : log.wstring()) << L"\r\n";
    details << L"崩溃报告: " << (crash.empty() ? L"未找到" : crash.wstring()) << L"\r\n\r\n";
    details << L"分析: " << classifyFailure(logText) << L"\r\n\r\n";

    if (!logText.empty()) {
        details << L"最近日志:\r\n" << tailLines(logText, 30) << L"\r\n";
    }
    if (!crashText.empty()) {
        details << L"崩溃报告摘要:\r\n" << tailLines(crashText, 20);
    }

    const std::wstring text = details.str();
    copyTextToClipboard(nullptr, text);
    const std::wstring prompt = text
            + L"\r\n\r\n详情已复制到剪贴板。\r\n选择“是”打开日志，选择“否”打开崩溃报告，选择“取消”关闭。";
    const int choice = MessageBoxW(nullptr, prompt.c_str(),
                                   L"FLFQuickStart - 实例启动失败",
                                   MB_ICONERROR | MB_YESNOCANCEL);
    if (choice == IDYES) {
        if (!log.empty()) {
            openPath(log, true);
        } else {
            openPath(config.launcherRoot, false);
        }
    } else if (choice == IDNO) {
        if (!crash.empty()) {
            openPath(crash, true);
        } else {
            openPath(config.launcherRoot / L"crash-report", false);
        }
    }
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
        const DWORD createError = GetLastError();
        showLaunchFailure(config, commandLine, 0, createError);
        return 1;
    }

    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    if (exitCode != 0) {
        showLaunchFailure(config, commandLine, exitCode, 0);
    }
    return static_cast<int>(exitCode);
}
