#include "ui/mainwindow.h"

#include "core/http.h"
#include "core/installer.h"
#include "core/launcher.h"
#include "core/meta.h"
#include "core/mods.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <fstream>

namespace flf {
namespace {

QString toQString(const std::string& value) {
    return QString::fromUtf8(value);
}

std::string toStdString(const QString& value) {
    return value.toUtf8().toStdString();
}

QString isolationLabel(const Layer& layer) {
    if (layer.mode == "private") return QStringLiteral("私有");
    if (layer.mode == "shared") return QStringLiteral("共享");
    return QStringLiteral("组: ") + toQString(layer.group);
}

Layer layerFromIndex(int index, const QString& group) {
    Layer layer;
    layer.mode = index == 0 ? "private" : index == 1 ? "shared" : "group";
    layer.group = toStdString(group.isEmpty() ? QStringLiteral("default") : group);
    return layer;
}

QString makeId(const QString& name) {
    QString id;
    for (QChar c : name.toLower()) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            id.append(c);
        } else if (!id.isEmpty() && !id.endsWith('-')) {
            id.append('-');
        }
    }
    if (id.isEmpty()) {
        id = "instance";
    }
    QString candidate = id;
    int suffix = 2;
    while (Workspace::get().find(toStdString(candidate)) != nullptr) {
        candidate = id + "-" + QString::number(suffix++);
    }
    return candidate;
}

bool progressDownload(const QString& title,
                      const std::string& url,
                      const std::filesystem::path& target,
                      QString* error) {
    QProgressDialog dialog(title, QString(), 0, 100);
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setCancelButton(nullptr);
    dialog.setMinimumDuration(0);
    dialog.show();

    const bool ok = httpDownload(url, target, [&](std::int64_t received, std::int64_t total) {
        if (total > 0) {
            dialog.setValue(static_cast<int>(received * 100 / total));
        }
        QCoreApplication::processEvents();
        return true;
    }, nullptr);
    if (!ok) {
        *error = QObject::tr("下载失败: %1").arg(QString::fromStdString(url));
    }
    return ok;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("FLFHMCLauncher");
    resize(980, 640);

    auto* toolbar = addToolBar("main");
    toolbar->setMovable(false);

    auto* createAction = toolbar->addAction("新建实例");
    auto* launchAction = toolbar->addAction("启动");
    auto* quickAction = toolbar->addAction("快速启动");
    auto* modsAction = toolbar->addAction("模组");
    auto* folderAction = toolbar->addAction("打开目录");
    toolbar->addSeparator();
    auto* updateAction = toolbar->addAction("检查更新");
    auto* settingsAction = toolbar->addAction("设置");
    auto* deleteAction = toolbar->addAction("删除");

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"名称", "版本", "加载器", "隔离", "Java"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);

    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);

    status_ = new QLabel(this);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->addWidget(table_, 3);
    layout->addWidget(new QLabel("日志", central));
    layout->addWidget(log_, 1);
    layout->addWidget(status_);
    setCentralWidget(central);

    connect(createAction, &QAction::triggered, this, &MainWindow::createInstance);
    connect(launchAction, &QAction::triggered, this, &MainWindow::launchSelected);
    connect(quickAction, &QAction::triggered, this, &MainWindow::quickStartSelected);
    connect(modsAction, &QAction::triggered, this, &MainWindow::openMods);
    connect(folderAction, &QAction::triggered, this, &MainWindow::openFolder);
    connect(updateAction, &QAction::triggered, this, &MainWindow::checkUpdate);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteInstance);
    connect(table_, &QTableWidget::doubleClicked, this, &MainWindow::launchSelected);

    refreshInstances();
    log(QStringLiteral("工作区: %1").arg(QString::fromStdWString(Workspace::get().home().wstring())));
}

void MainWindow::log(const QString& message) {
    log_->appendPlainText(QStringLiteral("[%1] %2")
                                  .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), message));
}

