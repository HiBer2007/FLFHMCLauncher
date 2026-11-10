#pragma once

#include "core/workspace.h"

#include <filesystem>
#include <map>
#include <string>

namespace flf {

namespace fs = std::filesystem;

/// `.flfhmclpack` 的导出/导入选项。
struct ModpackOptions {
    bool includeMinecraft = true;
    bool includeHmcl = true;
    bool includeMods = true;
    bool includeProfile = true;
    bool includeRuntime = true;
    /// true：mods 里的 jar 直接放进包；false：只记录链接，导入时下载。
    bool embedMods = true;
    /// 链接模式下的 mod 下载地址，key 为 jar 文件名。
    std::map<std::string, std::string> modLinks;
};

struct ModpackResult {
    bool ok = false;
    std::string error;
    std::string instanceId;   // 导入后的实例 uuid
    std::string instanceName;
    std::size_t fileCount = 0;
    std::uint64_t totalBytes = 0;
};

/// 导出实例为 `.flfhmclpack`。扩展名由调用方保证，core 也接受任意后缀。
ModpackResult exportModpack(const Instance& instance, const fs::path& target,
                            const ModpackOptions& options = {});

/// 导入 `.flfhmclpack`。成功时把新实例写入 Workspace 并保存 instances.json。
ModpackResult importModpack(const fs::path& source,
                            const ModpackOptions& options = {});

/// 是否为 `.flfhmclpack`（大小写不敏感）。
bool isModpackFile(const fs::path& path);

} // namespace flf
