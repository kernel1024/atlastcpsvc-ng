#ifndef SERVICE_H
#define SERVICE_H

#include <QCoreApplication>
#include <QPointer>
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
    void initializeServer(QCoreApplication *app);
    QPointer<CServer> daemon() const;

protected:
    void start() override;
    void pause() override;
    void resume() override;

private:
    Q_DISABLE_COPY(CService)
    Q_DISABLE_MOVE(CService)

    QPointer<CServer> m_daemon;
    static void logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
};

#endif // SERVICE_H
