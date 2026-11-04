#include "core/http.h"

#include <windows.h>
#include <winhttp.h>

#include <QCryptographicHash>

#include <cstdlib>
#include <fstream>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>

namespace flf {
namespace {

std::wstring widen(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(),
                                         static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                        result.data(), size);
    return result;
}

struct UrlParts {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool secure = true;
};

bool crackUrl(const std::string& url, UrlParts& parts, std::string* error) {
    const std::wstring wide = widen(url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wide.c_str(), static_cast<DWORD>(wide.size()), 0, &components)) {
        if (error) *error = "invalid URL: " + url;
        return false;
    }
    parts.host.assign(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.lpszExtraInfo && components.dwExtraInfoLength > 0) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (path.empty()) {
        path = L"/";
    }
    parts.path = path;
    parts.port = components.nPort;
    parts.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
}

class WinHttpHandle {
public:
    explicit WinHttpHandle(HINTERNET handle = nullptr) : handle_(handle) {}
    ~WinHttpHandle() {
        if (handle_) WinHttpCloseHandle(handle_);
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    WinHttpHandle(WinHttpHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    WinHttpHandle& operator=(WinHttpHandle&& other) noexcept {
        if (this != &other) {
            if (handle_) WinHttpCloseHandle(handle_);
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }
    HINTERNET get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

private:
    HINTERNET handle_ = nullptr;
};

WinHttpHandle openSession() {
    return WinHttpHandle(WinHttpOpen(L"FLFHMCLauncher/0.1",
                                     WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0));
}

WinHttpHandle openRequest(HINTERNET session, const UrlParts& parts) {
    WinHttpHandle connect(WinHttpConnect(session, parts.host.c_str(), parts.port, 0));
    if (!connect) {
        return WinHttpHandle();
    }
    DWORD flags = parts.secure ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request(WinHttpOpenRequest(connect.get(), L"GET", parts.path.c_str(),
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (request) {
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY,
                         &policy, sizeof(policy));
        DWORD decompress = WINHTTP_DECOMPRESSION_FLAG_ALL;
        WinHttpSetOption(request.get(), WINHTTP_OPTION_DECOMPRESSION,
                         &decompress, sizeof(decompress));
    }
    return request;
}

bool queryStatus(HINTERNET request, int& status) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &value, &size,
                             WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    status = static_cast<int>(value);
    return true;
}

bool queryContentLength(HINTERNET request, std::int64_t& length) {
    wchar_t buffer[64]{};
    DWORD size = sizeof(buffer);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH,
                             WINHTTP_HEADER_NAME_BY_INDEX, buffer, &size,
                             WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    length = _wtoi64(buffer);
    return true;
}

} // namespace

HttpResult httpGet(const std::string& url) {
    HttpResult result;
    UrlParts parts;
    if (!crackUrl(url, parts, &result.error)) {
        return result;
    }

    WinHttpHandle session = openSession();
    if (!session) {
        result.error = "WinHttpOpen failed";
        return result;
    }
    WinHttpHandle request = openRequest(session.get(), parts);
    if (!request) {
        result.error = "WinHttpOpenRequest failed";
        return result;
    }
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
            || !WinHttpReceiveResponse(request.get(), nullptr)) {
        result.error = "HTTP request failed: " + url;
        return result;
    }

    queryStatus(request.get(), result.status);
    if (result.status < 200 || result.status >= 300) {
        result.error = "HTTP " + std::to_string(result.status) + ": " + url;
        return result;
    }

    DWORD available = 0;
    do {
        available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available)) {
            result.error = "WinHttpQueryDataAvailable failed";
            return result;
        }
        if (available == 0) {
            break;
        }
        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), available, &read)) {
            result.error = "WinHttpReadData failed";
            return result;
        }
        result.body.append(buffer.data(), read);
    } while (available > 0);

    result.ok = true;
    return result;
}

namespace {

bool addRange(HINTERNET request, std::int64_t start, std::int64_t end) {
    std::wstring header = L"Range: bytes=" + std::to_wstring(start);
    header += L"-";
    if (end >= 0) header += std::to_wstring(end);
    return WinHttpAddRequestHeaders(request, header.c_str(), static_cast<DWORD>(-1),
                                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE) != FALSE;
}

bool queryContentRange(HINTERNET request, std::int64_t& total) {
    wchar_t buffer[128]{};
    DWORD size = sizeof(buffer);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_RANGE,
                             WINHTTP_HEADER_NAME_BY_INDEX, buffer, &size,
                             WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    const std::wstring value(buffer);
    const auto slash = value.find_last_of(L'/');
    if (slash == std::wstring::npos) return false;
    total = _wtoi64(value.c_str() + slash + 1);
    return total > 0;
}

bool probeRange(const UrlParts& parts, std::int64_t& total) {
    WinHttpHandle session = openSession();
    if (!session) return false;
    WinHttpHandle request = openRequest(session.get(), parts);
    if (!request || !addRange(request.get(), 0, 0)) return false;
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
            || !WinHttpReceiveResponse(request.get(), nullptr)) {
        return false;
    }
    int status = 0;
    queryStatus(request.get(), status);
    return status == 206 && queryContentRange(request.get(), total);
}

bool readBodyToFile(HINTERNET request, std::ofstream& out,
                    const std::function<bool(std::int64_t)>& onBytes,
                    std::string* error) {
    DWORD available = 0;
    do {
        available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            if (error) *error = "WinHttpQueryDataAvailable failed";
            return false;
        }
        if (available == 0) break;
        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer.data(), available, &read)) {
            if (error) *error = "WinHttpReadData failed";
            return false;
        }
        out.write(buffer.data(), read);
        if (onBytes && !onBytes(read)) {
            if (error) *error = "已取消";
            return false;
        }
    } while (available > 0);
    return true;
}

} // namespace

