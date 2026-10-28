#pragma once

#include "DbClient.h"

#include <QStringList>

struct TableInfo
{
    QString schema;
    QString name;
};

struct ColumnInfo
{
    QString name;
    QString baseType;          // 如 varchar / int
    QString fullType;          // 如 varchar(50) / int(11) unsigned
    int length = 0;
    bool nullable = true;
    bool primaryKey = false;
    bool autoIncrement = false;
    bool unique = false;
    QString defaultValue;
    QString comment;
};

struct ColumnSpec
{
    QString name;
    QString type;              // 基础类型，如 VARCHAR / integer / DECIMAL / ENUM
    int length = 0;            // varchar/char 长度；DECIMAL 总位数；0 表示忽略
    int scale = 0;             // DECIMAL 小数位数
    QString enumValues;        // ENUM/SET 候选值，逗号分隔，如 "a,b,c"
    bool primaryKey = false;
    bool autoIncrement = false;
    bool notNull = false;
    bool unique = false;
    bool index = false;        // 单独索引（非主键/唯一）
    QString fkTable;           // 外键目标表（空表示无 FK）
    QString fkColumn;          // 外键目标列
    QString defaultValue;      // 留空表示无默认值
    QString comment;           // 仅 MySQL
};

struct TableSpec
{
    QString schema;            // PostgreSQL 使用
    QString name;
    QList<ColumnSpec> columns;
};

/// 数据库方言/元数据访问（MySQL 与 PostgreSQL）
class Metadata
{
public:
    static QStringList typeList(DbType type);
    static QStringList charsetList();
    static QStringList collationList(const QString &charset);

    static QStringList databases(DbClient *c, QString *err);
    static QList<TableInfo> tables(DbClient *c, const QString &database, QString *err);
    static QList<ColumnInfo> columns(DbClient *c, const QString &database,
                                     const QString &schema, const QString &table,
                                     QString *err);
    static qint64 tableRowCount(DbClient *c, const QString &database,
                                const QString &schema, const QString &table,
                                QString *err);

    static QString qualifiedTable(DbClient *c, const QString &database,
                                  const QString &schema, const QString &table);
    static QString dataQuerySql(DbClient *c, const QString &database,
                                const QString &schema, const QString &table,
                                const QString &where, const QString &orderBy,
                                int limit, int offset);
    static QString countQuerySql(DbClient *c, const QString &database,
                                 const QString &schema, const QString &table,
                                 const QString &where);

    static QString createDatabaseSql(DbType type, const QString &name,
                                     const QString &charset, const QString &collate,
                                     const QString &owner);
    static QString dropDatabaseSql(DbType type, const QString &name);
    static QString createTableSql(DbType type, const QString &database, const TableSpec &spec);
    /// 设计表（修改字段）：基于新旧字段名匹配，生成一条或多条 ALTER 语句。
    /// 不处理主键结构调整与自增属性迁移。
    static QStringList alterTableSqls(DbType type, const QString &database,
                                      const QString &schema, const QString &table,
                                      const QList<ColumnInfo> &oldColumns,
                                      const TableSpec &spec);
    static QString dropTableSql(DbType type, const QString &database,
                                const QString &schema, const QString &table);
};
