#include "core/http.h"
#include "core/installer.h"
#include "core/launcher.h"
#include "core/meta.h"
#include "core/workspace.h"

#include <QCoreApplication>

#include <cstdio>
#include <iostream>

using namespace flf;

namespace {

void printVersions() {
    std::string error;
    const auto versions = fetchVersionList(&error);
    std::cout << "versions: " << versions.size();
    if (!error.empty()) std::cout << " (error: " << error << ")";
    std::cout << "\n";
    for (std::size_t i = 0; i < versions.size() && i < 5; ++i) {
        std::cout << "  " << versions[i].id << " [" << versions[i].type << "]\n";
    }
}

void printFabric(const std::string& game) {
    std::string error;
    const auto loaders = fetchFabricLoaders(game, &error);
    std::cout << "fabric loaders for " << game << ": " << loaders.size();
    if (!error.empty()) std::cout << " (error: " << error << ")";
    std::cout << "\n";
    for (std::size_t i = 0; i < loaders.size() && i < 5; ++i) {
        std::cout << "  " << loaders[i] << "\n";
    }
}

void printHmcl() {
    std::string error;
    const auto release = fetchHmclRelease(&error);
    std::cout << "hmcl: ok=" << release.ok << " version=" << release.version
              << " error=" << (error.empty() ? release.error : error) << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    flf::Workspace::get().initialize();

    const QStringList args = app.arguments();
    if (args.size() >= 2 && args[1] == "selftest") {
        printVersions();
        printFabric("1.21.1");
        printHmcl();
        return 0;
    }

    if (args.size() >= 2 && args[1] == "install") {
        flf::Instance instance;
        instance.id = args.value(2, "cli-test").toStdString();
        instance.name = instance.id;
        instance.gameVersion = args.value(3, "1.21.1").toStdString();
        instance.loader = "fabric";
        instance.loaderVersion = args.value(4, "").toStdString();
        if (instance.loaderVersion.empty()) {
            std::string error;
            const auto loaders = fetchFabricLoaders(instance.gameVersion, &error);
            if (loaders.empty()) {
                std::cerr << "no fabric loaders: " << error << "\n";
                return 2;
            }
            instance.loaderVersion = loaders.front();
        }
        instance.javaPath = args.value(5, "").toStdString();

        std::cout << "installing " << instance.id << " " << instance.gameVersion
                  << " fabric " << instance.loaderVersion << "\n";
        std::string error;
        const bool ok = flf::installInstance(instance, [](const std::string& step, int percent) {
            std::cout << "[" << percent << "%] " << step << std::endl;
        }, &error);
        if (!ok) {
            std::cerr << "install failed: " << error << "\n";
            return 3;
        }

        flf::Workspace::get().instances().push_back(instance);
        flf::Workspace::get().saveInstances();
        std::cout << "installed\n";
        return 0;
    }

    if (args.size() >= 3 && args[1] == "launch") {
        std::string error;
        const flf::Instance* instance = flf::Workspace::get().find(args[2].toStdString());
        if (!instance) {
            std::cerr << "instance not found: " << args[2].toStdString() << "\n";
            return 2;
        }
        if (!flf::launchInstance(*instance, &error)) {
            std::cerr << "launch failed: " << error << "\n";
            return 3;
        }
        std::cout << "launched\n";
        return 0;
    }

    std::cout << "usage: FLFLauncherCli selftest | install <id> <game> [loader] [java] | launch <id>\n";
    return 1;
}