bool httpDownload(const std::string& url,
                  const fs::path& destination,
                  const std::function<bool(std::int64_t, std::int64_t)>& progress,
                  std::string* error) {
    if (url.empty() || url.size() < 10) {
        if (error) *error = "下载 URL 为空";
        return false;
    }
    UrlParts parts;
    if (!crackUrl(url, parts, error)) return false;

    std::int64_t total = 0;
    const bool rangeSupported = probeRange(parts, total);
    std::error_code ec;
    fs::create_directories(destination.parent_path(), ec);
    const fs::path partial = destination.string() + ".part";
    std::int64_t existing = fs::exists(partial, ec)
            ? static_cast<std::int64_t>(fs::file_size(partial, ec)) : 0;
    if (ec || existing >= total) existing = 0;

    // 分片下载：大于 4MB 且支持 Range，且没有可续传的分片
    if (rangeSupported && total >= 4 * 1024 * 1024 && existing == 0) {
        constexpr int kChunks = 4;
        std::vector<std::thread> workers;
        std::vector<fs::path> chunkFiles;
        std::atomic<std::int64_t> received{0};
        std::atomic<bool> failed{false};
        std::mutex progressMutex;
        std::string firstError;

        for (int i = 0; i < kChunks; ++i) {
            const std::int64_t begin = total * i / kChunks;
            const std::int64_t end = total * (i + 1) / kChunks - 1;
            const fs::path chunk = partial.string() + "." + std::to_string(i);
            chunkFiles.push_back(chunk);
            workers.emplace_back([&, begin, end, chunk]() {
                WinHttpHandle session = openSession();
                WinHttpHandle request = session ? openRequest(session.get(), parts) : WinHttpHandle();
                if (!request || !addRange(request.get(), begin, end)
                        || !WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                        || !WinHttpReceiveResponse(request.get(), nullptr)) {
                    failed = true;
                    return;
                }
                int status = 0;
                queryStatus(request.get(), status);
                if (status != 206) {
                    failed = true;
                    return;
                }
                std::ofstream out(chunk, std::ios::binary | std::ios::trunc);
                if (!out) {
                    failed = true;
                    return;
                }
                if (!readBodyToFile(request.get(), out, [&](std::int64_t bytes) {
                        const auto now = received.fetch_add(bytes) + bytes;
                        if (progress) {
                            std::lock_guard<std::mutex> lock(progressMutex);
                            if (!progress(now, total)) return false;
                        }
                        return true;
                    }, &firstError)) {
                    failed = true;
                }
            });
        }
        for (auto& worker : workers) worker.join();
        if (!failed) {
            std::ofstream combined(partial, std::ios::binary | std::ios::trunc);
            for (const auto& chunk : chunkFiles) {
                std::ifstream input(chunk, std::ios::binary);
                combined << input.rdbuf();
                input.close();
                fs::remove(chunk, ec);
            }
            combined.close();
        } else {
            for (const auto& chunk : chunkFiles) fs::remove(chunk, ec);
        }
        if (failed) {
            if (error) *error = firstError.empty() ? "分片下载失败" : firstError;
            return false;
        }
    } else {
        WinHttpHandle session = openSession();
        if (!session) {
            if (error) *error = "WinHttpOpen failed";
            return false;
        }
        WinHttpHandle request = openRequest(session.get(), parts);
        if (!request) {
            if (error) *error = "WinHttpOpenRequest failed";
            return false;
        }
        const bool resuming = rangeSupported && existing > 0;
        if (resuming && !addRange(request.get(), existing, -1)) {
            if (error) *error = "无法设置续传范围";
            return false;
        }
        if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                || !WinHttpReceiveResponse(request.get(), nullptr)) {
            if (error) *error = "HTTP request failed: " + url;
            return false;
        }
        int status = 0;
        queryStatus(request.get(), status);
        if (status < 200 || status >= 300) {
            if (error) *error = "HTTP " + std::to_string(status) + ": " + url;
            return false;
        }
        if (!resuming) {
            queryContentLength(request.get(), total);
            existing = 0;
        }
        std::ofstream out(partial, std::ios::binary | (resuming ? std::ios::app : std::ios::trunc));
        if (!out) {
            if (error) *error = "无法写入: " + partial.string();
            return false;
        }
        std::int64_t received = existing;
        if (!readBodyToFile(request.get(), out, [&](std::int64_t bytes) {
                received += bytes;
                return !progress || progress(received, total);
            }, error)) {
            out.close();
            if (error && *error == "已取消") fs::remove(partial, ec);
            return false;
        }
        out.close();
    }

    fs::rename(partial, destination, ec);
    if (ec) {
        fs::remove(destination, ec);
        fs::rename(partial, destination, ec);
    }
    if (ec) {
        if (error) *error = "重命名失败: " + ec.message();
        return false;
    }
    if (progress) progress(total, total);
    return true;
}
std::string sha1File(const fs::path& file) {
    QCryptographicHash hash(QCryptographicHash::Sha1);
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return {};
    }
    std::vector<char> buffer(64 * 1024);
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        hash.addData(buffer.data(), static_cast<int>(in.gcount()));
    }
    return hash.result().toHex().toStdString();
}

std::string sha256File(const fs::path& file) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return {};
    }
    std::vector<char> buffer(64 * 1024);
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        hash.addData(buffer.data(), static_cast<int>(in.gcount()));
    }
    return hash.result().toHex().toStdString();
}

std::string readFile(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace flf
