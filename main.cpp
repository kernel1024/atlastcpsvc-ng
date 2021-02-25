#include <QApplication>
#include <windows.h>
#include "mainwindow.h"
#include "service.h"
#include "qsl.h"

#if _WIN32 || _WIN64
    #ifdef _WIN64
        #pragma error("Incompatible mode with 32bit ATLAS engine. Aborting.")
    #endif
#else
    #pragma error("Compile this source with MSVC 32bit compiler. Aborting.")
#endif

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QSL("kernel1024"));
    QCoreApplication::setApplicationName(QSL("atlastcpsvc-ng"));

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
