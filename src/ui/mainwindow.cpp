#include "ui/mainwindow.h"

#include "core/installer.h"
#include "core/launcher.h"
#include "core/meta.h"
#include "core/mods.h"

#include <QApplication>
#include <QCheckBox>
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
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <fstream>

#ifdef FLF_HAVE_HIBERGUI
#include "toast_notification.h"
#endif

namespace flf {
namespace {
QSettings uiSettings() {
    return QSettings(QSettings::IniFormat, QSettings::UserScope, "FLFHMCL", "launcher");
}
QString qs(const std::string& v) { return QString::fromUtf8(v); }
std::string ss(const QString& v) { return v.toUtf8().toStdString(); }

QString makeId(const QString& name) {
    QString id;
    for (QChar c : name.toLower()) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) id.append(c);
        else if (!id.isEmpty() && !id.endsWith('-')) id.append('-');
    }
    if (id.isEmpty()) id = "instance";
    QString candidate = id;
    int suffix = 2;
    while (Workspace::get().find(ss(candidate)) != nullptr) candidate = id + "-" + QString::number(suffix++);
    return candidate;
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("FLFHMCLauncher");
    resize(1080, 720);
    setMinimumSize(900, 600);
    dark_ = uiSettings().value("appearance/dark", false).toBool();
    applyTheme();
    buildUi();
    refreshInstances();
}

void MainWindow::applyTheme() {
    const QString bg = dark_ ? "#0f141c" : "#f3f7fd";
    const QString card = dark_ ? "#18202b" : "#ffffff";
    const QString text = dark_ ? "#e8eef7" : "#101828";
    const QString sub = dark_ ? "#94a3b8" : "#64748b";
    const QString border = dark_ ? "#2b3748" : "#dbe6f5";
    const QString input = dark_ ? "#0d141e" : "#ffffff";
    const QString hover = dark_ ? "#223046" : "#eaf2ff";
    const QString qss = QString(R"(
        QMainWindow, QDialog, QMessageBox, QProgressDialog { background: %1; color: %3; }
        QWidget { color: %3; font-family: "Microsoft YaHei", "Segoe UI"; font-size: 13px; }
        QLabel { background: transparent; color: %3; }
        QLabel#title { font-size: 22px; font-weight: 700; }
        QLabel#section { font-size: 16px; font-weight: 600; }
        QLabel#sub, QLabel#hint { color: %4; font-size: 12px; }
        QFrame#topbar { background: #2f7cf6; }
        QFrame#sidebar { background: %2; border-right: 1px solid %6; }
        QFrame#card, QFrame#instanceCard { background: %2; border: 1px solid %6; border-radius: 12px; }
        QFrame#instanceCard:hover { border-color: #2f7cf6; }
        QFrame#instanceCard[selected="true"] { border: 2px solid #2f7cf6; }
        QPushButton { background: %2; color: %3; border: 1px solid %6; border-radius: 8px; padding: 8px 14px; }
        QPushButton:hover { background: %8; }
        QPushButton#primary { background: #2f7cf6; color: white; border: none; font-weight: 600; padding: 10px 18px; }
        QPushButton#primary:hover { background: #2569d8; }
        QPushButton#nav { background: transparent; color: #eaf1ff; border: none; padding: 8px 14px; font-size: 14px; }
        QPushButton#nav:hover { background: rgba(255,255,255,0.16); }
        QPushButton#side { background: transparent; color: %3; border: none; text-align: left; padding: 10px 14px; }
        QPushButton#side:hover { background: %8; }
        QLineEdit, QListWidget { background: %7; color: %3; border: 1px solid %6; border-radius: 7px; padding: 7px; }
        QLineEdit:focus { border-color: #2f7cf6; }
        QCheckBox { color: %3; spacing: 8px; }
        QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }
        QMenu { background: %2; color: %3; border: 1px solid %6; }
        QMenu::item:selected { background: %8; }
    )").arg(bg, card, text, sub, border, "", input, hover);
    if (qApp) qApp->setStyleSheet(qss);
}

