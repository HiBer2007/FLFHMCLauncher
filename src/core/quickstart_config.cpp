#include "core/quickstart_config.h"

#include <fstream>
#include <sstream>

namespace flf {
namespace {

/// 极简 JSON 字符串字段提取（quickstart.json 只有平铺字符串字段）。
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

} // namespace

bool loadQuickStartConfig(const std::filesystem::path& exeDir,
                          QuickStartConfig& out,
                          std::string& error) {
    const auto file = exeDir / "quickstart.json";
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        error = "quickstart.json not found: " + file.string();
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    const std::string json = buffer.str();

    std::string root;
    std::string instance;
    if (!extractString(json, "root", root)) {
        error = "quickstart.json missing root";
        return false;
    }
    if (!extractString(json, "instance", instance)) {
        error = "quickstart.json missing instance";
        return false;
    }

    out.launcherRoot = std::filesystem::path(root);
    out.instance = instance;
    extractString(json, "java", out.java);
    return true;
}

} // namespace flf
