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
#include <cstdio>
#include <QTimer>
#include <QVector>
#include <QProcess>
#include <QDebug>


QtServiceController::QtServiceController(const QString &name)
 : d_ptr(new QtServiceControllerPrivate())
{
    Q_D(QtServiceController);
    d->q_ptr = this;
    d->serviceName = name;
}

QtServiceController::~QtServiceController()
{
    delete d_ptr;
}

QString QtServiceController::serviceName() const
{
    Q_D(const QtServiceController);
    return d->serviceName;
}

bool QtServiceController::install(const QString &serviceFilePath, const QString &account,
                const QString &password)
{
    QStringList arguments { QSL("-i"), account, password };
    return (QProcess::execute(serviceFilePath, arguments) == 0);
}

bool QtServiceController::start()
{
    return start(QStringList());
}

class QtServiceStarter : public QObject
{
    Q_OBJECT
public:
    QtServiceStarter(QObject *parent, QtServiceBasePrivate *service)
        : QObject(parent), d_ptr(service) {}
private:
    QtServiceBasePrivate *d_ptr;
public Q_SLOTS:
    void slotStart()
    {
        d_ptr->startService();
    }
};
#include "qtservice.moc"

QtServiceBase *QtServiceBasePrivate::instance = nullptr;

QtServiceBasePrivate::QtServiceBasePrivate(QtServiceBase *base, const QString &name)
    : q_ptr(base),
      startupType(QtServiceController::ManualStartup),
      controller(name),
      sysd(nullptr)
{

}

QtServiceBasePrivate::~QtServiceBasePrivate() = default;

void QtServiceBasePrivate::startService()
{
    Q_Q(QtServiceBase);
    q->start();
}

QtServiceBase::QtServiceBase(int argc, char **argv, const QString &name)
{
    Q_ASSERT(!QtServiceBasePrivate::instance);
    QtServiceBasePrivate::instance = this;

    const int maxNameLength = 255;

    QString nm(name);
    if (nm.length() > maxNameLength) {
        qWarning() << "QtService: 'name' is longer than 255 characters.";
        nm.truncate(maxNameLength);
    }
    if (nm.contains('\\')) {
        qWarning() << "QtService: 'name' contains backslashes '\\'.";
        nm.replace(QChar('\\'), QChar('\0'));
    }

    d_ptr.reset(new QtServiceBasePrivate(this,nm));
    for (int i = 0; i < argc; ++i)
        d_ptr->args.append(QString::fromLocal8Bit(argv[i]));
}

QtServiceBase::~QtServiceBase()
{
    QtServiceBasePrivate::instance = nullptr;
}

QString QtServiceBase::serviceName() const
{
    Q_D(const QtServiceBase);
    return d->controller.serviceName();
}

QString QtServiceBase::serviceDescription() const
{
    Q_D(const QtServiceBase);
    return d->serviceDescription;
}

void QtServiceBase::setServiceDescription(const QString &description)
{
    Q_D(QtServiceBase);
    d->serviceDescription = description;
}

QtServiceController::StartupType QtServiceBase::startupType() const
{
    Q_D(const QtServiceBase);
    return d->startupType;
}

bool QtServiceBase::isInstalled() const
{
    Q_D(const QtServiceBase);
    return d->controller.isInstalled();
}

void QtServiceBase::setStartupType(QtServiceController::StartupType type)
{
    Q_D(QtServiceBase);
    d->startupType = type;
}

QtServiceBase::ServiceFlags QtServiceBase::serviceFlags() const
{
    Q_D(const QtServiceBase);
    return d->serviceFlags;
}

int QtServiceBase::exec()
{
    Q_D(QtServiceBase);

    if (d->args.size() > 1) {
        QString a =  d->args.at(1);
        if (a == QSL("-i") || a == QSL("-install")) {
            if (!d->controller.isInstalled()) {
                QString account;
                QString password;
                if (d->args.size() > 2)
                    account = d->args.at(2);
                if (d->args.size() > 3)
                    password = d->args.at(3);
                if (!d->install(account, password)) {
                    qCritical() << "The service could not be installed: " <<  serviceName();
                    return -1;
                }
                qInfo() << "The service has been installed: " << serviceName() << ", " << d->filePath();
            } else {
                qCritical() << "The service is already installed: " << serviceName();
            }
            return 0;
        }

        if (a == QSL("-u") || a == QSL("-uninstall")) {
            if (d->controller.isInstalled()) {
                if (!d->controller.uninstall()) {
                    qCritical() << "The service could not be uninstalled: " << serviceName();
                    return -1;
                }
                qInfo() << "The service has been uninstalled: " << serviceName();
            } else {
                qCritical() << "The service is not installed: " << serviceName();
            }
            return 0;
        }

        if (a == QSL("-t") || a == QSL("-terminate")) {
            if (!d->controller.stop())
                qCritical() << "The service could not be stopped.";
            return 0;
        }

        if (a == QSL("-p") || a == QSL("-pause")) {
            d->controller.pause();
            return 0;
        }

        if (a == QSL("-r") || a == QSL("-resume")) {
            d->controller.resume();
            return 0;
        }

        if (a == QSL("-h") || a == QSL("-help")) {
            qInfo() << QSL("  %1 - [i|u|t|p|r|h]").arg(d->args.at(0));
            qInfo() << "    -i(nstall) [account] [password]: Install the service, optionally using given account and password";
            qInfo() << "    -u(ninstall)                   : Uninstall the service.";
            qInfo() << "    -t(erminate)                   : Stop the service.";
            qInfo() << "    -p(ause)                       : Pause the service.";
            qInfo() << "    -r(esume)                      : Resume the service.";
            qInfo() << "    -h(elp)                        : Show this help.";
            qInfo() << "No arguments: Start in interactive mode (or service on non-interactive session).";
            return 0;
        }
    }

    if (!d->start()) {
        qCritical() << "The service could not start: " << serviceName();
        return -4;
    }
    return 0;
}

QtServiceBase *QtServiceBase::instance()
{
    return QtServiceBasePrivate::instance;
}

void QtServiceBase::stop()
{
}

void QtServiceBase::pause()
{
}

void QtServiceBase::resume()
{
}
