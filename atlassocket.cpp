#include "atlassocket.h"

CAtlasSocket::CAtlasSocket(QObject *parent)
    : QSslSocket(parent)
{
    m_authenticated = false;
    m_direction = CAtlasServer::Atlas_JE;
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

void CAtlasSocket::setDirection(const CAtlasServer::AtlasDirection &direction)
{
    m_direction = direction;
}
