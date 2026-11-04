#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace flf {

namespace fs = std::filesystem;

struct HttpResult {
    bool ok = false;
    int status = 0;
    std::string body;
    std::string error;
};

/// 同步 GET（WinHTTP，支持 HTTPS / 自动重定向）。
HttpResult httpGet(const std::string& url);

/// 同步下载到文件；progress(已接收, 总大小) 返回 false 可取消。
bool httpDownload(const std::string& url,
                  const fs::path& destination,
                  const std::function<bool(std::int64_t, std::int64_t)>& progress,
                  std::string* error);

/// 计算文件 SHA-1（小写十六进制）。
std::string sha1File(const fs::path& file);

/// 读取小文件为字符串。
std::string readFile(const fs::path& file);

} // namespace flf