void MainWindow::refreshInstances() {
    const auto& instances = Workspace::get().instances();
    table_->setRowCount(static_cast<int>(instances.size()));
    for (int row = 0; row < static_cast<int>(instances.size()); ++row) {
        const auto& instance = instances[row];
        auto* name = new QTableWidgetItem(toQString(instance.name));
        name->setData(Qt::UserRole, toQString(instance.id));
        table_->setItem(row, 0, name);
        table_->setItem(row, 1, new QTableWidgetItem(toQString(instance.gameVersion)));
        table_->setItem(row, 2, new QTableWidgetItem(
                toQString(instance.loader + " " + instance.loaderVersion)));
        table_->setItem(row, 3, new QTableWidgetItem(
                isolationLabel(instance.mods) + " / " + isolationLabel(instance.minecraft)
                + " / " + isolationLabel(instance.hmclHome)));
        table_->setItem(row, 4, new QTableWidgetItem(toQString(instance.javaPath)));
    }
    table_->resizeColumnsToContents();
    status_->setText(QStringLiteral("共 %1 个实例").arg(instances.size()));
}

Instance* MainWindow::selectedInstance() {
    const int row = table_->currentRow();
    if (row < 0) {
        return nullptr;
    }
    const QString id = table_->item(row, 0)->data(Qt::UserRole).toString();
    return Workspace::get().find(toStdString(id));
}

void MainWindow::selectInstance(const std::string& id) {
    for (int row = 0; row < table_->rowCount(); ++row) {
        if (table_->item(row, 0)->data(Qt::UserRole).toString() == toQString(id)) {
            table_->selectRow(row);
            return;
        }
    }
}

void MainWindow::createInstance() {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    std::string error;
    const auto versions = fetchVersionList(&error);
    QApplication::restoreOverrideCursor();
    if (versions.empty()) {
        QMessageBox::critical(this, "获取版本失败", toQString(error));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("新建实例");
    auto* form = new QFormLayout(&dialog);

    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setText("新实例");
    auto* versionCombo = new QComboBox(&dialog);
    for (const auto& version : versions) {
        if (version.type == "release" || version.type == "snapshot") {
            versionCombo->addItem(toQString(version.id));
        }
    }
    auto* loaderCombo = new QComboBox(&dialog);
    auto* javaCombo = new QComboBox(&dialog);
    javaCombo->addItem("自动（优先本机，缺失时下载 Temurin 21）", QString());
    for (const auto& java : detectJava()) {
        javaCombo->addItem(toQString(java.version + "  " + java.path), toQString(java.path));
    }

    auto* modsMode = new QComboBox(&dialog);
    auto* mcMode = new QComboBox(&dialog);
    auto* hmclMode = new QComboBox(&dialog);
    for (auto* combo : {modsMode, mcMode, hmclMode}) {
        combo->addItems({"私有", "共享", "组"});
    }
    auto* modsGroup = new QLineEdit("default", &dialog);
    auto* mcGroup = new QLineEdit("default", &dialog);
    auto* hmclGroup = new QLineEdit("default", &dialog);

    form->addRow("名称", nameEdit);
    form->addRow("Minecraft 版本", versionCombo);
    form->addRow("Fabric Loader", loaderCombo);
    form->addRow("Java", javaCombo);
    form->addRow("模组隔离", modsMode);
    form->addRow("模组组名", modsGroup);
    form->addRow("存档/资源隔离", mcMode);
    form->addRow("存档组名", mcGroup);
    form->addRow("HMCL 配置隔离", hmclMode);
    form->addRow("配置组名", hmclGroup);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);

    auto updateLoader = [&]() {
        loaderCombo->clear();
        std::string fetchError;
        const auto loaders = fetchFabricLoaders(toStdString(versionCombo->currentText()), &fetchError);
        for (const auto& loader : loaders) {
            loaderCombo->addItem(toQString(loader));
        }
        if (loaders.empty()) {
            loaderCombo->addItem("0.16.10");
        }
    };
    connect(versionCombo, &QComboBox::currentTextChanged, &dialog, [&](const QString&) {
        updateLoader();
    });
    updateLoader();

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty() || versionCombo->currentText().isEmpty()) {
        QMessageBox::warning(this, "参数不完整", "名称和版本不能为空");
        return;
    }

    Instance instance;
    instance.id = toStdString(makeId(name));
    instance.name = toStdString(name);
    instance.gameVersion = toStdString(versionCombo->currentText());
    instance.loaderVersion = toStdString(loaderCombo->currentText());
    instance.javaPath = toStdString(javaCombo->currentData().toString());
    instance.mods = layerFromIndex(modsMode->currentIndex(), modsGroup->text());
    instance.minecraft = layerFromIndex(mcMode->currentIndex(), mcGroup->text());
    instance.hmclHome = layerFromIndex(hmclMode->currentIndex(), hmclGroup->text());
    instance.createdAt = static_cast<std::int64_t>(QDateTime::currentSecsSinceEpoch());

    QProgressDialog progress("安装实例", QString(), 0, 100, this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.show();

    std::string installError;
    const bool ok = installInstance(instance, [&](const std::string& step, int percent) {
        progress.setLabelText(toQString(step));
        progress.setValue(percent);
        QCoreApplication::processEvents();
    }, &installError);

    if (!ok) {
        QMessageBox::critical(this, "安装失败", toQString(installError));
        return;
    }

    Workspace::get().instances().push_back(instance);
    Workspace::get().saveInstances();
    std::ofstream(Workspace::get().instanceDir(instance) / "instance.json")
            << toJson(instance).dump(2);

    refreshInstances();
    selectInstance(instance.id);
    log(QStringLiteral("已创建实例 %1（%2 / Fabric %3）")
                .arg(toQString(instance.name), toQString(instance.gameVersion),
                     toQString(instance.loaderVersion)));
}

