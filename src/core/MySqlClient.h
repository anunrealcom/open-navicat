#pragma once

#include "DbClient.h"

/// 基于 MariaDB Connector/C (libmariadb.dll) 的 MySQL/MariaDB 客户端。
/// 通过 QLibrary 运行时加载，未安装客户端库时程序仍可启动。
class MySqlClient : public DbClient
{
    Q_OBJECT
public:
    MySqlClient();
    ~MySqlClient() override;

    bool open(const ConnectionProfile &profile, QString *err) override;
    void close() override;
    bool isOpen() const override;
    bool execute(const QString &sql, const QVariantList &params,
                 DbResult *result, QString *err) override;
    QString quoteLiteral(const QVariant &v) const override;
    QString quoteIdent(const QString &ident) const override;
    bool setCurrentDatabase(const QString &db, QString *err) override;
    QString currentDatabase() const override { return m_currentDb; }
    DbType type() const override { return DbType::MySQL; }

private:
    void *m_handle = nullptr;   //< MYSQL*，在 .cpp 中解释
};
