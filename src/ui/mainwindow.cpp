#include "ui/mainwindow.h"

#include "core/installer.h"
#include "core/launcher.h"
#include "core/meta.h"
#include "core/mods.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTreeWidget>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QPointer>
#include <QClipboard>
#include <QApplication>
#include <QToolButton>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <thread>
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
        QProgressBar { background: %7; border: 1px solid %6; border-radius: 4px; height: 7px; text-align: center; }
        QProgressBar::chunk { background: #2f7cf6; border-radius: 3px; }
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
    createPage_ = buildCreatePage();
    installPage_ = buildInstallPage();
    pages_->addWidget(launchPage_);
    pages_->addWidget(settingsPage_);
    pages_->addWidget(helpPage_);
    pages_->addWidget(managePage_);
    pages_->addWidget(modsPage_);
    pages_->addWidget(createPage_);
    pages_->addWidget(installPage_);
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
    addOp("更新实例组件", [this]() { showUpdateDialog(); });
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
    validateToggle_ = new QCheckBox("启动前校验文件", appearance);
    validateToggle_->setChecked(Workspace::get().config().validateBeforeLaunch);
    updateToggle_ = new QCheckBox("启动前检查更新", appearance);
    updateToggle_->setChecked(Workspace::get().config().checkUpdatesBeforeLaunch);
    instanceValidate_ = new QCheckBox("当前实例：校验文件", appearance);
    instanceUpdate_ = new QCheckBox("当前实例：检查更新", appearance);
    a->addWidget(validateToggle_);
    a->addWidget(updateToggle_);
    a->addWidget(instanceValidate_);
    a->addWidget(instanceUpdate_);
    connect(validateToggle_, &QCheckBox::toggled, this, [](bool on) {
        Workspace::get().config().validateBeforeLaunch = on;
        Workspace::get().saveConfig();
    });
    connect(updateToggle_, &QCheckBox::toggled, this, [](bool on) {
        Workspace::get().config().checkUpdatesBeforeLaunch = on;
        Workspace::get().saveConfig();
    });
    connect(instanceValidate_, &QCheckBox::toggled, this, [this](bool on) {
        if (Instance* instance = selectedInstance()) {
            instance->validateBeforeLaunch = on;
            Workspace::get().saveInstances();
        }
    });
    connect(instanceUpdate_, &QCheckBox::toggled, this, [this](bool on) {
        if (Instance* instance = selectedInstance()) {
            instance->checkUpdatesBeforeLaunch = on;
            Workspace::get().saveInstances();
        }
    });
    auto* behaviorLabel = new QLabel("启动后：", appearance);
    behaviorGlobal_ = new QComboBox(appearance);
    behaviorGlobal_->addItem("保持启动器显示", "keep");
    behaviorGlobal_->addItem("隐藏启动器", "hide");
    behaviorGlobal_->addItem("退出启动器", "exit");
    behaviorGlobal_->setCurrentIndex(std::max(0, behaviorGlobal_->findData(
            qs(Workspace::get().config().launchBehavior))));
    connect(behaviorGlobal_, &QComboBox::currentIndexChanged, this, [this](int) {
        Workspace::get().config().launchBehavior =
                ss(behaviorGlobal_->currentData().toString());
        Workspace::get().saveConfig();
    });
    a->addWidget(behaviorLabel);
    a->addWidget(behaviorGlobal_);

    auto* instanceBehavior = new QLabel("当前实例覆盖：", appearance);
    behaviorInstance_ = new QComboBox(appearance);
    behaviorInstance_->addItem("继承全局", "");
    behaviorInstance_->addItem("保持启动器显示", "keep");
    behaviorInstance_->addItem("隐藏启动器", "hide");
    behaviorInstance_->addItem("退出启动器", "exit");
    connect(behaviorInstance_, &QComboBox::currentIndexChanged, this, [this](int) {
        Instance* instance = selectedInstance();
        if (!instance) return;
        instance->launchBehavior = ss(behaviorInstance_->currentData().toString());
        Workspace::get().saveInstances();
    });
    a->addWidget(instanceBehavior);
    a->addWidget(behaviorInstance_);
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

QWidget* MainWindow::buildCreatePage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(12);

    auto* title = new QLabel("新建实例", page);
    title->setObjectName("title");
    layout->addWidget(title);

    auto* card = new QFrame(page);
    card->setObjectName("card");
    auto* form = new QVBoxLayout(card);
    form->setSpacing(8);

    auto* nameLabel = new QLabel("实例名称", card);
    form->addWidget(nameLabel);
    createName_ = new QLineEdit(card);
    createName_->setText("我的实例");
    form->addWidget(createName_);
    createError_ = new QLabel("", card);
    createError_->setStyleSheet("color: #ef4444;");
    form->addWidget(createError_);

    form->addWidget(new QLabel("HMCL 版本", card));
    auto* hmclRow = new QHBoxLayout();
    createHmclChannel_ = new QComboBox(card);
    createHmclChannel_->addItem("稳定版", "stable");
    createHmclChannel_->addItem("预览版", "preview");
    createHmcl_ = new QComboBox(card);
    createHmcl_->addItem("正在加载 HMCL 版本…");
    createHmcl_->setEnabled(false);
    hmclRow->addWidget(createHmclChannel_, 1);
    hmclRow->addWidget(createHmcl_, 2);
    hmclRefresh_ = new QPushButton("刷新", card);
    hmclStop_ = new QPushButton("停止", card);
    hmclStop_->setEnabled(false);
    hmclRow->addWidget(hmclRefresh_);
    hmclRow->addWidget(hmclStop_);
    form->addLayout(hmclRow);
    hmclBusy_ = new QProgressBar(card);
    hmclBusy_->setRange(0, 0);
    hmclBusy_->setTextVisible(false);
    hmclBusy_->setFixedHeight(4);
    form->addWidget(hmclBusy_);

    form->addWidget(new QLabel("FLFHMCL 版本", card));
    auto* agentRow = new QHBoxLayout();
    createAgentChannel_ = new QComboBox(card);
    createAgentChannel_->addItem("稳定版", "stable");
    createAgentChannel_->addItem("预览版", "preview");
    createAgent_ = new QComboBox(card);
    createAgent_->addItem("正在加载 FLFHMCL 版本…");
    createAgent_->setEnabled(false);
    agentRow->addWidget(createAgentChannel_, 1);
    agentRow->addWidget(createAgent_, 2);
    agentRefresh_ = new QPushButton("刷新", card);
    agentStop_ = new QPushButton("停止", card);
    agentStop_->setEnabled(false);
    agentRow->addWidget(agentRefresh_);
    agentRow->addWidget(agentStop_);
    form->addLayout(agentRow);
    agentBusy_ = new QProgressBar(card);
    agentBusy_->setRange(0, 0);
    agentBusy_->setTextVisible(false);
    agentBusy_->setFixedHeight(4);
    form->addWidget(agentBusy_);

    auto* fabricLabel = new QLabel("Fabric Loader 版本", card);
    form->addWidget(fabricLabel);
    auto* fabricRow = new QHBoxLayout();
    createFabric_ = new QComboBox(card);
    createFabric_->addItem("正在加载 Fabric 版本…");
    createFabric_->setEnabled(false);
    fabricRefresh_ = new QPushButton("刷新", card);
    fabricStop_ = new QPushButton("停止", card);
    fabricStop_->setEnabled(false);
    fabricRow->addWidget(createFabric_, 1);
    fabricRow->addWidget(fabricRefresh_);
    fabricRow->addWidget(fabricStop_);
    form->addLayout(fabricRow);
    fabricBusy_ = new QProgressBar(card);
    fabricBusy_->setRange(0, 0);
    fabricBusy_->setTextVisible(false);
    fabricBusy_->setFixedHeight(4);
    form->addWidget(fabricBusy_);

    connect(createHmclChannel_, &QComboBox::currentIndexChanged, this, [this](int) {
        fillVersionCombo(createHmclChannel_, createHmcl_, hmclStable_, hmclPreview_);
    });
    connect(createAgentChannel_, &QComboBox::currentIndexChanged, this, [this](int) {
        fillVersionCombo(createAgentChannel_, createAgent_, agentStable_, agentPreview_);
    });
    connect(hmclRefresh_, &QPushButton::clicked, this, [this]() { startHmclLoad(); });
    connect(hmclStop_, &QPushButton::clicked, this, [this]() { stopHmclLoad(); });
    connect(agentRefresh_, &QPushButton::clicked, this, [this]() { startAgentLoad(); });
    connect(agentStop_, &QPushButton::clicked, this, [this]() { stopAgentLoad(); });
    connect(fabricRefresh_, &QPushButton::clicked, this, [this]() { startFabricLoad(); });
    connect(fabricStop_, &QPushButton::clicked, this, [this]() { stopFabricLoad(); });
    createIsolated_ = new QCheckBox("隔离 .minecraft 与 .hmcl 到 .launcher/<实例>", card);
    form->addWidget(createIsolated_);

    layout->addWidget(card);
    layout->addStretch();

    auto* buttons = new QHBoxLayout();
    auto* back = new QPushButton("返回", page);
    auto* start = new QPushButton("开始下载", page);
    start->setObjectName("primary");
    buttons->addWidget(back);
    buttons->addStretch();
    buttons->addWidget(start);
    layout->addLayout(buttons);

    connect(back, &QPushButton::clicked, this, [this]() {
        stopAgentLoad();
        stopHmclLoad();
        stopFabricLoad();
        showPage(0);
    });
    connect(start, &QPushButton::clicked, this, &MainWindow::beginInstall);
    return page;
}

