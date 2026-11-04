#include "core/app_bootstrap.h"
#include "core/flf_paths.h"

#include <QApplication>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>

int main(int argc, char* argv[]) {
    flf::bootstrap();

    QApplication app(argc, argv);
    QApplication::setApplicationName("FLFHMCLauncher");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/icon/flfhmlauncher-256.png")));

    QMainWindow window;
    window.setWindowTitle("FLFHMCLauncher");
    auto* label = new QLabel(
        QStringLiteral("FLFHMCLauncher 骨架已启动\n\n.flfhmcl: %1")
            .arg(QString::fromStdWString(flf::flfHome().wstring())),
        &window);
    label->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(label);
    window.resize(720, 480);
    window.show();

    const int code = QApplication::exec();
    flf::shutdown();
    return code;
}
