#pragma once

#include "core/workspace.h"

#include <QMainWindow>

class QTableWidget;
class QPlainTextEdit;
class QLabel;

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
    void log(const QString& message);
    void selectInstance(const std::string& id);

    QTableWidget* table_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QLabel* status_ = nullptr;
};

} // namespace flf
