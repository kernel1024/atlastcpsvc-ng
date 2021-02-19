#ifndef CSERVER_H
#define CSERVER_H

#include <QTcpServer>
#include <QSslKey>
#include <QSslCertificate>
#include "atlas.h"

namespace CDefaults {
const int atlPort = 18000;
const QHostAddress::SpecialAddress atlHost = QHostAddress::AnyIPv4;
}

class CServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit CServer(QObject *parent = nullptr);
    ~CServer() override;

    void start();
    void pause();
    void resume();

private:
    Q_DISABLE_COPY(CServer)

    int m_atlasPort { CDefaults::atlPort };
    bool m_disabled { false };
    QHostAddress m_atlasHost;
    QSslKey m_privateKey;
    QSslCertificate m_serverCert;
    QStringList m_clientTokens;
    QString m_atlasEnv;

    void loadSettings();

public:
    int atlasPort() const;
    QHostAddress atlasHost() const;
    QSslKey privateKey() const;
    QSslCertificate serverCert() const;
    QStringList clientTokens() const;
    QString atlasEnv() const;

    void setAtlasPort(int port);
    void setAtlasHost(const QHostAddress &host);
    void setPrivateKey(const QSslKey &privateKey);
    void setServerCert(const QSslCertificate &serverCert);
    void setAtlasEnv(const QString &atlasEnv);
    void deleteToken(const QString &token);
    void addToken(const QString &token);

protected:
    void incomingConnection(qintptr socket) override;

private Q_SLOTS:
    void readClient();
    void discardClient();
    void acceptConnections();

public Q_SLOTS:
    bool saveSettings();
    void closeSocket();

};

#endif // CSERVER_H
