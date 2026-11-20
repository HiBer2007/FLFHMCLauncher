#include "core/app_bootstrap.h"
#include "core/installer.h"
#include "core/modpack.h"
#include "core/workspace.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QTimer>

#include <filesystem>

int main(int argc, char* argv[]) {
    flf::bootstrap();

    QApplication app(argc, argv);
    QApplication::setApplicationName("FLFHMCLauncher");
#ifdef FLF_LAUNCHER_VERSION
    QApplication::setApplicationVersion(QStringLiteral(FLF_LAUNCHER_VERSION));
#else
    QApplication::setApplicationVersion("0.0.0");
#endif
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/icon/flfhmlauncher-256.png")));

    flf::Workspace::get().initialize();

    flf::MainWindow window;
    window.show();

    // 支持“打开方式”/拖到 exe 上的 .flfhmclpack；不主动注册关联。
    for (const QString& arg : QApplication::arguments()) {
        if (flf::isModpackFile(std::filesystem::u8path(arg.toStdString()))) {
            const QString pack = arg;
            QTimer::singleShot(0, &window, [&window, pack]() {
                window.importModpackFile(pack);
            });
            break;
        }
    }

    // 首次运行释放 CrashTracker；不阻塞主流程，失败只记录。
    QTimer::singleShot(1500, &window, []() {
        std::string error;
        if (!flf::ensureCrashTracker([](std::int64_t, std::int64_t) { return true; }, &error)) {
            qWarning("CrashTracker: %s", error.c_str());
        }
    });

    const int code = QApplication::exec();
    flf::shutdown();
    return code;
}
