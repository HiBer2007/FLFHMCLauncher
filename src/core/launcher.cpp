#include "core/launcher.h"

#include "core/meta.h"
#include "core/http.h"
#include "core/path_util.h"
#include "core/quickstart_config.h"

#include <QProcess>

#include <windows.h>
#include <algorithm>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <sstream>
#include <set>
#include <vector>
#include <fstream>

namespace flf {
namespace {

std::string quote(const std::string& value) {
    if (value.find(' ') == std::string::npos && value.find('\t') == std::string::npos) {
        return value;
    }
    std::string result = "\"";
    for (char c : value) {
        if (c == '"') {
            result += "\\\"";
        } else {
            result.push_back(c);
        }
    }
    result += "\"";
    return result;
}

std::wstring widenUtf8(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size);
    return result;
}

std::wstring quoteWide(const std::wstring& value) {
    if (value.find(L' ') == std::wstring::npos && value.find(L'\t') == std::wstring::npos) {
        return value;
    }
    std::wstring result = L"\"";
    for (wchar_t c : value) {
        if (c == L'"') {
            result += L"\\\"";
        } else {
            result.push_back(c);
        }
    }
    result += L"\"";
    return result;
}

std::string joinCommand(const LaunchPlan& plan) {
    std::ostringstream stream;
    stream << quote(plan.program);
    for (const auto& argument : plan.arguments) {
        stream << ' ' << quote(argument);
    }
    return stream.str();
}

/// 版本号按点分段比较，无法解析时返回 false。
bool versionAtLeast(const std::string& version, int major, int minor, int patch) {
    int parts[3] = {0, 0, 0};
    int index = 0;
    for (char c : version) {
        if (c == '.') {
            if (++index >= 3) break;
        } else if (c >= '0' && c <= '9') {
            parts[index] = parts[index] * 10 + (c - '0');
        } else {
            break;
        }
    }
    if (parts[0] != major) return parts[0] > major;
    if (parts[1] != minor) return parts[1] > minor;
    return parts[2] >= patch;
}

/// 启动方式提供者。新增 FLFHMCL 启动协议时只需实现并注册一个 Provider。
class LaunchProvider {
public:
    virtual ~LaunchProvider() = default;
    virtual std::string id() const = 0;
    virtual bool supports(const Instance& instance) const = 0;
    virtual void configure(LaunchPlan& plan, const Workspace& workspace,
                           const Instance& instance, bool quickStart) const = 0;

protected:
    static void addCommon(LaunchPlan& plan, const Workspace& workspace,
                          const Instance& instance, const fs::path& gameDir,
                          const fs::path& hmclHome) {
        plan.arguments.insert(plan.arguments.end(), {
            "-javaagent:" + pathUtf8(workspace.agentJar(instance)),
            "-Dflf.libraries.dir=" + pathUtf8(workspace.librariesDir()),
            "-Dflfhmcl.gameDir=" + pathUtf8(gameDir),
            "-Dflf.instance=" + instance.id,
            "-Dflf.loader.version=" + instance.loaderVersion,
            "-Dflf.mclhr.manifest=" + pathUtf8(workspace.manifestPath(instance)),
            "-Dflf.profile=" + pathUtf8(workspace.profileDir(instance)),
            "-Dflf.version=" + instance.agentVersion,
            "-Dflf.home=" + pathUtf8(workspace.home()),
            "-Dflfhmcl.update.managed=true",
            "-Dhmcl.home=" + pathUtf8(hmclHome),
            "-Dhmcl.dir=" + pathUtf8(hmclHome),
        });
    }
};

/// 旧版 FLFHMCL（<= 2.1.4）：以 -jar HMCL 启动，由 Agent 拦截入口。
class LegacyJarProvider final : public LaunchProvider {
public:
    std::string id() const override { return "legacy-jar"; }
    bool supports(const Instance&) const override { return true; }
    void configure(LaunchPlan& plan, const Workspace& workspace,
                   const Instance& instance, bool quickStart) const override {
        const fs::path gameDir = workspace.resolveMinecraft(instance);
        const fs::path hmclHome = workspace.resolveHmclHome(instance);
        addCommon(plan, workspace, instance, gameDir, hmclHome);
        plan.arguments.push_back("-jar");
        plan.arguments.push_back(pathUtf8(workspace.hmclJar(instance)));
        if (quickStart) {
            plan.arguments.push_back("--flf-instance=" + instance.id);
        }
    }
};

/// 收集本实例的 FLF/HMCL 模组。HMCL 由 Fabric Knot 托管，私有模组目录
/// `.flfhmcl/mods/<uuid>` 必须通过 `fabric.addMods` 显式加入，
/// 否则只有 agent 会被加载，HMCL 的模组一个都不会生效。
std::string hmclModPaths(const Workspace& workspace, const Instance& instance) {
    std::string paths = pathUtf8(workspace.agentJar(instance));
    const fs::path dir = workspace.resolveMods(instance);
    std::error_code ec;
    std::vector<fs::path> jars;
    if (fs::is_directory(dir, ec)) {
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file(ec)) {
                continue;
            }
            const fs::path path = entry.path();
            if (path.extension() == ".jar") {
                jars.push_back(path);
            }
        }
    }
    std::sort(jars.begin(), jars.end());
    for (const auto& jar : jars) {
        paths += ";";
        paths += pathUtf8(jar);
    }
    return paths;
}

