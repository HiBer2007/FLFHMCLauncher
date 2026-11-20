// FLFHMCLauncher 自包含引导器。
//
// 发布版只有一个 FLFHMCLauncher.exe：
//   [PE 引导器][payload][footer]
// 首次运行把 payload 里的 GUI / quickstart / DLL 释放到
//   <exe>\\.flfhmcl\\bin
// 然后启动 FLFHMCLauncherApp.exe，并把真正的启动器目录通过环境变量传下去。
//
// 引导器本身使用 /MT，不依赖 VC runtime / Qt / spdlog。

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr char kFooterMagic[8] = {'F', 'L', 'F', 'E', 'N', 'D', '0', '1'};
constexpr char kPayloadMagic[8] = {'F', 'L', 'F', 'P', 'A', 'Y', '0', '1'};
constexpr std::uint32_t kPayloadVersion = 1;
constexpr std::size_t kFooterSize = 24;

#pragma pack(push, 1)
struct Footer {
    char magic[8];
    std::uint64_t offset;
    std::uint64_t size;
};
#pragma pack(pop)

struct Entry {
    std::string name;
    std::uint64_t size = 0;
    std::uint64_t offset = 0;
};

struct Payload {
    std::vector<Entry> entries;
    std::vector<char> data;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

void fail(const std::wstring& message, const std::wstring& title = L"FLFHMCLauncher") {
    MessageBoxW(nullptr, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
}

std::wstring widen(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size);
    return result;
}

fs::path modulePath() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    return fs::path(std::wstring(buffer.data(), length));
}

bool readFooter(std::ifstream& input, const std::uint64_t fileSize, Footer& footer) {
    if (fileSize < kFooterSize) return false;
    input.seekg(static_cast<std::streamoff>(fileSize - kFooterSize));
    input.read(reinterpret_cast<char*>(&footer), sizeof(footer));
    if (!input) return false;
    return std::equal(std::begin(footer.magic), std::end(footer.magic),
                      std::begin(kFooterMagic));
}

bool readPayload(const fs::path& exe, Payload& payload, std::wstring& error) {
    std::ifstream input(exe, std::ios::binary);
    if (!input) {
        error = L"无法打开启动器文件";
        return false;
    }
    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end <= 0) {
        error = L"启动器文件为空";
        return false;
    }
    const std::uint64_t fileSize = static_cast<std::uint64_t>(end);
    Footer footer{};
    if (!readFooter(input, fileSize, footer)) {
        return false;
    }
    if (footer.size == 0 || footer.offset > fileSize
            || footer.size > fileSize - footer.offset
            || footer.offset + footer.size > fileSize - kFooterSize) {
        error = L"启动器 payload 损坏";
        return false;
    }
    std::vector<char> raw(static_cast<std::size_t>(footer.size));
    input.seekg(static_cast<std::streamoff>(footer.offset));
    input.read(raw.data(), static_cast<std::streamsize>(raw.size()));
    if (!input) {
        error = L"读取启动器 payload 失败";
        return false;
    }
    if (raw.size() < 16 || !std::equal(std::begin(kPayloadMagic), std::end(kPayloadMagic),
                                       raw.begin())) {
        error = L"启动器 payload 标识无效";
        return false;
    }
    const auto* cursor = reinterpret_cast<const unsigned char*>(raw.data());
    const std::uint32_t version = *reinterpret_cast<const std::uint32_t*>(cursor + 8);
    const std::uint32_t count = *reinterpret_cast<const std::uint32_t*>(cursor + 12);
    if (version != kPayloadVersion || count > 64) {
        error = L"启动器 payload 版本不受支持";
        return false;
    }
    std::size_t position = 16;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (position + 4 > raw.size()) {
            error = L"启动器 payload 条目损坏";
            return false;
        }
        const std::uint32_t nameSize = *reinterpret_cast<const std::uint32_t*>(
                raw.data() + position);
        position += 4;
        if (nameSize == 0 || nameSize > 260 || position + nameSize + 8 > raw.size()) {
            error = L"启动器 payload 文件名损坏";
            return false;
        }
        Entry entry;
        entry.name.assign(raw.data() + position, nameSize);
        position += nameSize;
        entry.size = *reinterpret_cast<const std::uint64_t*>(raw.data() + position);
        position += 8;
        if (entry.size > raw.size() - position) {
            error = L"启动器 payload 文件大小损坏";
            return false;
        }
        if (entry.name.find('/') != std::string::npos
                || entry.name.find('\\') != std::string::npos
                || entry.name.find(':') != std::string::npos
                || entry.name == "." || entry.name == "..") {
            error = L"启动器 payload 包含非法文件名";
            return false;
        }
        entry.offset = position;
        position += static_cast<std::size_t>(entry.size);
        payload.entries.push_back(std::move(entry));
    }
    payload.data = std::move(raw);
    payload.offset = footer.offset;
    payload.size = footer.size;
    return true;
}

