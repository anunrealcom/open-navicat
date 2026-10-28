#pragma once

#include "ConnectionProfile.h"

#include <QHash>
#include <QObject>
#include <QPointer>

class DbClient;

/// 运行期会话：维护 profileId -> DbClient
class SessionManager : public QObject
{
    Q_OBJECT
public:
    static SessionManager &instance();

    /// 打开（或复用）一个连接
    DbClient *open(const ConnectionProfile &profile, QString *err);
    DbClient *client(const QString &profileId) const;
    bool isOpen(const QString &profileId) const;
    void close(const QString &profileId);
    void closeAll();

private:
    SessionManager() = default;
    QHash<QString, DbClient *> m_clients;
};
