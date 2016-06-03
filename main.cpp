#include <QApplication>
#include <QSettings>
#include "mainwindow.h"
#include "service.h"

int main(int argc, char *argv[])
{
    if (CService::isInteractiveSession()) {
        QApplication a(argc, argv);
        MainWindow w;
        w.show();
        return a.exec();
    } else {
        CService service(argc, argv);
        return service.exec();
    }
}
