#pragma once

#include "core/workspace.h"

#include <QMainWindow>

class QLabel;
class QStackedWidget;
class QVBoxLayout;
class QWidget;
class QPushButton;
class QCheckBox;
class QListWidget;

namespace HiBerGUI {
class ToastNotification;
}

namespace flf {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void refreshInstances();
    void createInstance();
    void launchSelected();
    void quickStartSelected();
    void openMods();
    void openSettings();
    void checkUpdate();
    void openFolder();
    void deleteInstance();

private:
    Instance* selectedInstance();
    void buildUi();
    QWidget* buildTopBar();
    QWidget* buildSidebar();
    QWidget* buildLaunchPage();
    QWidget* buildSettingsPage();
    QWidget* buildHelpPage();
    QWidget* buildManagePage();
    QWidget* buildModsPage();
    QWidget* buildInstanceCard(const Instance& instance);
    void showPage(int index);
    void openManage(const std::string& id);
    void refreshManage();
    void refreshMods();
    void applyTheme();
    void showToast(const QString& title, const QString& detail, bool error = false);
    void runInstall(const QString& name);
    void setCurrent(const std::string& id);
    void fadeIn(QWidget* widget);

    QStackedWidget* pages_ = nullptr;
    QVBoxLayout* cardsLayout_ = nullptr;
    QWidget* launchPage_ = nullptr;
    QWidget* settingsPage_ = nullptr;
    QWidget* helpPage_ = nullptr;
    QWidget* managePage_ = nullptr;
    QWidget* modsPage_ = nullptr;
    QListWidget* instanceList_ = nullptr;
    QListWidget* modsList_ = nullptr;
    QLabel* modsTitle_ = nullptr;
    QLabel* status_ = nullptr;
    QCheckBox* darkToggle_ = nullptr;
    HiBerGUI::ToastNotification* toast_ = nullptr;
    std::string currentId_;
    bool dark_ = false;
};

} // namespace flf
