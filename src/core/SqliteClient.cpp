#include "SqliteClient.h"

#include <QElapsedTimer>
#include <QMetaType>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QUuid>

SqliteClient::SqliteClient(QObject *parent)
    : DbClient()
{
    Q_UNUSED(parent);
    m_connName = QStringLiteral("sqlite-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

SqliteClient::~SqliteClient()
{
    close();
    if (QSqlDatabase::contains(m_connName))
        QSqlDatabase::removeDatabase(m_connName);
}

bool SqliteClient::applyConnection(const ConnectionProfile &profile, QString *err)
{
    m_lastErrno = 0;
    m_lastSqlstate.clear();
    if (!QSqlDatabase::drivers().contains(QStringLiteral("QSQLITE"))) {
        m_lastError = tr("当前 Qt 未包含 QSQLITE 驱动。");
        if (err) *err = formatLastError();
        return false;
    }

    if (QSqlDatabase::contains(m_connName))
        QSqlDatabase::removeDatabase(m_connName);

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connName);
    // SQLite 连接：database 字段存文件路径，host 留空
    db.setDatabaseName(profile.database.isEmpty() ? QLatin1String(":memory:")
                                                  : profile.database);

    if (!db.open()) {
        const QSqlError &e = db.lastError();
        m_lastError = e.text();
        if (err) *err = formatLastError();
        return false;
    }
    m_lastErrno = 0;
    m_lastSqlstate.clear();
    return true;
}

bool SqliteClient::open(const ConnectionProfile &profile, QString *err)
{
    if (!applyConnection(profile, err))
        return false;

    m_profile = profile;
    m_currentDb = profile.database.isEmpty() ? QLatin1String(":memory:") : profile.database;
    return true;
}

void SqliteClient::close()
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

bool SqliteClient::isOpen() const
{
    return QSqlDatabase::contains(m_connName) &&
           QSqlDatabase::database(m_connName, false).isOpen();
}

bool SqliteClient::execute(const QString &sql, const QVariantList &params,
                           DbResult *result, QString *err)
{
    if (!QSqlDatabase::contains(m_connName)) {
        m_lastError = tr("连接未打开");
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
            m_lastError = q.lastError().text();
            if (err) *err = formatLastError();
            return false;
        }
        for (const QVariant &v : params)
            q.addBindValue(v);
        ok = q.exec();
    }

    if (!ok) {
        m_lastError = q.lastError().text();
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

QString SqliteClient::quoteLiteral(const QVariant &v) const
{
    if (!v.isValid() || v.isNull())
        return QStringLiteral("NULL");

    const int id = v.userType();
    if (id == QMetaType::Bool)
        return v.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    if (id == QMetaType::Int || id == QMetaType::UInt || id == QMetaType::LongLong ||
        id == QMetaType::ULongLong || id == QMetaType::Double || id == QMetaType::Float) {
        return v.toString();
    }
    QString s = (id == QMetaType::QByteArray) ? QString::fromUtf8(v.toByteArray()) : v.toString();
    s.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + s + QLatin1Char('\'');
}

QString SqliteClient::quoteIdent(const QString &ident) const
{
    // SQLite 用双引号，但也允许方括号
    QStringList parts = ident.split(QLatin1Char('.'));
    for (QString &p : parts) {
        if (p.startsWith(QLatin1Char('"')))
            continue;
        p.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        p = QLatin1Char('"') + p + QLatin1Char('"');
    }
    return parts.join(QLatin1Char('.'));
}

bool SqliteClient::setCurrentDatabase(const QString &db, QString *err)
{
    if (db.isEmpty() || db == m_currentDb)
        return true;

    ConnectionProfile p = m_profile;
    p.database = db;
    close();
    if (!applyConnection(p, err))
        return false;
    m_currentDb = db;
    m_profile = p;
    return true;
}

QString SqliteClient::currentDatabase() const
{
    return m_currentDb;
}
