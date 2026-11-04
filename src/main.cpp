#include "core/app_bootstrap.h"
#include "core/installer.h"
#include "core/workspace.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QTimer>

int main(int argc, char* argv[]) {
    flf::bootstrap();

    QApplication app(argc, argv);
    QApplication::setApplicationName("FLFHMCLauncher");
    QApplication::setApplicationVersion("0.2.0");
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/icon/flfhmlauncher-256.png")));

    flf::Workspace::get().initialize();

    flf::MainWindow window;
    window.show();

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
