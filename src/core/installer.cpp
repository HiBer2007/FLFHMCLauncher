#include "core/installer.h"

#include "core/http.h"
#include "core/meta.h"

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <set>

namespace flf {
namespace {

constexpr const char* kJavaFxVersion = "17.0.13";

std::string mavenPathFor(const std::string& group,
                         const std::string& artifact,
                         const std::string& version,
                         const std::string& classifier) {
    std::string path = group;
    std::replace(path.begin(), path.end(), '.', '/');
    path += "/" + artifact + "/" + version + "/" + artifact + "-" + version;
    if (!classifier.empty()) {
        path += "-" + classifier;
    }
    return path + ".jar";
}

std::string flatNameFor(const std::string& group,
                        const std::string& artifact,
                        const std::string& version,
                        const std::string& classifier) {
    std::string name = group;
    std::replace(name.begin(), name.end(), '.', '_');
    name += "_" + artifact + "_" + version;
    if (!classifier.empty()) {
        name += "-" + classifier;
    }
    return name + ".jar";
}

void parseCoordinate(const std::string& coordinate,
                     std::string& group, std::string& artifact,
                     std::string& version, std::string& classifier) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : coordinate) {
        if (c == ':') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    if (parts.size() >= 3) {
        group = parts[0];
        artifact = parts[1];
        version = parts[2];
        if (parts.size() >= 4) classifier = parts[3];
    }
}

bool copyFile(const fs::path& from, const fs::path& to, std::string* error) {
    std::error_code ec;
    fs::create_directories(to.parent_path(), ec);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        if (error) *error = "复制失败: " + from.string() + " -> " + to.string();
        return false;
    }
    return true;
}

bool ensureJava(Instance& instance, const ProgressFn& progress, std::string* error) {
    if (!instance.javaPath.empty() && fs::exists(fs::u8path(instance.javaPath))) {
        return true;
    }
    const auto javas = detectJava();
    if (!javas.empty()) {
        instance.javaPath = javas.front().path;
        return true;
    }

    progress("下载 Java 21（Temurin）", 5);
    const std::string path = installJava([&](std::int64_t received, std::int64_t total) {
        if (total > 0) {
            progress("下载 Java 21（Temurin）",
                     5 + static_cast<int>(received * 15 / total));
        }
        return true;
    }, error);
    if (path.empty()) {
        return false;
    }
    instance.javaPath = path;
    return true;
}

bool installJavaFx(const ProgressFn& progress, std::string* error) {
    for (const char* component : {"base", "graphics", "controls"}) {
        const fs::path target = Workspace::get().librariesDir()
                / ("org_openjfx_javafx-" + std::string(component) + "_"
                   + kJavaFxVersion + "-win.jar");
        if (fs::exists(target)) {
            continue;
        }
        const std::string url = javaFxUrl(component, kJavaFxVersion);
        if (!httpDownload(url, target, nullptr, error)) {
            return false;
        }
    }
    progress("JavaFX 已就绪", 25);
    return true;
}

bool installFabric(Instance& instance, const ProgressFn& progress, std::string* error) {
    std::string fetchError;
    const auto profile = fetchFabricProfile(instance.gameVersion, instance.loaderVersion, &fetchError);
    if (profile.is_null() || profile.empty()) {
        if (error) *error = "获取 Fabric profile 失败: " + fetchError;
        return false;
    }

    const fs::path fabricDir = Workspace::get().fabricDir(instance);
    std::error_code ec;
    fs::create_directories(fabricDir, ec);
    std::ofstream(fabricDir / "profile.json") << profile.dump(2);

    const auto libraries = profile.value("libraries", nlohmann::json::array());
    int index = 0;
    for (const auto& library : libraries) {
        const std::string name = library.value("name", "");
        const std::string baseUrl = library.value("url", "https://maven.fabricmc.net/");
        std::string group, artifact, version, classifier;
        parseCoordinate(name, group, artifact, version, classifier);
        if (group.empty()) {
            continue;
        }
        const fs::path target = Workspace::get().librariesDir()
                / flatNameFor(group, artifact, version, classifier);
        if (!fs::exists(target)) {
            const std::string url = baseUrl + mavenPathFor(group, artifact, version, classifier);
            if (!httpDownload(url, target, nullptr, error)) {
                return false;
            }
        }
        ++index;
        progress("Fabric 库 " + std::to_string(index) + "/"
                         + std::to_string(libraries.size()), 30 + index * 30 / std::max<int>(1, libraries.size()));
    }
    progress("Fabric 已就绪", 60);
    return true;
}