void MainWindow::launchSelected() {
    Instance* instance = selectedInstance();
    if (!instance) {
        QMessageBox::information(this, "启动", "请先选择实例");
        return;
    }
    std::string error;
    if (!launchInstance(*instance, &error)) {
        QMessageBox::critical(this, "启动失败", toQString(error));
        return;
    }
    log(QStringLiteral("已启动实例 %1").arg(toQString(instance->name)));
}

void MainWindow::quickStartSelected() {
    Instance* instance = selectedInstance();
    if (!instance) {
        QMessageBox::information(this, "快速启动", "请先选择实例");
        return;
    }
    const QString dir = QFileDialog::getExistingDirectory(this, "选择快速启动器输出目录");
    if (dir.isEmpty()) {
        return;
    }
    std::string error;
    if (!writeQuickStart(*instance, std::filesystem::path(dir.toStdWString()), &error)) {
        QMessageBox::critical(this, "生成快速启动器失败", toQString(error));
        return;
    }
    log(QStringLiteral("已生成快速启动器: %1").arg(dir));
    QMessageBox::information(this, "完成",
                             "已生成 FLFQuickStart.exe 与 quickstart.json，可直接启动该实例。");
}

void MainWindow::openMods() {
    Instance* instance = selectedInstance();
    if (!instance) {
        QMessageBox::information(this, "模组", "请先选择实例");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("模组管理 - " + toQString(instance->name));
    auto* layout = new QVBoxLayout(&dialog);
    auto* list = new QListWidget(&dialog);
    auto* row = new QHBoxLayout();
    auto* importButton = new QPushButton("导入", &dialog);
    auto* deleteButton = new QPushButton("删除", &dialog);
    auto* closeButton = new QPushButton("关闭", &dialog);
    row->addWidget(importButton);
    row->addWidget(deleteButton);
    row->addStretch();
    row->addWidget(closeButton);
    layout->addWidget(list);
    layout->addLayout(row);

    auto reload = [&]() {
        list->clear();
        for (const auto& mod : listMods(*instance)) {
            auto* item = new QListWidgetItem(toQString(mod.fileName), list);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(mod.enabled ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, toQString(mod.path.string()));
            item->setData(Qt::UserRole + 1, mod.enabled);
        }
    };
    reload();

    connect(list, &QListWidget::itemChanged, &dialog, [&](QListWidgetItem* item) {
        if (item->data(Qt::UserRole + 1).toBool() == (item->checkState() == Qt::Checked)) {
            return;
        }
        const fs::path path = std::filesystem::path(item->data(Qt::UserRole).toString().toStdWString());
        ModEntry entry{path, path.filename().string(), item->checkState() == Qt::Checked};
        std::string error;
        if (!setModEnabled(*instance, entry, entry.enabled, &error)) {
            QMessageBox::warning(&dialog, "操作失败", toQString(error));
        }
        reload();
    });

    connect(importButton, &QPushButton::clicked, &dialog, [&]() {
        const QStringList files = QFileDialog::getOpenFileNames(
                &dialog, "选择模组", {}, "Fabric 模组 (*.jar)");
        for (const auto& file : files) {
            std::string error;
            if (!importMod(*instance, std::filesystem::path(file.toStdWString()), &error)) {
                QMessageBox::warning(&dialog, "导入失败", toQString(error));
            }
        }
        reload();
    });

    connect(deleteButton, &QPushButton::clicked, &dialog, [&]() {
        auto* item = list->currentItem();
        if (!item) return;
        if (QMessageBox::question(&dialog, "删除", "确定删除该模组文件？") != QMessageBox::Yes) return;
        ModEntry entry{std::filesystem::path(item->data(Qt::UserRole).toString().toStdWString()),
                       toStdString(item->text()), true};
        std::string error;
        if (!deleteMod(entry, &error)) {
            QMessageBox::warning(&dialog, "删除失败", toQString(error));
        }
        reload();
    });

    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.resize(560, 420);
    dialog.exec();
}

void MainWindow::openSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle("设置");
    auto* form = new QFormLayout(&dialog);

    auto* agentEdit = new QLineEdit(toQString(Workspace::get().config().agentJar), &dialog);
    agentEdit->setPlaceholderText("留空则使用启动器目录/FLFHMCL-agent.jar");
    auto* browse = new QPushButton("浏览", &dialog);
    auto* row = new QHBoxLayout();
    row->addWidget(agentEdit);
    row->addWidget(browse);

    form->addRow("FLFHMCL Agent", row);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);

    connect(browse, &QPushButton::clicked, &dialog, [&]() {
        const QString file = QFileDialog::getOpenFileName(&dialog, "选择 Agent jar", {},
                                                          "Java Agent (*.jar)");
        if (!file.isEmpty()) agentEdit->setText(file);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        Workspace::get().config().agentJar = toStdString(agentEdit->text());
        Workspace::get().saveConfig();
        log("设置已保存");
    }
}

