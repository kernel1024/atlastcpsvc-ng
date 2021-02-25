#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScopedPointer>
#include "qtservice.h"
#include "service.h"

namespace Ui {
class MainWindow;
}

class CMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CMainWindow(QWidget *parent = 0);
    ~CMainWindow() override;

    void updateService(CService *service);
    void updateWidgets();

private:
    Ui::MainWindow *ui { nullptr };
    CService* m_service { nullptr };
    void updateSSLWidgets();

public slots:
    void addToken();
    void delToken();
    void loadKey();
    void loadCert();
    void clearKey();
    void clearCert();
    void updatePort(int port);
    void updateHost();
    void changeEnvironment(const QString &env);
    void saveSettings();
    void installSerivce();
    void uninstallSerivce();
};

#endif // MAINWINDOW_H