bool extractPayload(const fs::path& binDir, const Payload& payload, std::wstring& error) {
    std::error_code ec;
    fs::create_directories(binDir, ec);
    if (ec) {
        error = L"创建 .flfhmcl\\bin 失败: " + widen(ec.message());
        return false;
    }
    for (const Entry& entry : payload.entries) {
        const fs::path target = binDir / fs::path(widen(entry.name));
        const fs::path temp = target.wstring() + L".tmp";
        {
            std::ofstream output(temp, std::ios::binary | std::ios::trunc);
            if (!output) {
                error = L"写入 " + target.wstring() + L" 失败";
                return false;
            }
            output.write(payload.data.data() + entry.offset,
                         static_cast<std::streamsize>(entry.size));
            if (!output) {
                error = L"写入 " + target.wstring() + L" 失败";
                return false;
            }
        }
        if (!MoveFileExW(temp.c_str(), target.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::error_code removeError;
            fs::remove(temp, removeError);
            error = L"替换 " + target.wstring() + L" 失败（文件可能正在运行）";
            return false;
        }
    }
    const std::string marker = std::to_string(payload.offset) + "-"
            + std::to_string(payload.size);
    std::ofstream stamp(binDir / ".payload", std::ios::binary | std::ios::trunc);
    stamp << marker;
    return true;
}

bool payloadCurrent(const fs::path& binDir, const Payload& payload,
                    const fs::path& app) {
    if (!fs::is_regular_file(app)) return false;
    for (const Entry& entry : payload.entries) {
        if (!fs::is_regular_file(binDir / fs::path(widen(entry.name)))) {
            return false;
        }
    }
    std::ifstream stamp(binDir / ".payload", std::ios::binary);
    if (!stamp) return false;
    std::string marker;
    std::getline(stamp, marker);
    return marker == std::to_string(payload.offset) + "-" + std::to_string(payload.size);
}

std::wstring commandLineAfterExe() {
    LPWSTR commandLine = GetCommandLineW();
    if (!commandLine) return {};
    LPWSTR cursor = commandLine;
    if (*cursor == L'"') {
        ++cursor;
        while (*cursor && *cursor != L'"') ++cursor;
        if (*cursor == L'"') ++cursor;
    } else {
        while (*cursor && !iswspace(*cursor)) ++cursor;
    }
    while (*cursor && iswspace(*cursor)) ++cursor;
    return cursor;
}

std::wstring quote(const std::wstring& value) {
    if (value.find(L' ') == std::wstring::npos && value.find(L'\t') == std::wstring::npos) {
        return value;
    }
    return L"\"" + value + L"\"";
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const fs::path exe = modulePath();
    if (exe.empty()) {
        fail(L"无法确定 FLFHMCLauncher.exe 路径");
        return 1;
    }
    const fs::path launcherDir = exe.parent_path();
    const fs::path binDir = launcherDir / L".flfhmcl" / L"bin";
    fs::path app = binDir / L"FLFHMCLauncherApp.exe";

    Payload payload;
    std::wstring error;
    const bool hasPayload = readPayload(exe, payload, error);
    if (hasPayload) {
        if (!payloadCurrent(binDir, payload, app)
                && !extractPayload(binDir, payload, error)) {
            // 已释放过（例如正在运行）时，继续尝试启动现有版本。
            if (!fs::exists(app)) {
                fail(error);
                return 1;
            }
        }
    } else if (!error.empty()) {
        fail(error);
        return 1;
    }

    // 开发目录：引导器未打包 payload 时，GUI 通常与它并排。
    if (!hasPayload && !fs::exists(app)) {
        app = launcherDir / L"FLFHMCLauncherApp.exe";
    }

    if (!fs::exists(app)) {
        fail(L"找不到 FLFHMCLauncherApp.exe。\n请使用完整发布包或重新构建。");
        return 1;
    }

    SetEnvironmentVariableW(L"FLFHMCL_LAUNCHER_DIR", launcherDir.c_str());
    const std::wstring args = commandLineAfterExe();
    std::wstring command = quote(app.wstring());
    if (!args.empty()) {
        command += L" ";
        command += args;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = command;
    if (!CreateProcessW(app.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE,
                        CREATE_UNICODE_ENVIRONMENT, nullptr, launcherDir.c_str(),
                        &startup, &process)) {
        fail(L"启动 FLFHMCLauncherApp.exe 失败");
        return 1;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
