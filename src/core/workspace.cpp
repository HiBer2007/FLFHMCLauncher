#include "core/workspace.h"

#include "core/flf_paths.h"

#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_set>
#include <stdexcept>

namespace flf {
namespace {

nlohmann::json layerToJson(const Layer& layer) {
    return {{"mode", layer.mode}, {"group", layer.group}};
}

Layer layerFromJson(const nlohmann::json& json) {
    Layer layer;
    layer.mode = json.value("mode", "private");
    layer.group = json.value("group", "default");
    if (layer.mode != "private" && layer.mode != "shared" && layer.mode != "group") {
        layer.mode = "private";
    }
    return layer;
}

std::string sanitizeId(const std::string& value) {
    std::string id;
    for (char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                || c == '-' || c == '_') {
            id.push_back(c);
        } else if (!id.empty() && id.back() != '-') {
            id.push_back('-');
        }
    }
    return id;
}

bool writeJson(const fs::path& file, const nlohmann::json& json) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << json.dump(2);
    return out.good();
}

} // namespace

std::string newInstanceUuid() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 15);
    std::ostringstream out;
    out << std::hex;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) out << "-";
        if (i == 12) out << "4";
        else if (i == 16) out << "a";
        else out << dist(rng);
    }
    return out.str();
}

nlohmann::json toJson(const Instance& instance) {
    return {
        {"uuid", instance.uuid},
        {"id", instance.id},
        {"name", instance.name},
        {"gameVersion", instance.gameVersion},
        {"loader", instance.loader},
        {"loaderVersion", instance.loaderVersion},
        {"javaPath", instance.javaPath},
        {"mods", layerToJson(instance.mods)},
        {"minecraft", layerToJson(instance.minecraft)},
        {"hmclHome", layerToJson(instance.hmclHome)},
        {"launchBehavior", instance.launchBehavior},
        {"createdAt", instance.createdAt},
    };
}

Instance instanceFromJson(const nlohmann::json& json) {
    Instance instance;
    instance.uuid = json.value("uuid", "");
    instance.id = json.value("id", "");
    instance.name = json.value("name", instance.id);
    instance.gameVersion = json.value("gameVersion", "");
    instance.loader = json.value("loader", "fabric");
    instance.loaderVersion = json.value("loaderVersion", "");
    instance.javaPath = json.value("javaPath", "");
    instance.launchBehavior = json.value("launchBehavior", "");
    instance.createdAt = json.value("createdAt", 0LL);
    if (json.contains("mods")) instance.mods = layerFromJson(json["mods"]);
    if (json.contains("minecraft")) instance.minecraft = layerFromJson(json["minecraft"]);
    if (json.contains("hmclHome")) instance.hmclHome = layerFromJson(json["hmclHome"]);
    return instance;
}

Workspace& Workspace::get() {
    static Workspace workspace;
    return workspace;
}

void Workspace::initialize() {
    home_ = flf::ensureLayout();
    launcherDir_ = flf::executableDirectory();

    const fs::path configFile = home_ / "config" / "launcher.json";
    const fs::path instancesFile = home_ / "config" / "instances.json";

    if (fs::exists(configFile)) {
        try {
            std::ifstream in(configFile, std::ios::binary);
            nlohmann::json json;
            in >> json;
            config_.schema = json.value("schema", 1);
            config_.hmclVersion = json.value("hmclVersion", "");
            config_.agentJar = json.value("agentJar", "");
            config_.lastInstance = json.value("lastInstance", "");
            config_.mirrors = json.value("mirrors", nlohmann::json::object());
            config_.launchBehavior = json.value("launchBehavior", "keep");
        } catch (...) {
            // 配置损坏时使用默认值
        }
    }

    if (fs::exists(instancesFile)) {
        try {
            std::ifstream in(instancesFile, std::ios::binary);
            nlohmann::json json;
            in >> json;
            std::unordered_set<std::string> seenIds;
            std::unordered_set<std::string> seenUuids;
            for (const auto& item : json.value("instances", nlohmann::json::array())) {
                Instance instance = instanceFromJson(item);
                if (instance.id.empty()) {
                    continue;
                }
                // 兼容旧数据：同一 id 的重复条目只保留一份（它们指向同一目录）
                if (!seenIds.insert(instance.id).second) {
                    continue;
                }
                if (instance.uuid.empty() || !seenUuids.insert(instance.uuid).second) {
                    instance.uuid = newInstanceUuid();
                    seenUuids.insert(instance.uuid);
                }
                instances_.push_back(instance);
            }
            saveInstances();
        } catch (...) {
            // 实例文件损坏时保持为空
        }
    }
}

