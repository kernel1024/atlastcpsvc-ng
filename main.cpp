#include <QApplication>
#include <windows.h>
#include "mainwindow.h"
#include "service.h"
#include "qsl.h"

// TODO: cleanup unsupported features in QtService

int main(int argc, char *argv[])
{
    bool is64bit = (sizeof(void*) > 4);

    QCoreApplication::setOrganizationName(QSL("kernel1024"));
    QCoreApplication::setApplicationName(QSL("atlastcpsvc-ng"));

    CService service(argc, argv);

    if (CService::testProcessToken(CService::Process_IsInteractive) && (argc <= 1)) {

        if (is64bit) {
            MessageBoxW(nullptr, L"Compiled in 64bit mode, incompatible with 32bit ATLAS engine. Aborting.",
                        L"AtlasTCPsvc-NG", MB_ICONEXCLAMATION | MB_OK);
            return 1;
        }

        QApplication app(argc, argv);
        QGuiApplication::setApplicationDisplayName(QSL("AtlasTCPsvc-NG"));
        CMainWindow mainWindow;

        service.initializeServer(&app);
        mainWindow.updateService(&service);

        mainWindow.show();
        return app.exec();
    }

    if (is64bit) return 1;

    return service.exec();
}
