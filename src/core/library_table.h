#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace flf {

namespace fs = std::filesystem;

struct LibraryRecord {
    std::string group;
    std::string artifact;
    std::string version;
    std::string classifier;
    std::string file;
    std::string source;
    std::string sha256;
    std::uintmax_t size = 0;
};

/// 已安装库的记录表（libraries.json），用于跳过重复下载、校验文件与生成 mclhr 清单。
class LibraryTable {
public:
    static LibraryTable& instance();

    /// 确保库存在：按记录校验大小；缺失/损坏时重新下载并记录。
    bool ensure(const std::string& group,
                const std::string& artifact,
                const std::string& version,
                const std::string& classifier,
                const std::string& baseUrl,
                std::string* error);

    const std::vector<LibraryRecord>& entries() const { return entries_; }

    /// 记录表路径（.flfhmcl/libraries.json）。
    const fs::path& file() const { return file_; }

private:
    LibraryTable();

    void load();
    void save();
    void record(const LibraryRecord& entry);
    static std::string keyOf(const std::string& group, const std::string& artifact,
                             const std::string& version, const std::string& classifier);

    fs::path file_;
    std::vector<LibraryRecord> entries_;
};

} // namespace flf
