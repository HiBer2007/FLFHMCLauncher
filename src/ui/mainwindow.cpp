#include "ui/mainwindow.h"

#include "core/http.h"
#include "core/installer.h"
#include "core/launcher.h"
#include "core/meta.h"
#include "core/mods.h"

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <fstream>

#ifdef FLF_HAVE_HIBERGUI
#include "toast_notification.h"
#endif

namespace flf {
namespace {

QString qs(const std::string& value) {
    return QString::fromUtf8(value);
}

std::string ss(const QString& value) {
    return value.toUtf8().toStdString();
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
    while (Workspace::get().find(ss(candidate)) != nullptr) {
        candidate = id + "-" + QString::number(suffix++);
    }
    return candidate;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("FLFHMCLauncher");
    resize(1040, 700);
    setMinimumSize(860, 560);

    setStyleSheet(R"(
        QMainWindow { background: #f5f7fb; }
        QLabel#heroTitle { font-size: 30px; font-weight: 700; color: #14213d; }
        QLabel#heroSub { font-size: 14px; color: #64748b; }
        QLabel#sectionTitle { font-size: 18px; font-weight: 600; color: #1e293b; }
        QLabel#cardTitle { font-size: 17px; font-weight: 600; color: #0f172a; }
        QLabel#cardSub { font-size: 12px; color: #64748b; }
        QFrame#card { background: #ffffff; border: 1px solid #e2e8f0; border-radius: 14px; }
        QFrame#card:hover { border-color: #bcd2f7; }
        QFrame#card[selected="true"] { border: 2px solid #2f7cf6; }
        QPushButton#primary {
            background: #2f7cf6; color: white; border: none; border-radius: 9px;
            padding: 10px 18px; font-size: 14px; font-weight: 600;
        }
        QPushButton#primary:hover { background: #2569d8; }
        QPushButton#secondary {
            background: #eef3fb; color: #2456a6; border: none; border-radius: 9px;
            padding: 9px 14px; font-size: 13px;
        }
        QPushButton#secondary:hover { background: #dde8fa; }
        QPushButton#ghost { background: transparent; color: #64748b; border: none; padding: 6px 8px; }
        QPushButton#ghost:hover { color: #2f7cf6; }
        QLabel#step { font-size: 14px; color: #334155; }
        QLabel#hint { font-size: 12px; color: #94a3b8; }
    )");

    buildUi();
    refreshInstances();
}

void MainWindow::buildUi() {
    pages_ = new QStackedWidget(this);
    setCentralWidget(pages_);

    welcomePage_ = buildWelcomePage();
    homePage_ = buildHomePage();
    pages_->addWidget(welcomePage_);
    pages_->addWidget(homePage_);

#ifdef FLF_HAVE_HIBERGUI
    toast_ = new HiBerGUI::ToastNotification(this);
#endif
}

QWidget* MainWindow::buildWelcomePage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(64, 56, 64, 56);
    layout->setSpacing(14);

    auto* title = new QLabel("欢迎使用 FLFHMCLauncher", page);
    title->setObjectName("heroTitle");
    auto* sub = new QLabel("三次点击，就能开始游戏。其余交给启动器。", page);
    sub->setObjectName("heroSub");
    layout->addWidget(title);
    layout->addWidget(sub);
    layout->addSpacing(16);

    const QStringList steps = {
        "1  给实例起个名字",
        "2  选择 HMCL 版本（默认最新）",
        "3  等待准备完成，点击启动",
    };
    for (const QString& step : steps) {
        auto* label = new QLabel(step, page);
        label->setObjectName("step");
        layout->addWidget(label);
    }

    layout->addSpacing(18);
    auto* start = new QPushButton("开始创建实例", page);
    start->setObjectName("primary");
    start->setFixedWidth(180);
    connect(start, &QPushButton::clicked, this, &MainWindow::createInstance);
    layout->addWidget(start);
    layout->addStretch();

    auto* hint = new QLabel("已有实例会自动显示在首页。", page);
    hint->setObjectName("hint");
    layout->addWidget(hint);
    return page;
}