QWidget* MainWindow::buildInstallPage() {
    auto* page = new QWidget(this);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(16);

    auto* stats = new QFrame(page);
    stats->setObjectName("card");
    stats->setFixedWidth(220);
    auto* statLayout = new QVBoxLayout(stats);
    auto* statTitle = new QLabel("安装进度", stats);
    statTitle->setObjectName("section");
    statLayout->addWidget(statTitle);
    installPercent_ = new QLabel("0 %", stats);
    installPercent_->setStyleSheet("font-size: 26px; font-weight: 700;");
    statLayout->addWidget(installPercent_);
    installBar_ = new QProgressBar(stats);
    installBar_->setRange(0, 100);
    installBar_->setTextVisible(false);
    installBar_->setFixedHeight(7);
    statLayout->addWidget(installBar_);
    auto* hint = new QLabel("下载在后台进行，界面不会卡住。", stats);
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    statLayout->addWidget(hint);
    statLayout->addStretch();
    layout->addWidget(stats);

    auto* right = new QFrame(page);
    right->setObjectName("card");
    auto* rightLayout = new QVBoxLayout(right);
    installTitle_ = new QLabel("正在准备…", right);
    installTitle_->setObjectName("section");
    rightLayout->addWidget(installTitle_);
    installRightBar_ = new QProgressBar(right);
    installRightBar_->setRange(0, 100);
    installRightBar_->setTextVisible(false);
    installRightBar_->setFixedHeight(7);
    rightLayout->addWidget(installRightBar_);
    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* stepsHost = new QWidget(scroll);
    installStepsLayout_ = new QVBoxLayout(stepsHost);
    installStepsLayout_->setContentsMargins(0, 0, 0, 0);
    installStepsLayout_->setSpacing(8);
    installStepsLayout_->addStretch();
    scroll->setWidget(stepsHost);
    rightLayout->addWidget(scroll, 1);
    installRetry_ = new QPushButton("重试", right);
    installRetry_->setObjectName("primary");
    installRetry_->hide();
    rightLayout->addWidget(installRetry_, 0, Qt::AlignRight);
    connect(installRetry_, &QPushButton::clicked, this, &MainWindow::beginInstall);
    layout->addWidget(right, 1);
    return page;
}

