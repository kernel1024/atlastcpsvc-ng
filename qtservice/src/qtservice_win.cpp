/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Solutions component.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtservice.h"
#include "qtservice_p.h"
#include "qsl.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QLibrary>
#include <QMutex>
#include <QSemaphore>
#include <QProcess>
#include <QSettings>
#include <QTextStream>
#include <qt_windows.h>
#include <QWaitCondition>
#include <QAbstractEventDispatcher>
#include <QVector>
#include <QThread>
#include <QSettings>
#include <QAbstractNativeEventFilter>
#include <cstdio>
#include <array>

namespace CDefaults {
const int logEventIdMargin = 1000;
const int handlerThreadStopTimeoutMS = 1000;
const auto eventLogCategory = "Application";
const auto eventMessageFile = "C:\\Windows\\System32\\mscoree.dll";
}

bool QtServiceController::isInstalled() const
{
    Q_D(const QtServiceController);
    bool result = false;

    // Open the Service Control Manager
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, 0);
    if (hSCM) {
        // Try to open the service
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_QUERY_CONFIG);

        if (hService) {
            result = true;
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return result;
}

bool QtServiceController::isRunning() const
{
    Q_D(const QtServiceController);
    bool result = false;

    // Open the Service Control Manager
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, 0);
    if (hSCM) {
        // Try to open the service
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_QUERY_STATUS);
        if (hService) {
            SERVICE_STATUS info;
            int res = QueryServiceStatus(hService, &info);
            if (res)
                result = info.dwCurrentState != SERVICE_STOPPED;
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return result;
}


QString QtServiceController::serviceFilePath() const
{
    Q_D(const QtServiceController);
    QString result;

    // Open the Service Control Manager
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, 0);
    if (hSCM) {
        // Try to open the service
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_QUERY_CONFIG);
        if (hService) {
            DWORD sizeNeeded = 0;
            char data[8 * 1024];
            if (QueryServiceConfigW(hService, (LPQUERY_SERVICE_CONFIG)data, 8 * 1024, &sizeNeeded)) {
                LPQUERY_SERVICE_CONFIG config = (LPQUERY_SERVICE_CONFIG)data;
                result = QString::fromUtf16((const ushort*)config->lpBinaryPathName);
            }
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return result;
}

QString QtServiceController::serviceDescription() const
{
    Q_D(const QtServiceController);
    QString result;

    // Open the Service Control Manager
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, 0);
    if (hSCM) {
        // Try to open the service
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_QUERY_CONFIG);
        if (hService) {
            DWORD dwBytesNeeded;
            char data[8 * 1024];
            if (QueryServiceConfig2W(
                        hService,
                        SERVICE_CONFIG_DESCRIPTION,
                        (unsigned char *)data,
                        8096,
                        &dwBytesNeeded)) {
                LPSERVICE_DESCRIPTION desc = (LPSERVICE_DESCRIPTION)data;
                if (desc->lpDescription)
                    result = QString::fromUtf16((const ushort*)desc->lpDescription);
            }
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return result;
}

QtServiceController::StartupType QtServiceController::startupType() const
{
    Q_D(const QtServiceController);
    StartupType result = ManualStartup;

    // Open the Service Control Manager
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, 0);
    if (hSCM) {
        // Try to open the service
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_QUERY_CONFIG);
        if (hService) {
            DWORD sizeNeeded = 0;
            char data[8 * 1024];
            if (QueryServiceConfigW(hService, (QUERY_SERVICE_CONFIG *)data, 8 * 1024, &sizeNeeded)) {
                QUERY_SERVICE_CONFIG *config = (QUERY_SERVICE_CONFIG *)data;
                result = config->dwStartType == SERVICE_DEMAND_START ? ManualStartup : AutoStartup;
            }
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return result;
}

