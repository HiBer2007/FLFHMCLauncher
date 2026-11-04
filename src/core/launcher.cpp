#include "core/launcher.h"

#include "core/meta.h"
#include "core/http.h"

#include <QProcess>

#include <windows.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <sstream>
#include <set>
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
            "-javaagent:" + workspace.agentJar(instance).string(),
            "-Dflf.libraries.dir=" + workspace.librariesDir().string(),
            "-Dflfhmcl.gameDir=" + gameDir.string(),
            "-Dflf.instance=" + instance.id,
            "-Dflf.loader.version=" + instance.loaderVersion,
            "-Dflf.mclhr.manifest=" + workspace.manifestPath(instance).string(),
            "-Dflf.profile=" + workspace.profileDir(instance).string(),
            "-Dflf.version=" + instance.agentVersion,
            "-Dflf.home=" + workspace.home().string(),
            "-Dflfhmcl.update.managed=true",
            "-Dhmcl.home=" + hmclHome.string(),
            "-Dhmcl.dir=" + hmclHome.string(),
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
        plan.arguments.push_back(workspace.hmclJar(instance).string());
        if (quickStart) {
            plan.arguments.push_back("--flf-instance=" + instance.id);
        }
    }
};

/// 新版 FLFHMCL（>= 2.1.5）：直接启动 KnotClient，HMCL 由 Fabric 加载。
class KnotProvider final : public LaunchProvider {
public:
    std::string id() const override { return "knot"; }
    bool supports(const Instance& instance) const override {
        return versionAtLeast(instance.agentVersion, 2, 1, 5);
    }
    void configure(LaunchPlan& plan, const Workspace& workspace,
                   const Instance& instance, bool quickStart) const override {
        const fs::path gameDir = workspace.resolveMinecraft(instance);
        const fs::path hmclHome = workspace.resolveHmclHome(instance);
        addCommon(plan, workspace, instance, gameDir, hmclHome);
        plan.arguments.insert(plan.arguments.end(), {
            "-Dflf.knot.direct=true",
            "-Dfabric.addMods=" + workspace.agentJar(instance).string(),
            "-Dflf.hmcl.jar=" + workspace.hmclJar(instance).string(),
            "-cp",
            workspace.agentJar(instance).string(),
            "net.fabricmc.loader.impl.launch.knot.KnotClient",
        });
        plan.arguments.push_back("--gameDir");
        plan.arguments.push_back(gameDir.string());
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
        arguments << QString::fromStdString(argument);
    }
    const bool started = QProcess::startDetached(
            QString::fromStdString(plan.program),
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
    std::string command = quote(plan.program);
    for (const auto& argument : plan.arguments) {
        command += " " + quote(argument);
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

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;

    PROCESS_INFORMATION process{};
    std::vector<char> buffer(command.begin(), command.end());
    buffer.push_back('\0');
    const std::string cwd = plan.workingDir.string();
    if (!CreateProcessA(nullptr, buffer.data(), nullptr, nullptr, TRUE,
                        CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW, nullptr, cwd.c_str(),
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
bool writeQuickStart(const Instance& instance, const fs::path& outDir, std::string* error) {
    std::error_code ec;
    fs::create_directories(outDir, ec);

    const fs::path source = Workspace::get().launcherDir() / "FLFQuickStart.exe";
    const fs::path target = outDir / "FLFQuickStart.exe";
    if (source != target) {
        fs::copy_file(source, target, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            if (error) *error = "复制 FLFQuickStart.exe 失败: " + ec.message();
            return false;
        }
    }

    const LaunchPlan plan = buildLaunchPlan(instance, true);
    nlohmann::json json = {
        {"root", Workspace::get().home().string()},
        {"instance", instance.id},
        {"command", joinCommand(plan)},
        {"workingDir", plan.workingDir.string()},
    };
    std::ofstream out(outDir / "quickstart.json", std::ios::binary | std::ios::trunc);
    out << json.dump(2);
    if (!out.good()) {
        if (error) *error = "写入 quickstart.json 失败";
        return false;
    }
    return true;
}

} // namespace flf
