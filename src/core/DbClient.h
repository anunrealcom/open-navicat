#pragma once

#include "ConnectionProfile.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QBitArray>

/// 查询结果（统一按文本承载，NULL 用 nullFlags 标记）
struct DbResult
{
    QStringList columns;
    QList<QStringList> rows;
    QList<QBitArray> nullFlags;
    qint64 affectedRows = -1;
    qint64 insertId = 0;
    qint64 elapsedMs = 0;
    bool hasResult = false;

    int rowCount() const { return rows.size(); }
    bool isNull(int r, int c) const
    {
        return r < nullFlags.size() && c < nullFlags[r].size() && nullFlags[r].at(c);
    }
};

/// 数据库客户端抽象（MySQL 走原生 libmariadb；PostgreSQL 走 QPSQL）
class DbClient : public QObject
{
    Q_OBJECT
public:
    static DbClient *create(DbType type, QObject *parent = nullptr);

    ~DbClient() override = default;

    virtual bool open(const ConnectionProfile &profile, QString *err) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    /// 执行 SQL。params 与 ? 占位符按位置绑定（MySQL 端做客户端转义）。
    virtual bool execute(const QString &sql,
                         const QVariantList &params = {},
                         DbResult *result = nullptr,
                         QString *err = nullptr) = 0;

    virtual QString quoteLiteral(const QVariant &v) const = 0;
    virtual QString quoteIdent(const QString &ident) const = 0;
    virtual bool setCurrentDatabase(const QString &db, QString *err) = 0;
    virtual QString currentDatabase() const = 0;

    virtual DbType type() const = 0;
    QString lastError() const { return m_lastError; }
    int lastErrno() const { return m_lastErrno; }
    QString lastSqlstate() const { return m_lastSqlstate; }

    /// 将 errno + SQLSTATE 拼入主错误文本，便于用户直接定位
    QString formatLastError() const
    {
        QString msg = m_lastError;
        if (m_lastErrno != 0 || !m_lastSqlstate.isEmpty()) {
            msg += QStringLiteral("  (errno=%1").arg(m_lastErrno);
            if (!m_lastSqlstate.isEmpty())
                msg += QStringLiteral(", SQLSTATE=%1").arg(m_lastSqlstate);
            msg += QLatin1Char(')');
        }
        return msg;
    }

    ConnectionProfile profile() const { return m_profile; }

protected:
    ConnectionProfile m_profile;
    QString m_lastError;
    int m_lastErrno = 0;
    QString m_lastSqlstate;
    QString m_currentDb;
};
