#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "server.h"
#include "qtservice.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void updateServiceController(QtServiceController *svctl=NULL);

private:
    Ui::MainWindow *ui;
    CServer *daemon;
    QtServiceController* m_svctl;
    void initializeUiParams();
    void showSSLParams();

public slots:
    void addToken();
    void delToken();
    void loadKey();
    void loadCert();
    void clearKey();
    void clearCert();
    void updatePort(int port);
    void changeEnvironment(const QString &env);
    void saveSettings();
    void installSerivce();
    void uninstallSerivce();
};

#endif // MAINWINDOW_H
