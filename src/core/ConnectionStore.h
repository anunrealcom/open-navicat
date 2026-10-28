#pragma once

#include "ConnectionProfile.h"

#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QStringList>

/// 单条查询历史
struct QueryHistoryItem
{
    qint64 rowid = 0;
    QString profileId;
    QString database;
    QString sql;
    qint64 timestamp = 0;   // unix ms
    int elapsedMs = 0;
    bool success = true;
};

/// 连接配置仓库：基于 SQLite 的本地持久化
class ConnectionStore : public QObject
{
    Q_OBJECT
public:
    static ConnectionStore &instance();

    QList<ConnectionProfile> loadAll();
    ConnectionProfile load(const QString &id);
    bool save(const ConnectionProfile &profile);
    bool remove(const QString &id);

    QString storageLocation() const;

    // 查询历史（最近 N 条）
    void saveQuery(const QString &profileId, const QString &database,
                   const QString &sql, int elapsedMs, bool success);
    QList<QueryHistoryItem> queryHistory(const QString &profileId, int limit = 500);
    void clearQueryHistory(const QString &profileId = QString());

private:
    ConnectionStore();
    bool ensureSchema();

    QSqlDatabase m_db;
};