bool QtServiceController::uninstall()
{
    Q_D(QtServiceController);
    bool result = false;

    // Open the Service Control Manager
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (hSCM) {
        // Try to open the service
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), DELETE);
        if (hService) {
            if (DeleteService(hService))
                result = true;
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }

    if (result) {
        QSettings registry(QSL("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\EventLog\\%1")
                           .arg(QString::fromUtf8(CDefaults::eventLogCategory)),QSettings::NativeFormat);
        registry.beginGroup(d->serviceName);
        registry.remove(QString());
        registry.endGroup();
    }

    return result;
}

bool QtServiceController::start(const QStringList &args)
{
    Q_D(QtServiceController);
    bool result = false;

    // Open the Service Control Manager
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCM) {
        // Try to open the service
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_START);
        if (hService) {
            QVector<const wchar_t *> argv(args.size());
            for (int i = 0; i < args.size(); ++i)
                argv[i] = (const wchar_t*)args.at(i).utf16();

            if (StartServiceW(hService, args.size(), argv.data()))
                result = true;
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return result;
}

bool QtServiceController::stop()
{
    Q_D(QtServiceController);
    bool result = false;

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCM) {
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_STOP|SERVICE_QUERY_STATUS);
        if (hService) {
            SERVICE_STATUS status;
            if (ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
                bool stopped = status.dwCurrentState == SERVICE_STOPPED;
                int i = 0;
                while(!stopped && i < 10) {
                    Sleep(200);
                    if (!QueryServiceStatus(hService, &status))
                        break;
                    stopped = status.dwCurrentState == SERVICE_STOPPED;
                    ++i;
                }
                result = stopped;
            } else {
                qErrnoWarning(GetLastError(), "stopping");
            }
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return result;
}

bool QtServiceController::pause()
{
    Q_D(QtServiceController);
    bool result = false;

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCM) {
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_PAUSE_CONTINUE);
        if (hService) {
            SERVICE_STATUS status;
            if (ControlService(hService, SERVICE_CONTROL_PAUSE, &status))
                result = true;
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return result;
}

bool QtServiceController::resume()
{
    Q_D(QtServiceController);
    bool result = false;

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCM) {
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_PAUSE_CONTINUE);
        if (hService) {
            SERVICE_STATUS status;
            if (ControlService(hService, SERVICE_CONTROL_CONTINUE, &status))
                result = true;
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return result;
}

