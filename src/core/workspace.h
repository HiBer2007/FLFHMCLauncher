#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace flf {

namespace fs = std::filesystem;

/// 某一层（mods / minecraft / hmclHome）的隔离方式。
struct Layer {
    std::string mode = "private";  // private / shared / group
    std::string group = "default";
};

struct Instance {
    std::string id;
    std::string name;
    std::string gameVersion;
    std::string loader = "fabric";
    std::string loaderVersion;
    std::string javaPath;      // 空 = 自动
    Layer mods;
    Layer minecraft;
    Layer hmclHome;
    std::int64_t createdAt = 0;
};

struct AppConfig {
    int schema = 1;
    std::string hmclVersion;
    std::string agentJar;      // 可覆盖 Agent 路径
    std::string lastInstance;
    nlohmann::json mirrors = nlohmann::json::object();
};

/// 工作区：`.flfhmcl` 根、配置、实例列表。
class Workspace {
public:
    static Workspace& get();

    void initialize();

    fs::path home() const;
    fs::path launcherDir() const;

    AppConfig& config();
    std::vector<Instance>& instances();

    Instance* find(const std::string& id);
    const Instance* find(const std::string& id) const;

    void saveConfig();
    void saveInstances();

    fs::path instanceDir(const Instance& instance) const;
    fs::path sharedDir(const char* kind, const Layer& layer) const;
    fs::path resolveMods(const Instance& instance) const;
    fs::path resolveMinecraft(const Instance& instance) const;
    fs::path resolveHmclHome(const Instance& instance) const;

    fs::path librariesDir() const;
    fs::path hmclJar() const;
    fs::path agentJar() const;
    fs::path javaDir() const;
    fs::path fabricDir(const Instance& instance) const;

private:
    Workspace() = default;

    fs::path home_;
    fs::path launcherDir_;
    AppConfig config_;
    std::vector<Instance> instances_;
};

nlohmann::json toJson(const Instance& instance);
Instance instanceFromJson(const nlohmann::json& json);

} // namespace flf
