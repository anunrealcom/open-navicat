#pragma once

#include "FramelessDialog.h"
#include "core/ConnectionProfile.h"
#include "core/Metadata.h"

class QLineEdit;
class QTableWidget;

class TableDesignerDialog : public FramelessDialog
{
    Q_OBJECT
public:
    TableDesignerDialog(DbType type, const QString &database, const QString &schema,
                        QWidget *parent = nullptr);

    /// 设计模式：载入已有表结构
    void loadColumns(const QList<ColumnInfo> &columns);
    void setTableName(const QString &name);

    TableSpec tableSpec() const;
    QString tableName() const;

private slots:
    void addColumn();
    void removeColumn();

private:
    void setupRow(int row, const ColumnInfo *ci = nullptr);

    DbType m_type;
    QString m_database;
    QString m_schema;
    QLineEdit *m_tableName = nullptr;
    QTableWidget *m_grid = nullptr;
    int m_colCount = 0;
};