void MainWindow::checkUpdate() {
    QApplication::setOverrideCursor(Qt::WaitCursor);
    std::string error;
    const HmclRelease release = fetchHmclRelease(&error);
    QApplication::restoreOverrideCursor();
    if (!release.ok) {
        QMessageBox::warning(this, "检查更新", toQString(error));
        return;
    }

    const std::string current = Workspace::get().config().hmclVersion;
    if (!current.empty() && current == release.version) {
        QMessageBox::information(this, "检查更新", "已经是最新版本: " + toQString(current));
        return;
    }

    if (QMessageBox::question(this, "发现新版本",
                              QStringLiteral("HMCL %1 可用（当前 %2），是否下载？")
                                      .arg(toQString(release.version),
                                           toQString(current.empty() ? "未知" : current)))
            != QMessageBox::Yes) {
        return;
    }

    QProgressDialog progress("下载 HMCL", QString(), 0, 100, this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.show();

    const bool ok = httpDownload(release.jarUrl, Workspace::get().hmclJar(),
                                 [&](std::int64_t received, std::int64_t total) {
        if (total > 0) progress.setValue(static_cast<int>(received * 100 / total));
        QCoreApplication::processEvents();
        return true;
    }, &error);

    if (!ok) {
        QMessageBox::critical(this, "下载失败", toQString(error));
        return;
    }
    if (!release.sha1.empty() && sha1File(Workspace::get().hmclJar()) != release.sha1) {
        QMessageBox::critical(this, "校验失败", "HMCL jar SHA-1 不匹配");
        return;
    }

    Workspace::get().config().hmclVersion = release.version;
    Workspace::get().saveConfig();
    log(QStringLiteral("HMCL 已更新到 %1").arg(toQString(release.version)));
}

void MainWindow::openFolder() {
    Instance* instance = selectedInstance();
    if (!instance) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
                QString::fromStdWString(Workspace::get().home().wstring())));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(
            QString::fromStdWString(Workspace::get().instanceDir(*instance).wstring())));
}

void MainWindow::deleteInstance() {
    Instance* instance = selectedInstance();
    if (!instance) {
        return;
    }
    if (QMessageBox::question(this, "删除实例",
                              QStringLiteral("确定从列表移除“%1”？文件不会删除。")
                                      .arg(toQString(instance->name)))
            != QMessageBox::Yes) {
        return;
    }
    const std::string id = instance->id;
    auto& instances = Workspace::get().instances();
    instances.erase(std::remove_if(instances.begin(), instances.end(),
                                   [&](const Instance& item) { return item.id == id; }),
                    instances.end());
    Workspace::get().saveInstances();
    refreshInstances();
    log(QStringLiteral("已移除实例 %1").arg(toQString(id)));
}

} // namespace flf
