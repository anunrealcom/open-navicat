#pragma once

#include "DbClient.h"

#include <QString>
#include <QUuid>

class QSqlDatabase;

/// SQLite 客户端（走 Qt QSQLITE 驱动）
class SqliteClient : public DbClient
{
    Q_OBJECT
public:
    explicit SqliteClient(QObject *parent = nullptr);
    ~SqliteClient() override;

    bool open(const ConnectionProfile &profile, QString *err) override;
    void close() override;
    bool isOpen() const override;

    bool execute(const QString &sql,
                 const QVariantList &params = {},
                 DbResult *result = nullptr,
                 QString *err = nullptr) override;

    QString quoteLiteral(const QVariant &v) const override;
    QString quoteIdent(const QString &ident) const override;
    bool setCurrentDatabase(const QString &db, QString *err) override;
    QString currentDatabase() const override;
    DbType type() const override { return DbType::SQLite; }

private:
    bool applyConnection(const ConnectionProfile &profile, QString *err);

    QString m_connName;
};