/// 新版 FLFHMCL（>= 2.1.5）：直接启动 KnotClient，HMCL 由 Fabric 加载。
class KnotProvider final : public LaunchProvider {
public:
    std::string id() const override { return "knot"; }
    bool supports(const Instance& instance) const override {
        return versionAtLeast(instance.agentVersion, 2, 1, 6);
    }
    void configure(LaunchPlan& plan, const Workspace& workspace,
                   const Instance& instance, bool quickStart) const override {
        const fs::path gameDir = workspace.resolveMinecraft(instance);
        const fs::path hmclHome = workspace.resolveHmclHome(instance);
        addCommon(plan, workspace, instance, gameDir, hmclHome);
        plan.arguments.insert(plan.arguments.end(), {
            "-Dflf.knot.direct=true",
            "-Dfabric.addMods=" + hmclModPaths(workspace, instance),
            "-Dflf.hmcl.jar=" + pathUtf8(workspace.hmclJar(instance)),
            "-cp",
            pathUtf8(workspace.agentJar(instance)),
            "net.fabricmc.loader.impl.launch.knot.KnotClient",
        });
        plan.arguments.push_back("--gameDir");
        plan.arguments.push_back(pathUtf8(gameDir));
        plan.arguments.push_back("--hmclMainClass");
        plan.arguments.push_back("org.jackhuang.hmcl.Launcher");
        if (quickStart) {
            plan.arguments.push_back("--flf-instance=" + instance.id);
        }
    }
};

const std::vector<std::unique_ptr<LaunchProvider>>& launchProviders() {
    static const std::vector<std::unique_ptr<LaunchProvider>> providers = [] {
        std::vector<std::unique_ptr<LaunchProvider>> list;
        list.push_back(std::make_unique<KnotProvider>());
        list.push_back(std::make_unique<LegacyJarProvider>());
        return list;
    }();
    return providers;
}

const LaunchProvider& providerFor(const Instance& instance) {
    for (const auto& provider : launchProviders()) {
        if (provider->supports(instance)) {
            return *provider;
        }
    }
    return *launchProviders().back();
}

} // namespace

/// HMCL 3.16.x 用 Files.isWritable 判断配置目录，Windows 上遇到
/// “Everyone 拒绝删除子项”的继承 ACL 会误判为 Samba 共享并跳过初始化。
/// 这里用文件系统访问检查模拟该判断，必要时移除继承的 Everyone 拒绝项。
bool needsHmclAclRepair(const fs::path& directory) {
    HANDLE handle = CreateFileW(
            directory.c_str(),
            FILE_ADD_FILE | FILE_DELETE_CHILD,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        return false;
    }
    return GetLastError() == ERROR_ACCESS_DENIED;
}