bool QtServiceController::sendCommand(int code)
{
    Q_D(QtServiceController);
    bool result = false;

    if ((code < 0) || (code > 127) || !isRunning())
        return result;

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (hSCM) {
        const std::wstring wServiceName = d->serviceName.toStdWString();
        SC_HANDLE hService = OpenServiceW(hSCM, wServiceName.c_str(), SERVICE_USER_DEFINED_CONTROL);
        if (hService) {
            SERVICE_STATUS status;
            if (ControlService(hService, 128 + code, &status))
                result = true;
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }
    return result;
}

void QtServiceBase::logMessage(const QString &message, MessageType type,
                               int id, uint category, const QByteArray &data)
{
    Q_D(QtServiceBase);

    WORD wType;
    switch (type) {
        case Error: wType = EVENTLOG_ERROR_TYPE; break;
        case Warning: wType = EVENTLOG_WARNING_TYPE; break;
        case Information: wType = EVENTLOG_INFORMATION_TYPE; break;
        default: wType = EVENTLOG_SUCCESS; break;
    }
    // TODO: remove all utf16() for wstrings
    HANDLE h = RegisterEventSourceW(0, reinterpret_cast<const wchar_t*>(d->controller.serviceName().utf16()));
    if (h) {
        const wchar_t *msg = (wchar_t*)message.utf16();
        const char *bindata = data.size() ? data.constData() : 0;
        ReportEventW(h, wType, category, CDefaults::logEventIdMargin + id, 0, 1, data.size(),(const wchar_t **)&msg,
                     const_cast<char *>(bindata));
        DeregisterEventSource(h);
    }
}

class QtServiceControllerHandler : public QObject
{
    Q_OBJECT
public:
    QtServiceControllerHandler(QtServiceSysPrivate *sys);

protected:
    void customEvent(QEvent *e);

private:
    QtServiceSysPrivate *d_sys;
};

class QtServiceSysPrivate
{
public:
    enum {
        QTSERVICE_STARTUP = 256
    };
    QtServiceSysPrivate();

    void setStatus( DWORD dwState );
    void setServiceFlags(QtServiceBase::ServiceFlags flags);
    DWORD serviceFlags(QtServiceBase::ServiceFlags flags) const;
    inline bool available() const;
    static void WINAPI serviceMain( DWORD dwArgc, wchar_t** lpszArgv );
    static void WINAPI handler( DWORD dwOpcode );

    SERVICE_STATUS status;
    SERVICE_STATUS_HANDLE serviceStatus;
    QStringList serviceArgs;

    static QtServiceSysPrivate *instance;

    QWaitCondition condition;
    QMutex mutex;
    QSemaphore startSemaphore;
    QSemaphore startSemaphore2;

    QtServiceControllerHandler *controllerHandler;

    void handleCustomEvent(QEvent *e);
};

QtServiceControllerHandler::QtServiceControllerHandler(QtServiceSysPrivate *sys)
    : QObject(), d_sys(sys)
{

}

void QtServiceControllerHandler::customEvent(QEvent *e)
{
    d_sys->handleCustomEvent(e);
}


QtServiceSysPrivate *QtServiceSysPrivate::instance = 0;

QtServiceSysPrivate::QtServiceSysPrivate()
{
    instance = this;
}

inline bool QtServiceSysPrivate::available() const
{
    return true; // TODO: remove this
}

void WINAPI QtServiceSysPrivate::serviceMain(DWORD dwArgc, wchar_t** lpszArgv)
{
    if (!instance || !QtServiceBase::instance())
        return;

    // Windows spins off a random thread to call this function on
    // startup, so here we just signal to the QApplication event loop
    // in the main thread to go ahead with start()'ing the service.

    for (DWORD i = 0; i < dwArgc; i++)
        instance->serviceArgs.append(QString::fromUtf16((unsigned short*)lpszArgv[i]));

    instance->startSemaphore.release(); // let the qapp creation start
    instance->startSemaphore2.acquire(); // wait until its done
    // Register the control request handler
    instance->serviceStatus = RegisterServiceCtrlHandlerW((TCHAR*)QtServiceBase::instance()->serviceName().utf16(), handler);

    if (!instance->serviceStatus) // cannot happen - something is utterly wrong
        return;

    handler(QTSERVICE_STARTUP); // Signal startup to the application -
    // causes QtServiceBase::start() to be called in the main thread

    // The MSDN doc says that this thread should just exit - the service is
    // running in the main thread (here, via callbacks in the handler thread).
}


// The handler() is called from the thread that called
// StartServiceCtrlDispatcher, i.e. our HandlerThread, and
// not from the main thread that runs the event loop, so we
// have to post an event to ourselves, and use a QWaitCondition
// and a QMutex to synchronize.
void QtServiceSysPrivate::handleCustomEvent(QEvent *e)
{
    int code = e->type() - QEvent::User;

    switch(code) {
        case QTSERVICE_STARTUP: // Startup
            QtServiceBase::instance()->start();
            break;
        case SERVICE_CONTROL_STOP:
            QtServiceBase::instance()->stop();
            QCoreApplication::instance()->quit();
            break;
        case SERVICE_CONTROL_PAUSE:
            QtServiceBase::instance()->pause();
            break;
        case SERVICE_CONTROL_CONTINUE:
            QtServiceBase::instance()->resume();
            break;
        default:
            if (code >= 128 && code <= 255)
                QtServiceBase::instance()->processCommand(code - 128);
            break;
    }

    mutex.lock();
    condition.wakeAll();
    mutex.unlock();
}

void WINAPI QtServiceSysPrivate::handler( DWORD code )
{
    if (!instance)
        return;

    instance->mutex.lock();
    switch (code) {
        case QTSERVICE_STARTUP: // QtService startup (called from WinMain when started)
            instance->setStatus(SERVICE_START_PENDING);
            QCoreApplication::postEvent(instance->controllerHandler, new QEvent(QEvent::Type(QEvent::User + code)));
            instance->condition.wait(&instance->mutex);
            instance->setStatus(SERVICE_RUNNING);
            break;
        case SERVICE_CONTROL_STOP: // 1
            instance->setStatus(SERVICE_STOP_PENDING);
            QCoreApplication::postEvent(instance->controllerHandler, new QEvent(QEvent::Type(QEvent::User + code)));
            instance->condition.wait(&instance->mutex);
            // status will be reported as stopped by start() when qapp::exec returns
            break;

        case SERVICE_CONTROL_PAUSE: // 2
            instance->setStatus(SERVICE_PAUSE_PENDING);
            QCoreApplication::postEvent(instance->controllerHandler, new QEvent(QEvent::Type(QEvent::User + code)));
            instance->condition.wait(&instance->mutex);
            instance->setStatus(SERVICE_PAUSED);
            break;

        case SERVICE_CONTROL_CONTINUE: // 3
            instance->setStatus(SERVICE_CONTINUE_PENDING);
            QCoreApplication::postEvent(instance->controllerHandler, new QEvent(QEvent::Type(QEvent::User + code)));
            instance->condition.wait(&instance->mutex);
            instance->setStatus(SERVICE_RUNNING);
            break;

        case SERVICE_CONTROL_INTERROGATE: // 4
            break;

        case SERVICE_CONTROL_SHUTDOWN: // 5
            // Don't waste time with reporting stop pending, just do it
            QCoreApplication::postEvent(instance->controllerHandler, new QEvent(QEvent::Type(QEvent::User + SERVICE_CONTROL_STOP)));
            instance->condition.wait(&instance->mutex);
            // status will be reported as stopped by start() when qapp::exec returns
            break;

        default:
            if ( code >= 128 && code <= 255 ) {
                QCoreApplication::postEvent(instance->controllerHandler, new QEvent(QEvent::Type(QEvent::User + code)));
                instance->condition.wait(&instance->mutex);
            }
            break;
    }

    instance->mutex.unlock();

    // Report current status
    if (instance->available() && instance->status.dwCurrentState != SERVICE_STOPPED)
        SetServiceStatus(instance->serviceStatus, &instance->status);
}

void QtServiceSysPrivate::setStatus(DWORD state)
{
    if (!available())
        return;
    status.dwCurrentState = state;
    SetServiceStatus(serviceStatus, &status);
}

void QtServiceSysPrivate::setServiceFlags(QtServiceBase::ServiceFlags flags)
{
    if (!available())
        return;
    status.dwControlsAccepted = serviceFlags(flags);
    SetServiceStatus(serviceStatus, &status);
}

DWORD QtServiceSysPrivate::serviceFlags(QtServiceBase::ServiceFlags flags) const
{
    DWORD control = 0;
    if (flags & QtServiceBase::CanBeSuspended)
        control |= SERVICE_ACCEPT_PAUSE_CONTINUE;
    if (!(flags & QtServiceBase::CannotBeStopped))
        control |= SERVICE_ACCEPT_STOP;
    if (flags & QtServiceBase::NeedsStopOnShutdown)
        control |= SERVICE_ACCEPT_SHUTDOWN;

    return control;
}

#include "qtservice_win.moc"


class HandlerThread : public QThread
{
private:
    bool success;
    bool console;

public:
    explicit HandlerThread(QObject *parent = nullptr)
        : QThread(parent),
          success(true),
          console(false)
    {}

    bool calledOk() const { return success; }
    bool runningAsConsole() const { return console; }

protected:
    void run() override
    {
        std::wstring svcName = QtServiceBase::instance()->serviceName().toStdWString();
        std::array<SERVICE_TABLE_ENTRYW,2> st {};
        st[0].lpServiceName = &svcName[0];
        st[0].lpServiceProc = QtServiceSysPrivate::serviceMain;
        st[1].lpServiceName = nullptr;
        st[1].lpServiceProc = nullptr;

        success = (StartServiceCtrlDispatcherW(&st[0]) != 0); // should block

        if (!success) {
            if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
                // Means we're started from console, not from service mgr
                // start() will ask the mgr to start another instance of us as a service instead
                console = true;
            }
            else {
                QtServiceBase::instance()->logMessage(QString("The Service failed to start [%1]").arg(qt_error_string(GetLastError())), QtServiceBase::Error);
            }
            QtServiceSysPrivate::instance->startSemaphore.release();  // let start() continue, since serviceMain won't be doing it
        }
    }
};

