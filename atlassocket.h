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

    CAtlas::AtlasDirection direction() const;
    void setDirection(CAtlas::AtlasDirection direction);

private:
    bool m_authenticated { false };
    CAtlas::AtlasDirection m_direction { CAtlas::Atlas_JE };

};

#endif // CATLASSOCKET_H