void MainWindow::buildUi() {
    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildTopBar());

    auto* body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);


    pages_ = new QStackedWidget(root);
    launchPage_ = buildLaunchPage();
    settingsPage_ = buildSettingsPage();
    helpPage_ = buildHelpPage();
    managePage_ = buildManagePage();
    modsPage_ = buildModsPage();
    pages_->addWidget(launchPage_);
    pages_->addWidget(settingsPage_);
    pages_->addWidget(helpPage_);
    pages_->addWidget(managePage_);
    pages_->addWidget(modsPage_);
    body->addWidget(pages_, 1);
    layout->addLayout(body, 1);
    setCentralWidget(root);

#ifdef FLF_HAVE_HIBERGUI
    toast_ = new HiBerGUI::ToastNotification(this);
#endif
}

QWidget* MainWindow::buildTopBar() {
    auto* bar = new QFrame(this);
    bar->setObjectName("topbar");
    bar->setFixedHeight(52);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(18, 0, 12, 0);
    layout->setSpacing(6);

    auto* logo = new QLabel("FLFHMCL", bar);
    logo->setStyleSheet("color: white; font-size: 18px; font-weight: 700;");
    layout->addWidget(logo);
    layout->addSpacing(16);

    auto addNav = [&](const QString& text, auto callback) {
        auto* button = new QPushButton(text, bar);
        button->setObjectName("nav");
        connect(button, &QPushButton::clicked, this, callback);
        layout->addWidget(button);
    };
    addNav("启动", [this]() { showPage(0); });
    addNav("新建", [this]() { createInstance(); });
    addNav("设置", [this]() { showPage(1); });
    addNav("帮助", [this]() { showPage(2); });
    layout->addStretch();

    auto* theme = new QPushButton(dark_ ? "浅色" : "深色", bar);
    theme->setObjectName("nav");
    connect(theme, &QPushButton::clicked, this, [this, theme]() {
        dark_ = !dark_;
        uiSettings().setValue("appearance/dark", dark_);
        applyTheme();
        theme->setText(dark_ ? "浅色" : "深色");
        if (darkToggle_) darkToggle_->setChecked(dark_);
    });
    layout->addWidget(theme);
    return bar;
}

QWidget* MainWindow::buildSidebar() {
    auto* side = new QFrame(this);
    side->setObjectName("sidebar");
    side->setFixedWidth(190);
    auto* layout = new QVBoxLayout(side);
    layout->setContentsMargins(12, 16, 12, 16);
    layout->setSpacing(4);

    auto add = [&](const QString& text, int page) {
        auto* button = new QPushButton(text, side);
        button->setObjectName("side");
        connect(button, &QPushButton::clicked, this, [this, page]() { showPage(page); });
        layout->addWidget(button);
    };
    add("启动HMCL", 0);
    add("设置", 1);
    add("帮助与关于", 2);
    layout->addStretch();
    auto* version = new QLabel(QStringLiteral("HMCL %1").arg(qs(Workspace::get().config().hmclVersion).isEmpty() ? QStringLiteral("3.16.4") : qs(Workspace::get().config().hmclVersion)), side);
    version->setObjectName("hint");
    layout->addWidget(version);
    return side;
}