QWidget* MainWindow::buildHomePage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 26, 32, 26);
    layout->setSpacing(16);

    auto* header = new QHBoxLayout();
    homeTitle_ = new QLabel("我的实例", page);
    homeTitle_->setObjectName("sectionTitle");
    header->addWidget(homeTitle_);
    header->addStretch();

    auto* newButton = new QPushButton("新建实例", page);
    newButton->setObjectName("secondary");
    connect(newButton, &QPushButton::clicked, this, &MainWindow::createInstance);
    header->addWidget(newButton);

    auto* settings = new QPushButton("设置", page);
    settings->setObjectName("ghost");
    connect(settings, &QPushButton::clicked, this, &MainWindow::openSettings);
    header->addWidget(settings);
    layout->addLayout(header);

    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget { background: #f5f7fb; border: none; }");
    auto* content = new QWidget(scroll);
    content->setStyleSheet("background: #f5f7fb;");
    cardsLayout_ = new QVBoxLayout(content);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(12);
    cardsLayout_->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll, 1);

    status_ = new QLabel(page);
    status_->setObjectName("hint");
    layout->addWidget(status_);
    return page;
}

QWidget* MainWindow::buildInstanceCard(const Instance& instance) {
    auto* card = new QFrame(this);
    card->setObjectName("card");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setProperty("instanceId", qs(instance.id));

    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(20, 16, 16, 16);
    row->setSpacing(14);

    auto* text = new QVBoxLayout();
    text->setSpacing(4);
    auto* name = new QLabel(qs(instance.name), card);
    name->setObjectName("cardTitle");
    QString hmclVersion = qs(Workspace::get().config().hmclVersion);
    if (hmclVersion.isEmpty()) {
        hmclVersion = "3.16.4";
    }
    auto* sub = new QLabel(
            QStringLiteral("HMCL %1  ·  已准备好").arg(hmclVersion),
            card);
    sub->setObjectName("cardSub");
    text->addWidget(name);
    text->addWidget(sub);
    row->addLayout(text, 1);

    auto* more = new QPushButton("更多", card);
    more->setObjectName("ghost");
    auto* menu = new QMenu(more);
    menu->addAction("打开实例目录", this, [this, id = instance.id]() {
        setCurrent(id);
        openFolder();
    });
    menu->addAction("管理模组", this, [this, id = instance.id]() {
        setCurrent(id);
        openMods();
    });
    menu->addAction("创建快速启动器", this, [this, id = instance.id]() {
        setCurrent(id);
        quickStartSelected();
    });
    menu->addSeparator();
    menu->addAction("删除实例", this, [this, id = instance.id]() {
        setCurrent(id);
        deleteInstance();
    });
    more->setMenu(menu);
    row->addWidget(more);

    auto* launch = new QPushButton("启动", card);
    launch->setObjectName("primary");
    launch->setMinimumWidth(92);
    launch->setStyleSheet(
            "QPushButton { background: #2f7cf6; color: white; border: none; border-radius: 9px; padding: 9px 16px; font-weight: 600; }"
            "QPushButton:hover { background: #2569d8; }");
    connect(launch, &QPushButton::clicked, this, [this, id = instance.id]() {
        setCurrent(id);
        launchSelected();
    });
    row->addWidget(launch);
    return card;
}

