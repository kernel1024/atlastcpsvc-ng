#include <QApplication>
#include "mainwindow.h"
#include "service.h"
#include "qsl.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QSL("kernel1024"));
    QCoreApplication::setApplicationName(QSL("atlastcpsvc-ng"));

    CService service(argc, argv);
    if (CService::testProcessToken(CService::Process_IsInteractive) && argc<=1)
    {
        QGuiApplication::setApplicationDisplayName(QSL("AtlasTCPsvc-NG"));
        QApplication a(argc, argv);
        MainWindow w;

        QtServiceController ctl(service.serviceName());
        w.updateServiceController(&ctl);

        w.show();
        return a.exec();
    }

    return service.exec();
}
