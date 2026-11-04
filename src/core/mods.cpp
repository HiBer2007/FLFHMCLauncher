#include "core/mods.h"

#include <algorithm>

namespace flf {

std::vector<ModEntry> listMods(const Instance& instance) {
    std::vector<ModEntry> mods;
    const fs::path dir = Workspace::get().resolveMods(instance);
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        return mods;
    }
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        const bool disabled = name.size() > 9 && name.substr(name.size() - 9) == ".disabled";
        if (!disabled && entry.path().extension() != ".jar") {
            continue;
        }
        mods.push_back({entry.path(), name, !disabled});
    }
    std::sort(mods.begin(), mods.end(), [](const ModEntry& a, const ModEntry& b) {
        return a.fileName < b.fileName;
    });
    return mods;
}

bool setModEnabled(const Instance& instance, const ModEntry& mod, bool enabled, std::string* error) {
    const fs::path source = mod.path;
    fs::path target;
    if (enabled) {
        if (source.filename().string().size() > 9) {
            const std::string name = source.filename().string();
            if (name.substr(name.size() - 9) == ".disabled") {
                target = source.parent_path() / name.substr(0, name.size() - 9);
            }
        }
    } else {
        target = source.parent_path() / (source.filename().string() + ".disabled");
    }
    if (target.empty()) {
        return true;
    }
    std::error_code ec;
    fs::rename(source, target, ec);
    if (ec) {
        if (error) *error = "重命名失败: " + ec.message();
        return false;
    }
    return true;
}

bool importMod(const Instance& instance, const fs::path& source, std::string* error) {
    const fs::path dir = Workspace::get().resolveMods(instance);
    std::error_code ec;
    fs::create_directories(dir, ec);
    fs::copy_file(source, dir / source.filename(), fs::copy_options::overwrite_existing, ec);
    if (ec) {
        if (error) *error = "导入模组失败: " + ec.message();
        return false;
    }
    return true;
}

bool deleteMod(const ModEntry& mod, std::string* error) {
    std::error_code ec;
    fs::remove(mod.path, ec);
    if (ec) {
        if (error) *error = "删除失败: " + ec.message();
        return false;
    }
    return true;
}

} // namespace flf
