#include "core/launcher.h"

#include "core/meta.h"

#include <QProcess>

#include <windows.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <sstream>

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
    // HMCL 以 user.dir 为基准找 .minecraft；.hmcl 由 hmcl.home/hmcl.dir 指定。
    // 默认与 .flfhmcl 同级；开启隔离时放到 .launcher/<uuid>/ 下。
    fs::path base;
    if (instance.isolated) {
        base = workspace.home() / ".launcher" / instance.uuid;
    } else {
        base = workspace.home().parent_path();
    }
    const fs::path gameDir = base / ".minecraft";
    const fs::path hmclHome = base / ".hmcl";
    std::error_code ec;
    fs::create_directories(gameDir, ec);
    fs::create_directories(hmclHome, ec);
    plan.workingDir = base;
    plan.arguments = {
        "-javaagent:" + workspace.agentJar().string(),
        "-Dflf.libraries.dir=" + workspace.librariesDir().string(),
        "-Dflfhmcl.gameDir=" + gameDir.string(),
        "-Dflf.instance=" + instance.id,
        "-Dflf.home=" + workspace.home().string(),
        "-Dflfhmcl.update.managed=true",
        "-Dhmcl.home=" + hmclHome.string(),
        "-Dhmcl.dir=" + hmclHome.string(),
        "-jar",
        workspace.hmclJar().string(),
    };
    if (quickStart) {
        plan.arguments.push_back("--flf-instance=" + instance.id);
    }
    return plan;
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
                        CREATE_NEW_PROCESS_GROUP, nullptr, cwd.c_str(),
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
