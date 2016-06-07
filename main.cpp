#include <QApplication>
#include <QMessageBox>
#include <QSysInfo>
#include "mainwindow.h"
#include "service.h"

int main(int argc, char *argv[])
{
    CService service(argc, argv);
    if (CService::testProcessToken(CService::Process_IsInteractive) && argc<=1)
    {
        QApplication a(argc, argv);
        MainWindow w;

        QtServiceController ctl(service.serviceName());
        w.updateServiceController(&ctl);

        w.show();
        return a.exec();
    } else
        return service.exec();
}
