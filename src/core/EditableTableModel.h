#pragma once

#include <QAbstractTableModel>
#include <QBitArray>
#include <QStringList>

class DbClient;

/// 通用可编辑数据模型：分页加载 + UPDATE/INSERT/DELETE（依赖主键）
class EditableTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit EditableTableModel(QObject *parent = nullptr);

    void bind(DbClient *client, const QString &database,
              const QString &schema, const QString &table);

    bool loadPage(const QString &where, const QString &orderBy,
                  int page, int pageSize, QString *err);
    bool submitEdit(int row, int column, const QString &newValue, QString *err);
    bool insertEmptyRow(QString *err);
    bool deleteRows(const QList<int> &rows, QString *err);
    bool setCellNull(int row, int column, QString *err);

    int rowCount(const QModelIndex & = {}) const override;
    int columnCount(const QModelIndex & = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &) const override;

signals:
    void editFailed(const QString &reason);

public:
    qint64 totalCount() const { return m_total; }
    int page() const { return m_page; }
    int pageSize() const { return m_pageSize; }
    bool hasPrimaryKey() const { return !m_pkColumns.isEmpty(); }
    QString tableTitle() const;

private:
    QString whereClause(const QString &oldValue, bool isNull) const;
    bool reloadAfterMutation(QString *err);

    DbClient *m_client = nullptr;
    QString m_database;
    QString m_schema;
    QString m_table;

    QStringList m_columns;
    QList<QStringList> m_rows;
    QList<QBitArray> m_nulls;
    QList<int> m_pkColumns;

    QString m_where;
    QString m_orderBy;
    int m_page = 0;
    int m_pageSize = 100;
    qint64 m_total = 0;
};