/*
  Ignore WM_ENDSESSION system events, since they make the Qt kernel quit
*/

class QtServiceAppEventFilter : public QAbstractNativeEventFilter
{
public:
    QtServiceAppEventFilter() = default;
    ~QtServiceAppEventFilter() = default;
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
};

bool QtServiceAppEventFilter::nativeEventFilter(const QByteArray &, void *message, long *result)
{
    MSG *winMessage = static_cast<MSG*>(message);
    if ((winMessage->message == WM_ENDSESSION) && ((winMessage->lParam & ENDSESSION_LOGOFF) > 0)) {
        *result = TRUE;
        return true;
    }
    return false;
}

Q_GLOBAL_STATIC(QtServiceAppEventFilter, qtServiceAppEventFilter)

/* There are three ways we can be started:

   - By a service controller (e.g. the Services control panel), with
   no (service-specific) arguments. ServiceBase::exec() will then call
   start() below, and the service will start.

   - From the console, but with no (service-specific) arguments. This
   means we should ask a controller to start the service (i.e. another
   instance of this executable), and then just terminate. We discover
   this case (as different from the above) by the fact that
   StartServiceCtrlDispatcher will return an error, instead of blocking.

   - From the console, with -e(xec) argument. ServiceBase::exec() will
   then call ServiceBasePrivate::exec(), which calls
   ServiceBasePrivate::run(), which runs the application as a normal
   program.
*/