fs::path Workspace::home() const {
    return home_;
}

fs::path Workspace::launcherDir() const {
    return launcherDir_;
}

AppConfig& Workspace::config() {
    return config_;
}

std::vector<Instance>& Workspace::instances() {
    return instances_;
}

Instance* Workspace::find(const std::string& id) {
    for (auto& instance : instances_) {
        if (!instance.uuid.empty() && instance.uuid == id) {
            return &instance;
        }
    }
    for (auto& instance : instances_) {
        if (instance.id == id) {
            return &instance;
        }
    }
    return nullptr;
}

const Instance* Workspace::find(const std::string& id) const {
    for (const auto& instance : instances_) {
        if (instance.id == id) {
            return &instance;
        }
    }
    return nullptr;
}

void Workspace::saveConfig() {
    nlohmann::json json = {
        {"schema", config_.schema},
        {"hmclVersion", config_.hmclVersion},
        {"agentJar", config_.agentJar},
        {"lastInstance", config_.lastInstance},
        {"mirrors", config_.mirrors},
        {"launchBehavior", config_.launchBehavior},
    };
    writeJson(home_ / "config" / "launcher.json", json);
}

void Workspace::saveInstances() {
    nlohmann::json json;
    json["instances"] = nlohmann::json::array();
    for (const auto& instance : instances_) {
        json["instances"].push_back(toJson(instance));
    }
    writeJson(home_ / "config" / "instances.json", json);
}

fs::path Workspace::instanceDir(const Instance& instance) const {
    return home_ / "instances" / instance.id;
}

fs::path Workspace::sharedDir(const char* kind, const Layer& layer) const {
    const std::string group = layer.group.empty() ? "default" : sanitizeId(layer.group);
    return home_ / "shared" / kind / group;
}

fs::path Workspace::resolveMods(const Instance& instance) const {
    if (instance.mods.mode == "private") {
        return instanceDir(instance) / "mods";
    }
    return sharedDir("mods", instance.mods);
}

fs::path Workspace::resolveMinecraft(const Instance& instance) const {
    if (instance.minecraft.mode == "private") {
        return instanceDir(instance) / "minecraft";
    }
    return sharedDir("minecraft", instance.minecraft);
}

fs::path Workspace::resolveHmclHome(const Instance& instance) const {
    if (instance.hmclHome.mode == "private") {
        return instanceDir(instance) / "hmcl-home";
    }
    return sharedDir("hmcl-home", instance.hmclHome);
}

fs::path Workspace::librariesDir() const {
    return home_ / "libraries";
}

fs::path Workspace::hmclJar() const {
    return home_ / "hmcl" / "HMCL.jar";
}

fs::path Workspace::agentJar() const {
    if (!config_.agentJar.empty()) {
        return fs::u8path(config_.agentJar);
    }
    const fs::path local = launcherDir_ / "FLFHMCL-agent.jar";
    if (fs::exists(local)) {
        return local;
    }
    return home_ / "hmcl" / "FLFHMCL-agent.jar";
}

fs::path Workspace::javaDir() const {
    return home_ / "java";
}

fs::path Workspace::fabricDir(const Instance& instance) const {
    return home_ / "fabric" / instance.gameVersion / instance.loaderVersion;
}

} // namespace flf
