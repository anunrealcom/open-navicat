#include "ConnectionStore.h"

#include "SimpleCrypt.h"

#include <QDateTime>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

ConnectionStore &ConnectionStore::instance()
{
    static ConnectionStore s;
    return s;
}

ConnectionStore::ConnectionStore()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    const QString path = dir + QLatin1String("/datamg.db");

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("datamg-config"));
    m_db.setDatabaseName(path);
    if (!m_db.open())
        qWarning("ConnectionStore: cannot open config db: %s",
                 qPrintable(m_db.lastError().text()));
    else
        ensureSchema();
}

bool ConnectionStore::ensureSchema()
{
    QSqlQuery q(m_db);
    return q.exec(
               QStringLiteral("CREATE TABLE IF NOT EXISTS connections ("
                              "  id TEXT PRIMARY KEY,"
                              "  name TEXT NOT NULL,"
                              "  type INTEGER NOT NULL,"
                              "  host TEXT,"
                              "  port INTEGER,"
                              "  username TEXT,"
                              "  password TEXT,"
                              "  database_name TEXT,"
                              "  updated_at TEXT DEFAULT (datetime('now'))"
                              ")")) &&
           q.exec(
               QStringLiteral("CREATE TABLE IF NOT EXISTS query_history ("
                              "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                              "  profile_id TEXT NOT NULL,"
                              "  database TEXT,"
                              "  sql TEXT NOT NULL,"
                              "  timestamp INTEGER NOT NULL,"
                              "  elapsed_ms INTEGER DEFAULT 0,"
                              "  success INTEGER DEFAULT 1"
                              ");"
                              "CREATE INDEX IF NOT EXISTS idx_qh_profile "
                              "ON query_history(profile_id,timestamp)"));
}

QString ConnectionStore::storageLocation() const
{
    return m_db.databaseName();
}

QList<ConnectionProfile> ConnectionStore::loadAll()
{
    QList<ConnectionProfile> list;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT id,name,type,host,port,username,password,database_name "
            "FROM connections ORDER BY name COLLATE NOCASE")))
        return list;

    while (q.next()) {
        ConnectionProfile p;
        p.id = q.value(0).toString();
        p.name = q.value(1).toString();
        p.type = ConnectionProfile::typeFromInt(q.value(2).toInt());
        p.host = q.value(3).toString();
        p.port = q.value(4).toInt();
        p.username = q.value(5).toString();
        p.password = SimpleCrypt::decode(q.value(6).toString());
        p.database = q.value(7).toString();
        list.append(p);
    }
    return list;
}

ConnectionProfile ConnectionStore::load(const QString &id)
{
    ConnectionProfile p;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id,name,type,host,port,username,password,database_name "
                             "FROM connections WHERE id=?"));
    q.addBindValue(id);
    if (q.exec() && q.next()) {
        p.id = q.value(0).toString();
        p.name = q.value(1).toString();
        p.type = ConnectionProfile::typeFromInt(q.value(2).toInt());
        p.host = q.value(3).toString();
        p.port = q.value(4).toInt();
        p.username = q.value(5).toString();
        p.password = SimpleCrypt::decode(q.value(6).toString());
        p.database = q.value(7).toString();
    }
    return p;
}

bool ConnectionStore::save(const ConnectionProfile &profile)
{
    ConnectionProfile p = profile;
    if (p.id.isEmpty())
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO connections(id,name,type,host,port,username,password,database_name)"
        " VALUES(?,?,?,?,?,?,?,?)"
        " ON CONFLICT(id) DO UPDATE SET"
        " name=excluded.name,type=excluded.type,host=excluded.host,port=excluded.port,"
        " username=excluded.username,password=excluded.password,"
        " database_name=excluded.database_name,updated_at=datetime('now')"));
    q.addBindValue(p.id);
    q.addBindValue(p.name);
    q.addBindValue(ConnectionProfile::typeToInt(p.type));
    q.addBindValue(p.host);
    q.addBindValue(p.port);
    q.addBindValue(p.username);
    q.addBindValue(SimpleCrypt::encode(p.password));
    q.addBindValue(p.database);
    return q.exec();
}

bool ConnectionStore::remove(const QString &id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM connections WHERE id=?"));
    q.addBindValue(id);
    return q.exec();
}

void ConnectionStore::saveQuery(const QString &profileId, const QString &database,
                                const QString &sql, int elapsedMs, bool success)
{
    if (!m_db.isOpen() || profileId.isEmpty())
        return;
    const QString trimmed = sql.trimmed();
    if (trimmed.isEmpty())
        return;

    // 先清掉完全相同的一条，避免重复
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM query_history WHERE profile_id=? AND sql=?"));
    q.addBindValue(profileId);
    q.addBindValue(trimmed);
    q.exec();

    // 插入新的
    QSqlQuery q2(m_db);
    q2.prepare(QStringLiteral("INSERT INTO query_history(profile_id,database,sql,timestamp,"
                              "elapsed_ms,success) VALUES(?,?,?,?,?,?)"));
    q2.addBindValue(profileId);
    q2.addBindValue(database);
    q2.addBindValue(trimmed);
    q2.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q2.addBindValue(elapsedMs);
    q2.addBindValue(success ? 1 : 0);
    q2.exec();

    // 清理：保留最新 500 条
    QSqlQuery q3(m_db);
    q3.prepare(QStringLiteral("DELETE FROM query_history WHERE id NOT IN "
                              "(SELECT id FROM query_history WHERE profile_id=? "
                              " ORDER BY timestamp DESC LIMIT 500)"));
    q3.addBindValue(profileId);
    q3.exec();
}

QList<QueryHistoryItem> ConnectionStore::queryHistory(const QString &profileId, int limit)
{
    QList<QueryHistoryItem> list;
    if (!m_db.isOpen())
        return list;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id,profile_id,database,sql,timestamp,elapsed_ms,success "
        "FROM query_history WHERE profile_id=? "
        "ORDER BY timestamp DESC LIMIT ?"));
    q.addBindValue(profileId);
    q.addBindValue(limit);
    if (q.exec()) {
        while (q.next()) {
            QueryHistoryItem it;
            it.rowid = q.value(0).toLongLong();
            it.profileId = q.value(1).toString();
            it.database = q.value(2).toString();
            it.sql = q.value(3).toString();
            it.timestamp = q.value(4).toLongLong();
            it.elapsedMs = q.value(5).toInt();
            it.success = q.value(6).toInt() == 1;
            list << it;
        }
    }
    return list;
}

void ConnectionStore::clearQueryHistory(const QString &profileId)
{
    if (!m_db.isOpen())
        return;
    QSqlQuery q(m_db);
    if (profileId.isEmpty())
        q.exec(QStringLiteral("DELETE FROM query_history"));
    else {
        q.prepare(QStringLiteral("DELETE FROM query_history WHERE profile_id=?"));
        q.addBindValue(profileId);
        q.exec();
    }
}
