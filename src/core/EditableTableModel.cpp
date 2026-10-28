#include "EditableTableModel.h"

#include "DbClient.h"
#include "Metadata.h"

#include <QBrush>
#include <QColor>
#include <QFont>

EditableTableModel::EditableTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void EditableTableModel::bind(DbClient *client, const QString &database,
                              const QString &schema, const QString &table)
{
    beginResetModel();
    m_client = client;
    m_database = database;
    m_schema = schema;
    m_table = table;
    m_columns.clear();
    m_rows.clear();
    m_nulls.clear();
    m_pkColumns.clear();
    m_total = 0;
    m_page = 0;
    endResetModel();
}

QString EditableTableModel::tableTitle() const
{
    return m_schema.isEmpty() ? m_table : m_schema + QLatin1Char('.') + m_table;
}

bool EditableTableModel::loadPage(const QString &where, const QString &orderBy,
                                  int page, int pageSize, QString *err)
{
    if (!m_client) {
        if (err) *err = tr("未绑定数据源");
        return false;
    }

    // 第一次加载时解析主键
    if (m_columns.isEmpty()) {
        const QList<ColumnInfo> cols =
            Metadata::columns(m_client, m_database, m_schema, m_table, err);
        if (cols.isEmpty() && err && !err->isEmpty())
            return false;
        for (const ColumnInfo &ci : cols) {
            m_columns << ci.name;
            if (ci.primaryKey)
                m_pkColumns << m_columns.size() - 1;
        }
    }

    m_where = where;
    m_orderBy = orderBy;
    m_pageSize = qMax(1, pageSize);
    m_page = qMax(0, page);

    DbResult cnt;
    if (!m_client->execute(
            Metadata::countQuerySql(m_client, m_database, m_schema, m_table, m_where), {},
            &cnt, err))
        return false;
    m_total = cnt.rows.isEmpty() ? 0 : cnt.rows.first().value(0).toLongLong();

    const int maxPage = qMax(0, int((m_total + m_pageSize - 1) / m_pageSize) - 1);
    if (m_page > maxPage)
        m_page = maxPage;

    DbResult res;
    const QString sql = Metadata::dataQuerySql(m_client, m_database, m_schema, m_table,
                                               m_where, m_orderBy,
                                               m_pageSize, m_page * m_pageSize);
    if (!m_client->execute(sql, {}, &res, err))
        return false;

    beginResetModel();
    m_rows = res.rows;
    m_nulls = res.nullFlags;
    // 若 SELECT * 列顺序与元数据不一致，以结果集列名为准
    if (!res.columns.isEmpty())
        m_columns = res.columns;
    endResetModel();
    return true;
}

bool EditableTableModel::reloadAfterMutation(QString *err)
{
    return loadPage(m_where, m_orderBy, m_page, m_pageSize, err);
}

QString EditableTableModel::whereClause(const QString &oldValue, bool isNull) const
{
    if (isNull)
        return QStringLiteral("IS NULL");
    return QStringLiteral("= %1").arg(m_client->quoteLiteral(oldValue));
}

bool EditableTableModel::submitEdit(int row, int column, const QString &newValue, QString *err)
{
    if (!m_client || row < 0 || row >= m_rows.size() || column < 0 || column >= m_columns.size())
        return false;
    if (m_pkColumns.isEmpty()) {
        if (err) *err = tr("该表没有主键，无法安全地编辑数据");
        return false;
    }

    QStringList conds;
    for (int pk : m_pkColumns) {
        const QString old = m_rows[row].value(pk);
        const bool oldNull = pk < m_nulls[row].size() && m_nulls[row].at(pk);
        conds << m_client->quoteIdent(m_columns[pk]) + QLatin1Char(' ') +
                      whereClause(old, oldNull);
    }

    const QString sql =
        QStringLiteral("UPDATE %1 SET %2=%3 WHERE %4")
            .arg(Metadata::qualifiedTable(m_client, m_database, m_schema, m_table),
                 m_client->quoteIdent(m_columns[column]),
                 m_client->quoteLiteral(newValue),
                 conds.join(QStringLiteral(" AND ")));
    if (!m_client->execute(sql, {}, nullptr, err))
        return false;
    return reloadAfterMutation(err);
}

