#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"
#include "service.h"
#include "qsl.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QSL("kernel1024"));
    QCoreApplication::setApplicationName(QSL("atlastcpsvc-ng"));

    if (sizeof(void*) > 4) {
        QMessageBox::critical(nullptr,QCoreApplication::applicationName(),
                              QSL("Compiled in 64bit mode, incompatible with 32bit ATLAS engine. Aborting."));
        return 1;
    }

    CService service(argc, argv);

    if (CService::testProcessToken(CService::Process_IsInteractive) && (argc <= 1)) {
        QApplication app(argc, argv);
        QGuiApplication::setApplicationDisplayName(QSL("AtlasTCPsvc-NG"));
        CMainWindow mainWindow;

        service.initializeServer(&app);
        mainWindow.updateService(&service);

        mainWindow.show();
        return app.exec();
    }

    return service.exec();
}