QWidget* MainWindow::buildLaunchPage() {
    auto* page = new QWidget(this);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(16);

    auto* ops = new QFrame(page);
    ops->setObjectName("card");
    ops->setFixedWidth(210);
    auto* opLayout = new QVBoxLayout(ops);
    auto* opTitle = new QLabel("操作", ops);
    opTitle->setObjectName("section");
    opLayout->addWidget(opTitle);

    auto addOp = [&](const QString& text, auto callback, const char* style = "side") {
        auto* button = new QPushButton(text, ops);
        button->setObjectName(style);
        connect(button, &QPushButton::clicked, this, callback);
        opLayout->addWidget(button);
    };
    addOp("新建实例", [this]() { createInstance(); }, "primary");
    addOp("启动选中的实例", [this]() { launchSelected(); });
    addOp("模组管理", [this]() { openMods(); });
    addOp("打开实例目录", [this]() { openFolder(); });
    addOp("创建快速启动器", [this]() { quickStartSelected(); });
    addOp("删除实例", [this]() { deleteInstance(); });
    opLayout->addStretch();
    auto* hint = new QLabel("先在右侧选择实例，再执行左侧操作。", ops);
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    opLayout->addWidget(hint);
    layout->addWidget(ops);

    auto* right = new QWidget(page);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    auto* title = new QLabel("实例列表", right);
    title->setObjectName("title");
    rightLayout->addWidget(title);

    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    cardsLayout_ = new QVBoxLayout(content);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(10);
    cardsLayout_->addStretch();
    scroll->setWidget(content);
    rightLayout->addWidget(scroll, 1);

    status_ = new QLabel(right);
    status_->setObjectName("hint");
    rightLayout->addWidget(status_);
    layout->addWidget(right, 1);
    return page;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        const QString id = watched->property("instanceId").toString();
        if (!id.isEmpty()) {
            setCurrent(ss(id));
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
QWidget* MainWindow::buildSettingsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(12);

    auto* title = new QLabel("设置", page);
    title->setObjectName("title");
    layout->addWidget(title);

    auto* appearance = new QFrame(page);
    appearance->setObjectName("card");
    auto* a = new QVBoxLayout(appearance);
    auto* aTitle = new QLabel("外观", appearance);
    aTitle->setObjectName("section");
    a->addWidget(aTitle);
    darkToggle_ = new QCheckBox("深色模式", appearance);
    darkToggle_->setChecked(dark_);
    connect(darkToggle_, &QCheckBox::toggled, this, [this](bool on) {
        dark_ = on;
        uiSettings().setValue("appearance/dark", dark_);
        applyTheme();
    });
    a->addWidget(darkToggle_);
    layout->addWidget(appearance);

    auto* data = new QFrame(page);
    data->setObjectName("card");
    auto* d = new QVBoxLayout(data);
    auto* dTitle = new QLabel("数据", data);
    dTitle->setObjectName("section");
    d->addWidget(dTitle);
    auto* path = new QLabel(QStringLiteral("目录：%1").arg(QString::fromStdWString(Workspace::get().home().wstring())), data);
    path->setObjectName("sub");
    path->setWordWrap(true);
    d->addWidget(path);
    auto* open = new QPushButton("打开数据目录", data);
    connect(open, &QPushButton::clicked, this, &MainWindow::openFolder);
    d->addWidget(open, 0, Qt::AlignLeft);
    layout->addWidget(data);

    auto* update = new QFrame(page);
    update->setObjectName("card");
    auto* u = new QVBoxLayout(update);
    auto* uTitle = new QLabel("更新", update);
    uTitle->setObjectName("section");
    u->addWidget(uTitle);
    auto* check = new QPushButton("检查 HMCL 更新", update);
    connect(check, &QPushButton::clicked, this, &MainWindow::checkUpdate);
    u->addWidget(check, 0, Qt::AlignLeft);
    auto* hint = new QLabel("Java、隔离与代理等高级选项保留在配置文件中。", update);
    hint->setObjectName("hint");
    u->addWidget(hint);
    layout->addWidget(update);
    layout->addStretch();
    return page;
}

QWidget* MainWindow::buildHelpPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(12);

    auto* title = new QLabel("帮助与关于", page);
    title->setObjectName("title");
    layout->addWidget(title);

    auto* card = new QFrame(page);
    card->setObjectName("card");
    auto* c = new QVBoxLayout(card);
    auto* text = new QLabel("面向新手的 FLFHMCL 启动器：选择实例，点击启动即可。\n\n"
                            "遇到问题可打开数据目录查看日志，或在项目仓库反馈。", card);
    text->setWordWrap(true);
    c->addWidget(text);
    auto* link = new QPushButton("打开项目主页", card);
    connect(link, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/HiBer2007/FLFHMCL"));
    });
    c->addWidget(link, 0, Qt::AlignLeft);
    layout->addWidget(card);
    layout->addStretch();
    return page;
}

