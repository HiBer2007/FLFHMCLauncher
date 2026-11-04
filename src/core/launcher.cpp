#include "core/launcher.h"

#include "core/meta.h"

#include <QProcess>

#include <fstream>
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
    plan.workingDir = workspace.resolveMinecraft(instance);
    plan.arguments = {
        "-javaagent:" + workspace.agentJar().string(),
        "-Dflf.libraries.dir=" + workspace.librariesDir().string(),
        "-Dflfhmcl.gameDir=" + plan.workingDir.string(),
        "-Dflf.instance=" + instance.id,
        "-Dflfhmcl.update.managed=true",
        "-Dhmcl.home=" + workspace.resolveHmclHome(instance).string(),
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
