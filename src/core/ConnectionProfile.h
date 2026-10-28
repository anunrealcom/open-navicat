#pragma once

#include <QString>
#include <QMetaType>

enum class DbType {
    MySQL,
    PostgreSQL,
    SQLite
};

/// 数据库连接配置（持久化到本地 SQLite）
struct ConnectionProfile {
    QString id;
    QString name;
    DbType type = DbType::MySQL;
    QString host = QStringLiteral("127.0.0.1");
    int port = 3306;
    QString username;
    QString password;
    QString database;           //< 初始/默认数据库，可空（MySQL）

    static DbType typeFromInt(int v);
    static int typeToInt(DbType t);
    static int defaultPort(DbType t);
    QString typeDisplay() const;

    bool isValid() const { return !name.isEmpty() && !host.isEmpty(); }
};
Q_DECLARE_METATYPE(ConnectionProfile)
