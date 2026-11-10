#pragma once

#include "core/workspace.h"

#include <filesystem>
#include <map>
#include <string>

namespace flf {

namespace fs = std::filesystem;

/// `.flfhmclpack` 的导出/导入选项。
struct HmclSelection {
    bool config = true;       ///< .hmcl/config（账户、下载源、启动器设置）
    bool state = true;        ///< .hmcl/state（窗口、用户状态）
    bool cache = false;       ///< .hmcl/cache
    bool logs = false;        ///< .hmcl/logs
    bool sanitizeTokens = true; ///< 自动移除 token/password/secret 字段
};

struct ModpackOptions {
    bool includeMinecraft = true;
    bool includeHmcl = true;
    bool includeMods = true;
    bool includeProfile = true;
    bool includeRuntime = true;
    /// .minecraft/logs 默认不打包。
    bool includeMinecraftLogs = false;
    /// true：未在 modLinks 中登记的 mod 嵌入 jar；false：全部按链接导出。
    bool embedMods = true;
    /// 链接模式：key 为 jar 文件名，value 为下载地址；登记后该 mod 只写链接。
    std::map<std::string, std::string> modLinks;
    HmclSelection hmcl;
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
