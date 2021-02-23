#include <QFileInfo>
#include <QScopeGuard>
#include <windows.h>
#include <lmcons.h>
#include <string>
#include <array>

#include "service.h"
#include "qsl.h"

void CService::logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QtServiceBase::MessageType svcType = QtServiceBase::MessageType::Success;
    const QString logMsg = qFormatLogMessage(type,context,msg);
    switch (type) {
        case QtInfoMsg:
        case QtDebugMsg:
            svcType = QtServiceBase::MessageType::Information;
            break;
        case QtCriticalMsg:
        case QtFatalMsg:
            svcType = QtServiceBase::MessageType::Error;
            break;
        case QtWarningMsg:
            svcType = QtServiceBase::MessageType::Warning;
            break;
    }

    QtServiceBase *service = QtServiceBase::instance();
    if (service)
        service->logMessage(logMsg,svcType,1000);

    fprintf(stderr, "%s", logMsg.toLocal8Bit().constData()); // NOLINT
}

int CService::runAs(const QString& app, const QString& arguments, bool waitToFinish)
{
    QString appFullPath = app;
    if (appFullPath.isEmpty())
        appFullPath = QCoreApplication::applicationFilePath();

    QFileInfo fi(appFullPath);
    if (!fi.exists()) {
        qCritical() << "Executable module not found. Incorrect parameters.";
        return -1;
    }

    std::wstring wAppFullPath = appFullPath.toStdWString();
    std::wstring wArguments = arguments.toStdWString();

    // Setup the required structure
    SHELLEXECUTEINFO ShExecInfo;
    memset(&ShExecInfo, 0, sizeof(SHELLEXECUTEINFO));
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.lpVerb = L"runas";
    if (!wAppFullPath.empty())
        ShExecInfo.lpFile = &wAppFullPath[0];
    if (!wArguments.empty())
        ShExecInfo.lpParameters = &wArguments[0];
    ShExecInfo.nShow = SW_SHOW;

    // Spawn the process
    if (ShellExecuteEx(&ShExecInfo) == FALSE)
        return -2; // Failed to execute process

    if (waitToFinish) {
        WaitForSingleObject(ShExecInfo.hProcess,INFINITE);
        DWORD ec = 0;
        GetExitCodeProcess(ShExecInfo.hProcess,&ec);
        return ec;
    }

    return 0;
}

QString CService::getCurrentUserName()
{
    std::wstring acUserName;
    acUserName.resize(UNLEN + 1,L'\0');
    DWORD nUserName = acUserName.length();
    if (GetUserNameW(&acUserName[0], &nUserName) != FALSE) {
        wsZeroRightTrim(acUserName);
        return QString::fromStdWString(acUserName);
    }

    return QString();
}

bool CService::testProcessToken(ProcessToken checkToken)
{
    HANDLE hProcessToken = nullptr;
    PSID InteractiveSid = nullptr;

    auto cleanup = qScopeGuard([&InteractiveSid, &hProcessToken]{
        if (InteractiveSid)
            FreeSid(InteractiveSid);
        if (hProcessToken)
            CloseHandle(hProcessToken);
    });

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hProcessToken) == FALSE)
        return false;

    if (checkToken == Process_HaveAdminRights) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hProcessToken, TokenElevation, &elevation, cbSize, &cbSize) != FALSE)
            return (elevation.TokenIsElevated != 0);
        return false;
    }

    if (checkToken==Process_IsInteractive) {
        std::vector<BYTE> bufGroupInfo;
        DWORD groupLength = bufGroupInfo.size();

        if (GetTokenInformation(hProcessToken, TokenGroups, nullptr, 0, &groupLength) == FALSE) {
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                return false;

            bufGroupInfo.resize(groupLength);
            if (GetTokenInformation(hProcessToken, TokenGroups, &bufGroupInfo[0], groupLength, &groupLength) == FALSE)
                return false;

            SID_IDENTIFIER_AUTHORITY siaNt = SECURITY_NT_AUTHORITY;
            if (AllocateAndInitializeSid(&siaNt, 1, SECURITY_INTERACTIVE_RID, 0, 0,
                0, 0, 0, 0, 0, &InteractiveSid) == FALSE)
                return false;

            auto* const groupInfo = reinterpret_cast<PTOKEN_GROUPS>(&bufGroupInfo[0]);
            for (DWORD i = 0; i < groupInfo->GroupCount ; i++) {
                if (EqualSid(groupInfo->Groups[i].Sid, InteractiveSid) != FALSE) // NOLINT
                    return true;
            }
        }
    }

    return false;
}

CService::CService(int argc, char **argv)
    : QtService<QCoreApplication>(argc, argv, QSL("ATLAS TCP NG Service"))
{
    qSetMessagePattern(QSL("%{if-debug}Debug%{endif}"
                           "%{if-info}Info%{endif}"
                           "%{if-warning}Warning%{endif}"
                           "%{if-critical}Error%{endif}"
                           "%{if-fatal}Fatal%{endif}"
                           ": %{message} (%{file}:%{line})"));
    qInstallMessageHandler(CService::logMessageHandler);

    setServiceDescription(QSL("ATLAS translation engine TCP service with SSL support, implemented with Qt."));
    setServiceFlags(QtServiceBase::CanBeSuspended);
    setStartupType(QtServiceController::AutoStartup);
}

CService::~CService() = default;

void CService::initializeServer(QCoreApplication* app)
{
    if (m_daemon) return;
    if (app == nullptr) return;

    m_daemon = new CServer(app);
}

void CService::start()
{
    if (m_daemon.isNull()) {
        initializeServer(application());
        if (m_daemon.isNull()) {
            qCritical() << "Failed to initialize ATLAS service";
            QCoreApplication::quit();
        }
    }

    if (!m_daemon->start() || !m_daemon->isListening()) {
        qCritical() << "Failed to start ATLAS service";
        QCoreApplication::quit();
    }
}

void CService::pause()
{
    m_daemon->pause();
}

void CService::resume()
{
    m_daemon->resume();
}

QPointer<CServer> CService::daemon() const
{
    return m_daemon;
}