QWidget* MainWindow::buildManagePage() {
    auto* page = new QWidget(this);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(16);

    auto* ops = new QFrame(page);
    ops->setObjectName("card");
    ops->setFixedWidth(220);
    auto* opLayout = new QVBoxLayout(ops);
    auto* opTitle = new QLabel("实例操作", ops);
    opTitle->setObjectName("section");
    opLayout->addWidget(opTitle);

    auto addOp = [&](const QString& text, auto callback) {
        auto* button = new QPushButton(text, ops);
        button->setObjectName("side");
        connect(button, &QPushButton::clicked, this, callback);
        opLayout->addWidget(button);
    };
    addOp("概览", [this]() { showPage(0); });
    addOp("模组管理", [this]() { openMods(); });
    addOp("打开实例目录", [this]() { openFolder(); });
    addOp("创建快速启动器", [this]() { quickStartSelected(); });
    addOp("删除实例", [this]() { deleteInstance(); });
    opLayout->addStretch();
    layout->addWidget(ops);

    auto* right = new QFrame(page);
    right->setObjectName("card");
    auto* rightLayout = new QVBoxLayout(right);
    auto* title = new QLabel("选择实例", right);
    title->setObjectName("section");
    rightLayout->addWidget(title);
    auto* hint = new QLabel("左侧选择要执行的操作，操作会在当前窗口内打开。", right);
    hint->setObjectName("hint");
    rightLayout->addWidget(hint);
    instanceList_ = new QListWidget(right);
    connect(instanceList_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* item) {
                if (item) {
                    setCurrent(ss(item->data(Qt::UserRole).toString()));
                }
            });
    rightLayout->addWidget(instanceList_, 1);
    layout->addWidget(right, 1);
    return page;
}

QWidget* MainWindow::buildModsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(12);

    modsTitle_ = new QLabel("模组管理", page);
    modsTitle_->setObjectName("title");
    layout->addWidget(modsTitle_);

    modsList_ = new QListWidget(page);
    layout->addWidget(modsList_, 1);

    auto* row = new QHBoxLayout();
    auto* importButton = new QPushButton("导入模组", page);
    auto* deleteButton = new QPushButton("删除", page);
    auto* backButton = new QPushButton("返回", page);
    row->addWidget(importButton);
    row->addWidget(deleteButton);
    row->addStretch();
    row->addWidget(backButton);
    layout->addLayout(row);

    connect(importButton, &QPushButton::clicked, this, [this]() {
        Instance* instance = selectedInstance();
        if (!instance) {
            return;
        }
        const QString file = QFileDialog::getOpenFileName(this, "选择模组", {}, "Mods (*.jar)");
        if (file.isEmpty()) {
            return;
        }
        std::string error;
        if (!importMod(*instance, std::filesystem::path(file.toStdWString()), &error)) {
            showToast("导入失败", qs(error), true);
            return;
        }
        refreshMods();
        showToast("已导入模组", QFileInfo(file).fileName(), false);
    });
    connect(deleteButton, &QPushButton::clicked, this, [this]() {
        auto* item = modsList_ ? modsList_->currentItem() : nullptr;
        if (!item) {
            return;
        }
        std::string error;
        deleteMod(ModEntry{std::filesystem::path(item->data(Qt::UserRole).toString().toStdWString()), "", true}, &error);
        refreshMods();
    });
    connect(backButton, &QPushButton::clicked, this, [this]() { showPage(0); });
    connect(modsList_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        Instance* instance = selectedInstance();
        if (!instance) {
            return;
        }
        const fs::path path = std::filesystem::path(item->data(Qt::UserRole).toString().toStdWString());
        ModEntry entry{path, path.filename().string(), item->checkState() == Qt::Checked};
        std::string error;
        setModEnabled(*instance, entry, entry.enabled, &error);
    });
    return page;
}
QWidget* MainWindow::buildInstanceCard(const Instance& instance) {
    auto* card = new QFrame(this);
    card->setObjectName("instanceCard");
    card->setProperty("instanceId", qs(instance.uuid));
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->installEventFilter(this);

    auto* row = new QHBoxLayout(card);
    row->setContentsMargins(20, 14, 14, 14);
    row->setSpacing(12);
    auto* text = new QVBoxLayout();
    text->setSpacing(3);
    auto* name = new QLabel(qs(instance.name), card);
    name->setStyleSheet("font-size: 16px; font-weight: 600;");
    QString version = qs(Workspace::get().config().hmclVersion);
    if (version.isEmpty()) version = "3.16.4";
    auto* sub = new QLabel(QStringLiteral("HMCL %1  ·  已准备好").arg(version), card);
    sub->setObjectName("sub");
    text->addWidget(name);
    text->addWidget(sub);
    row->addLayout(text, 1);

    auto* launch = new QPushButton("启动", card);
    launch->setObjectName("primary");
    launch->setMinimumWidth(88);
    connect(launch, &QPushButton::clicked, this, [this, id = instance.id]() { setCurrent(id); launchSelected(); });
    row->addWidget(launch);
    return card;
}

