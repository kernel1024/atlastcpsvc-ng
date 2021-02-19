#include <QFileInfo>
#include <windows.h>
#include <lmcons.h>
#include <array>
#include <string>
#include "service.h"
#include "qsl.h"

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
    // Setup the required structure
    SHELLEXECUTEINFO ShExecInfo;
    memset(&ShExecInfo, 0, sizeof(SHELLEXECUTEINFO));
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.lpVerb = L"runas";
    if (appFullPath.length() > 0)
        ShExecInfo.lpFile = reinterpret_cast<const WCHAR *>(appFullPath.utf16());
    if (arguments.length() > 0)
        ShExecInfo.lpParameters = reinterpret_cast<const WCHAR *>(arguments.utf16());
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
    acUserName.resize(UNLEN + 1);
    DWORD nUserName = acUserName.length();
    if (GetUserNameW(acUserName.data(), &nUserName))
        return QString::fromWCharArray(acUserName.data());
    return QString();
}

bool CService::testProcessToken(ProcessToken checkToken)
{
    HANDLE hProcessToken = NULL;
    DWORD groupLength = 50;

    PTOKEN_GROUPS groupInfo = (PTOKEN_GROUPS)LocalAlloc(0, groupLength);

    SID_IDENTIFIER_AUTHORITY siaNt = SECURITY_NT_AUTHORITY;
    PSID InteractiveSid = NULL;
    PSID ServiceSid = NULL;
    DWORD i;

    // Start with assumption that process is an EXE, not a Service.
    bool fret = true;

    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_QUERY,
                          &hProcessToken))
        goto ret;

    if (groupInfo == NULL)
        goto ret;

    if (!GetTokenInformation(hProcessToken, TokenGroups, groupInfo,
        groupLength, &groupLength))
    {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            goto ret;

        LocalFree(groupInfo);
        groupInfo = NULL;
        groupInfo = (PTOKEN_GROUPS)LocalAlloc(0, groupLength);

        if (groupInfo == NULL)
            goto ret;

        if (checkToken==Process_IsInteractive) {
            if (!GetTokenInformation(hProcessToken, TokenGroups, groupInfo,
                                     groupLength, &groupLength))
                goto ret;
        } else if (checkToken==Process_HaveAdminRights) {
            fret = false;
            TOKEN_ELEVATION elevation;
            DWORD cbSize = sizeof(TOKEN_ELEVATION);
            if (GetTokenInformation(hProcessToken, TokenElevation, &elevation,
                                    sizeof(elevation), &cbSize))
                fret = elevation.TokenIsElevated!=0;
            goto ret;
        } else
            goto ret;
    }


    if (!AllocateAndInitializeSid(&siaNt, 1, SECURITY_INTERACTIVE_RID, 0, 0,
        0, 0, 0, 0, 0, &InteractiveSid))
        goto ret;

    if (!AllocateAndInitializeSid(&siaNt, 1, SECURITY_SERVICE_RID, 0, 0, 0,
        0, 0, 0, 0, &ServiceSid))
        goto ret;

    for (i = 0; i < groupInfo->GroupCount ; i += 1)
    {
        SID_AND_ATTRIBUTES sanda = groupInfo->Groups[i];
        PSID Sid = sanda.Sid;

        if (EqualSid(Sid, InteractiveSid))
            goto ret;
        else if (EqualSid(Sid, ServiceSid)) {
            fret = false;
            goto ret;
        }
    }

    fret = false;

ret:
    if (InteractiveSid)
        FreeSid(InteractiveSid);
    if (ServiceSid)
        FreeSid(ServiceSid);
    if (groupInfo)
        LocalFree(groupInfo);
    if (hProcessToken)
        CloseHandle(hProcessToken);

    return(fret);
}

CService::CService(int argc, char **argv)
    : QtService<QCoreApplication>(argc, argv, QSL("ATLAS TCP NG Service"))
{
    setServiceDescription(QSL("ATLAS translation engine TCP service with SSL support, implemented with Qt."));
    setServiceFlags(QtServiceBase::CanBeSuspended);
    setStartupType(QtServiceController::AutoStartup);
}

CService::~CService() = default;

void CService::start()
{
    QCoreApplication *app = application();

    daemon.reset(new CServer(app));
    daemon->start();

    if (!daemon->isListening()) {
        qCritical() << "Failed to start ATLAS service";
        QCoreApplication::quit();
    }
}

void CService::pause()
{
    daemon->pause();
}

void CService::resume()
{
    daemon->resume();
}
