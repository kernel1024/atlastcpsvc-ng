#ifndef CATLASSOCKET_H
#define CATLASSOCKET_H

#include <QSslSocket>
#include "atlas.h"

class CAtlasSocket : public QSslSocket
{
    Q_OBJECT
public:
    explicit CAtlasSocket(QObject *parent = Q_NULLPTR);

    bool authenticated() const;
    void setAuthenticated(bool authenticated);

    CAtlasServer::AtlasDirection direction() const;
    void setDirection(const CAtlasServer::AtlasDirection &direction);

private:
    bool m_authenticated;
    CAtlasServer::AtlasDirection m_direction;

};

#endif // CATLASSOCKET_H
