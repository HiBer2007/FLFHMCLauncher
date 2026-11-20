#include "core/modpack.h"

#include "core/http.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QStringList>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace flf {
namespace {

constexpr const char* kFormat = "flfhmclpack";
constexpr int kSchema = 1;

struct ExportEntry {
    std::string role;
    fs::path relative;
    fs::path source;
    bool directory = false;
    std::uint64_t size = 0;
    std::string sha256;
};

std::string pathString(const fs::path& path) {
    const auto u8 = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

std::string lowerExtension(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

bool safeRelative(const fs::path& path) {
    if (path.empty() || path.is_absolute()) {
        return false;
    }
    for (const auto& part : path) {
        const std::string s = part.string();
        if (s == ".." || s == "." || s.empty()) {
            return false;
        }
    }
    return true;
}

bool runTar(const QStringList& arguments, const fs::path& workDir, std::string* error) {
    QString program = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (program.isEmpty()) {
        program = QStringLiteral("tar.exe");
    }
    QProcess process;
    process.setWorkingDirectory(QString::fromStdWString(workDir.wstring()));
    process.start(program, arguments);
    if (!process.waitForStarted(10000)) {
        if (error) *error = "无法启动 tar.exe";
        return false;
    }
    process.waitForFinished(-1);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = "tar.exe 失败: " + process.readAllStandardError().toStdString();
        }
        return false;
    }
    return true;
}

bool copyInto(const fs::path& source, const fs::path& destination, std::string* error) {
    std::error_code ec;
    fs::create_directories(destination.parent_path(), ec);
    if (ec) {
        if (error) *error = "无法创建目录: " + destination.parent_path().string();
        return false;
    }
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        if (error) *error = "复制失败: " + source.string() + " -> " + destination.string();
        return false;
    }
    return true;
}

void addFileEntry(std::vector<ExportEntry>& entries, std::set<std::string>& seen,
                  const std::string& role, const fs::path& relative,
                  const fs::path& source, std::string* error) {
    const std::string key = role + "|" + pathString(relative);
    if (!seen.insert(key).second) {
        return;
    }
    ExportEntry entry;
    entry.role = role;
    entry.relative = relative;
    entry.source = source;
    entry.size = fs::file_size(source);
    entry.sha256 = sha256File(source);
    entries.push_back(std::move(entry));
    if (error && entries.back().sha256.empty()) {
        *error = "无法计算 SHA-256: " + source.string();
    }
}

void collectDirectory(const fs::path& root, const std::string& role,
                      std::vector<ExportEntry>& entries, std::set<std::string>& seen,
                      std::string* error, const ModpackOptions* options = nullptr) {
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return;
    }
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        const fs::path source = it->path();
        const fs::path relative = fs::relative(source, root, ec);
        if (ec || !safeRelative(relative)) {
            ec.clear();
            continue;
        }
        if (role == "minecraft" && options && !options->includeMinecraftLogs) {
            const std::string first = relative.begin()->string();
            if (first == "logs" || first == "crash-reports") {
                continue;
            }
        }
        const fs::file_status status = fs::symlink_status(source, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        if (fs::is_symlink(status)) {
            // 不跟随符号链接，避免把实例目录以外的内容打进包。
            continue;
        }
        if (fs::is_directory(status)) {
            ExportEntry entry;
            entry.role = role;
            entry.relative = relative;
            entry.directory = true;
            const std::string key = role + "|" + pathString(relative);
            if (seen.insert(key).second) {
                entries.push_back(std::move(entry));
            }
            continue;
        }
        if (!fs::is_regular_file(status)) {
            continue;
        }
        const std::string ext = lowerExtension(source);
        if (role == "mods" && ext == ".jar") {
            // mods 由 files + mods 清单共同描述；先按普通文件收集。
        }
        addFileEntry(entries, seen, role, relative, source, error);
    }
}

fs::path roleRoot(const Workspace& workspace, const Instance& instance,
                  const std::string& role) {
    if (role == "minecraft") return workspace.resolveMinecraft(instance);
    if (role == "hmcl") return workspace.resolveHmclHome(instance);
    if (role == "mods") return workspace.resolveMods(instance);
    if (role == "profile") return workspace.profileDir(instance);
    return workspace.instanceDir(instance);
}

