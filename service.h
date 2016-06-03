#ifndef SERVICE_H
#define SERVICE_H

#include <QCoreApplication>
#include "qtservice.h"
#include "server.h"

class CService : public QtService<QCoreApplication>
{
public:
    CService(int argc, char **argv);
    static bool isInteractiveSession();

protected:
    void start();
    void pause();
    void resume();

private:
    CServer *daemon;
};

#endif // SERVICE_H
