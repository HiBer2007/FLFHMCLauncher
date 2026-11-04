#include "core/flf_paths.h"

#include <windows.h>

#include <array>
#include <stdexcept>

namespace flf {
namespace {

std::filesystem::path modulePath() {
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0) {
        throw std::runtime_error("GetModuleFileNameW failed");
    }
    return std::filesystem::path(std::wstring(buffer.data(), length));
}

} // namespace

std::filesystem::path executableDirectory() {
    static const std::filesystem::path directory = modulePath().parent_path();
    return directory;
}

std::filesystem::path flfHome() {
    return executableDirectory() / L".flfhmcl";
}

std::filesystem::path logFile() {
    return flfHome() / L"logs" / L"launcher.log";
}

std::filesystem::path ensureLayout() {
    const std::filesystem::path root = flfHome();

    const wchar_t* directories[] = {
        L"config",
        L"libraries",
        L"java",
        L"javafx",
        L"fabric",
        L"hmcl",
        L"tools/CrashTracker",
        L"instances",
        L"cache/downloads",
        L"cache/meta",
        L"logs",
        L"crash-report",
    };

    std::error_code error;
    for (const wchar_t* relative : directories) {
        std::filesystem::create_directories(root / relative, error);
        if (error) {
            throw std::runtime_error("create directory failed: " + error.message());
        }
    }
    return root;
}

} // namespace flf
