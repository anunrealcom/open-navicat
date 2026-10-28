#pragma once

#include "DbClient.h"

/// PostgreSQL 客户端（Qt SQL QPSQL 驱动，运行期依赖 libpq.dll）
class PgClient : public DbClient
{
    Q_OBJECT
public:
    PgClient();
    ~PgClient() override;

    bool open(const ConnectionProfile &profile, QString *err) override;
    void close() override;
    bool isOpen() const override;
    bool execute(const QString &sql, const QVariantList &params,
                 DbResult *result, QString *err) override;
    QString quoteLiteral(const QVariant &v) const override;
    QString quoteIdent(const QString &ident) const override;
    bool setCurrentDatabase(const QString &db, QString *err) override;
    QString currentDatabase() const override { return m_currentDb; }
    DbType type() const override { return DbType::PostgreSQL; }

private:
    bool applyConnection(const ConnectionProfile &profile, QString *err);
    QString m_connName;
};