bool runIcacls(const QStringList& arguments, int timeoutMs = 20000) {
    QProcess process;
    process.start(QStringLiteral("icacls"), arguments);
    if (!process.waitForStarted(2000)) {
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    return process.exitCode() == 0;
}

void repairHmclAcl(const fs::path& directory) {
    if (!needsHmclAclRepair(directory)) {
        return;
    }
    const QString path = QString::fromStdWString(directory.wstring());
    const QString user = qEnvironmentVariable("USERNAME").trimmed();
    // 只改目录自身，避免在 UI 线程递归遍历大目录。
    // 先去掉继承的 deny，再给当前用户显式 FullControl：
    // HMCL 的 Files.isWritable 会检查 delete-child，Authenticated Users 的
    // Modify 在这个目录上并不足以通过该检查。
    if (!runIcacls({path, QStringLiteral("/remove:d"), QStringLiteral("Everyone"),
                    QStringLiteral("/C"), QStringLiteral("/Q")})) {
        qWarning("HMCL config ACL repair failed for %s", qPrintable(path));
        return;
    }
    if (!user.isEmpty()) {
        runIcacls({path, QStringLiteral("/grant:r"),
                   user + QStringLiteral(":(OI)(CI)F"),
                   QStringLiteral("/T"), QStringLiteral("/C"), QStringLiteral("/Q")});
    }
    runIcacls({path, QStringLiteral("/setintegritylevel"),
               QStringLiteral("M"), QStringLiteral("/C"), QStringLiteral("/Q")});
    if (needsHmclAclRepair(directory)) {
        qWarning("HMCL config ACL is still not writable: %s", qPrintable(path));
    }
}

LaunchPlan buildLaunchPlan(const Instance& instance, bool quickStart) {
    LaunchPlan plan;
    if (!instance.javaPath.empty()) {
        plan.program = instance.javaPath;
    } else {
        const auto javas = detectJava();
        if (!javas.empty()) {
            plan.program = javas.front().path;
        }
    }

    const Workspace& workspace = Workspace::get();
    const fs::path gameDir = workspace.resolveMinecraft(instance);
    const fs::path hmclHome = workspace.resolveHmclHome(instance);
    std::error_code ec;
    fs::create_directories(gameDir, ec);
    fs::create_directories(hmclHome, ec);
    repairHmclAcl(hmclHome);
    plan.workingDir = gameDir.parent_path();

    const LaunchProvider& provider = providerFor(instance);
    plan.provider = provider.id();
    provider.configure(plan, workspace, instance, quickStart);
    return plan;
}

bool validateInstanceFiles(const Instance& instance, std::string* error) {
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    if (instance.javaPath.empty() || !fs::exists(instance.javaPath)) {
        return fail("Java 路径不存在");
    }
    const Workspace& workspace = Workspace::get();
    if (!fs::exists(workspace.agentJar(instance))) return fail("FLFHMCL Agent 不存在");
    if (!fs::exists(workspace.hmclJar(instance))) return fail("HMCL jar 不存在");
    if (!instance.hmclSha1.empty()
            && sha1File(workspace.hmclJar(instance)) != instance.hmclSha1) {
        return fail("HMCL jar SHA-1 校验失败");
    }

    const fs::path manifestPath = workspace.manifestPath(instance);
    if (!fs::exists(manifestPath)) return fail("mclhr 清单不存在");
    if (!instance.manifestSha256.empty()
            && sha256File(manifestPath) != instance.manifestSha256) {
        return fail("mclhr 清单哈希不匹配");
    }

    try {
        std::ifstream input(manifestPath);
        const auto manifest = nlohmann::json::parse(input);
        const auto modules = manifest.value("modules", nlohmann::json::array());
        std::set<std::string> seen;
        bool hasLibrary = false;
        for (const auto& module : modules) {
            for (const auto& library : module.value("libraries", nlohmann::json::array())) {
                const std::string group = library.value("group", "");
                const std::string artifact = library.value("artifact", "");
                const std::string version = library.value("version", "");
                const std::string classifier = library.value("classifier", "");
                if (group.empty() || artifact.empty() || version.empty()) {
                    return fail("mclhr 清单包含无效库条目");
                }
                if (group == "net.fabricmc" && artifact == "fabric-loader"
                        && version != instance.loaderVersion) {
                    return fail("mclhr 清单包含错误的 Fabric Loader 版本");
                }
                const std::string key = group + ":" + artifact + ":" + version + ":" + classifier;
                if (!seen.insert(key).second) {
                    return fail("mclhr 清单包含重复库: " + key);
                }
                hasLibrary = true;
            }
        }
        if (!hasLibrary) return fail("mclhr 清单没有库");
    } catch (const std::exception& e) {
        return fail(std::string("mclhr 清单解析失败: ") + e.what());
    }
    return true;
}
bool launchInstance(const Instance& instance, std::string* error, bool quickStart) {
    if (instance.javaPath.empty()) {
        if (error) *error = "实例没有配置 Java";
        return false;
    }
    if (!fs::exists(Workspace::get().hmclJar())) {
        if (error) *error = "HMCL.jar 不存在，请先安装实例";
        return false;
    }
    if (!fs::exists(Workspace::get().agentJar())) {
        if (error) *error = "FLFHMCL-agent.jar 不存在";
        return false;
    }

    const LaunchPlan plan = buildLaunchPlan(instance, quickStart);
    QStringList arguments;
    for (const auto& argument : plan.arguments) {
        arguments << QString::fromUtf8(argument.c_str());
    }
    const bool started = QProcess::startDetached(
            QString::fromUtf8(plan.program.c_str()),
            arguments,
            QString::fromStdWString(plan.workingDir.wstring()));
    if (!started && error) {
        *error = "启动 Java 失败";
    }
    return started;
}

bool launchInstanceManaged(const Instance& instance,
                           const std::function<void(const std::string&)>& onOutput,
                           const std::function<void(int, const std::string&)>& onExit,
                           std::string* error) {
    const LaunchPlan plan = buildLaunchPlan(instance, false);
    std::wstring command = quoteWide(widenUtf8(plan.program));
    for (const auto& argument : plan.arguments) {
        command += L" " + quoteWide(widenUtf8(argument));
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &attributes, 0)) {
        if (error) *error = "创建输出管道失败";
        return false;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;

    PROCESS_INFORMATION process{};
    std::wstring commandBuffer = command;
    const std::wstring cwd = widenUtf8(pathUtf8(plan.workingDir));
    if (!CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, TRUE,
                        CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW, nullptr,
                        cwd.empty() ? nullptr : cwd.c_str(),
                        &startup, &process)) {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        if (error) *error = "启动 Java 失败 (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(writePipe);
    CloseHandle(process.hThread);

    auto output = std::make_shared<std::string>();
    auto lock = std::make_shared<std::mutex>();
    std::thread([readPipe, output, lock, onOutput]() {
        char chunk[4096];
        DWORD read = 0;
        while (ReadFile(readPipe, chunk, sizeof(chunk), &read, nullptr) && read > 0) {
            {
                std::lock_guard<std::mutex> guard(*lock);
                output->append(chunk, read);
            }
            if (onOutput) {
                onOutput(std::string(chunk, read));
            }
        }
        CloseHandle(readPipe);
    }).detach();

    std::thread([process, output, lock, onExit]() {
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(process.hProcess, &code);
        CloseHandle(process.hProcess);
        std::string collected;
        {
            std::lock_guard<std::mutex> guard(*lock);
            collected = *output;
        }
        if (onExit) {
            onExit(static_cast<int>(code), collected);
        }
    }).detach();
    return true;
}
bool writeQuickStart(const Instance& instance, const fs::path& targetExe, std::string* error) {
    if (targetExe.filename().empty()) {
        if (error) *error = "快速启动器文件名不能为空";
        return false;
    }
    std::error_code ec;
    const fs::path directory = targetExe.has_parent_path()
            ? targetExe.parent_path() : fs::current_path();
    fs::create_directories(directory, ec);

    fs::path source = Workspace::get().home() / "bin" / "FLFQuickStart.exe";
    if (!fs::exists(source)) {
        source = Workspace::get().launcherDir() / "FLFQuickStart.exe";
    }
    if (source != targetExe) {
        fs::copy_file(source, targetExe, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            if (error) *error = "复制 FLFQuickStart.exe 失败: " + ec.message();
            return false;
        }
    }

    const LaunchPlan plan = buildLaunchPlan(instance, true);
    nlohmann::json json = {
        {"root", pathUtf8(Workspace::get().home())},
        {"instance", instance.id},
        {"command", joinCommand(plan)},
        {"workingDir", pathUtf8(plan.workingDir)},
    };
    std::string embedError;
    if (!appendQuickStartConfig(targetExe, json.dump(2), embedError)) {
        if (error) *error = embedError;
        return false;
    }
    return true;
}

} // namespace flf