std::string sanitizeId(std::string value) {
    std::string out;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '-' || c == '_') {
            out.push_back(static_cast<char>(c));
        } else if (!out.empty() && out.back() != '-') {
            out.push_back('-');
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "imported-pack" : out;
}

bool writeJson(const fs::path& file, const nlohmann::json& json, std::string* error) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error) *error = "无法写入清单: " + file.string();
        return false;
    }
    out << json.dump(2);
    if (!out.good()) {
        if (error) *error = "写入清单失败: " + file.string();
        return false;
    }
    return true;
}

nlohmann::json readJson(const fs::path& file, std::string* error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        if (error) *error = "找不到整合包清单: " + file.string();
        return {};
    }
    try {
        nlohmann::json json;
        in >> json;
        return json;
    } catch (const std::exception& e) {
        if (error) *error = std::string("整合包清单损坏: ") + e.what();
        return {};
    }
}

void cleanupImported(const Instance& instance) {
    Workspace& ws = Workspace::get();
    std::error_code ec;
    fs::remove_all(ws.instanceDir(instance), ec);
    fs::remove_all(ws.profileDir(instance), ec);
    fs::remove_all(ws.resolveMods(instance), ec);
    if (instance.isolated) {
        fs::remove_all(ws.home().parent_path() / ".launcher"
                       / (instance.uuid.empty() ? instance.id : instance.uuid), ec);
    }
}

bool sensitiveKey(const std::string& key) {
    std::string lower;
    lower.reserve(key.size());
    for (unsigned char c : key) {
        lower.push_back(static_cast<char>(std::tolower(c)));
    }
    return lower.find("token") != std::string::npos
            || lower.find("password") != std::string::npos
            || lower.find("secret") != std::string::npos
            || lower.find("credential") != std::string::npos
            || lower.find("session") != std::string::npos;
}

void stripSensitive(nlohmann::json& node) {
    if (node.is_object()) {
        for (auto it = node.begin(); it != node.end();) {
            if (sensitiveKey(it.key())) {
                it = node.erase(it);
            } else {
                stripSensitive(it.value());
                ++it;
            }
        }
    } else if (node.is_array()) {
        for (auto& item : node) {
            stripSensitive(item);
        }
    }
}

void sanitizeJsonFile(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return;
    try {
        nlohmann::json json;
        in >> json;
        in.close();
        stripSensitive(json);
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (out) out << json.dump(2);
    } catch (...) {
        // 不是 JSON 或结构异常：保留原文件
    }
}

bool copyHmclFiltered(const fs::path& source, const fs::path& destination,
                      const HmclSelection& selection, std::string* error) {
    std::error_code ec;
    if (!fs::is_directory(source, ec)) {
        return true;
    }
    for (fs::recursive_directory_iterator it(source, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        const fs::path relative = fs::relative(it->path(), source, ec);
        if (ec || !safeRelative(relative)) {
            ec.clear();
            continue;
        }
        const std::string first = relative.begin()->string();
        if ((first == "logs" || first == "log") && !selection.logs) continue;
        if (first == "cache" && !selection.cache) continue;
        if (first == "config" && !selection.config) continue;
        if (first == "state" && !selection.state) continue;

        const fs::file_status status = fs::symlink_status(it->path(), ec);
        if (ec || fs::is_symlink(status)) {
            ec.clear();
            continue;
        }
        const fs::path target = destination / relative;
        if (fs::is_directory(status)) {
            fs::create_directories(target, ec);
            continue;
        }
        if (!fs::is_regular_file(status)) continue;
        if (!copyInto(it->path(), target, error)) {
            return false;
        }
        if (selection.sanitizeTokens && lowerExtension(target) == ".json") {
            sanitizeJsonFile(target);
        }
    }
    return true;
}

} // namespace

bool isModpackFile(const fs::path& path) {
    return lowerExtension(path) == ".flfhmclpack";
}

