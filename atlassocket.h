#ifndef CATLASSOCKET_H
#define CATLASSOCKET_H

#include <QSslSocket>
#include "atlas.h"

class CAtlasSocket : public QSslSocket
{
    Q_OBJECT
public:
    explicit CAtlasSocket(QObject *parent = nullptr);

    bool authenticated() const;
    void setAuthenticated(bool authenticated);

    CAtlasServer::AtlasDirection direction() const;
    void setDirection(CAtlasServer::AtlasDirection direction);

private:
    bool m_authenticated { false };
    CAtlasServer::AtlasDirection m_direction { CAtlasServer::Atlas_JE };

};

#endif // CATLASSOCKET_H
