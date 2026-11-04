#include "core/meta.h"

#include "core/http.h"
#include "core/workspace.h"

#include <QProcess>
#include <QStandardPaths>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace flf {
namespace {

std::string jsonString(const nlohmann::json& json, const char* key,
                       const std::string& fallback = {}) {
    if (!json.contains(key)) {
        return fallback;
    }
    if (json[key].is_string()) {
        return json[key].get<std::string>();
    }
    return fallback;
}

std::string urlEncode(const std::string& value) {
    std::string result;
    char buffer[4];
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            result.push_back(static_cast<char>(c));
        } else {
            std::snprintf(buffer, sizeof(buffer), "%%%02X", c);
            result += buffer;
        }
    }
    return result;
}

fs::path findJavaExe(const fs::path& root) {
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        return {};
    }
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); ++it) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (it->is_regular_file(ec) && it->path().filename() == "java.exe") {
            return it->path();
        }
    }
    return {};
}

} // namespace

std::vector<VersionEntry> fetchVersionList(std::string* error) {
    std::vector<VersionEntry> versions;
    const HttpResult result = httpGet("https://piston-meta.mojang.com/mc/game/version_manifest.json");
    if (!result.ok) {
        if (error) *error = result.error;
        return versions;
    }
    try {
        const auto json = nlohmann::json::parse(result.body);
        for (const auto& item : json.at("versions")) {
            VersionEntry entry;
            entry.id = item.value("id", "");
            entry.type = item.value("type", "");
            entry.url = item.value("url", "");
            if (!entry.id.empty()) {
                versions.push_back(entry);
            }
        }
    } catch (const std::exception& e) {
        if (error) *error = std::string("解析版本列表失败: ") + e.what();
        versions.clear();
    }
    return versions;
}

std::vector<std::string> fetchFabricLoaders(const std::string& gameVersion, std::string* error) {
    std::vector<std::string> loaders;
    const std::string url = "https://meta.fabricmc.net/v2/versions/loader/" + urlEncode(gameVersion);
    const HttpResult result = httpGet(url);
    if (!result.ok) {
        if (error) *error = result.error;
        return loaders;
    }
    try {
        const auto json = nlohmann::json::parse(result.body);
        for (const auto& item : json) {
            const std::string version = item.value("loader", nlohmann::json::object())
                                                .value("version", "");
            if (!version.empty()) {
                loaders.push_back(version);
            }
        }
    } catch (const std::exception& e) {
        if (error) *error = std::string("解析 Fabric 版本失败: ") + e.what();
    }
    return loaders;
}

nlohmann::json fetchFabricProfile(const std::string& gameVersion,
                                  const std::string& loaderVersion,
                                  std::string* error) {
    const std::string url = "https://meta.fabricmc.net/v2/versions/loader/"
            + urlEncode(gameVersion) + "/" + urlEncode(loaderVersion) + "/profile/json";
    const HttpResult result = httpGet(url);
    if (!result.ok) {
        if (error) *error = result.error;
        return {};
    }
    try {
        return nlohmann::json::parse(result.body);
    } catch (const std::exception& e) {
        if (error) *error = std::string("解析 Fabric profile 失败: ") + e.what();
        return {};
    }
}

HmclRelease fetchHmclRelease(std::string* error) {
    HmclRelease release;
    const std::string url =
            "https://hmcl.huangyuhui.net/api/update_link?version=0.0&channel=stable";
    const HttpResult result = httpGet(url);
    if (!result.ok) {
        release.error = result.error;
        if (error) *error = result.error;
        return release;
    }
    try {
        const auto json = nlohmann::json::parse(result.body);
        release.version = json.value("version", "");
        release.jarUrl = json.value("jar", "");
        release.sha1 = json.value("jarsha1", "");
        release.ok = !release.version.empty() && !release.jarUrl.empty();
        if (!release.ok) {
            release.error = "HMCL 更新信息不完整";
        }
    } catch (const std::exception& e) {
        release.error = std::string("解析 HMCL 更新信息失败: ") + e.what();
    }
    if (error && !release.error.empty()) *error = release.error;
    return release;
}

std::string javaFxUrl(const std::string& component, const std::string& version) {
    return "https://repo1.maven.org/maven2/org/openjfx/javafx-" + component + "/"
            + version + "/javafx-" + component + "-" + version + "-win.jar";
}

std::vector<JavaInfo> detectJava() {
    std::vector<JavaInfo> result;

    auto add = [&](const fs::path& exe, const std::string& version) {
        if (exe.empty()) return;
        std::error_code ec;
        if (!fs::exists(exe, ec)) return;
        for (const auto& item : result) {
            if (item.path == exe.string()) return;
        }
        result.push_back({exe.string(), version});
    };

    std::error_code ec;
    const fs::path managed = Workspace::get().javaDir();
    if (fs::exists(managed, ec)) {
        for (const auto& entry : fs::directory_iterator(managed, ec)) {
            if (!entry.is_directory()) continue;
            const fs::path exe = findJavaExe(entry.path());
            add(exe, entry.path().filename().string());
        }
    }

    if (const char* javaHome = std::getenv("JAVA_HOME")) {
        add(fs::u8path(javaHome) / "bin" / "java.exe", "JAVA_HOME");
    }

    const QString found = QStandardPaths::findExecutable("java");
    if (!found.isEmpty()) {
        add(fs::path(found.toStdWString()), "PATH");
    }

    for (const fs::path& root : {
             fs::u8path("C:/Program Files/Java"),
             fs::u8path("C:/Program Files/Eclipse Adoptium"),
             fs::u8path("C:/Program Files/Microsoft")}) {
        if (!fs::exists(root, ec)) continue;
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (!entry.is_directory()) continue;
            add(findJavaExe(entry.path()), entry.path().filename().string());
        }
    }

    return result;
}

std::string installJava(const std::function<bool(std::int64_t, std::int64_t)>& progress,
                        std::string* error) {
    const fs::path target = Workspace::get().javaDir() / "temurin-21";
    const fs::path javaExe = target / "bin" / "java.exe";
    if (fs::exists(javaExe)) {
        return javaExe.string();
    }

    const fs::path archive = Workspace::get().home() / "cache" / "downloads" / "temurin-21.zip";
    const std::string url =
            "https://api.adoptium.net/v3/binary/latest/21/ga/windows/x64/jdk/hotspot/normal/eclipse";
    if (!httpDownload(url, archive, progress, error)) {
        return {};
    }

    std::error_code ec;
    fs::create_directories(target, ec);
    const int code = QProcess::execute("tar", {
            "-xf", QString::fromStdWString(archive.wstring()),
            "-C", QString::fromStdWString(target.wstring())});
    if (code != 0) {
        if (error) *error = "解压 Java 失败（tar 返回 " + std::to_string(code) + "）";
        return {};
    }

    const fs::path found = findJavaExe(target);
    if (found.empty()) {
        if (error) *error = "解压后未找到 java.exe";
        return {};
    }
    fs::remove(archive, ec);
    return found.string();
}

} // namespace flf
