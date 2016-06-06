#include <QApplication>
#include <QMessageBox>
#include <QSysInfo>
#include "mainwindow.h"
#include "service.h"

int main(int argc, char *argv[])
{
    QStringList args;
    for (int i=0;i<argc;i++)
        args.append(QString::fromLocal8Bit(argv[i]));

    CService service(argc, argv);
    if (CService::testProcessToken(CService::Process_IsInteractive) &&
            (args.contains("elevated") || args.count()<=1))
    {
        // Request to restart with elevated privileges on Vista+ using UAC
        if ((QSysInfo::windowsVersion()>=QSysInfo::WV_VISTA) &&
                (!CService::testProcessToken(CService::Process_HaveAdminRights))) {
            if (!CService::restartAsAdmin(argc, argv))
                QMessageBox::critical(0,QString("AtlasTCPSvc-NG"),
                                      QString("Failed to start with elevated privileges. Start aborted."));
            return 0;
        }

        QApplication a(argc, argv);
        MainWindow w;

        QtServiceController ctl(service.serviceName());
        w.updateServiceController(&ctl);

        w.show();
        return a.exec();
    } else
        return service.exec();
}