void MainWindow::fillVersionCombo(QComboBox* channel, QComboBox* version,
                                  const QStringList& stable, const QStringList& preview) {
    if (!channel || !version) return;
    const bool usePreview = channel->currentData().toString() == "preview";
    const QStringList& list = usePreview ? preview : stable;
    version->clear();
    for (const QString& item : list) {
        version->addItem(item, item);
    }
    if (list.isEmpty()) {
        version->addItem("暂无可用版本");
        version->setEnabled(false);
    } else {
        version->setEnabled(true);
    }
}
void MainWindow::beginInstall() {
    const QString name = createName_ ? createName_->text().trimmed() : QString();
    if (name.isEmpty()) {
        createError_->setText("名称不能为空");
        return;
    }
    QString duplicateName;
    for (const auto& instance : Workspace::get().instances()) {
        if (qs(instance.name) == name) {
            duplicateName = "已有同名实例；仍会按 UUID 独立存储";
        }
    }
    if (createError_) createError_->setText(duplicateName);

    const QString id = makeId(name);
    Instance instance;
    instance.uuid = newInstanceUuid();
    instance.id = ss(id);
    instance.name = ss(name);
    instance.gameVersion = "1.21.1";
    instance.loaderVersion = createFabric_ && !createFabric_->currentData().toString().isEmpty()
            ? ss(createFabric_->currentData().toString()) : "0.19.5";
    instance.hmclVersion = createHmcl_ ? ss(createHmcl_->currentData().toString()) : "";
    instance.isolated = createIsolated_ && createIsolated_->isChecked();
    instance.agentVersion = createAgent_ ? ss(createAgent_->currentData().toString()) : "2.1.4";
    instance.createdAt = static_cast<std::int64_t>(QDateTime::currentSecsSinceEpoch());

    installDetailBars_.clear();
    installDetailPercents_.clear();
    installStarted_.clear();
    installCompleted_.clear();
    installPinned_.clear();
    if (installStepsLayout_) {
        while (installStepWidgets_.size() < 7) {
            auto* card = new QFrame(this);
            card->setObjectName("card");
            auto* layout = new QVBoxLayout(card);
            layout->setContentsMargins(10, 8, 10, 8);
            auto* header = new QHBoxLayout();
            auto* label = new QLabel("· 等待", card);
            auto* percent = new QLabel("0%", card);
            percent->setObjectName("hint");
            percent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto* toggle = new QToolButton(card);
            toggle->setText("▾");
            toggle->setAutoRaise(true);
            header->addWidget(label, 1);
            header->addWidget(percent);
            header->addWidget(toggle);
            layout->addLayout(header);
            auto* bar = new QProgressBar(card);
            bar->setRange(0, 100);
            bar->setValue(0);
            bar->setTextVisible(false);
            bar->setFixedHeight(6);
            layout->addWidget(bar);
            auto* body = new QWidget(card);
            auto* details = new QVBoxLayout(body);
            details->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(body);
            installStepsLayout_->insertWidget(installStepsLayout_->count() - 1, card);
            installStepWidgets_.append(card);
            installStepLabels_.append(label);
            installStepPercents_.append(percent);
            installStepBars_.append(bar);
            installStepDetails_.append(details);
            installStepToggles_.append(toggle);
            installStepBodies_.append(body);
            const int stepIndex = installStepWidgets_.size() - 1;
            connect(toggle, &QToolButton::clicked, this, [this, stepIndex, body, toggle]() {
                const bool show = !body->isVisible();
                body->setVisible(show);
                toggle->setText(show ? "▾" : "▸");
                if (show) installPinned_.insert(stepIndex);
                else installPinned_.remove(stepIndex);
            });
        }
        const QStringList names = {"准备 Java", "下载 JavaFX", "安装 Fabric",
                                   "安装依赖库", "下载 HMCL", "下载 FLFHMCL Agent",
                                   "写入实例配置"};
        for (int i = 0; i < names.size(); ++i) {
            installStepLabels_[i]->setText("· " + names[i]);
            installStepBars_[i]->setRange(0, 0);
            if (i < installStepPercents_.size()) installStepPercents_[i]->setText("0%");
            while (QLayoutItem* item = installStepDetails_[i]->takeAt(0)) {
                if (item->widget()) item->widget()->deleteLater();
                delete item;
            }
        }
    }
    installBar_->setRange(0, 0);
    lastInstallPercent_ = 0;
    installPercent_->setText("准备中…");
    installTitle_->setText("正在准备 " + name);
    showPage(6);

    std::thread([this, instance]() mutable {
        std::string error;
        const bool ok = installInstance(instance, [this](const std::string& step, int percent) {
            QMetaObject::invokeMethod(this, [this, step = qs(step), percent]() {
                updateInstallStep(step, percent);
            }, Qt::QueuedConnection);
        }, &error);
        QMetaObject::invokeMethod(this, [this, instance, ok, error = qs(error)]() {
            finishInstall(instance, ok, error);
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::updateInstallStep(const QString& step, int percent) {
    if (!installBar_ || !installRightBar_) return;
    static const int bounds[] = {0, 5, 10, 30, 62, 75, 92, 100};

    int mapped = -1;
    if (step.startsWith("准备 Java")) mapped = 0;
    else if (step.contains("JavaFX")) mapped = 1;
    else if (step.startsWith("安装 Fabric") || step.startsWith("Fabric 库")) mapped = 2;
    else if (step.contains("HMCL 依赖")) mapped = 3;
    else if (step.startsWith("HMCL ")) mapped = 4;
    else if (step.contains("Agent") || step.startsWith("FLF API") || step.startsWith("FLFHMCL 依赖")) mapped = 5;
    else if (step.contains("写入") || step == "完成") mapped = 6;

    if (mapped >= 0) {
        for (int i = 0; i < mapped; ++i) {
            installStarted_.insert(i);
            installCompleted_.insert(i);
        }
        installStarted_.insert(mapped);
        if (mapped == 6 && percent >= 98) installCompleted_.insert(6);
    }

    installTitle_->setText(step);
    const int span = mapped >= 0 ? std::max(1, bounds[mapped + 1] - bounds[mapped]) : 1;
    const int activeLocal = mapped >= 0
            ? qBound(0, (percent - bounds[mapped]) * 100 / span, 100) : 0;

    // 更新详情行
    if (mapped >= 0 && mapped < installStepDetails_.size()) {
        QString detailKey = step;
        int detailPercent = activeLocal;
        static const QRegularExpression percentPattern("\\s+(\\d+)%$");
        const auto match = percentPattern.match(detailKey);
        if (match.hasMatch()) {
            detailPercent = match.captured(1).toInt();
            detailKey = detailKey.left(match.capturedStart()).trimmed();
        }
        const bool detail = detailKey.contains(':') || detailKey.startsWith("JavaFX")
                || detailKey.startsWith("HMCL ") || detailKey.contains("依赖 ");
        if (detail) {
            QProgressBar* detailBar = installDetailBars_.value(detailKey, nullptr);
            if (detailBar == nullptr) {
                auto* row = new QWidget(this);
                auto* rowLayout = new QVBoxLayout(row);
                rowLayout->setContentsMargins(16, 0, 0, 0);
                auto* header = new QHBoxLayout();
                auto* label = new QLabel(detailKey, row);
                label->setObjectName("hint");
                auto* percentLabel = new QLabel("pending", row);
                percentLabel->setObjectName("hint");
                header->addWidget(label, 1);
                header->addWidget(percentLabel);
                rowLayout->addLayout(header);
                detailBar = new QProgressBar(row);
                detailBar->setRange(0, 0);
                detailBar->setTextVisible(false);
                detailBar->setFixedHeight(5);
                rowLayout->addWidget(detailBar);
                installDetailPercents_.insert(detailKey, percentLabel);
                installStepDetails_[mapped]->addWidget(row);
                installDetailBars_.insert(detailKey, detailBar);
            }
            if (detailPercent > 0) {
                if (detailBar->maximum() == 0) detailBar->setRange(0, 100);
                detailBar->setValue(detailPercent);
                if (auto* label = installDetailPercents_.value(detailKey, nullptr)) {
                    label->setText(QStringLiteral("%1%").arg(detailPercent));
                }
            } else if (installDetailPercents_.value(detailKey, nullptr) != nullptr) {
                installDetailPercents_.value(detailKey)->setText("pending");
            }
        }
    }

    double overallSum = 0;
    bool allPending = true;
    for (int i = 0; i < installStepBars_.size(); ++i) {
        int value = 0;
        bool pending = false;
        if (installCompleted_.contains(i)) {
            value = 100;
            allPending = false;
        } else if (installStarted_.contains(i)) {
            if (i == mapped && activeLocal > 0) {
                value = activeLocal;
                allPending = false;
            }
            int childCount = 0;
            double childSum = 0;
            if (i < installStepDetails_.size()) {
                QVBoxLayout* details = installStepDetails_[i];
                for (int j = 0; j < details->count(); ++j) {
                    if (QWidget* row = details->itemAt(j)->widget()) {
                        if (QProgressBar* bar = row->findChild<QProgressBar*>()) {
                            if (bar->maximum() > 0) {
                                childSum += bar->value();
                                ++childCount;
                            }
                        }
                    }
                }
            }
            if (childCount > 0) {
                value = static_cast<int>(childSum / childCount);
                allPending = false;
            } else if (value == 0) {
                pending = true;
            }
        } else {
            pending = true;
        }

        if (pending) {
            if (installStepBars_[i]->maximum() != 0) installStepBars_[i]->setRange(0, 0);
            if (i < installStepPercents_.size()) installStepPercents_[i]->setText("pending");
        } else {
            if (installStepBars_[i]->maximum() == 0) installStepBars_[i]->setRange(0, 100);
            installStepBars_[i]->setValue(value);
            if (i < installStepPercents_.size()) {
                installStepPercents_[i]->setText(QStringLiteral("%1%").arg(value));
            }
        }
        overallSum += pending ? 0 : value;
    }

    const int overall = qBound(0, static_cast<int>(overallSum / std::max<int>(1, installStepBars_.size())), 100);
    if (allPending) {
        if (installBar_->maximum() != 0) installBar_->setRange(0, 0);
        if (installRightBar_->maximum() != 0) installRightBar_->setRange(0, 0);
        installPercent_->setText("pending");
    } else {
        if (installBar_->maximum() == 0) installBar_->setRange(0, 100);
        if (installRightBar_->maximum() == 0) installRightBar_->setRange(0, 100);
        installBar_->setValue(overall);
        installRightBar_->setValue(overall);
        installPercent_->setText(QStringLiteral("%1 %").arg(overall));
    }

    for (int i = 0; i < installStepBodies_.size(); ++i) {
        const bool completed = installCompleted_.contains(i);
        const bool show = !completed || installPinned_.contains(i);
        installStepBodies_[i]->setVisible(show);
        if (i < installStepToggles_.size()) installStepToggles_[i]->setText(show ? "▾" : "▸");
    }
}
void MainWindow::beginRepair(const Instance& instance) {
    repairMode_ = true;
    if (installStepsLayout_) {
        const QStringList names = {"检查 Java", "检查 Agent", "检查 HMCL",
                                   "检查 Fabric Loader", "补全依赖库",
                                   "重建配置", "重新校验"};
        for (int i = 0; i < names.size() && i < installStepLabels_.size(); ++i) {
            installStepLabels_[i]->setText("· " + names[i]);
            installStepBars_[i]->setRange(0, 0);
            if (i < installStepPercents_.size()) installStepPercents_[i]->setText("pending");
            while (QLayoutItem* item = installStepDetails_[i]->takeAt(0)) {
                if (item->widget()) item->widget()->deleteLater();
                delete item;
            }
        }
        installDetailBars_.clear();
        installDetailPercents_.clear();
        installStarted_.clear();
        installCompleted_.clear();
    }
    if (installRetry_) installRetry_->hide();
    installTitle_->setText("正在修复 " + qs(instance.name));
    installPercent_->setText("pending");
    installBar_->setRange(0, 0);
    installRightBar_->setRange(0, 0);
    showPage(6);

    Instance work = instance;
    std::thread([this, work]() mutable {
        std::string error;
        const bool ok = repairInstance(work, [this](const std::string& step, int percent) {
            QMetaObject::invokeMethod(this, [this, step = qs(step), percent]() {
                updateInstallStep(step, percent);
            }, Qt::QueuedConnection);
        }, &error);
        QMetaObject::invokeMethod(this, [this, work, ok, error = qs(error)]() {
            finishRepair(work, ok, error);
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::finishRepair(const Instance& instance, bool ok, const QString& error) {
    repairMode_ = false;
    if (!ok) {
        installFailed_ = true;
        installTitle_->setText("修复失败");
        installPercent_->setText("失败");
        if (installRetry_) installRetry_->show();
        showToast("修复失败", error, true);
        return;
    }
    Instance* target = Workspace::get().find(instance.uuid);
    if (target) {
        *target = instance;
        Workspace::get().saveInstances();
    }
    std::string validationError;
    if (target && !validateInstanceFiles(*target, &validationError)) {
        showToast("修复后校验失败", qs(validationError), true);
        return;
    }
    refreshInstances();
    showPage(0);
    showToast("修复完成", "实例已恢复，可以启动", false);
}
void MainWindow::finishInstall(const Instance& instance, bool ok, const QString& error) {
    if (!ok) {
        installFailed_ = true;
        installTitle_->setText("安装失败");
        installPercent_->setText("失败");
        if (installRetry_) installRetry_->show();
        showToast("安装失败", error, true);
        return;
    }
    Workspace::get().instances().push_back(instance);
    Workspace::get().saveInstances();
    refreshInstances();
    setCurrent(instance.uuid);
    showPage(0);
    showToast("实例已创建", QStringLiteral("%1 已准备完成").arg(qs(instance.name)), false);
}
void MainWindow::showUpdateDialog() {
    Instance* instance = selectedInstance();
    if (instance == nullptr) {
        showToast("未选择实例", "请先在右侧选择一个实例", true);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("更新实例组件");
    dialog.setMinimumWidth(460);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(10);

    auto* hint = new QLabel("勾选要更新的组件，并选择目标版本。", &dialog);
    hint->setObjectName("hint");
    layout->addWidget(hint);

    auto makeRow = [&](const QString& name, const QString& current,
                       QCheckBox*& check, QComboBox*& combo) {
        auto* row = new QHBoxLayout();
        check = new QCheckBox(name + QStringLiteral("（当前 %1）").arg(current), &dialog);
        combo = new QComboBox(&dialog);
        combo->addItem("加载中…");
        combo->setEnabled(false);
        combo->setMinimumWidth(160);
        row->addWidget(check, 1);
        row->addWidget(combo);
        layout->addLayout(row);
    };

    QCheckBox* hmclCheck = nullptr;
    QCheckBox* agentCheck = nullptr;
    QCheckBox* loaderCheck = nullptr;
    QComboBox* hmclCombo = nullptr;
    QComboBox* agentCombo = nullptr;
    QComboBox* loaderCombo = nullptr;
    makeRow("HMCL", qs(instance->hmclVersion), hmclCheck, hmclCombo);
    makeRow("FLFHMCL", qs(instance->agentVersion), agentCheck, agentCombo);
    makeRow("Fabric Loader", qs(instance->loaderVersion), loaderCheck, loaderCombo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("开始更新");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QPointer<QComboBox> hmclGuard = hmclCombo;
    QPointer<QComboBox> agentGuard = agentCombo;
    QPointer<QComboBox> loaderGuard = loaderCombo;

    std::thread([this, hmclGuard]() {
        std::string error;
        auto versions = fetchHmclVersions(nullptr, &error);
        QStringList list;
        for (const auto& version : versions) list.append(qs(version));
        QMetaObject::invokeMethod(this, [hmclGuard, list]() {
            if (!hmclGuard) return;
            hmclGuard->clear();
            for (const QString& version : list) hmclGuard->addItem(version, version);
            hmclGuard->setEnabled(!list.isEmpty());
        }, Qt::QueuedConnection);
    }).detach();

    std::thread([this, agentGuard]() {
        std::string error;
        const FlfRelease release = fetchFlfRelease("flfhmcl", &error);
        QStringList list;
        for (const auto& item : release.versions) list.append(qs(item.version));
        QMetaObject::invokeMethod(this, [agentGuard, list]() {
            if (!agentGuard) return;
            agentGuard->clear();
            for (const QString& version : list) agentGuard->addItem(version, version);
            agentGuard->setEnabled(!list.isEmpty());
        }, Qt::QueuedConnection);
    }).detach();

    std::thread([this, loaderGuard]() {
        std::string error;
        auto loaders = fetchFabricLoaders("1.21.1", &error);
        QStringList list;
        for (const auto& loader : loaders) list.append(qs(loader));
        QMetaObject::invokeMethod(this, [loaderGuard, list]() {
            if (!loaderGuard) return;
            loaderGuard->clear();
            for (const QString& loader : list) loaderGuard->addItem(loader, loader);
            loaderGuard->setEnabled(!list.isEmpty());
        }, Qt::QueuedConnection);
    }).detach();

    if (dialog.exec() != QDialog::Accepted) return;
    if (!hmclCheck->isChecked() && !agentCheck->isChecked() && !loaderCheck->isChecked()) {
        showToast("未选择组件", "请至少勾选一个组件", false);
        return;
    }

    Instance updated = *instance;
    if (hmclCheck->isChecked() && hmclCombo->currentData().isValid()) {
        updated.hmclVersion = ss(hmclCombo->currentData().toString());
        updated.hmclSha1.clear();
    }
    if (agentCheck->isChecked() && agentCombo->currentData().isValid()) {
        updated.agentVersion = ss(agentCombo->currentData().toString());
    }
    if (loaderCheck->isChecked() && loaderCombo->currentData().isValid()) {
        updated.loaderVersion = ss(loaderCombo->currentData().toString());
    }
    beginComponentUpdate(updated);
}

void MainWindow::beginComponentUpdate(const Instance& instance) {
    repairMode_ = true;
    if (installStepsLayout_) {
        const QStringList names = {"准备 Java", "安装 HMCL", "安装 FLFHMCL Agent",
                                   "安装 Fabric Loader", "补全依赖库",
                                   "重建配置", "校验"};
        for (int i = 0; i < names.size() && i < installStepLabels_.size(); ++i) {
            installStepLabels_[i]->setText("· " + names[i]);
            installStepBars_[i]->setRange(0, 0);
            if (i < installStepPercents_.size()) installStepPercents_[i]->setText("pending");
            while (QLayoutItem* item = installStepDetails_[i]->takeAt(0)) {
                if (item->widget()) item->widget()->deleteLater();
                delete item;
            }
        }
        installDetailBars_.clear();
        installDetailPercents_.clear();
        installStarted_.clear();
        installCompleted_.clear();
    }
    if (installRetry_) installRetry_->hide();
    installTitle_->setText("正在更新 " + qs(instance.name));
    installPercent_->setText("pending");
    installBar_->setRange(0, 0);
    installRightBar_->setRange(0, 0);
    showPage(6);

    Instance work = instance;
    std::thread([this, work]() mutable {
        std::string error;
        const bool ok = installInstance(work, [this](const std::string& step, int percent) {
            QMetaObject::invokeMethod(this, [this, step = qs(step), percent]() {
                updateInstallStep(step, percent);
            }, Qt::QueuedConnection);
        }, &error);
        QMetaObject::invokeMethod(this, [this, work, ok, error = qs(error)]() {
            repairMode_ = false;
            if (!ok) {
                installFailed_ = true;
                installTitle_->setText("更新失败");
                installPercent_->setText("失败");
                if (installRetry_) installRetry_->show();
                showToast("更新失败", error, true);
                return;
            }
            if (Instance* target = Workspace::get().find(work.uuid)) {
                *target = work;
                Workspace::get().saveInstances();
            }
            refreshInstances();
            showPage(0);
            showToast("更新完成", qs(work.name) + " 的组件已更新", false);
        }, Qt::QueuedConnection);
    }).detach();
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
    addOp("更新实例组件", [this]() { showUpdateDialog(); });
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
    if (instanceValidate_ && instanceUpdate_) {
        Instance* selected = selectedInstance();
        instanceValidate_->setChecked(selected ? selected->validateBeforeLaunch : true);
        instanceUpdate_->setChecked(selected ? selected->checkUpdatesBeforeLaunch : true);
    }
    if (behaviorInstance_) {
        behaviorInstance_->blockSignals(true);
        Instance* instance = selectedInstance();
        const QString value = instance ? qs(instance->launchBehavior) : QString();
        behaviorInstance_->setCurrentIndex(std::max(0, behaviorInstance_->findData(value)));
        behaviorInstance_->blockSignals(false);
    }
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
    if (!error) {
        auto* toast = new QLabel(this);
        toast->setAttribute(Qt::WA_DeleteOnClose);
        toast->setStyleSheet(
                "background: #1f2a3a; color: #e8eef7; border: 1px solid #3b82f6;"
                "border-radius: 8px; padding: 10px 16px;");
        toast->setText(detail.isEmpty() ? title : title + "\n" + detail);
        toast->adjustSize();
        toast->move(width() - toast->width() - 28, 64);
        toast->show();
        toast->raise();
        QTimer::singleShot(2600, toast, &QLabel::close);
        return;
    }
#ifdef FLF_HAVE_HIBERGUI
    if (toast_) { toast_->showError(title, detail, 4000); return; }
#endif
    QMessageBox::warning(this, title, detail);
}

void MainWindow::createInstance() {
    showPage(5);
    startAgentLoad();
    startHmclLoad();
    startFabricLoad();
}

void MainWindow::startAgentLoad() {
    const int token = ++agentToken_;
    if (createAgent_) {
        createAgent_->clear();
        createAgent_->addItem("正在加载 FLFHMCL 版本…");
        createAgent_->setEnabled(false);
    }
    if (agentBusy_) agentBusy_->show();
    if (agentStop_) agentStop_->setEnabled(true);
    if (agentRefresh_) agentRefresh_->setEnabled(false);

    std::thread([this, token]() {
        std::string error;
        const FlfRelease release = fetchFlfRelease("flfhmcl", &error);
        QStringList versions;
        QString latest;
        if (release.ok) {
            for (const auto& item : release.versions) versions.append(qs(item.version));
            latest = qs(release.version);
        }
        QMetaObject::invokeMethod(this, [this, token, versions, latest]() {
            if (token != agentToken_) return;
            if (agentBusy_) agentBusy_->hide();
            if (agentStop_) agentStop_->setEnabled(false);
            if (agentRefresh_) agentRefresh_->setEnabled(true);
            if (!createAgent_) return;
            agentStable_.clear();
            agentPreview_.clear();
            for (const QString& version : versions) {
                if (version.contains('-')) agentPreview_.append(version);
                else agentStable_.append(version);
            }
            if (agentStable_.isEmpty()) agentStable_.append(latest.isEmpty() ? "2.1.4" : latest);
            fillVersionCombo(createAgentChannel_, createAgent_, agentStable_, agentPreview_);
            const int index = createAgent_->findData(latest);
            if (index >= 0) createAgent_->setCurrentIndex(index);
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::startHmclLoad() {
    const int token = ++hmclToken_;
    if (createHmcl_) {
        createHmcl_->clear();
        createHmcl_->addItem("正在加载 HMCL 版本…");
        createHmcl_->setEnabled(false);
    }
    if (hmclBusy_) hmclBusy_->show();
    if (hmclStop_) hmclStop_->setEnabled(true);
    if (hmclRefresh_) hmclRefresh_->setEnabled(false);

    std::thread([this, token]() {
        bool githubAvailable = false;
        std::string error;
        const auto versions = fetchHmclVersions(&githubAvailable, &error);
        QStringList list;
        for (const auto& version : versions) list.append(qs(version));
        QMetaObject::invokeMethod(this, [this, token, list, githubAvailable]() {
            if (token != hmclToken_) return;
            if (hmclBusy_) hmclBusy_->hide();
            if (hmclStop_) hmclStop_->setEnabled(false);
            if (hmclRefresh_) hmclRefresh_->setEnabled(true);
            if (!createHmcl_) return;
            hmclStable_.clear();
            hmclPreview_.clear();
            for (const QString& version : list) {
                if (version.count('.') >= 3) hmclPreview_.append(version);
                else hmclStable_.append(version);
            }
            if (hmclStable_.isEmpty() && hmclPreview_.isEmpty()) hmclStable_.append("3.16.4");
            fillVersionCombo(createHmclChannel_, createHmcl_, hmclStable_, hmclPreview_);
            if (!githubAvailable) {
                showToast("HMCL 版本列表不完整", "GitHub 不可用，仅显示官方最新版本", false);
            }
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::startFabricLoad() {
    const int token = ++fabricToken_;
    if (createFabric_) {
        createFabric_->clear();
        createFabric_->addItem("正在加载 Fabric 版本…");
        createFabric_->setEnabled(false);
    }
    if (fabricBusy_) fabricBusy_->show();
    if (fabricStop_) fabricStop_->setEnabled(true);
    if (fabricRefresh_) fabricRefresh_->setEnabled(false);

    std::thread([this, token]() {
        std::string error;
        const auto loaders = fetchFabricLoaders("1.21.1", &error);
        QStringList list;
        for (const auto& loader : loaders) list.append(qs(loader));
        QMetaObject::invokeMethod(this, [this, token, list]() {
            if (token != fabricToken_) return;
            if (fabricBusy_) fabricBusy_->hide();
            if (fabricStop_) fabricStop_->setEnabled(false);
            if (fabricRefresh_) fabricRefresh_->setEnabled(true);
            if (!createFabric_) return;
            createFabric_->setEnabled(true);
            createFabric_->clear();
            for (const QString& loader : list) createFabric_->addItem("Fabric " + loader, loader);
            if (list.isEmpty()) createFabric_->addItem("Fabric 0.19.5", "0.19.5");
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::stopAgentLoad() {
    ++agentToken_;
    if (agentBusy_) agentBusy_->hide();
    if (agentStop_) agentStop_->setEnabled(false);
    if (agentRefresh_) agentRefresh_->setEnabled(true);
    if (createAgent_) {
        createAgent_->clear();
        createAgent_->addItem("已停止");
        createAgent_->setEnabled(false);
    }
}

void MainWindow::stopHmclLoad() {
    ++hmclToken_;
    if (hmclBusy_) hmclBusy_->hide();
    if (hmclStop_) hmclStop_->setEnabled(false);
    if (hmclRefresh_) hmclRefresh_->setEnabled(true);
    if (createHmcl_) {
        createHmcl_->clear();
        createHmcl_->addItem("已停止");
        createHmcl_->setEnabled(false);
    }
}

void MainWindow::stopFabricLoad() {
    ++fabricToken_;
    if (fabricBusy_) fabricBusy_->hide();
    if (fabricStop_) fabricStop_->setEnabled(false);
    if (fabricRefresh_) fabricRefresh_->setEnabled(true);
    if (createFabric_) {
        createFabric_->clear();
        createFabric_->addItem("已停止");
        createFabric_->setEnabled(false);
    }
}
void MainWindow::runInstall(const QString& name, bool isolated, const QString& agentVersion) {
    Instance instance;
    instance.uuid = newInstanceUuid();
    instance.id = ss(makeId(name));
    instance.name = ss(name);
    instance.gameVersion = "1.21.1";
    instance.loaderVersion = createFabric_ && !createFabric_->currentData().toString().isEmpty()
            ? ss(createFabric_->currentData().toString()) : "0.19.5";
    instance.hmclVersion = createHmcl_ ? ss(createHmcl_->currentData().toString()) : "";
    instance.isolated = isolated;
    instance.agentVersion = ss(agentVersion);
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
    {
        std::error_code ec;
        const fs::path profile = Workspace::get().profileDir(instance);
        fs::create_directories(profile, ec);
        std::ofstream(profile / "instance.json") << toJson(instance).dump(2);
        nlohmann::json launch = {
            {"instance", instance.uuid},
            {"gameDir", Workspace::get().resolveMinecraft(instance).string()},
            {"hmclHome", Workspace::get().resolveHmclHome(instance).string()},
            {"mods", Workspace::get().resolveMods(instance).string()},
            {"manifest", Workspace::get().manifestPath(instance).string()},
        };
        std::ofstream(profile / "launch.json") << launch.dump(2);
    }
    progress.setValue(100);
    refreshInstances();
    setCurrent(instance.id);
    showPage(0);
    showToast("实例已就绪", "现在可以点击“启动”开始游戏", false);
}

void MainWindow::launchSelected() {
    Instance* instance = selectedInstance();
    if (!instance) {
        showToast("还没有选择实例", "先点一张实例卡片，再点启动", true);
        return;
    }
    if (launching_.contains(qs(instance->uuid))) {
        showToast("该实例正在运行", "可以同时启动其他实例", false);
        return;
    }

    {
        std::error_code ec;
        const fs::path profile = Workspace::get().profileDir(*instance);
        fs::create_directories(profile, ec);
        if (!fs::exists(profile / "instance.json")) {
            std::ofstream(profile / "instance.json") << toJson(*instance).dump(2);
        }
        if (!fs::exists(profile / "launch.json")) {
            nlohmann::json launch = {
                {"instance", instance->uuid},
                {"gameDir", Workspace::get().resolveMinecraft(*instance).string()},
                {"hmclHome", Workspace::get().resolveHmclHome(*instance).string()},
                {"mods", Workspace::get().resolveMods(*instance).string()},
                {"manifest", Workspace::get().manifestPath(*instance).string()},
            };
            std::ofstream(profile / "launch.json") << launch.dump(2);
        }
    }
    if (instance->validateBeforeLaunch && Workspace::get().config().validateBeforeLaunch) {
        std::string validationError;
        if (!validateInstanceFiles(*instance, &validationError)) {
            showToast("文件校验失败", qs(validationError) + "，正在尝试修复", false);
            beginRepair(*instance);
            return;
        }
    }
    if (instance->checkUpdatesBeforeLaunch && Workspace::get().config().checkUpdatesBeforeLaunch) {
        std::thread([this]() {
            std::string updateError;
            const HmclRelease latest = fetchHmclRelease(&updateError);
            if (!latest.ok) return;
            if (latest.version != Workspace::get().config().hmclVersion) {
                QMetaObject::invokeMethod(this, [this, version = qs(latest.version)]() {
                    showToast("发现 HMCL 更新", QStringLiteral("HMCL %1 可用").arg(version), false);
                }, Qt::QueuedConnection);
            }
        }).detach();
    }
    const QString name = qs(instance->name);
    const QString logPath = QString::fromStdWString(
            (Workspace::get().home() / "logs"
             / ("instance-" + instance->uuid + ".log")).wstring());
    QDir().mkpath(QFileInfo(logPath).absolutePath());
    runLog_.clear();
    launching_.insert(qs(instance->uuid));

    std::string error;
    const bool ok = launchInstanceManaged(
            *instance,
            [this, logPath, uuid = qs(instance->uuid)](const std::string& chunk) {
                const QString text = QString::fromUtf8(chunk.data(), static_cast<int>(chunk.size()));
                QMetaObject::invokeMethod(this, [this, text, logPath, uuid]() {
                    runLog_ += text;
                    QFile file(logPath);
                    if (file.open(QIODevice::Append)) {
                        file.write(text.toUtf8());
                    }
                    if (!started_.contains(uuid) && text.contains("Loading Done!")) {
                        started_.insert(uuid);
                        showToast("HMCL 启动完成", "可以开始使用了", false);
                    }
                }, Qt::QueuedConnection);
            },
            [this, name, uuid = qs(instance->uuid)](int code, const std::string& output) {
                const QString text = QString::fromUtf8(output.data(), static_cast<int>(output.size()));
                QMetaObject::invokeMethod(this, [this, code, text, name, uuid]() {
                    launching_.remove(uuid);
                    started_.remove(uuid);
                    const bool failed = code != 0
                            || text.contains("Bootstrap failed")
                            || text.contains("ExceptionInInitializerError")
                            || text.contains("duplicate fabric loader");
                    if (!failed) {
                        showToast("HMCL 已退出", name + " 正常退出", false);
                        return;
                    }
                    QStringList details;
                    for (const QString& line : text.split('\n')) {
                        if (line.contains("Exception") || line.contains("Error")
                                || line.contains("Caused by") || line.contains("crash")) {
                            details.append(line.trimmed());
                        }
                    }
                    if (details.size() > 12) {
                        details = details.mid(details.size() - 12);
                    }
                    const QString summary = details.isEmpty()
                            ? QStringLiteral("未从输出中找到异常行，请查看实例日志。")
                            : details.join("\n");
                    QString cause;
                    QString advice;
                    if (text.contains("Configuration hasn't been loaded")
                            && text.contains("AnimationUtils")) {
                        cause = "HMCL 关闭动画初始化失败";
                        advice = "这是 FLFHMCL 在 HMCL 关闭阶段提前加载 JavaFX 动画类导致的异常，不是实例文件损坏。建议更新 FLFHMCL；临时可关闭 HMCL 的窗口动画。";
                    } else if (text.contains("duplicate fabric loader")) {
                        cause = "检测到重复的 Fabric Loader 版本";
                        advice = "点击“修复实例”会按实例选定的版本重建清单，并清理重复 Loader。";
                    } else if (text.contains("Bootstrap failed")) {
                        cause = "Fabric Loader 引导失败";
                        advice = "点击“修复实例”可补全 Agent、HMCL、Fabric Loader 并重建配置。";
                    } else if (text.contains("UnsupportedClassVersion")) {
                        cause = "Java 版本不匹配";
                        advice = "请把实例 Java 切换到更高版本，或更新目标组件。";
                    } else if (text.contains("OutOfMemoryError")) {
                        cause = "内存不足";
                        advice = "请降低内存分配或关闭其它程序。";
                    } else if (text.contains("NoClassDefFoundError")) {
                        cause = "类加载失败";
                        advice = "可能是组件版本不匹配或库缺失，可尝试修复实例。";
                    } else {
                        cause = "HMCL 异常退出";
                        advice = "可查看实例日志，或点击“复制诊断”反馈。";
                    }
                    QMessageBox box(this);
                    box.setWindowTitle("HMCL 异常退出");
                    box.setText(cause);
                    box.setInformativeText(QStringLiteral("%1\n\n退出代码：%2\n\n%3")
                            .arg(advice).arg(failed && code == 0 ? 1 : code).arg(summary));
                    auto* repairButton = box.addButton("修复实例", QMessageBox::AcceptRole);
                    auto* copyButton = box.addButton("复制诊断", QMessageBox::ActionRole);
                    box.addButton("关闭", QMessageBox::RejectRole);
                    box.exec();
                    if (box.clickedButton() == repairButton) {
                        if (Instance* target = Workspace::get().find(ss(uuid))) {
                            beginRepair(*target);
                        }
                    } else if (box.clickedButton() == copyButton) {
                        QApplication::clipboard()->setText(
                                QStringLiteral("%1\n退出代码：%2\n\n%3")
                                        .arg(cause).arg(code).arg(text));
                        showToast("诊断已复制", "可以粘贴到反馈中", false);
                    }
                    showToast("HMCL 异常退出", cause, true);
                }, Qt::QueuedConnection);
            },
            &error);

    if (!ok) {
        launching_.remove(qs(instance->uuid));
        showToast("启动失败", qs(error), true);
        return;
    }

    std::string behavior = instance->launchBehavior;
    if (behavior.empty()) {
        behavior = Workspace::get().config().launchBehavior;
    }
    if (behavior == "hide") {
        hide();
    } else if (behavior == "exit") {
        QApplication::quit();
        return;
    }
    showToast("已启动HMCL", QStringLiteral("正在打开 %1").arg(name), false);
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
    showToast("正在检查更新…", "请稍候", false);
    std::thread([this]() {
        std::string error;
        const HmclRelease release = fetchHmclRelease(&error);
        QMetaObject::invokeMethod(this, [this, release, error = qs(error)]() {
            if (!release.ok) {
                showToast("检查更新失败", error, true);
                return;
            }
            if (release.version == Workspace::get().config().hmclVersion) {
                showToast("已经是最新版本", QStringLiteral("HMCL %1").arg(qs(release.version)), false);
                return;
            }
            showToast("发现新版本", QStringLiteral("HMCL %1 可用").arg(qs(release.version)), false);
        }, Qt::QueuedConnection);
    }).detach();
}

void MainWindow::openFolder() {
    Instance* instance = selectedInstance();
    const auto dir = instance ? Workspace::get().profileDir(*instance) : Workspace::get().home();
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
    fs::remove_all(Workspace::get().profileDir(*instance), ec);
    if (instance->isolated) {
        fs::remove_all(Workspace::get().home().parent_path() / ".launcher" / instance->uuid, ec);
    }
    fs::remove_all(Workspace::get().home() / "mods" / instance->uuid, ec);
    currentId_.clear();
    refreshInstances();
    showToast("已删除实例", "", false);
}

} // namespace flf
