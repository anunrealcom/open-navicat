#include "PgClient.h"

#include <QElapsedTimer>
#include <QMetaType>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

PgClient::PgClient()
{
    m_connName = QStringLiteral("pg-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

PgClient::~PgClient()
{
    close();
    if (QSqlDatabase::contains(m_connName)) {
        QSqlDatabase::removeDatabase(m_connName);
    }
}

bool PgClient::applyConnection(const ConnectionProfile &profile, QString *err)
{
    m_lastErrno = 0;
    m_lastSqlstate.clear();
    if (!QSqlDatabase::drivers().contains(QStringLiteral("QPSQL"))) {
        m_lastError = tr("当前 Qt 未包含 QPSQL 驱动，或程序目录缺少 libpq.dll 及其依赖。");
        if (err) *err = formatLastError();
        return false;
    }

    if (QSqlDatabase::contains(m_connName))
        QSqlDatabase::removeDatabase(m_connName);

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), m_connName);
    db.setHostName(profile.host);
    db.setPort(profile.port > 0 ? profile.port : 5432);
    db.setUserName(profile.username);
    db.setPassword(profile.password);
    db.setDatabaseName(profile.database.isEmpty() ? QStringLiteral("postgres")
                                                  : profile.database);
    db.setConnectOptions(QStringLiteral("connect_timeout=10"));

    if (!db.open()) {
        const QSqlError &e = db.lastError();
        m_lastError = e.text();
        m_lastErrno = 0;
        m_lastSqlstate.clear();
        if (err) *err = formatLastError();
        return false;
    }
    m_lastErrno = 0;
    m_lastSqlstate.clear();
    return true;
}

bool PgClient::open(const ConnectionProfile &profile, QString *err)
{
    if (!applyConnection(profile, err))
        return false;

    m_profile = profile;
    m_currentDb = profile.database.isEmpty() ? QStringLiteral("postgres") : profile.database;

    DbResult r;
    QString e;
    if (execute(QStringLiteral("SELECT current_database()"), {}, &r, &e) && r.rowCount() > 0)
        m_currentDb = r.rows.first().value(0);
    return true;
}

void PgClient::close()
{
    if (QSqlDatabase::contains(m_connName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connName);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_connName);
    }
    m_currentDb.clear();
}

bool PgClient::isOpen() const
{
    return QSqlDatabase::contains(m_connName) &&
           QSqlDatabase::database(m_connName, false).isOpen();
}

bool PgClient::execute(const QString &sql, const QVariantList &params,
                       DbResult *result, QString *err)
{
    if (!QSqlDatabase::contains(m_connName)) {
        m_lastError = tr("连接未打开");
        m_lastErrno = 0;
        m_lastSqlstate.clear();
        if (err) *err = formatLastError();
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connName, false);
    QSqlQuery q(db);
    QElapsedTimer timer;
    timer.start();

    bool ok = false;
    if (params.isEmpty()) {
        ok = q.exec(sql);
    } else {
        if (!q.prepare(sql)) {
            const QSqlError &e = q.lastError();
            m_lastError = e.text();
            m_lastErrno = 0;
            m_lastSqlstate.clear();
            if (err) *err = formatLastError();
            return false;
        }
        for (const QVariant &v : params)
            q.addBindValue(v);
        ok = q.exec();
    }

    if (!ok) {
        const QSqlError &e = q.lastError();
        m_lastError = e.text();
        m_lastErrno = 0;
        m_lastSqlstate.clear();
        if (err) *err = formatLastError();
        return false;
    }

    if (result) {
        result->columns.clear();
        result->rows.clear();
        result->nullFlags.clear();

        const QSqlRecord rec = q.record();
        const int n = rec.count();
        if (n > 0) {
            for (int i = 0; i < n; ++i)
                result->columns << rec.fieldName(i);
            while (q.next()) {
                QStringList vals;
                QBitArray nulls(n);
                for (int i = 0; i < n; ++i) {
                    const QVariant v = q.value(i);
                    if (v.isNull()) {
                        nulls.setBit(i);
                        vals << QString();
                    } else if (v.userType() == QMetaType::QByteArray) {
                        vals << QString::fromUtf8(v.toByteArray());
                    } else {
                        vals << v.toString();
                    }
                }
                result->rows << vals;
                result->nullFlags << nulls;
            }
            result->hasResult = true;
            result->affectedRows = -1;
        } else {
            result->hasResult = false;
            result->affectedRows = q.numRowsAffected();
        }
        result->elapsedMs = timer.elapsed();
    }
    return true;
}

QString PgClient::quoteLiteral(const QVariant &v) const
{
    if (!v.isValid() || v.isNull())
        return QStringLiteral("NULL");

    const int id = v.userType();
    if (id == QMetaType::Bool)
        return v.toBool() ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    if (id == QMetaType::Int || id == QMetaType::UInt || id == QMetaType::LongLong ||
        id == QMetaType::ULongLong || id == QMetaType::Double || id == QMetaType::Float) {
        return v.toString();
    }
    QString s = (id == QMetaType::QByteArray) ? QString::fromUtf8(v.toByteArray()) : v.toString();
    s.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + s + QLatin1Char('\'');
}

QString PgClient::quoteIdent(const QString &ident) const
{
    QStringList parts = ident.split(QLatin1Char('.'));
    for (QString &p : parts) {
        if (p.startsWith(QLatin1Char('"')))
            continue;
        p.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        p = QLatin1Char('"') + p + QLatin1Char('"');
    }
    return parts.join(QLatin1Char('.'));
}

bool PgClient::setCurrentDatabase(const QString &db, QString *err)
{
    // PostgreSQL 不能在会话内切换数据库，需要用新库名重连。
    if (db.isEmpty() || db == m_currentDb)
        return true;

    ConnectionProfile p = m_profile;
    p.database = db;
    close();
    if (!applyConnection(p, err))
        return false;
    m_currentDb = db;
    return true;
}
