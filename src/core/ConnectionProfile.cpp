#include "ConnectionProfile.h"

DbType ConnectionProfile::typeFromInt(int v)
{
    switch (v) {
    case static_cast<int>(DbType::PostgreSQL): return DbType::PostgreSQL;
    case static_cast<int>(DbType::SQLite):     return DbType::SQLite;
    default:                                    return DbType::MySQL;
    }
}

int ConnectionProfile::typeToInt(DbType t)
{
    return static_cast<int>(t);
}

int ConnectionProfile::defaultPort(DbType t)
{
    if (t == DbType::PostgreSQL) return 5432;
    if (t == DbType::SQLite)     return 0;
    return 3306;
}

QString ConnectionProfile::typeDisplay() const
{
    switch (type) {
    case DbType::PostgreSQL: return QStringLiteral("PostgreSQL");
    case DbType::SQLite:     return QStringLiteral("SQLite");
    default:                  return QStringLiteral("MySQL");
    }
}
