#ifndef CSERVER_H
#define CSERVER_H

#include <QTcpServer>
#include <QSslKey>
#include <QSslCertificate>
#include "atlas.h"

class CServer : public QTcpServer
{
public:
    CServer(bool interactive, QObject *parent = 0);

    void pause();

    void resume();

private:
    int m_port;
    bool m_disabled;
    QSslKey m_privateKey;
    QSslCertificate m_serverCert;
    bool m_interactive;
    QStringList m_clientTokens;
    QList<qintptr> m_authList;
    QString m_atlasEnv;
    QHash<qintptr,CAtlasServer::AtlasDirection> m_direction;

    void loadSettings();
    void incomingConnection(int socket);

public:
    int atlasPort() const;
    QSslKey privateKey() const;
    QSslCertificate serverCert() const;
    QStringList clientTokens() const;
    QString atlasEnv() const;

    void setAtlasPort(int port);
    void setPrivateKey(const QSslKey &privateKey);
    void setServerCert(const QSslCertificate &serverCert);
    void setAtlasEnv(const QString &atlasEnv);
    void deleteToken(const QString &token);
    void addToken(const QString &token);

public slots:
    bool saveSettings();

private slots:
    void readClient();
    void discardClient();

};

#endif // CSERVER_H