void MainWindow::refreshInstances() {
    if (!cardsLayout_) {
        return;
    }
    while (cardsLayout_->count() > 1) {
        QLayoutItem* item = cardsLayout_->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    const auto& instances = Workspace::get().instances();
    for (const auto& instance : instances) {
        cardsLayout_->insertWidget(cardsLayout_->count() - 1, buildInstanceCard(instance));
    }

    if (instances.empty()) {
        showPage(welcomePage_);
    } else {
        showPage(homePage_);
        status_->setText(QStringLiteral("共 %1 个实例 · 点击启动开始游戏").arg(instances.size()));
        if (currentId_.empty()) {
            setCurrent(instances.front().id);
        }
    }
}

void MainWindow::showPage(QWidget* page) {
    if (pages_->currentWidget() != page) {
        pages_->setCurrentWidget(page);
        fadeIn(page);
    }
}

void MainWindow::fadeIn(QWidget* widget) {
    auto* effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    auto* animation = new QPropertyAnimation(effect, "opacity", widget);
    animation->setDuration(180);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::setCurrent(const std::string& id) {
    currentId_ = id;
    for (QFrame* card : findChildren<QFrame*>("card")) {
        const bool selected = card->property("instanceId").toString() == qs(id);
        card->setProperty("selected", selected);
        card->style()->unpolish(card);
        card->style()->polish(card);
    }
}

Instance* MainWindow::selectedInstance() {
    if (currentId_.empty()) {
        return nullptr;
    }
    return Workspace::get().find(currentId_);
}

void MainWindow::showToast(const QString& title, const QString& detail, bool error) {
#ifdef FLF_HAVE_HIBERGUI
    if (toast_) {
        toast_->showError(title, detail, error ? 4000 : 2600);
        return;
    }
#endif
    if (error) {
        QMessageBox::warning(this, title, detail);
    }
}

void MainWindow::createInstance() {
    QDialog dialog(this);
    dialog.setWindowTitle("新建实例");
    dialog.setMinimumWidth(460);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(14);
    layout->setContentsMargins(24, 22, 24, 20);

    auto* title = new QLabel("给新实例起个名字", &dialog);
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText("例如：我的世界");
    nameEdit->setText("我的实例");
    nameEdit->selectAll();
    layout->addWidget(nameEdit);

    std::string error;
    const HmclRelease release = fetchHmclRelease(&error);
    const QString hmclVersion = release.ok ? qs(release.version) : "3.16.4";

    auto* version = new QLabel(
            QStringLiteral("HMCL 版本：%1（自动使用最新版）").arg(hmclVersion), &dialog);
    version->setObjectName("cardSub");
    layout->addWidget(version);

    auto* hint = new QLabel("Java、Fabric 和依赖库会自动准备，不需要手动配置。", &dialog);
    hint->setWordWrap(true);
    hint->setObjectName("hint");
    layout->addWidget(hint);

    auto* buttons = new QHBoxLayout();
    auto* cancel = new QPushButton("取消", &dialog);
    cancel->setObjectName("secondary");
    auto* create = new QPushButton("创建并准备", &dialog);
    create->setObjectName("primary");
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(create);
    layout->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(create, &QPushButton::clicked, &dialog, [&]() {
        if (nameEdit->text().trimmed().isEmpty()) {
            showToast("名称不能为空", "请先给实例起个名字", true);
            return;
        }
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    runInstall(nameEdit->text().trimmed());
}

void MainWindow::runInstall(const QString& name) {
    Instance instance;
    instance.id = ss(makeId(name));
    instance.name = ss(name);
    instance.gameVersion = "1.21.1";
    instance.loaderVersion = "0.19.5";
    instance.createdAt = static_cast<std::int64_t>(QDateTime::currentSecsSinceEpoch());

    QProgressDialog progress("正在准备，请稍候…", QString(), 0, 100, this);
    progress.setWindowTitle("准备实例");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.setValue(2);
    progress.show();

    std::string installError;
    const bool ok = installInstance(instance, [&](const std::string&, int percent) {
        progress.setValue(std::max(2, percent));
        QCoreApplication::processEvents();
    }, &installError);

    if (!ok) {
        progress.close();
        showToast("准备失败", qs(installError), true);
        return;
    }

    Workspace::get().instances().push_back(instance);
    Workspace::get().saveInstances();
    std::ofstream(Workspace::get().instanceDir(instance) / "instance.json")
            << toJson(instance).dump(2);

    progress.setValue(100);
    refreshInstances();
    setCurrent(instance.id);
    showToast("实例已就绪", QStringLiteral("现在可以点击“启动”开始游戏了"), false);
}

void MainWindow::launchSelected() {
    Instance* instance = selectedInstance();
    if (!instance) {
        showToast("还没有选择实例", "先点一张实例卡片，再点启动", true);
        return;
    }
    std::string error;
    if (!launchInstance(*instance, &error)) {
        showToast("启动失败", qs(error), true);
        return;
    }
    showToast("已启动", QStringLiteral("正在打开 %1").arg(qs(instance->name)), false);
}

void MainWindow::quickStartSelected() {
    Instance* instance = selectedInstance();
    if (!instance) {
        showToast("还没有选择实例", "请先选择实例", true);
        return;
    }
    const QString dir = QFileDialog::getExistingDirectory(this, "选择快速启动器输出目录");
    if (dir.isEmpty()) {
        return;
    }
    std::string error;
    if (!writeQuickStart(*instance, std::filesystem::path(dir.toStdWString()), &error)) {
        showToast("生成失败", qs(error), true);
        return;
    }
    showToast("快速启动器已生成", "可直接用 FLFQuickStart.exe 启动该实例", false);
}

void MainWindow::openMods() {
    Instance* instance = selectedInstance();
    if (!instance) {
        showToast("还没有选择实例", "请先选择实例", true);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("模组 - " + qs(instance->name));
    dialog.resize(560, 420);
    auto* layout = new QVBoxLayout(&dialog);
    auto* list = new QListWidget(&dialog);
    auto* row = new QHBoxLayout();
    auto* importButton = new QPushButton("导入模组", &dialog);
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
            auto* item = new QListWidgetItem(qs(mod.fileName), list);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(mod.enabled ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, qs(mod.path.string()));
        }
    };
    reload();

    connect(list, &QListWidget::itemChanged, &dialog, [&](QListWidgetItem* item) {
        const fs::path path = std::filesystem::path(item->data(Qt::UserRole).toString().toStdWString());
        ModEntry entry{path, path.filename().string(), item->checkState() == Qt::Checked};
        std::string modError;
        setModEnabled(*instance, entry, entry.enabled, &modError);
    });
    connect(importButton, &QPushButton::clicked, &dialog, [&]() {
        const QString file = QFileDialog::getOpenFileName(this, "选择模组", {}, "Mods (*.jar)");
        if (file.isEmpty()) {
            return;
        }
        std::string importError;
        if (!importMod(*instance, std::filesystem::path(file.toStdWString()), &importError)) {
            showToast("导入失败", qs(importError), true);
            return;
        }
        reload();
        showToast("已导入模组", QFileInfo(file).fileName(), false);
    });
    connect(deleteButton, &QPushButton::clicked, &dialog, [&]() {
        auto* item = list->currentItem();
        if (!item) {
            return;
        }
        std::string modError;
        deleteMod(ModEntry{std::filesystem::path(item->data(Qt::UserRole).toString().toStdWString()), "", true}, &modError);
        reload();
    });
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void MainWindow::openSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle("设置");
    dialog.setMinimumWidth(460);
    auto* layout = new QVBoxLayout(&dialog);

    auto* title = new QLabel("基本设置", &dialog);
    title->setObjectName("sectionTitle");
    layout->addWidget(title);

    auto* path = new QLabel(QStringLiteral("数据目录：%1")
                                    .arg(QString::fromStdWString(Workspace::get().home().wstring())),
                            &dialog);
    path->setWordWrap(true);
    path->setObjectName("cardSub");
    layout->addWidget(path);

    auto* open = new QPushButton("打开数据目录", &dialog);
    open->setObjectName("secondary");
    connect(open, &QPushButton::clicked, this, &MainWindow::openFolder);
    layout->addWidget(open);

    auto* check = new QPushButton("检查 HMCL 更新", &dialog);
    check->setObjectName("secondary");
    connect(check, &QPushButton::clicked, this, &MainWindow::checkUpdate);
    layout->addWidget(check);

    auto* advanced = new QLabel(
            "高级选项（隔离、Java、代理）保留在配置文件中；普通使用不需要修改。", &dialog);
    advanced->setWordWrap(true);
    advanced->setObjectName("hint");
    layout->addWidget(advanced);

    auto* close = new QPushButton("完成", &dialog);
    close->setObjectName("primary");
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(close);
    dialog.exec();
}

void MainWindow::checkUpdate() {
    std::string error;
    const HmclRelease release = fetchHmclRelease(&error);
    if (!release.ok) {
        showToast("检查更新失败", qs(error), true);
        return;
    }
    if (release.version == Workspace::get().config().hmclVersion) {
        showToast("已经是最新版本", QStringLiteral("HMCL %1").arg(qs(release.version)), false);
        return;
    }
    showToast("发现新版本", QStringLiteral("HMCL %1 可用").arg(qs(release.version)), false);
}

void MainWindow::openFolder() {
    Instance* instance = selectedInstance();
    const auto dir = instance ? Workspace::get().instanceDir(*instance) : Workspace::get().home();
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdWString(dir.wstring())));
}

void MainWindow::deleteInstance() {
    Instance* instance = selectedInstance();
    if (!instance) {
        showToast("还没有选择实例", "请先选择实例", true);
        return;
    }
    if (QMessageBox::question(this, "删除实例",
                              QStringLiteral("确定删除“%1”吗？").arg(qs(instance->name)))
            != QMessageBox::Yes) {
        return;
    }
    const std::string id = instance->id;
    auto& list = Workspace::get().instances();
    list.erase(std::remove_if(list.begin(), list.end(),
                             [&](const Instance& item) { return item.id == id; }),
               list.end());
    Workspace::get().saveInstances();
    std::error_code ec;
    fs::remove_all(Workspace::get().instanceDir(*instance), ec);
    currentId_.clear();
    refreshInstances();
    showToast("已删除实例", "", false);
}

} // namespace flf
