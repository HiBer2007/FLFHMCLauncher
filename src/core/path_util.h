#pragma once

#include <filesystem>
#include <string>

namespace flf {

/// Windows 的 path::string() 返回当前 ANSI 代码页，中文路径会变成非法 UTF-8。
/// 所有写入 JSON / JVM 参数 / Qt 的路径都必须经过这里。
inline std::string pathUtf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

inline std::filesystem::path pathFromUtf8(const std::string& value) {
    const auto* data = reinterpret_cast<const char8_t*>(value.data());
    return std::filesystem::path(std::u8string(data, data + value.size()));
}

} // namespace flf
