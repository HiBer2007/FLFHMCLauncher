#include "core/installer.h"

#include "core/http.h"
#include "core/meta.h"
#include "core/library_table.h"

#include <windows.h>

#include <algorithm>
#include <fstream>
#include <set>

namespace flf {
namespace {

/// JavaFX 主版本必须与 JRE/JDK 主版本一致（17/21/22/...）。
std::string javaFxVersionFor(int javaMajor) {
    switch (javaMajor) {
        case 17: return "17.0.13";
        case 21: return "21.0.13";
        case 22: return "22.0.2";
        case 23: return "23.0.2";
        case 24: return "24.0.1";
        default: return javaMajor >= 21 ? "21.0.13" : "17.0.13";
    }
}

int detectJavaMajor(const std::string& javaExe) {
    try {
        fs::path home = fs::u8path(javaExe).parent_path().parent_path();
        std::ifstream release(home / "release");
        std::string line;
        while (std::getline(release, line)) {
            if (line.rfind("JAVA_VERSION=", 0) == 0) {
                std::string value = line.substr(13);
                if (!value.empty() && value.front() == '"') {
                    value.erase(0, 1);
                }
                int major = std::atoi(value.c_str());
                return major > 0 ? major : 21;
            }
        }
    } catch (...) {
        // fall through
    }
    return 21;
}

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

bool installJavaFx(const Instance& instance, const ProgressFn& progress, std::string* error) {
    const int javaMajor = detectJavaMajor(instance.javaPath);
    const std::string javaFxVersion = javaFxVersionFor(javaMajor);
    for (const char* component : {"base", "graphics", "controls"}) {
        if (!LibraryTable::instance().ensure(
                "org.openjfx", std::string("javafx-") + component,
                javaFxVersion, "win", "https://repo1.maven.org/maven2/", error)) {
            return false;
        }
    }
    progress("JavaFX " + javaFxVersion + " 已就绪（JDK " + std::to_string(javaMajor) + "）", 25);
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
        if (!LibraryTable::instance().ensure(group, artifact, version, classifier, baseUrl, error)) {
            return false;
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
    // FLFHMCL Agent 自身依赖
    static const Dependency flfDependencies[] = {
        {"org.apache.logging.log4j", "log4j-api", "2.24.3"},
        {"org.apache.logging.log4j", "log4j-core", "2.24.3"},
        {"org.apache.logging.log4j", "log4j-slf4j2-impl", "2.24.3"},
        {"org.slf4j", "slf4j-api", "2.0.16"},
        {"org.ow2.sat4j", "org.ow2.sat4j.core", "2.3.6"},
        {"org.ow2.sat4j", "org.ow2.sat4j.pb", "2.3.6"},
    };
    // HMCL 自身依赖
    static const Dependency hmclDependencies[] = {
        {"com.google.code.gson", "gson", "2.10.1"},
        {"com.google.guava", "guava", "33.0.0-jre"},
        {"com.google.guava", "failureaccess", "1.0.2"},
        {"com.google.guava", "listenablefuture", "9999.0-empty-to-avoid-conflict-with-guava"},
        {"com.google.code.findbugs", "jsr305", "3.0.2"},
        {"org.checkerframework", "checker-qual", "3.41.0"},
        {"com.google.errorprone", "error_prone_annotations", "2.23.0"},
    };

    auto installAll = [&](const Dependency* list, std::size_t count,
                          const char* owner, int basePercent, int span) {
        for (std::size_t i = 0; i < count; ++i) {
            const auto& dependency = list[i];
            if (!LibraryTable::instance().ensure(dependency.group, dependency.artifact,
                                                 dependency.version, "",
                                                 "https://repo1.maven.org/maven2/", error)) {
                return false;
            }
            const int percent = basePercent + static_cast<int>((i + 1) * span / count);
            progress(std::string(owner) + " " + std::to_string(i + 1) + "/" + std::to_string(count),
                     percent);
        }
        return true;
    };

    if (!installAll(flfDependencies, std::size(flfDependencies), "FLFHMCL 依赖", 62, 4)) {
        return false;
    }
    if (!installAll(hmclDependencies, std::size(hmclDependencies), "HMCL 依赖", 66, 4)) {
        return false;
    }
    // LWJGL 各平台 natives
    static const char* lwjglArtifacts[] = {"lwjgl", "lwjgl-glfw", "lwjgl-opengl"};
    for (const char* artifact : lwjglArtifacts) {
        if (!LibraryTable::instance().ensure("org.lwjgl", artifact, "3.3.6",
                                             "natives-windows",
                                             "https://repo1.maven.org/maven2/", error)) {
            return false;
        }
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

    const std::string javaFxVersion = javaFxVersionFor(detectJavaMajor(instance.javaPath));
    nlohmann::json libraries = nlohmann::json::array();
    std::set<std::string> written;
    for (const auto& entry : LibraryTable::instance().entries()) {
        if (entry.group.empty()) {
            continue;
        }
        // JavaFX 必须与当前实例的 JDK 主版本一致，忽略其他版本
        if (entry.group == "org.openjfx" && entry.version != javaFxVersion) {
            continue;
        }
        const std::string key = entry.group + ":" + entry.artifact + ":"
                + entry.version + ":" + entry.classifier;
        if (!written.insert(key).second) {
            continue;
        }
        nlohmann::json library = {
            {"group", entry.group},
            {"artifact", entry.artifact},
            {"version", entry.version},
        };
        if (!entry.classifier.empty()) {
            library["classifier"] = entry.classifier;
        }
        libraries.push_back(std::move(library));
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
    const fs::path versionFile = Workspace::get().home() / "agent.version";

    std::string metadataError;
    const FlfRelease release = fetchFlfRelease("flfhmcl", &metadataError);
    if (release.ok) {
        std::string installedVersion;
        if (fs::exists(versionFile)) {
            installedVersion = readFile(versionFile);
        }
        if (fs::exists(target) && installedVersion == release.version) {
            return true;
        }
        const fs::path temp = target.string() + ".tmp";
        if (httpDownload(release.downloadLink, temp, nullptr, error)) {
            if (!release.sha256.empty()) {
                const std::string actual = sha256File(temp);
                if (actual != release.sha256) {
                    std::error_code ec;
                    fs::remove(temp, ec);
                    if (error) *error = "FLFHMCL 下载校验失败";
                    return false;
                }
            }
            std::error_code ec;
            fs::create_directories(target.parent_path(), ec);
            fs::rename(temp, target, ec);
            if (!ec) {
                std::ofstream(versionFile) << release.version;
                return true;
            }
        }
    }

    if (fs::exists(target)) {
        return true;
    }
    const fs::path local = Workspace::get().launcherDir() / "FLFHMCL-agent.jar";
    if (fs::exists(local)) {
        return copyFile(local, target, error);
    }
    if (error) {
        *error = "无法获取 FLFHMCL Agent: " + (metadataError.empty() ? "本地文件缺失" : metadataError);
    }
    return false;
}

bool installFlfApi(Instance& instance, std::string* error) {
    std::string metadataError;
    const FlfRelease release = fetchFlfRelease("flfapi", &metadataError);
    if (!release.ok) {
        // 元数据尚未提供 FLF API 时跳过，不阻塞实例安装
        return true;
    }
    const std::string fileName = release.downloadLink.substr(release.downloadLink.find_last_of('/') + 1);
    const fs::path target = Workspace::get().resolveMods(instance) / fileName;
    if (fs::exists(target)) {
        return true;
    }
    if (!httpDownload(release.downloadLink, target, nullptr, error)) {
        return false;
    }
    if (!release.sha256.empty() && sha256File(target) != release.sha256) {
        std::error_code ec;
        fs::remove(target, ec);
        if (error) *error = "FLF API 下载校验失败";
        return false;
    }
    return true;
}

bool installFlfDependencies(const FlfRelease& release, std::string* error) {
    for (const auto& dependency : release.dependencies) {
        if (!LibraryTable::instance().ensure(dependency.group, dependency.artifact,
                                             dependency.version, dependency.classifier,
                                             "https://repo1.maven.org/maven2/", error)) {
            return false;
        }
    }
    return true;
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
    if (!installJavaFx(instance, progress, error)) {
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

    if (!installFlfApi(instance, error)) {
        return false;
    }

    std::string flfMetadataError;
    const FlfRelease flfRelease = fetchFlfRelease("flfhmcl", &flfMetadataError);
    if (flfRelease.ok && !installFlfDependencies(flfRelease, error)) {
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
