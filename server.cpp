#include <QSettings>
#include <QTcpSocket>
#include <QSslSocket>
#include <QUrl>
#include "qtservice.h"
#include "server.h"
#include "service.h"
#include <QDebug>

CServer::CServer(bool interactive, QObject *parent)
    : QTcpServer(parent)
{
    m_port = 18000;
    m_disabled = false;
    m_interactive = interactive;

    loadSettings();

    qInfo() << "Initializing ATLAS...";
    if (!atlasServer.isLoaded())
        if (!atlasServer.init(CAtlasServer::Atlas_JE, m_atlasEnv))
            return;

    qInfo() << "Checking for valid SSL keys...";
    if (m_privateKey.isNull() || m_serverCert.isNull()) return;

    qInfo() << "Start listening.";
    listen(QHostAddress::AnyIPv4, m_port);
}

CServer::~CServer()
{
    closeSocket();
}

void CServer::pause()
{
    m_disabled = true;
}

void CServer::resume()
{
    m_disabled = false;
}

void CServer::setAtlasEnv(const QString &atlasEnv)
{
    m_atlasEnv = atlasEnv;
}

void CServer::deleteToken(const QString &token)
{
    if (m_clientTokens.contains(token))
        m_clientTokens.removeAll(token);
}

void CServer::addToken(const QString &token)
{
    m_clientTokens.append(token);
}

void CServer::setServerCert(const QSslCertificate &serverCert)
{
    m_serverCert = serverCert;
}

void CServer::setPrivateKey(const QSslKey &privateKey)
{
    m_privateKey = privateKey;
}

void CServer::setAtlasPort(int port)
{
    m_port = port;
}

QString CServer::atlasEnv() const
{
    return m_atlasEnv;
}

QStringList CServer::clientTokens() const
{
    return m_clientTokens;
}

QSslCertificate CServer::serverCert() const
{
    return m_serverCert;
}

QSslKey CServer::privateKey() const
{
    return m_privateKey;
}

int CServer::atlasPort() const
{
    return m_port;
}

void CServer::loadSettings()
{
    qInfo() << "Loading settings...";
    m_serverCert.clear();
    m_privateKey.clear();

    QSettings settings(QSettings::SystemScope,"kernel1024","atlastcpsvc-ng");
    settings.beginGroup("Server");

    m_port = settings.value("port",18000).toInt();
    m_atlasEnv = settings.value("atlasEnvironment",QString("General")).toString();

    QByteArray ba;
    ba = settings.value("privateKey",QByteArray()).toByteArray();
    if (!ba.isEmpty())
        m_privateKey = QSslKey(ba,QSsl::Rsa,QSsl::Pem,QSsl::PrivateKey);
    ba = settings.value("serverCert",QByteArray()).toByteArray();
    if (!ba.isEmpty())
        m_serverCert = QSslCertificate(ba,QSsl::Pem);

    m_clientTokens = settings.value("clientTokens",QStringList()).value<QStringList>();

    settings.endGroup();
}

void CServer::incomingConnection(int socket)
{
    if (m_disabled)
        return;

    QSslSocket* s = new QSslSocket(this);
    connect(s, &QSslSocket::disconnected, this, &CServer::discardClient);
    if (s->setSocketDescriptor(socket)) {
        addPendingConnection(s);
        connect(s, &QSslSocket::readyRead, this, &CServer::readClient);
        s->setPrivateKey(m_privateKey);
        s->setLocalCertificate(m_serverCert);
        s->startServerEncryption();
    }
}

void CServer::readClient()
{
    if (m_disabled)
        return;

    QSslSocket* socket = (QSslSocket*)sender();
    if (!socket->isEncrypted()) return;

    bool needCloseSocket = false;
    bool handled = false;
    if (socket->canReadLine()) {
        QString cmd = socket->readLine().simplified();
        if (cmd.startsWith("INIT:"))
        {
            QString token = cmd;
            token.replace("INIT:", "");
            if (m_clientTokens.contains(token)) {
                m_authList.append(socket->socketDescriptor());
                m_direction[socket->socketDescriptor()]=CAtlasServer::Atlas_JE;
                socket->write("OK\r\n");
            } else {
                socket->write("ERR:NOT_AUTHORIZED\r\n");
                needCloseSocket = true;
            }
            handled = true;
        } else if (m_authList.contains(socket->socketDescriptor())) {
            if (cmd.startsWith("DIR:"))
            {
                QString dir = cmd.toUpper();
                dir.replace("DIR:", "");
                if (dir.startsWith("JE"))
                    m_direction[socket->socketDescriptor()]=CAtlasServer::Atlas_JE;
                else if (dir.startsWith("EJ"))
                    m_direction[socket->socketDescriptor()]=CAtlasServer::Atlas_EJ;
                else
                    m_direction[socket->socketDescriptor()]=CAtlasServer::Atlas_Auto;
                socket->write("OK\r\n");
                handled = true;
            } else if (cmd.startsWith("FIN"))
            {
                socket->write("OK\r\n");
                m_authList.removeAll(socket->socketDescriptor());
                m_direction.remove(socket->socketDescriptor());
                needCloseSocket = true;
                handled = true;
            } else if (cmd.startsWith("TR:"))
            {
                QString s = cmd;
                s.replace("TR:", "");
                s = QUrl::fromPercentEncoding(s.toLatin1()).trimmed();
                if (s.isEmpty())
                    socket->write("ERR:NULL_STR_DECODED\r\n");
                else {
                    s = atlasServer.translate(m_direction.value(socket->socketDescriptor(),CAtlasServer::Atlas_JE),s);
                    if (s.startsWith("ERR"))
                        socket->write("ERR:TRANS_FAILED\r\n");
                    else {
                        s = "RES:" + QString::fromLatin1(QUrl::toPercentEncoding(s)).trimmed() + "\r\n";
                        socket->write(s.toLatin1());
                    }
                }
                handled = true;
            }
        }
        if (!handled) {
            socket->write("ERR:NOT_RECOGNIZED\r\n");
            needCloseSocket = true;
        }

        socket->flush();

        if (needCloseSocket) {
            if (m_authList.contains(socket->socketDescriptor())) {
                m_authList.removeAll(socket->socketDescriptor());
                m_direction.remove(socket->socketDescriptor());
            }
            socket->close();
        }
    }
}

void CServer::discardClient()
{
}

void CServer::closeSocket()
{
    if (isListening())
        close();
}

bool CServer::saveSettings()
{
    qInfo() << "Saving settings...";
    QSettings settings(QSettings::SystemScope,"kernel1024","atlastcpsvc-ng");
    if (!settings.isWritable() || settings.status()!=QSettings::NoError)
        return false;
    settings.beginGroup("Server");

    settings.setValue("port",m_port);
    settings.setValue("atlasEnvironment",m_atlasEnv);

    QByteArray ba = m_privateKey.toPem();
    settings.setValue("privateKey",QVariant::fromValue(ba));
    ba = m_serverCert.toPem();
    settings.setValue("serverCert",QVariant::fromValue(ba));

    settings.setValue("clientTokens",QVariant::fromValue(m_clientTokens));

    settings.endGroup();

    return true;
}
