#include "core/quickstart_config.h"

#include "core/path_util.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>

namespace flf {
namespace {

/// 载荷布局：[JSON][uint64 little-endian length][8-byte magic]
constexpr char kMagic[8] = {'F', 'L', 'F', 'Q', 'S', '0', '1', '\n'};
constexpr std::size_t kFooterSize = 16;
constexpr std::uint64_t kMaxConfigSize = 64ull * 1024 * 1024;

bool readEmbeddedJson(const std::filesystem::path& exePath, std::string& json) {
    std::ifstream in(exePath, std::ios::binary | std::ios::ate);
    if (!in) {
        return false;
    }
    const std::streamoff end = in.tellg();
    if (end < static_cast<std::streamoff>(kFooterSize)) {
        return false;
    }
    char footer[kFooterSize]{};
    in.seekg(end - static_cast<std::streamoff>(kFooterSize));
    if (!in.read(footer, sizeof(footer))) {
        return false;
    }
    if (std::memcmp(footer + 8, kMagic, sizeof(kMagic)) != 0) {
        return false;
    }
    std::uint64_t length = 0;
    for (int i = 0; i < 8; ++i) {
        length |= static_cast<std::uint64_t>(
                static_cast<unsigned char>(footer[i])) << (8 * i);
    }
    if (length == 0 || length > kMaxConfigSize
            || length > static_cast<std::uint64_t>(end)
                    - static_cast<std::uint64_t>(kFooterSize)) {
        return false;
    }
    const std::streamoff start = end - static_cast<std::streamoff>(kFooterSize)
            - static_cast<std::streamoff>(length);
    in.seekg(start);
    json.resize(static_cast<std::size_t>(length));
    return static_cast<bool>(in.read(json.data(), static_cast<std::streamsize>(json.size())));
}

bool readSidecarJson(const std::filesystem::path& exePath, std::string& json) {
    const auto file = exePath.parent_path() / "quickstart.json";
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    json = buffer.str();
    return true;
}

/// 极简 JSON 字符串字段提取（配置只有平铺字符串字段）。
bool extractString(const std::string& json, const std::string& key, std::string& value) {
    const std::string needle = "\"" + key + "\"";
    const auto keyPos = json.find(needle);
    if (keyPos == std::string::npos) {
        return false;
    }
    const auto colon = json.find(':', keyPos + needle.size());
    if (colon == std::string::npos) {
        return false;
    }
    const auto quote = json.find('"', colon + 1);
    if (quote == std::string::npos) {
        return false;
    }
    std::string result;
    for (std::size_t i = quote + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (c == '\\' && i + 1 < json.size()) {
            result.push_back(json[++i]);
            continue;
        }
        if (c == '"') {
            value = result;
            return true;
        }
        result.push_back(c);
    }
    return false;
}

void replaceAll(std::string& value, const std::string& from, const std::string& to) {
    if (from.empty() || from == to) {
        return;
    }
    std::size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
}

bool parseConfig(const std::string& json, const std::filesystem::path& exePath,
                 QuickStartConfig& out, std::string& error) {
    std::string root;
    std::string instance;
    if (!extractString(json, "root", root)) {
        error = "快速启动配置缺少 root";
        return false;
    }
    if (!extractString(json, "instance", instance)) {
        error = "快速启动配置缺少 instance";
        return false;
    }

    std::error_code ec;
    std::filesystem::path rootPath = pathFromUtf8(root);
    if (rootPath.is_relative()) {
        rootPath = std::filesystem::weakly_canonical(
                exePath.parent_path() / rootPath, ec);
        if (ec) {
            rootPath = exePath.parent_path() / rootPath;
        }
    } else {
        rootPath = rootPath.lexically_normal();
    }
    out.launcherRoot = rootPath;
    out.instance = instance;
    extractString(json, "java", out.java);
    extractString(json, "command", out.command);
    extractString(json, "workingDir", out.workingDir);
    std::string mods;
    if (extractString(json, "modsDir", mods) && !mods.empty()) {
        out.modsDir = pathFromUtf8(mods);
        if (out.modsDir.is_relative()) {
            out.modsDir = rootPath / out.modsDir;
        }
        out.modsDir = out.modsDir.lexically_normal();
    }

    // root 是相对路径；command/workingDir 是生成时的绝对路径。
    // 整个目录搬走后，把旧根目录前缀替换成新的解析结果。
    std::string baseRoot;
    if (extractString(json, "baseRoot", baseRoot) && !baseRoot.empty()) {
        const std::string newRoot = rootPath.string();
        if (baseRoot != newRoot) {
            replaceAll(out.command, baseRoot, newRoot);
            replaceAll(out.workingDir, baseRoot, newRoot);
        }
    }
    return true;
}

} // namespace

bool appendQuickStartConfig(const std::filesystem::path& exePath,
                            const std::string& json,
                            std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(exePath.parent_path(), ec);

    // 如果目标 EXE 已有有效载荷，先截掉旧载荷再追加，避免反复生成时无限增长。
    std::uintmax_t keepSize = std::filesystem::file_size(exePath, ec);
    if (ec) {
        error = "读取快速启动器大小失败: " + ec.message();
        return false;
    }
    {
        std::ifstream in(exePath, std::ios::binary | std::ios::ate);
        if (in) {
            const std::streamoff end = in.tellg();
            if (end >= static_cast<std::streamoff>(kFooterSize)) {
                char footer[kFooterSize]{};
                in.seekg(end - static_cast<std::streamoff>(kFooterSize));
                if (in.read(footer, sizeof(footer))
                        && std::memcmp(footer + 8, kMagic, sizeof(kMagic)) == 0) {
                    std::uint64_t length = 0;
                    for (int i = 0; i < 8; ++i) {
                        length |= static_cast<std::uint64_t>(
                                static_cast<unsigned char>(footer[i])) << (8 * i);
                    }
                    if (length <= kMaxConfigSize
                            && length <= static_cast<std::uint64_t>(end)
                                    - static_cast<std::uint64_t>(kFooterSize)) {
                        keepSize = static_cast<std::uintmax_t>(
                                end - static_cast<std::streamoff>(kFooterSize)
                                - static_cast<std::streamoff>(length));
                    }
                }
            }
        }
    }
    if (keepSize < std::filesystem::file_size(exePath, ec)) {
        std::filesystem::resize_file(exePath, keepSize, ec);
        if (ec) {
            error = "清理旧快速启动配置失败: " + ec.message();
            return false;
        }
    }

    std::ofstream out(exePath, std::ios::binary | std::ios::app);
    if (!out) {
        error = "无法写入快速启动器: " + exePath.string();
        return false;
    }
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    const std::uint64_t length = static_cast<std::uint64_t>(json.size());
    char footer[kFooterSize]{};
    for (int i = 0; i < 8; ++i) {
        footer[i] = static_cast<char>((length >> (8 * i)) & 0xff);
    }
    std::memcpy(footer + 8, kMagic, sizeof(kMagic));
    out.write(footer, sizeof(footer));
    out.flush();
    if (!out.good()) {
        error = "写入内嵌快速启动配置失败";
        return false;
    }
    return true;
}

bool loadQuickStartConfig(const std::filesystem::path& exePath,
                          QuickStartConfig& out,
                          std::string& error) {
    std::string json;
    if (!readEmbeddedJson(exePath, json) && !readSidecarJson(exePath, json)) {
        error = "未找到内嵌快速启动配置，也没有同目录 quickstart.json: "
                + exePath.string();
        return false;
    }
    return parseConfig(json, exePath, out, error);
}

} // namespace flf
