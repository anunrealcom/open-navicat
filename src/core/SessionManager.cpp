#include "SessionManager.h"

#include "DbClient.h"

SessionManager &SessionManager::instance()
{
    static SessionManager s;
    return s;
}

DbClient *SessionManager::open(const ConnectionProfile &profile, QString *err)
{
    auto it = m_clients.find(profile.id);
    if (it != m_clients.end() && it.value()->isOpen())
        return it.value();

    if (it != m_clients.end()) {
        it.value()->deleteLater();
        m_clients.erase(it);
    }

    DbClient *client = DbClient::create(profile.type, this);
    if (!client->open(profile, err)) {
        client->deleteLater();
        return nullptr;
    }
    m_clients.insert(profile.id, client);
    return client;
}

DbClient *SessionManager::client(const QString &profileId) const
{
    return m_clients.value(profileId, nullptr);
}

bool SessionManager::isOpen(const QString &profileId) const
{
    DbClient *c = m_clients.value(profileId, nullptr);
    return c && c->isOpen();
}

void SessionManager::close(const QString &profileId)
{
    auto it = m_clients.find(profileId);
    if (it != m_clients.end()) {
        it.value()->close();
        it.value()->deleteLater();
        m_clients.erase(it);
    }
}

void SessionManager::closeAll()
{
    qDeleteAll(m_clients);
    m_clients.clear();
}