ModpackResult exportModpack(const Instance& instance, const fs::path& target,
                            const ModpackOptions& options) {
    ModpackResult result;
    QTemporaryDir staging;
    QTemporaryDir hmclTemp;
    if (!staging.isValid() || !hmclTemp.isValid()) {
        result.error = "无法创建临时目录";
        return result;
    }
    const fs::path root = fs::path(staging.path().toStdWString());
    const fs::path payload = root / "payload";
    std::error_code ec;
    fs::create_directories(payload, ec);

    std::vector<ExportEntry> entries;
    std::set<std::string> seen;

    auto addRole = [&](const char* role, const fs::path& source) {
        if (fs::exists(source, ec) && fs::is_directory(source, ec)) {
            collectDirectory(source, role, entries, seen, &result.error, &options);
        }
    };
    if (options.includeMinecraft) addRole("minecraft", Workspace::get().resolveMinecraft(instance));
    if (options.includeHmcl) {
        const fs::path hmclSource = Workspace::get().resolveHmclHome(instance);
        const fs::path filtered = fs::path(hmclTemp.path().toStdWString()) / "hmcl";
        fs::create_directories(filtered, ec);
        if (!copyHmclFiltered(hmclSource, filtered, options.hmcl, &result.error)) {
            return result;
        }
        addRole("hmcl", filtered);
    }
    if (options.includeMods) addRole("mods", Workspace::get().resolveMods(instance));
    if (options.includeProfile) addRole("profile", Workspace::get().profileDir(instance));

    // 运行时 jar：显式 target，导入时回到 .flfhmcl/hmcl 与 .flfhmcl/agents。
    std::map<std::string, std::string> runtimeTargets;
    auto addRuntime = [&](const fs::path& source, const std::string& relative, const std::string& targetPath) {
        if (source.empty() || !fs::is_regular_file(source, ec)) {
            return;
        }
        addFileEntry(entries, seen, "runtime", fs::path(relative), source, &result.error);
        runtimeTargets["runtime|" + relative] = targetPath;
    };
    if (options.includeRuntime) {
        const fs::path hmcl = Workspace::get().hmclJar(instance);
        const fs::path agent = Workspace::get().agentJar(instance);
        addRuntime(hmcl, "hmcl/" + hmcl.filename().string(),
                   "hmcl/" + hmcl.filename().string());
        addRuntime(agent, "agents/" + agent.filename().string(),
                   "agents/" + agent.filename().string());
    }

    nlohmann::json manifest = {
        {"format", kFormat},
        {"schema", kSchema},
        {"generator", {{"name", "FLFHMCLauncher"}, {"version", "0.3.9"}}},
        {"createdAt", std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch()).count()},
        {"name", instance.name},
        {"instance", toJson(instance)},
        {"workspace", {
            {"schema", Workspace::get().config().schema},
            {"hmclVersion", Workspace::get().config().hmclVersion},
            {"agentJar", Workspace::get().config().agentJar},
            {"launchBehavior", Workspace::get().config().launchBehavior},
            {"validateBeforeLaunch", Workspace::get().config().validateBeforeLaunch},
            {"checkUpdatesBeforeLaunch", Workspace::get().config().checkUpdatesBeforeLaunch},
        }},
        {"hmclSelection", {
            {"config", options.hmcl.config},
            {"state", options.hmcl.state},
            {"cache", options.hmcl.cache},
            {"logs", options.hmcl.logs},
            {"sanitizeTokens", options.hmcl.sanitizeTokens},
        }},
        {"includeMinecraftLogs", options.includeMinecraftLogs},
        {"mods", nlohmann::json::array()},
        {"directories", nlohmann::json::array()},
        {"files", nlohmann::json::array()},
    };

    for (const auto& entry : entries) {
        const std::string rel = pathString(entry.relative);
        const std::string archivePath = "payload/" + entry.role + "/" + rel;
        if (entry.directory) {
            manifest["directories"].push_back({
                {"path", rel}, {"role", entry.role}
            });
            fs::create_directories(root / archivePath, ec);
            continue;
        }
        const bool hasLink = options.modLinks.find(entry.source.filename().string())
                != options.modLinks.end();
        const bool linkOnlyMod = entry.role == "mods"
                && lowerExtension(entry.source) == ".jar"
                && (!options.embedMods || hasLink);
        if (!linkOnlyMod) {
            if (result.error.empty() && !copyInto(entry.source, root / archivePath, &result.error)) {
                return result;
            }
            manifest["files"].push_back({
                {"path", rel},
                {"role", entry.role},
                {"size", entry.size},
                {"sha256", entry.sha256},
            });
            if (entry.role == "runtime") {
                auto targetIt = runtimeTargets.find("runtime|" + rel);
                manifest["files"].back()["target"] =
                    targetIt == runtimeTargets.end() ? rel : targetIt->second;
            }
            result.fileCount++;
            result.totalBytes += entry.size;
        }

        if (entry.role == "mods" && lowerExtension(entry.source) == ".jar") {
            const bool embedded = !linkOnlyMod;
            nlohmann::json mod = {
                {"path", rel},
                {"role", "mods"},
                {"file", entry.source.filename().string()},
                {"size", entry.size},
                {"sha256", entry.sha256},
                {"embedded", embedded},
                {"url", ""},
            };
            if (!embedded) {
                auto it = options.modLinks.find(entry.source.filename().string());
                if (it != options.modLinks.end()) {
                    mod["url"] = it->second;
                }
            }
            manifest["mods"].push_back(std::move(mod));
        }
    }

    if (!writeJson(root / "flfhmclpack.json", manifest, &result.error)) {
        return result;
    }

    const fs::path tempZip = root / "pack.zip";
    QStringList args{
        QStringLiteral("-a"), QStringLiteral("-c"),
        QStringLiteral("-f"), QString::fromStdWString(tempZip.wstring()),
        QStringLiteral(".")
    };
    if (!runTar(args, root, &result.error)) {
        return result;
    }

    ec.clear();
    fs::create_directories(target.parent_path(), ec);
    fs::copy_file(tempZip, target, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        result.error = "写入整合包失败: " + ec.message();
        return result;
    }
    result.ok = true;
    result.instanceId = instance.uuid;
    result.instanceName = instance.name;
    return result;
}