bool installHmclDependencies(const ProgressFn& progress, std::string* error) {
    struct Dependency {
        const char* group;
        const char* artifact;
        const char* version;
    };
    static const Dependency dependencies[] = {
        {"org.apache.logging.log4j", "log4j-api", "2.24.3"},
        {"org.apache.logging.log4j", "log4j-core", "2.24.3"},
        {"org.apache.logging.log4j", "log4j-slf4j2-impl", "2.24.3"},
        {"org.slf4j", "slf4j-api", "2.0.16"},
        {"com.google.code.gson", "gson", "2.10.1"},
        {"com.google.guava", "guava", "33.0.0-jre"},
        {"com.google.guava", "failureaccess", "1.0.2"},
        {"com.google.guava", "listenablefuture", "9999.0-empty-to-avoid-conflict-with-guava"},
        {"com.google.code.findbugs", "jsr305", "3.0.2"},
        {"org.checkerframework", "checker-qual", "3.41.0"},
        {"com.google.errorprone", "error_prone_annotations", "2.23.0"},
        {"org.ow2.sat4j", "org.ow2.sat4j.core", "2.3.6"},
        {"org.ow2.sat4j", "org.ow2.sat4j.pb", "2.3.6"},
        {"org.lwjgl", "lwjgl", "3.3.6"},
        {"org.lwjgl", "lwjgl-glfw", "3.3.6"},
        {"org.lwjgl", "lwjgl-opengl", "3.3.6"},
    };

    int index = 0;
    const int total = static_cast<int>(std::size(dependencies));
    for (const auto& dependency : dependencies) {
        const fs::path target = Workspace::get().librariesDir()
                / flatNameFor(dependency.group, dependency.artifact, dependency.version, "");
        if (!fs::exists(target)) {
            const std::string url = "https://repo1.maven.org/maven2/"
                    + mavenPathFor(dependency.group, dependency.artifact, dependency.version, "");
            if (!httpDownload(url, target, nullptr, error)) {
                return false;
            }
        }
        ++index;
        progress("HMCL 依赖 " + std::to_string(index) + "/" + std::to_string(total),
                 62 + index * 10 / total);
    }
    return true;
}

bool installHmcl(const ProgressFn& progress, std::string* error) {
    std::string fetchError;
    const HmclRelease release = fetchHmclRelease(&fetchError);
    if (!release.ok) {
        if (error) *error = "获取 HMCL 版本失败: " + fetchError;
        return false;
    }
    const fs::path target = Workspace::get().hmclJar();
    if (!fs::exists(target) || sha1File(target) != release.sha1) {
        if (!httpDownload(release.jarUrl, target, nullptr, error)) {
            return false;
        }
    }
    if (!release.sha1.empty() && sha1File(target) != release.sha1) {
        if (error) *error = "HMCL jar SHA-1 校验失败";
        return false;
    }
    Workspace::get().config().hmclVersion = release.version;
    progress("HMCL " + release.version + " 已就绪", 88);
    return true;
}

bool writeMclhrManifest(const Instance& instance, std::string* error) {
    nlohmann::json manifest;
    manifest["modules"] = nlohmann::json::array();

    nlohmann::json libraries = nlohmann::json::array();
    for (const char* component : {"base", "graphics", "controls"}) {
        libraries.push_back({
            {"group", "org.openjfx"},
            {"artifact", std::string("javafx-") + component},
            {"version", kJavaFxVersion},
            {"classifier", "win"},
        });
    }

    static const struct { const char* group; const char* artifact; const char* version; } hmclDeps[] = {
        {"org.apache.logging.log4j", "log4j-api", "2.24.3"},
        {"org.apache.logging.log4j", "log4j-core", "2.24.3"},
        {"org.apache.logging.log4j", "log4j-slf4j2-impl", "2.24.3"},
        {"org.slf4j", "slf4j-api", "2.0.16"},
        {"com.google.code.gson", "gson", "2.10.1"},
        {"com.google.guava", "guava", "33.0.0-jre"},
        {"com.google.guava", "failureaccess", "1.0.2"},
        {"com.google.guava", "listenablefuture", "9999.0-empty-to-avoid-conflict-with-guava"},
        {"com.google.code.findbugs", "jsr305", "3.0.2"},
        {"org.checkerframework", "checker-qual", "3.41.0"},
        {"com.google.errorprone", "error_prone_annotations", "2.23.0"},
        {"org.ow2.sat4j", "org.ow2.sat4j.core", "2.3.6"},
        {"org.ow2.sat4j", "org.ow2.sat4j.pb", "2.3.6"},
        {"org.lwjgl", "lwjgl", "3.3.6"},
        {"org.lwjgl", "lwjgl-glfw", "3.3.6"},
        {"org.lwjgl", "lwjgl-opengl", "3.3.6"},
    };
    for (const auto& dependency : hmclDeps) {
        libraries.push_back({
            {"group", dependency.group},
            {"artifact", dependency.artifact},
            {"version", dependency.version},
        });
    }

    static const char* lwjglArtifacts[] = {"lwjgl", "lwjgl-glfw", "lwjgl-opengl"};
    for (const char* artifact : lwjglArtifacts) {
        libraries.push_back({{"group", "org.lwjgl"}, {"artifact", artifact}, {"version", "3.3.6"}});
        libraries.push_back({{"group", "org.lwjgl"}, {"artifact", artifact},
                             {"version", "3.3.6"}, {"classifier", "natives-windows"}});
    }

    const fs::path profileFile = Workspace::get().fabricDir(instance) / "profile.json";
    if (fs::exists(profileFile)) {
        try {
            const auto profile = nlohmann::json::parse(readFile(profileFile));
            for (const auto& library : profile.value("libraries", nlohmann::json::array())) {
                std::string group, artifact, version, classifier;
                parseCoordinate(library.value("name", ""), group, artifact, version, classifier);
                if (!group.empty()) {
                    libraries.push_back({
                        {"group", group},
                        {"artifact", artifact},
                        {"version", version},
                        {"classifier", classifier},
                    });
                }
            }
        } catch (...) {
            // 清单写不完整不阻塞启动
        }
    }

    manifest["modules"].push_back({
        {"id", "game"},
        {"libraries", libraries},
    });

    std::error_code ec;
    fs::create_directories(Workspace::get().instanceDir(instance), ec);
    std::ofstream out(Workspace::get().instanceDir(instance) / "mclhr.json", std::ios::trunc);
    out << manifest.dump(2);
    if (!out.good()) {
        if (error) *error = "写入 mclhr.json 失败";
        return false;
    }
    return true;
}

} // namespace

