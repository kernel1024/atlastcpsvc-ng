#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "server.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    CServer *daemon;
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
};

#endif // MAINWINDOW_H