ModpackResult importModpack(const fs::path& source, const ModpackOptions& options) {
    (void) options;
    ModpackResult result;
    if (!fs::is_regular_file(source)) {
        result.error = "找不到整合包: " + source.string();
        return result;
    }
    QTemporaryDir temp;
    if (!temp.isValid()) {
        result.error = "无法创建临时目录";
        return result;
    }
    const fs::path root = fs::path(temp.path().toStdWString());
    const fs::path zip = root / "pack.zip";
    std::error_code ec;
    fs::copy_file(source, zip, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        result.error = "读取整合包失败: " + ec.message();
        return result;
    }
    const fs::path extract = root / "extract";
    fs::create_directories(extract, ec);
    QStringList args{
        QStringLiteral("-x"), QStringLiteral("-f"),
        QString::fromStdWString(zip.wstring()),
        QStringLiteral("-C"), QString::fromStdWString(extract.wstring())
    };
    if (!runTar(args, root, &result.error)) {
        return result;
    }

    nlohmann::json manifest = readJson(extract / "flfhmclpack.json", &result.error);
    if (manifest.is_null()) {
        return result;
    }
    if (manifest.value("format", "") != kFormat
            || manifest.value("schema", 0) != kSchema) {
        result.error = "不是受支持的 .flfhmclpack（format/schema 不匹配）";
        return result;
    }
    if (!manifest.contains("instance") || !manifest["instance"].is_object()) {
        result.error = "整合包缺少 instance 元数据";
        return result;
    }

    Instance instance = instanceFromJson(manifest["instance"]);
    instance.uuid = newInstanceUuid();
    const std::string requestedName = manifest.value("name", instance.name);
    instance.name = requestedName.empty() ? "导入的整合包" : requestedName;
    std::string baseId = sanitizeId(instance.id.empty() ? instance.name : instance.id);
    std::string id = baseId;
    int suffix = 2;
    while (Workspace::get().find(id) != nullptr) {
        id = baseId + "-" + std::to_string(suffix++);
    }
    instance.id = id;
    instance.isolated = true;  // 导入包必须隔离 .minecraft/.hmcl，避免覆盖全局目录
    instance.mods.mode = "private";
    instance.minecraft.mode = "private";
    instance.hmclHome.mode = "private";
    instance.createdAt = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (instance.name.empty()) instance.name = id;

    Workspace& ws = Workspace::get();
    fs::create_directories(ws.instanceDir(instance), ec);
    fs::create_directories(ws.profileDir(instance), ec);
    fs::create_directories(ws.resolveMods(instance), ec);

    auto fail = [&](const std::string& message) {
        result.error = message;
        cleanupImported(instance);
        return result;
    };

    // 先建目录，保留空文件夹。
    for (const auto& dir : manifest.value("directories", nlohmann::json::array())) {
        const fs::path relative = fs::path(dir.value("path", ""));
        const std::string role = dir.value("role", "instance");
        if (!safeRelative(relative)) continue;
        fs::path rootDir = roleRoot(ws, instance, role);
        if (role == "runtime") continue;
        fs::create_directories(rootDir / relative, ec);
    }

    // 恢复文件，校验 SHA-256。
    for (const auto& file : manifest.value("files", nlohmann::json::array())) {
        const fs::path relative = fs::path(file.value("path", ""));
        const std::string role = file.value("role", "instance");
        if (!safeRelative(relative)) {
            return fail("整合包包含非法路径: " + relative.string());
        }
        fs::path sourceFile = extract / "payload" / role / relative;
        if (!fs::is_regular_file(sourceFile)) {
            return fail("整合包缺少文件: " + relative.string());
        }
        const std::string expected = file.value("sha256", "");
        if (!expected.empty() && sha256File(sourceFile) != expected) {
            return fail("文件校验失败: " + relative.string());
        }
        fs::path destinationRoot;
        if (role == "runtime") {
            const fs::path target = fs::path(file.value("target", ""));
            if (!safeRelative(target)) {
                return fail("运行时文件路径非法: " + target.string());
            }
            destinationRoot = ws.home();
            if (!copyInto(sourceFile, destinationRoot / target, &result.error)) {
                return fail(result.error);
            }
            continue;
        }
        destinationRoot = roleRoot(ws, instance, role);
        if (!copyInto(sourceFile, destinationRoot / relative, &result.error)) {
            return fail(result.error);
        }
        result.fileCount++;
        result.totalBytes += file.value("size", 0ULL);
    }

    // 链接型 mod：从 url 下载到隔离的 mods 目录。
    for (const auto& mod : manifest.value("mods", nlohmann::json::array())) {
        if (mod.value("embedded", true)) {
            continue;
        }
        const std::string url = mod.value("url", "");
        if (url.empty()) {
            return fail("mod 未嵌入且没有下载地址: " + mod.value("file", ""));
        }
        const fs::path destination = ws.resolveMods(instance) / mod.value("file", "");
        if (!httpDownload(url, destination, [](std::int64_t, std::int64_t) { return true; },
                          &result.error)) {
            return fail("下载 mod 失败: " + result.error);
        }
        const std::string expected = mod.value("sha256", "");
        if (!expected.empty() && sha256File(destination) != expected) {
            return fail("mod 校验失败: " + mod.value("file", ""));
        }
        result.fileCount++;
    }

    // 把原始清单留一份，便于追踪来源和再次导出。
    std::error_code copyEc;
    fs::copy_file(extract / "flfhmclpack.json", ws.instanceDir(instance) / "modpack.json",
                  fs::copy_options::overwrite_existing, copyEc);

    ws.instances().push_back(instance);
    ws.saveInstances();
    ws.config().lastInstance = instance.uuid;
    ws.saveConfig();

    result.ok = true;
    result.instanceId = instance.uuid;
    result.instanceName = instance.name;
    return result;
}

} // namespace flf

