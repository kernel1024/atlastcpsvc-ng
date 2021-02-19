#include "atlassocket.h"

CAtlasSocket::CAtlasSocket(QObject *parent)
    : QSslSocket(parent)
{
}

bool CAtlasSocket::authenticated() const
{
    return m_authenticated;
}

void CAtlasSocket::setAuthenticated(bool authenticated)
{
    m_authenticated = authenticated;
}

CAtlasServer::AtlasDirection CAtlasSocket::direction() const
{
    return m_direction;
}

void CAtlasSocket::setDirection(CAtlasServer::AtlasDirection direction)
{
    m_direction = direction;
}
