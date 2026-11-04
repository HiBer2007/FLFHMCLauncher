#include "core/http.h"

#include <windows.h>
#include <winhttp.h>

#include <QCryptographicHash>

#include <cstdlib>
#include <fstream>
#include <vector>

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

bool httpDownload(const std::string& url,
                  const fs::path& destination,
                  const std::function<bool(std::int64_t, std::int64_t)>& progress,
                  std::string* error) {
    UrlParts parts;
    if (!crackUrl(url, parts, error)) {
        return false;
    }

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

    std::int64_t total = 0;
    queryContentLength(request.get(), total);

    std::error_code ec;
    fs::create_directories(destination.parent_path(), ec);
    const fs::path partial = destination.string() + ".part";
    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error) *error = "无法写入: " + partial.string();
        return false;
    }

    std::int64_t received = 0;
    DWORD available = 0;
    bool cancelled = false;
    do {
        available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available)) {
            if (error) *error = "WinHttpQueryDataAvailable failed";
            out.close();
            fs::remove(partial, ec);
            return false;
        }
        if (available == 0) {
            break;
        }
        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), available, &read)) {
            if (error) *error = "WinHttpReadData failed";
            out.close();
            fs::remove(partial, ec);
            return false;
        }
        out.write(buffer.data(), read);
        received += read;
        if (progress && !progress(received, total)) {
            cancelled = true;
            break;
        }
    } while (available > 0);

    out.close();
    if (cancelled) {
        fs::remove(partial, ec);
        if (error) *error = "已取消";
        return false;
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

std::string readFile(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace flf
