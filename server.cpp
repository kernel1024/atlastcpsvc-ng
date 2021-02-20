#include <QSettings>
#include <QTcpSocket>
#include <QUrl>
#include "qtservice.h"
#include "server.h"
#include "service.h"
#include "atlassocket.h"
#include "qsl.h"
#include <QDebug>

CServer::CServer(QObject *parent)
    : QTcpServer(parent),
    m_atlasHost(QHostAddress(CDefaults::atlHost)),
    m_atlas(new CAtlasServer(this))
{
    loadSettings();
    connect(this, &QTcpServer::newConnection, this, &CServer::acceptConnections);

    if (!m_atlas->init(CAtlasServer::Atlas_JE, m_atlasEnv))
        qCritical() << "Unable to load ATLAS engine";
}

CServer::~CServer()
{
    closeSocket();
}

bool CServer::start()
{
    if (isListening()) return false;
    if (m_atlas.isNull()) return false;

    if (!isAtlasLoaded()) {
        qWarning() << "ATLAS engine not loaded";
        return false;
    }

    if (m_privateKey.isNull() || m_serverCert.isNull()) {
        qWarning() << "Credentials is empty, unable to open socket";
        return false;
    }

    listen(m_atlasHost, m_atlasPort);
    return true;
}

void CServer::pause()
{
    m_disabled = true;
}

void CServer::resume()
{
    m_disabled = false;
}

QHostAddress CServer::atlasHost() const
{
    return m_atlasHost;
}

void CServer::setAtlasHost(const QHostAddress &host)
{
    m_atlasHost = host;
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
    m_atlasPort = port;
}

QString CServer::atlasEnv() const
{
    return m_atlasEnv;
}

QStringList CServer::atlasEnvironments() const
{
    QStringList res;
    if (m_atlas)
        res = m_atlas->getEnvironments();
    return res;
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
    return m_atlasPort;
}

void CServer::loadSettings()
{
    m_serverCert.clear();
    m_privateKey.clear();

    QSettings settings;
    settings.beginGroup(QSL("Server"));

    m_atlasPort = settings.value(QSL("port"),CDefaults::atlPort).toInt();
    m_atlasHost = QHostAddress(settings.value(QSL("host"),
        QHostAddress(CDefaults::atlHost).toIPv4Address()).toUInt());
    m_atlasEnv = settings.value(QSL("atlasEnvironment"),QSL("General")).toString();

    QByteArray buf;
    buf = settings.value(QSL("privateKey"),QByteArray()).toByteArray();
    if (!buf.isEmpty())
        m_privateKey = QSslKey(buf,QSsl::Rsa,QSsl::Pem,QSsl::PrivateKey);
    buf = settings.value(QSL("serverCert"),QByteArray()).toByteArray();
    if (!buf.isEmpty())
        m_serverCert = QSslCertificate(buf,QSsl::Pem);

    m_clientTokens = settings.value(QSL("clientTokens"),QStringList()).toStringList();

    settings.endGroup();
}

bool CServer::isAtlasLoaded() const
{
    if (m_atlas)
        return m_atlas->isLoaded();

    return false;
}

void CServer::incomingConnection(qintptr socket)
{
    if (m_disabled)
        return;

    auto* s = new CAtlasSocket(this);
    if (s->setSocketDescriptor(socket))
        addPendingConnection(s);
}

void CServer::acceptConnections()
{
    while (hasPendingConnections()) {
        QTcpSocket* ts = nextPendingConnection();
        auto* s = qobject_cast<CAtlasSocket *>(ts);
        // Accept only our sockets, close others.
        if (s == nullptr) {
            if (ts) {
                ts->close();
                ts->deleteLater();
            }
            return;
        }

        connect(s, &CAtlasSocket::disconnected, this, &CServer::discardClient);
        connect(s, &CAtlasSocket::readyRead, this, &CServer::readClient);

        s->setPrivateKey(m_privateKey);
        s->setLocalCertificate(m_serverCert);
        s->startServerEncryption();
    }
}

void CServer::readClient()
{
    static const QString cmdInit(QSL("INIT:"));
    static const QString cmdDir(QSL("DIR:"));
    static const QString cmdTr(QSL("TR:"));
    static const QString cmdFin(QSL("FIN:"));

    auto* socket = qobject_cast<CAtlasSocket *>(sender());
    if (socket == nullptr) return;
    if (!socket->isEncrypted()) return;

    if (m_disabled || m_atlas.isNull()) {
        socket->setAuthenticated(false);
        socket->close();
        return;
    }

    bool needCloseSocket = false;
    bool handled = false;
    if (socket->canReadLine()) {
        QString cmd = QString::fromLatin1(socket->readLine().simplified());
        if (cmd.startsWith(cmdInit)) {
            QString token = cmd;
            token.remove(0,cmdInit.length());
            if (m_clientTokens.contains(token)) {
                socket->setAuthenticated(true);
                socket->setDirection(CAtlasServer::Atlas_JE);
                socket->write("OK\r\n");
            } else {
                socket->write("ERR:NOT_AUTHORIZED\r\n");
                needCloseSocket = true;
            }
            handled = true;

        } else if (socket->authenticated()) {
            if (cmd.startsWith(cmdDir)) {
                QString dir = cmd.toUpper();
                dir.remove(0,cmdDir.length());
                if (dir.startsWith(QSL("JE"))) {
                    socket->setDirection(CAtlasServer::Atlas_JE);
                } else if (dir.startsWith(QSL("EJ"))) {
                    socket->setDirection(CAtlasServer::Atlas_EJ);
                } else {
                    socket->setDirection(CAtlasServer::Atlas_Auto);
                }
                socket->write("OK\r\n");
                handled = true;

            } else if (cmd.startsWith(cmdFin)) {
                socket->write("OK\r\n");
                needCloseSocket = true;
                handled = true;

            } else if (cmd.startsWith(cmdTr)) {
                QString s = cmd;
                s.remove(0,cmdTr.length());
                s = QUrl::fromPercentEncoding(s.toLatin1()).trimmed();
                if (s.isEmpty()) {
                    socket->write("ERR:NULL_STR_DECODED\r\n");
                } else {
                    s = m_atlas->translate(socket->direction(),s);
                    if (s.startsWith(QSL("ERR"))) {
                        socket->write("ERR:TRANS_FAILED\r\n");
                    } else {
                        s = QSL("RES:%1\r\n").arg(QString::fromLatin1(QUrl::toPercentEncoding(s)).trimmed());
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
            socket->setAuthenticated(false);
            socket->close();
        }
    }
}

void CServer::discardClient()
{
    auto* s = qobject_cast<CAtlasSocket *>(sender());
    if (s)
        s->deleteLater();
}

void CServer::closeSocket()
{
    if (isListening())
        close();
}

bool CServer::saveSettings()
{
    QSettings settings;
    if (!settings.isWritable() || (settings.status() != QSettings::NoError))
        return false;

    settings.beginGroup(QSL("Server"));

    settings.setValue(QSL("port"),m_atlasPort);
    settings.setValue(QSL("host"),m_atlasHost.toIPv4Address());
    settings.setValue(QSL("atlasEnvironment"),m_atlasEnv);
    settings.setValue(QSL("privateKey"),QVariant::fromValue(m_privateKey.toPem()));
    settings.setValue(QSL("serverCert"),QVariant::fromValue(m_serverCert.toPem()));
    settings.setValue(QSL("clientTokens"),QVariant::fromValue(m_clientTokens));

    settings.endGroup();

    return true;
}
