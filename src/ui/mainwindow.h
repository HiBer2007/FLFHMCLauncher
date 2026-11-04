#pragma once

#include "core/workspace.h"

#include <QMainWindow>

class QLabel;
class QStackedWidget;
class QVBoxLayout;
class QWidget;
class QPushButton;

namespace HiBerGUI {
class ToastNotification;
}

namespace flf {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

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
    QWidget* buildWelcomePage();
    QWidget* buildHomePage();
    QWidget* buildInstanceCard(const Instance& instance);
    void showPage(QWidget* page);
    void showToast(const QString& title, const QString& detail, bool error = false);
    void runInstall(const QString& name);
    void setCurrent(const std::string& id);
    void fadeIn(QWidget* widget);

    QStackedWidget* pages_ = nullptr;
    QWidget* welcomePage_ = nullptr;
    QWidget* homePage_ = nullptr;
    QVBoxLayout* cardsLayout_ = nullptr;
    QLabel* homeTitle_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* launchButton_ = nullptr;
    HiBerGUI::ToastNotification* toast_ = nullptr;
    std::string currentId_;
};

} // namespace flf
