#include <windows.h>
#include "service.h"

bool CService::isInteractiveSession()
{
    HANDLE hProcessToken = NULL;
    DWORD groupLength = 50;

    PTOKEN_GROUPS groupInfo = (PTOKEN_GROUPS)LocalAlloc(0, groupLength);

    SID_IDENTIFIER_AUTHORITY siaNt = SECURITY_NT_AUTHORITY;
    PSID InteractiveSid = NULL;
    PSID ServiceSid = NULL;
    DWORD i;

    // Start with assumption that process is an EXE, not a Service.
    bool fExe = true;

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

        if (!GetTokenInformation(hProcessToken, TokenGroups, groupInfo,
            groupLength, &groupLength))
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
            fExe = false;
            goto ret;
        }
    }

    fExe = false;

ret:
    if (InteractiveSid)
        FreeSid(InteractiveSid);
    if (ServiceSid)
        FreeSid(ServiceSid);
    if (groupInfo)
        LocalFree(groupInfo);
    if (hProcessToken)
        CloseHandle(hProcessToken);

    return(fExe);
}

CService::CService(int argc, char **argv)
    : QtService<QCoreApplication>(argc, argv, "ATLAS TCP NG Service")
{
    setServiceDescription("ATLAS TCP service with SSL support, implemented with Qt");
    setServiceFlags(QtServiceBase::CanBeSuspended);
}

void CService::start()
{
    QCoreApplication *app = application();

    daemon = new CServer(false, app);

    if (!daemon->isListening()) {
        logMessage(QString("Failed to start ATLAS service"), QtServiceBase::Error);
        app->quit();
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
