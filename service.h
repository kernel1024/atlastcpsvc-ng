#ifndef SERVICE_H
#define SERVICE_H

#include <QCoreApplication>
#include <QScopedPointer>
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
    ~CService() override;
    static bool testProcessToken(ProcessToken checkToken);
    static int runAs(const QString& app, const QString& arguments,
                     bool waitToFinish = false);
    static QString getCurrentUserName();
protected:
    void start() override;
    void pause() override;
    void resume() override;

private:
    Q_DISABLE_COPY(CService)
    Q_DISABLE_MOVE(CService)

    QScopedPointer<CServer,QScopedPointerDeleteLater> daemon;
};

#endif // SERVICE_H
