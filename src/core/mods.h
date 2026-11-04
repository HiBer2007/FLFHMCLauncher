#pragma once

#include "core/workspace.h"

#include <filesystem>
#include <string>
#include <vector>

namespace flf {

struct ModEntry {
    fs::path path;
    std::string fileName;
    bool enabled = true;
};

std::vector<ModEntry> listMods(const Instance& instance);
bool setModEnabled(const Instance& instance, const ModEntry& mod, bool enabled, std::string* error);
bool importMod(const Instance& instance, const fs::path& source, std::string* error);
bool deleteMod(const ModEntry& mod, std::string* error);

} // namespace flf