bool downloadHmcl(const std::function<bool(std::int64_t, std::int64_t)>& progress,
                  std::string* error) {
    std::string fetchError;
    const HmclRelease release = fetchHmclRelease(&fetchError);
    if (!release.ok) {
        if (error) *error = fetchError;
        return false;
    }
    return httpDownload(release.jarUrl, Workspace::get().hmclJar(), progress, error);
}

bool ensureAgent(std::string* error) {
    const fs::path target = Workspace::get().agentJar();

    if (fs::exists(target)) {
        return true;
    }
    const fs::path local = Workspace::get().launcherDir() / "FLFHMCL-agent.jar";
    if (fs::exists(local)) {
        return copyFile(local, target, error);
    }
    if (error) *error = "未找到 FLFHMCL-agent.jar（请放到启动器目录或 hmcl/ 下）";
    return false;
}

bool installInstance(Instance& instance, const ProgressFn& progress, std::string* error) {
    std::error_code ec;
    fs::create_directories(Workspace::get().instanceDir(instance), ec);
    fs::create_directories(Workspace::get().resolveMods(instance), ec);
    fs::create_directories(Workspace::get().resolveMinecraft(instance), ec);
    fs::create_directories(Workspace::get().resolveHmclHome(instance), ec);

    progress("准备 Java", 5);
    if (!ensureJava(instance, progress, error)) {
        return false;
    }

    progress("下载 JavaFX", 10);
    if (!installJavaFx(progress, error)) {
        return false;
    }

    progress("安装 Fabric", 30);
    if (!installFabric(instance, progress, error)) {
        return false;
    }

    progress("安装 HMCL 依赖", 62);
    if (!installHmclDependencies(progress, error)) {
        return false;
    }

    progress("安装 HMCL", 75);
    if (!installHmcl(progress, error)) {
        return false;
    }

    if (!ensureAgent(error)) {
        return false;
    }

    if (!writeMclhrManifest(instance, error)) {
        return false;
    }

    progress("完成", 100);
    return true;
}

bool ensureCrashTracker(const std::function<bool(std::int64_t, std::int64_t)>& progress,
                        std::string* error) {
    const fs::path toolDir = Workspace::get().home() / "tools" / "CrashTracker";
    const fs::path tool = toolDir / "CrashTracker.exe";
    if (!fs::exists(tool)) {
        const HttpResult response =
                httpGet("https://api.github.com/repos/HiBer2007/CrashTracker/releases/latest");
        if (!response.ok) {
            if (error) *error = response.error;
            return false;
        }
        try {
            const auto json = nlohmann::json::parse(response.body);
            std::string url;
            for (const auto& asset : json.value("assets", nlohmann::json::array())) {
                if (asset.value("name", "") == "CrashTracker.exe") {
                    url = asset.value("browser_download_url", "");
                }
            }
            if (url.empty()) {
                if (error) *error = "CrashTracker Release 中没有 CrashTracker.exe";
                return false;
            }
            if (!httpDownload(url, tool, progress, error)) {
                return false;
            }
        } catch (const std::exception& e) {
            if (error) *error = std::string("解析 CrashTracker Release 失败: ") + e.what();
            return false;
        }
    }

    const fs::path beside = Workspace::get().launcherDir() / "CrashTracker.exe";
    if (!fs::exists(beside)) {
        std::error_code ec;
        fs::copy_file(tool, beside, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            // 硬链接失败则忽略：崩溃时找不到工具会退化为普通退出
        }
    }
    return true;
}

} // namespace flf
