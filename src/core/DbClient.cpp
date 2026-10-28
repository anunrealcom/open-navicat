#include "DbClient.h"

#include "MySqlClient.h"
#include "PgClient.h"
#include "SqliteClient.h"

DbClient *DbClient::create(DbType type, QObject *parent)
{
    switch (type) {
    case DbType::PostgreSQL: return new PgClient();
    case DbType::SQLite:     return new SqliteClient();
    default:                  return new MySqlClient();
    }
}
