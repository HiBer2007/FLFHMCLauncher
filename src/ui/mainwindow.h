#pragma once

#include "core/workspace.h"

#include <QMainWindow>
#include <QHash>
#include <QList>
#include <QSet>

class QLabel;
class QStackedWidget;
class QVBoxLayout;
class QWidget;
class QPushButton;
class QCheckBox;
class QListWidget;
class QTreeWidget;
class QToolButton;
class QPropertyAnimation;
class QComboBox;
class QLineEdit;
class QProgressBar;
class QListWidget;
class QTreeWidget;
class QToolButton;
class QPropertyAnimation;

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
    QWidget* buildCreatePage();
    QWidget* buildInstallPage();
    QWidget* buildInstanceCard(const Instance& instance);
    void showPage(int index);
    void openManage(const std::string& id);
    void refreshManage();
    void refreshMods();
    void beginInstall();
    void fillVersionCombo(QComboBox* channel, QComboBox* version,
                          const QStringList& stable, const QStringList& preview);
    void startAgentLoad();
    void startHmclLoad();
    void startFabricLoad();
    void stopAgentLoad();
    void stopHmclLoad();
    void stopFabricLoad();
    void updateInstallStep(const QString& step, int percent);
    void finishInstall(const Instance& instance, bool ok, const QString& error);
    void applyTheme();
    void showToast(const QString& title, const QString& detail, bool error = false);
    void runInstall(const QString& name, bool isolated = false, const QString& agentVersion = {});
    void setCurrent(const std::string& id);
    void fadeIn(QWidget* widget);

    QStackedWidget* pages_ = nullptr;
    QVBoxLayout* cardsLayout_ = nullptr;
    QWidget* launchPage_ = nullptr;
    QWidget* settingsPage_ = nullptr;
    QWidget* helpPage_ = nullptr;
    QWidget* managePage_ = nullptr;
    QWidget* modsPage_ = nullptr;
    QWidget* createPage_ = nullptr;
    QWidget* installPage_ = nullptr;
    QLineEdit* createName_ = nullptr;
    QComboBox* createAgent_ = nullptr;
    QComboBox* createHmcl_ = nullptr;
    QComboBox* createHmclChannel_ = nullptr;
    QComboBox* createAgentChannel_ = nullptr;
    QStringList hmclStable_, hmclPreview_, agentStable_, agentPreview_;
    QComboBox* createFabric_ = nullptr;
    QCheckBox* createIsolated_ = nullptr;
    QLabel* createError_ = nullptr;
    QProgressBar* agentBusy_ = nullptr;
    QProgressBar* hmclBusy_ = nullptr;
    QProgressBar* fabricBusy_ = nullptr;
    QPushButton* agentRefresh_ = nullptr;
    QPushButton* agentStop_ = nullptr;
    QPushButton* hmclRefresh_ = nullptr;
    QPushButton* hmclStop_ = nullptr;
    QPushButton* fabricRefresh_ = nullptr;
    QPushButton* fabricStop_ = nullptr;
    int agentToken_ = 0;
    int hmclToken_ = 0;
    int fabricToken_ = 0;
    QLabel* installTitle_ = nullptr;
    QLabel* installPercent_ = nullptr;
    QProgressBar* installBar_ = nullptr;
    QVBoxLayout* installStepsLayout_ = nullptr;
    QList<QWidget*> installStepWidgets_;
    QList<QLabel*> installStepLabels_;
    QList<QProgressBar*> installStepBars_;
    QList<QVBoxLayout*> installStepDetails_;
    QList<QLabel*> installStepPercents_;
    QList<QToolButton*> installStepToggles_;
    QList<QWidget*> installStepBodies_;
    QSet<int> installPinned_;
    QHash<QString, QProgressBar*> installDetailBars_;
    QHash<QString, QLabel*> installDetailPercents_;
    QProgressBar* installRightBar_ = nullptr;
    QPushButton* installRetry_ = nullptr;
    QPropertyAnimation* installAnim_ = nullptr;
    bool installFailed_ = false;
    int lastInstallPercent_ = 0;
    QListWidget* instanceList_ = nullptr;
    QListWidget* modsList_ = nullptr;
    QLabel* modsTitle_ = nullptr;
    QLabel* status_ = nullptr;
    QCheckBox* darkToggle_ = nullptr;
    QComboBox* behaviorGlobal_ = nullptr;
    QComboBox* behaviorInstance_ = nullptr;
    HiBerGUI::ToastNotification* toast_ = nullptr;
    QString runLog_;
    QSet<QString> launching_;
    QSet<QString> started_;
    std::string currentId_;
    bool dark_ = false;
};

} // namespace flf