bool EditableTableModel::insertEmptyRow(QString *err)
{
    if (!m_client)
        return false;
    const QString sql =
        m_client->type() == DbType::MySQL
            ? QStringLiteral("INSERT INTO %1 () VALUES ()")
                  .arg(Metadata::qualifiedTable(m_client, m_database, m_schema, m_table))
            : QStringLiteral("INSERT INTO %1 DEFAULT VALUES")
                  .arg(Metadata::qualifiedTable(m_client, m_database, m_schema, m_table));
    if (!m_client->execute(sql, {}, nullptr, err))
        return false;

    // 跳到最后一页显示新行
    if (m_total >= 0) {
        m_total += 1;
        m_page = int(m_total / m_pageSize);
    }
    return reloadAfterMutation(err);
}

bool EditableTableModel::deleteRows(const QList<int> &rows, QString *err)
{
    if (!m_client)
        return false;
    if (m_pkColumns.isEmpty()) {
        if (err) *err = tr("该表没有主键，无法安全地删除数据");
        return false;
    }
    for (int row : rows) {
        if (row < 0 || row >= m_rows.size())
            continue;
        QStringList conds;
        for (int pk : m_pkColumns) {
            const QString old = m_rows[row].value(pk);
            const bool oldNull = pk < m_nulls[row].size() && m_nulls[row].at(pk);
            conds << m_client->quoteIdent(m_columns[pk]) + QLatin1Char(' ') +
                          whereClause(old, oldNull);
        }
        const QString sql =
            QStringLiteral("DELETE FROM %1 WHERE %2")
                .arg(Metadata::qualifiedTable(m_client, m_database, m_schema, m_table),
                     conds.join(QStringLiteral(" AND ")));
        if (!m_client->execute(sql, {}, nullptr, err))
            return false;
    }
    if (m_page > 0 && m_rows.size() == rows.size())
        --m_page;
    return reloadAfterMutation(err);
}

bool EditableTableModel::setCellNull(int row, int column, QString *err)
{
    if (!m_client)
        return false;
    if (m_pkColumns.isEmpty()) {
        if (err) *err = tr("该表没有主键，无法安全地编辑数据");
        return false;
    }
    QStringList conds;
    for (int pk : m_pkColumns) {
        const QString old = m_rows[row].value(pk);
        const bool oldNull = pk < m_nulls[row].size() && m_nulls[row].at(pk);
        conds << m_client->quoteIdent(m_columns[pk]) + QLatin1Char(' ') +
                      whereClause(old, oldNull);
    }
    const QString sql =
        QStringLiteral("UPDATE %1 SET %2=NULL WHERE %3")
            .arg(Metadata::qualifiedTable(m_client, m_database, m_schema, m_table),
                 m_client->quoteIdent(m_columns[column]),
                 conds.join(QStringLiteral(" AND ")));
    if (!m_client->execute(sql, {}, nullptr, err))
        return false;
    return reloadAfterMutation(err);
}

int EditableTableModel::rowCount(const QModelIndex &) const
{
    return m_rows.size();
}

int EditableTableModel::columnCount(const QModelIndex &) const
{
    return m_columns.size();
}

QVariant EditableTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const int r = index.row();
    const int c = index.column();
    const bool isNull = c < m_nulls[r].size() && m_nulls[r].at(c);

    if (role == Qt::DisplayRole || role == Qt::EditRole)
        return isNull ? QString() : m_rows[r].value(c);

    if (role == Qt::ForegroundRole) {
        if (isNull)
            return QBrush(QColor("#9AA3B5"));
        return {};
    }
    if (role == Qt::FontRole && isNull) {
        QFont f;
        f.setItalic(true);
        return f;
    }
    if (role == Qt::ToolTipRole && isNull)
        return QStringLiteral("NULL");
    return {};
}

bool EditableTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid())
        return false;
    QString err;
    if (!submitEdit(index.row(), index.column(), value.toString(), &err)) {
        if (!err.isEmpty())
            emit editFailed(err);
        return false;
    }
    return true;
}

QVariant EditableTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < m_columns.size())
        return m_columns[section];
    return {};
}

Qt::ItemFlags EditableTableModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}
