#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace flf {

namespace fs = std::filesystem;

struct VersionEntry {
    std::string id;
    std::string type;
    std::string url;
};

struct HmclRelease {
    bool ok = false;
    std::string version;
    std::string jarUrl;
    std::string sha1;
    std::string error;
};

struct JavaInfo {
    std::string path;
    std::string version;
};

std::vector<VersionEntry> fetchVersionList(std::string* error);
std::vector<std::string> fetchFabricLoaders(const std::string& gameVersion, std::string* error);
nlohmann::json fetchFabricProfile(const std::string& gameVersion,
                                  const std::string& loaderVersion,
                                  std::string* error);

struct FlfDependency {
    std::string group;
    std::string artifact;
    std::string version;
    std::string classifier;
};

struct FlfRelease {
    bool ok = false;
    std::string version;
    std::string downloadLink;
    std::string releasePage;
    std::string sha256;
    std::vector<FlfDependency> dependencies;
    std::string error;
};

/// 从 metadata.hibernet.top 获取 FLF 组件元数据（flfhmcl / flfapi）。
FlfRelease fetchFlfRelease(const std::string& type, std::string* error);

HmclRelease fetchHmclRelease(std::string* error);

std::string javaFxUrl(const std::string& component, const std::string& version);

/// 扫描本机与 .flfhmcl/java 下可用的 Java。
std::vector<JavaInfo> detectJava();

/// 通过 Adoptium 下载 Temurin 21 并解压，返回 java.exe 路径；失败返回空。
std::string installJava(const std::function<bool(std::int64_t, std::int64_t)>& progress,
                        std::string* error);

} // namespace flf