bool QtServiceBasePrivate::start()
{
    sysInit();

    // Since StartServiceCtrlDispatcher() blocks waiting for service
    // control events, we need to call it in another thread, so that
    // the main thread can run the QApplication event loop.
    auto* ht = new HandlerThread();
    ht->start();

    QtServiceSysPrivate* sys = QtServiceSysPrivate::instance;

    // Wait until service args have been received by serviceMain.
    // If Windows doesn't call serviceMain (or
    // StartServiceControlDispatcher doesn't return an error) within
    // a timeout of 20 secs, something is very wrong; give up
    if (!sys->startSemaphore.tryAcquire(1, 20000))
        return false;

    if (!ht->calledOk()) {
        if (ht->runningAsConsole())
            return controller.start(args.mid(1));

        return false;
    }

    int argc = sys->serviceArgs.size();
    QVector<char *> argv(argc);
    QList<QByteArray> argvData;
    argvData.reserve(argc);
    for (int i = 0; i < argc; ++i)
        argvData.append(sys->serviceArgs.at(i).toLocal8Bit());
    for (int i = 0; i < argc; ++i)
        argv[i] = argvData[i].data();

    q_ptr->createApplication(argc, argv.data());
    QCoreApplication *app = QCoreApplication::instance();
    if (!app)
        return false;

    QAbstractEventDispatcher::instance()->installNativeEventFilter(qtServiceAppEventFilter());

    sys->controllerHandler = new QtServiceControllerHandler(sys);

    sys->startSemaphore2.release(); // let serviceMain continue (and end)

    sys->status.dwWin32ExitCode = q_ptr->executeApplication();
    sys->setStatus(SERVICE_STOPPED);

    if (ht->isRunning())
        ht->wait(CDefaults::handlerThreadStopTimeoutMS);         // let the handler thread finish
    delete sys->controllerHandler;
    sys->controllerHandler = nullptr;
    if (ht->isFinished())
        delete ht;
    delete app;
    sysCleanup();
    return true;
}

