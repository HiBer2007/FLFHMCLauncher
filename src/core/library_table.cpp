#include "core/library_table.h"

#include "core/http.h"
#include "core/workspace.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <system_error>

namespace flf {
namespace {

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

} // namespace

LibraryTable& LibraryTable::instance() {
    static LibraryTable table;
    return table;
}

LibraryTable::LibraryTable() {
    file_ = Workspace::get().home() / "libraries.json";
    load();
}

std::string LibraryTable::keyOf(const std::string& group, const std::string& artifact,
                                const std::string& version, const std::string& classifier) {
    return group + ":" + artifact + ":" + version + ":" + classifier;
}

void LibraryTable::load() {
    entries_.clear();
    if (!fs::exists(file_)) {
        return;
    }
    try {
        std::ifstream input(file_);
        nlohmann::json json;
        input >> json;
        for (const auto& item : json.value("libraries", nlohmann::json::array())) {
            LibraryRecord record;
            record.group = item.value("group", "");
            record.artifact = item.value("artifact", "");
            record.version = item.value("version", "");
            record.classifier = item.value("classifier", "");
            record.file = item.value("file", "");
            record.source = item.value("source", "");
            record.size = item.value("size", static_cast<std::uintmax_t>(0));
            if (!record.file.empty()) {
                entries_.push_back(std::move(record));
            }
        }
    } catch (...) {
        entries_.clear();
    }
}

void LibraryTable::save() {
    nlohmann::json json;
    json["version"] = 1;
    json["libraries"] = nlohmann::json::array();
    for (const auto& entry : entries_) {
        json["libraries"].push_back({
            {"group", entry.group},
            {"artifact", entry.artifact},
            {"version", entry.version},
            {"classifier", entry.classifier},
            {"file", entry.file},
            {"source", entry.source},
            {"size", entry.size},
        });
    }
    std::error_code ec;
    fs::create_directories(file_.parent_path(), ec);
    std::ofstream output(file_);
    output << json.dump(2);
}

void LibraryTable::record(const LibraryRecord& entry) {
    const std::string key = keyOf(entry.group, entry.artifact, entry.version, entry.classifier);
    for (auto& existing : entries_) {
        if (keyOf(existing.group, existing.artifact, existing.version, existing.classifier) == key) {
            existing = entry;
            return;
        }
    }
    entries_.push_back(entry);
}

bool LibraryTable::ensure(const std::string& group,
                          const std::string& artifact,
                          const std::string& version,
                          const std::string& classifier,
                          const std::string& baseUrl,
                          std::string* error) {
    const fs::path target = Workspace::get().librariesDir()
            / flatNameFor(group, artifact, version, classifier);
    const std::string key = keyOf(group, artifact, version, classifier);

    const LibraryRecord* existing = nullptr;
    for (const auto& entry : entries_) {
        if (keyOf(entry.group, entry.artifact, entry.version, entry.classifier) == key) {
            existing = &entry;
            break;
        }
    }

    std::error_code ec;
    const bool present = fs::exists(target, ec);
    if (present) {
        const auto size = fs::file_size(target, ec);
        if (!ec && (existing == nullptr || existing->size != size)) {
            record({group, artifact, version, classifier, target.filename().string(), "", size});
            save();
        }
        return true;
    }

    const std::string url = baseUrl + mavenPathFor(group, artifact, version, classifier);
    if (!httpDownload(url, target, nullptr, error)) {
        return false;
    }
    const auto size = fs::file_size(target, ec);
    record({group, artifact, version, classifier, target.filename().string(), url,
            ec ? 0 : static_cast<std::uintmax_t>(size)});
    save();
    return true;
}

} // namespace flf
