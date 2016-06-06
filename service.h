#ifndef SERVICE_H
#define SERVICE_H

#include <QCoreApplication>
#include "qtservice.h"
#include "server.h"

class CService : public QtService<QCoreApplication>
{
public:
    enum ProcessToken {
        Process_IsInteractive,
        Process_HaveAdminRights
    };

    CService(int argc, char **argv);
    static bool testProcessToken(ProcessToken checkToken);
    static bool restartAsAdmin(int argc, char *argv[]);
protected:
    void start();
    void pause();
    void resume();

private:
    CServer *daemon;
};

#endif // SERVICE_H