bool QtServiceBasePrivate::install(const QString &account, const QString &password) const
{
    bool result = false;

    // Open the Service Control Manager
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (hSCM) {
        QString acc = account;
        DWORD dwStartType = startupType == QtServiceController::AutoStartup ? SERVICE_AUTO_START : SERVICE_DEMAND_START;
        DWORD dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        std::wstring act;
        std::wstring pwd;
        if (!acc.isEmpty()) {
            // The act string must contain a string of the format "Domain\UserName",
            // so if only a username was specified without a domain, default to the local machine domain.
            if (!acc.contains(QChar('\\'))) {
                acc.prepend(QSL(".\\"));
            }
            if (!acc.endsWith(QSL("\\LocalSystem")))
                act = acc.toStdWString();
        }
        if (!password.isEmpty() && (!act.empty())) {
            pwd = password.toStdWString();
        }

        // Only set INTERACTIVE if act is LocalSystem. (and act should be empty if it is LocalSystem).
        if (act.empty()) dwServiceType |= SERVICE_INTERACTIVE_PROCESS;

        // Create the service
        std::wstring wpath = filePath().toStdWString();
        std::wstring wServiceName = controller.serviceName().toStdWString();
        SC_HANDLE hService = CreateServiceW(hSCM, wServiceName.c_str(), wServiceName.c_str(),
                                            SERVICE_ALL_ACCESS,
                                            dwServiceType, // QObject::inherits ( const char * className ) for no inter active ????
                                            dwStartType, SERVICE_ERROR_NORMAL, wpath.c_str(),
                                            nullptr, nullptr, nullptr,
                                            act.c_str(), pwd.c_str());
        if (hService) {
            result = true;
            if (!serviceDescription.isEmpty()) {
                std::wstring desc = serviceDescription.toStdWString();
                SERVICE_DESCRIPTION sdesc {};
                sdesc.lpDescription = &desc[0];
                ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &sdesc);
            }
            CloseServiceHandle(hService);
        }
        CloseServiceHandle(hSCM);
    }

    // install Event Log category
    const int typesSupported = 7; // Flags: error | warning | info
    if (result) {
        QSettings registry(QSL("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\EventLog\\%1")
                           .arg(QString::fromUtf8(CDefaults::eventLogCategory)),QSettings::NativeFormat);
        registry.beginGroup(controller.serviceName());
        registry.setValue(QSL("EventMessageFile"),CDefaults::eventMessageFile);
        registry.setValue(QSL("TypesSupported"),typesSupported);
        registry.endGroup();
    }

    return result;
}

QString QtServiceBasePrivate::filePath() const
{
    const int max_path_ntfs = 32767;
    std::wstring path;
    path.resize(_MAX_PATH,'\0');
    GetModuleFileNameW(nullptr, &path[0], path.length());
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        path.resize(max_path_ntfs,'\0');
        GetModuleFileNameW(nullptr, &path[0], path.length());
    }
    wsZeroRightTrim(path);
    return QString::fromStdWString(path);
}

bool QtServiceBasePrivate::sysInit()
{
    sysd = new QtServiceSysPrivate();

    sysd->serviceStatus			    = nullptr;
    sysd->status.dwServiceType		    = SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS;
    sysd->status.dwCurrentState		    = SERVICE_STOPPED;
    sysd->status.dwControlsAccepted         = sysd->serviceFlags(serviceFlags);
    sysd->status.dwWin32ExitCode	    = NO_ERROR;
    sysd->status.dwServiceSpecificExitCode  = 0;
    sysd->status.dwCheckPoint		    = 0;
    sysd->status.dwWaitHint		    = 0;

    return true;
}

void QtServiceBasePrivate::sysSetPath()
{

}

void QtServiceBasePrivate::sysCleanup()
{
    if (sysd) {
        delete sysd;
        sysd = nullptr;
    }
}

void QtServiceBase::setServiceFlags(QtServiceBase::ServiceFlags flags)
{
    if (d_ptr->serviceFlags == flags)
        return;
    d_ptr->serviceFlags = flags;
    if (d_ptr->sysd)
        d_ptr->sysd->setServiceFlags(flags);
}