void MainWindow::refreshInstances() {
    if (!cardsLayout_) return;
    while (cardsLayout_->count() > 1) {
        QLayoutItem* item = cardsLayout_->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    const auto& instances = Workspace::get().instances();
    if (instances.empty()) {
        auto* empty = new QFrame(this);
        empty->setObjectName("card");
        auto* layout = new QVBoxLayout(empty);
        auto* text = new QLabel("还没有实例\n点击右上角“新建实例”，三步就能开始游戏。", empty);
        text->setAlignment(Qt::AlignCenter);
        text->setObjectName("sub");
        layout->addWidget(text);
        auto* button = new QPushButton("新建实例", empty);
        button->setObjectName("primary");
        connect(button, &QPushButton::clicked, this, &MainWindow::createInstance);
        layout->addWidget(button, 0, Qt::AlignCenter);
        cardsLayout_->insertWidget(0, empty);
        status_->setText("准备就绪");
        return;
    }
    for (const auto& instance : instances) {
        cardsLayout_->insertWidget(cardsLayout_->count() - 1, buildInstanceCard(instance));
    }
    status_->setText(QStringLiteral("共 %1 个实例 · 点击启动打开 HMCL").arg(instances.size()));
    if (currentId_.empty()) setCurrent(instances.front().id);
}

void MainWindow::showPage(int index) {
    if (!pages_ || index < 0 || index >= pages_->count()) return;
    if (pages_->currentIndex() != index) {
        pages_->setCurrentIndex(index);
        fadeIn(pages_->currentWidget());
    }
}

void MainWindow::fadeIn(QWidget* widget) {
    auto* effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    auto* animation = new QPropertyAnimation(effect, "opacity", widget);
    animation->setDuration(160);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::setCurrent(const std::string& id) {
    currentId_ = id;
    for (QFrame* card : findChildren<QFrame*>("instanceCard")) {
        card->setProperty("selected", card->property("instanceId").toString() == qs(id));
        card->style()->unpolish(card);
        card->style()->polish(card);
    }
}

Instance* MainWindow::selectedInstance() {
    if (currentId_.empty()) return nullptr;
    return Workspace::get().find(currentId_);
}

void MainWindow::showToast(const QString& title, const QString& detail, bool error) {
#ifdef FLF_HAVE_HIBERGUI
    if (toast_) { toast_->showError(title, detail, error ? 4000 : 2600); return; }
#endif
    if (error) QMessageBox::warning(this, title, detail);
}

void MainWindow::createInstance() {
    QDialog dialog(this);
    dialog.setWindowTitle("新建实例");
    dialog.setMinimumWidth(460);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(14);

    auto* title = new QLabel("创建实例", &dialog);
    title->setObjectName("section");
    layout->addWidget(title);
    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText("给实例起个名字");
    nameEdit->setText("我的实例");
    nameEdit->selectAll();
    layout->addWidget(nameEdit);

    QString version = qs(Workspace::get().config().hmclVersion);
    if (version.isEmpty()) {
        version = "3.16.4";
    }
    auto* versionLabel = new QLabel(QStringLiteral("HMCL %1（自动使用最新版）").arg(version), &dialog);
    versionLabel->setObjectName("sub");
    layout->addWidget(versionLabel);
    auto* hint = new QLabel("Java、Fabric 与依赖库会自动准备，不需要手动配置。", &dialog);
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    auto* cancel = new QPushButton("取消", &dialog);
    auto* create = new QPushButton("创建", &dialog);
    create->setObjectName("primary");
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
    if (dialog.exec() != QDialog::Accepted) return;
    runInstall(nameEdit->text().trimmed());
}

void MainWindow::runInstall(const QString& name) {
    Instance instance;
    instance.uuid = newInstanceUuid();
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
    std::ofstream(Workspace::get().instanceDir(instance) / "instance.json") << toJson(instance).dump(2);
    progress.setValue(100);
    refreshInstances();
    setCurrent(instance.id);
    showPage(0);
    showToast("实例已就绪", "现在可以点击“启动”开始游戏", false);
}

void MainWindow::launchSelected() {
    Instance* instance = selectedInstance();
    if (!instance) { showToast("还没有选择实例", "先点一张实例卡片，再点启动", true); return; }
    std::string error;
    if (!launchInstance(*instance, &error)) { showToast("启动失败", qs(error), true); return; }
    showToast("已启动", QStringLiteral("正在打开 %1").arg(qs(instance->name)), false);
}

void MainWindow::quickStartSelected() {
    Instance* instance = selectedInstance();
    if (!instance) { showToast("还没有选择实例", "请先选择实例", true); return; }
    const QString dir = QFileDialog::getExistingDirectory(this, "选择快速启动器输出目录");
    if (dir.isEmpty()) return;
    std::string error;
    if (!writeQuickStart(*instance, std::filesystem::path(dir.toStdWString()), &error)) {
        showToast("生成失败", qs(error), true);
        return;
    }
    showToast("快速启动器已生成", "可直接用 FLFQuickStart.exe 启动", false);
}

void MainWindow::openMods() {
    if (!selectedInstance()) {
        showToast("还没有选择实例", "请先选择实例", true);
        return;
    }
    refreshMods();
    showPage(4);
}

void MainWindow::openManage(const std::string& id) {
    if (!id.empty()) {
        setCurrent(id);
    } else if (Workspace::get().instances().empty()) {
        showToast("还没有实例", "先创建一个实例", true);
        return;
    }
    refreshManage();
    showPage(3);
}

void MainWindow::refreshManage() {
    if (!instanceList_) {
        return;
    }
    instanceList_->clear();
    const auto& instances = Workspace::get().instances();
    for (const auto& instance : instances) {
        auto* item = new QListWidgetItem(qs(instance.name), instanceList_);
        item->setData(Qt::UserRole, qs(instance.uuid));
        if (instance.id == currentId_) {
            instanceList_->setCurrentItem(item);
        }
    }
}

void MainWindow::refreshMods() {
    Instance* instance = selectedInstance();
    if (!modsList_) {
        return;
    }
    modsList_->clear();
    if (!instance) {
        return;
    }
    if (modsTitle_) {
        modsTitle_->setText("模组管理 - " + qs(instance->name));
    }
    for (const auto& mod : listMods(*instance)) {
        auto* item = new QListWidgetItem(qs(mod.fileName), modsList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(mod.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, qs(mod.path.string()));
    }
}
void MainWindow::openSettings() { showPage(1); }

void MainWindow::checkUpdate() {
    std::string error;
    const HmclRelease release = fetchHmclRelease(&error);
    if (!release.ok) { showToast("检查更新失败", qs(error), true); return; }
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
    if (!instance) { showToast("还没有选择实例", "请先选择实例", true); return; }
    if (QMessageBox::question(this, "删除实例", QStringLiteral("确定删除“%1”吗？").arg(qs(instance->name))) != QMessageBox::Yes) return;
    const std::string id = instance->id;
    auto& list = Workspace::get().instances();
    list.erase(std::remove_if(list.begin(), list.end(), [&](const Instance& item) { return item.id == id; }), list.end());
    Workspace::get().saveInstances();
    std::error_code ec;
    fs::remove_all(Workspace::get().instanceDir(*instance), ec);
    currentId_.clear();
    refreshInstances();
    showToast("已删除实例", "", false);
}

} // namespace flf
