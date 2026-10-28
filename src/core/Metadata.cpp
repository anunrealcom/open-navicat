#include "Metadata.h"

#include <QRegularExpression>
#include <QSet>

QStringList Metadata::typeList(DbType type)
{
    if (type == DbType::SQLite)
        return {QStringLiteral("INTEGER"), QStringLiteral("REAL"), QStringLiteral("TEXT"),
                QStringLiteral("BLOB"), QStringLiteral("NUMERIC"), QStringLiteral("BOOLEAN"),
                QStringLiteral("DATE"), QStringLiteral("DATETIME"), QStringLiteral("VARCHAR"),
                QStringLiteral("CHAR"), QStringLiteral("INT"), QStringLiteral("FLOAT"),
                QStringLiteral("DOUBLE"), QStringLiteral("DECIMAL")};
    if (type == DbType::PostgreSQL)
        return {QStringLiteral("smallint"), QStringLiteral("integer"), QStringLiteral("bigint"),
                QStringLiteral("numeric"), QStringLiteral("real"),
                QStringLiteral("double precision"), QStringLiteral("boolean"),
                QStringLiteral("varchar"), QStringLiteral("char"), QStringLiteral("text"),
                QStringLiteral("date"), QStringLiteral("time"), QStringLiteral("timestamp"),
                QStringLiteral("timestamptz"), QStringLiteral("interval"),
                QStringLiteral("uuid"), QStringLiteral("json"), QStringLiteral("jsonb"),
                QStringLiteral("bytea")};
    return {QStringLiteral("INT"), QStringLiteral("BIGINT"), QStringLiteral("TINYINT"),
            QStringLiteral("SMALLINT"), QStringLiteral("MEDIUMINT"), QStringLiteral("DECIMAL"),
            QStringLiteral("FLOAT"), QStringLiteral("DOUBLE"), QStringLiteral("VARCHAR"),
            QStringLiteral("CHAR"), QStringLiteral("TEXT"), QStringLiteral("MEDIUMTEXT"),
            QStringLiteral("LONGTEXT"), QStringLiteral("DATE"), QStringLiteral("DATETIME"),
            QStringLiteral("TIMESTAMP"), QStringLiteral("TIME"), QStringLiteral("YEAR"),
            QStringLiteral("BLOB"), QStringLiteral("LONGBLOB"), QStringLiteral("JSON")};
}

QStringList Metadata::charsetList()
{
    return {QStringLiteral("utf8mb4"), QStringLiteral("utf8"), QStringLiteral("gbk"),
            QStringLiteral("latin1"), QStringLiteral("ascii")};
}

QStringList Metadata::collationList(const QString &charset)
{
    static const QHash<QString, QStringList> map = {
        {QStringLiteral("utf8mb4"),
         {QStringLiteral("utf8mb4_general_ci"), QStringLiteral("utf8mb4_unicode_ci"),
          QStringLiteral("utf8mb4_bin"), QStringLiteral("utf8mb4_0900_ai_ci")}},
        {QStringLiteral("utf8"),
         {QStringLiteral("utf8_general_ci"), QStringLiteral("utf8_unicode_ci"),
          QStringLiteral("utf8_bin")}},
        {QStringLiteral("gbk"),
         {QStringLiteral("gbk_chinese_ci"), QStringLiteral("gbk_bin")}},
        {QStringLiteral("latin1"),
         {QStringLiteral("latin1_swedish_ci"), QStringLiteral("latin1_bin")}},
        {QStringLiteral("ascii"),
         {QStringLiteral("ascii_general_ci"), QStringLiteral("ascii_bin")}},
    };
    return map.value(charset.toLower(), {QStringLiteral("default")});
}

QStringList Metadata::databases(DbClient *c, QString *err)
{
    QStringList result;
    DbResult r;
    QString sql;
    if (c->type() == DbType::PostgreSQL) {
        sql = QStringLiteral("SELECT datname FROM pg_database WHERE datallowconn ORDER BY 1");
    } else if (c->type() == DbType::SQLite) {
        // SQLite 只有一个"数据库"（就是连接的文件/内存库），直接返回当前名
        return {c->currentDatabase()};
    } else {
        sql = QStringLiteral("SHOW DATABASES");
    }

    if (!c->execute(sql, {}, &r, err))
        return result;
    for (const QStringList &row : std::as_const(r.rows))
        result << row.value(0);
    return result;
}

QList<TableInfo> Metadata::tables(DbClient *c, const QString &database, QString *err)
{
    QList<TableInfo> result;
    DbResult r;
    if (c->type() == DbType::SQLite) {
        const QString sql =
            QStringLiteral("SELECT name FROM sqlite_master "
                           "WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY 1");
        if (!c->execute(sql, {}, &r, err))
            return result;
        for (const QStringList &row : std::as_const(r.rows))
            result.append({QString(), row.value(0)});
    } else if (c->type() == DbType::PostgreSQL) {
        const QString sql =
            QStringLiteral("SELECT table_schema, table_name FROM information_schema.tables "
                           "WHERE table_type='BASE TABLE' "
                           "AND table_schema NOT IN ('pg_catalog','information_schema') "
                           "ORDER BY table_schema, table_name");
        if (!c->execute(sql, {}, &r, err))
            return result;
        for (const QStringList &row : std::as_const(r.rows))
            result.append({row.value(0), row.value(1)});
    } else {
        const QString sql =
            QStringLiteral("SELECT TABLE_NAME FROM information_schema.TABLES "
                           "WHERE TABLE_SCHEMA=? AND TABLE_TYPE='BASE TABLE' ORDER BY 1");
        if (!c->execute(sql, {database}, &r, err))
            return result;
        for (const QStringList &row : std::as_const(r.rows))
            result.append({QString(), row.value(0)});
    }
    return result;
}

QList<ColumnInfo> Metadata::columns(DbClient *c, const QString &database,
                                    const QString &schema, const QString &table, QString *err)
{
    QList<ColumnInfo> list;
    DbResult r;

    if (c->type() == DbType::SQLite) {
        // PRAGMA table_info(table) → cid, name, type, notnull, dflt_value, pk
        const QString sql = QStringLiteral("PRAGMA table_info(%1)")
                                .arg(c->quoteIdent(table));
        if (!c->execute(sql, {}, &r, err))
            return list;
        for (int ri = 0; ri < r.rowCount(); ++ri) {
            const QStringList &row = r.rows[ri];
            ColumnInfo ci;
            ci.name = row.value(1);
            ci.fullType = row.value(2);
            ci.nullable = row.value(3) == QLatin1String("0");
            ci.primaryKey = row.value(5) != QLatin1String("0");
            ci.defaultValue = r.isNull(ri, 4) ? QString() : row.value(4);
            static const QRegularExpression re(QStringLiteral("^([A-Za-z ]+)(?:\\((\\d+)[^)]*\\))?"));
            const QRegularExpressionMatch m = re.match(ci.fullType);
            ci.baseType = m.hasMatch() ? m.captured(1).trimmed().toUpper()
                                       : ci.fullType.toUpper();
            ci.length = m.hasMatch() ? m.captured(2).toInt() : 0;
            list << ci;
        }
    } else if (c->type() == DbType::MySQL) {
        const QString sql =
            QStringLiteral("SHOW FULL COLUMNS FROM %1")
                .arg(c->quoteIdent(database + QStringLiteral(".") + table));
        if (!c->execute(sql, {}, &r, err))
            return list;
        // Field Type Collation Null Key Default Extra Privileges Comment
        for (int ri = 0; ri < r.rowCount(); ++ri) {
            const QStringList &row = r.rows[ri];
            ColumnInfo ci;
            ci.name = row.value(0);
            ci.fullType = row.value(1);
            ci.nullable = row.value(3).compare(QStringLiteral("YES"), Qt::CaseInsensitive) == 0;
            ci.primaryKey = row.value(4).compare(QStringLiteral("PRI"), Qt::CaseInsensitive) == 0;
            ci.autoIncrement = row.value(6).contains(QStringLiteral("auto_increment"),
                                                     Qt::CaseInsensitive);
            ci.defaultValue = r.isNull(ri, 5) ? QString() : row.value(5);
            ci.comment = row.value(8);
            static const QRegularExpression re(QStringLiteral("^([A-Za-z ]+)(?:\\((\\d+)[^)]*\\))?"));
            const QRegularExpressionMatch m = re.match(ci.fullType);
            if (m.hasMatch()) {
                ci.baseType = m.captured(1).trimmed().toUpper();
                ci.length = m.captured(2).toInt();
            } else {
                ci.baseType = ci.fullType.toUpper();
            }
            list << ci;
        }
    } else {
        const QString sql =
            QStringLiteral(
                "SELECT a.attname, format_type(a.atttypid,a.atttypmod), a.attnotnull,"
                " pg_get_expr(d.adbin,d.adrelid),"
                " EXISTS(SELECT 1 FROM pg_index k WHERE k.indrelid=a.attrelid"
                " AND k.indisprimary AND a.attnum = ANY(k.indkey)) AS ispk,"
                " (pg_get_expr(d.adbin,d.adrelid) ILIKE '%nextval%'"
                "  OR pg_get_expr(d.adbin,d.adrelid) ILIKE '%identity%') AS isserial "
                "FROM pg_attribute a "
                "JOIN pg_class c ON c.oid=a.attrelid "
                "JOIN pg_namespace n ON n.oid=c.relnamespace "
                "LEFT JOIN pg_attrdef d ON d.adrelid=a.attrelid AND d.adnum=a.attnum "
                "WHERE n.nspname=? AND c.relname=?"
                " AND a.attnum>0 AND NOT a.attisdropped ORDER BY a.attnum");
        if (!c->execute(sql, {schema.isEmpty() ? QStringLiteral("public") : schema, table},
                        &r, err))
            return list;
        for (const QStringList &row : std::as_const(r.rows)) {
            ColumnInfo ci;
            ci.name = row.value(0);
            ci.fullType = row.value(1);
            ci.nullable = row.value(2) != QStringLiteral("true");
            ci.primaryKey = row.value(4) == QStringLiteral("true");
            ci.autoIncrement = row.value(5) == QStringLiteral("true");
            ci.defaultValue = row.value(3);
            static const QRegularExpression re(QStringLiteral("^([A-Za-z ]+)(?:\\((\\d+)[^)]*\\))?"));
            const QRegularExpressionMatch m = re.match(ci.fullType);
            ci.baseType = m.hasMatch() ? m.captured(1).trimmed().toLower()
                                       : ci.fullType.toLower();
            ci.length = m.hasMatch() ? m.captured(2).toInt() : 0;
            list << ci;
        }
    }
    return list;
}

qint64 Metadata::tableRowCount(DbClient *c, const QString &database,
                               const QString &schema, const QString &table, QString *err)
{
    DbResult r;
    if (!c->execute(countQuerySql(c, database, schema, table, {}), {}, &r, err))
        return -1;
    if (r.rowCount() > 0)
        return r.rows.first().value(0).toLongLong();
    return 0;
}

QString Metadata::qualifiedTable(DbClient *c, const QString &database,
                                 const QString &schema, const QString &table)
{
    if (c->type() == DbType::PostgreSQL) {
        const QString s = schema.isEmpty() ? QStringLiteral("public") : schema;
        return c->quoteIdent(s) + QLatin1Char('.') + c->quoteIdent(table);
    }
    return c->quoteIdent(database) + QLatin1Char('.') + c->quoteIdent(table);
}

QString Metadata::dataQuerySql(DbClient *c, const QString &database,
                               const QString &schema, const QString &table,
                               const QString &where, const QString &orderBy,
                               int limit, int offset)
{
    QString sql = QStringLiteral("SELECT * FROM %1")
                      .arg(qualifiedTable(c, database, schema, table));
    const QString w = where.trimmed();
    if (!w.isEmpty())
        sql += QStringLiteral(" WHERE ") + w;
    const QString ob = orderBy.trimmed();
    if (!ob.isEmpty())
        sql += QStringLiteral(" ORDER BY ") + ob;
    sql += QStringLiteral(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);
    return sql;
}

QString Metadata::countQuerySql(DbClient *c, const QString &database,
                                const QString &schema, const QString &table,
                                const QString &where)
{
    QString sql = QStringLiteral("SELECT COUNT(*) FROM %1")
                      .arg(qualifiedTable(c, database, schema, table));
    const QString w = where.trimmed();
    if (!w.isEmpty())
        sql += QStringLiteral(" WHERE ") + w;
    return sql;
}

QString Metadata::createDatabaseSql(DbType type, const QString &name,
                                    const QString &charset, const QString &collate,
                                    const QString &owner)
{
    if (type == DbType::PostgreSQL) {
        // name / owner 均为调用方 quoteIdent 后的标识符
        QString sql = QStringLiteral("CREATE DATABASE %1 ENCODING 'UTF8'").arg(name);
        if (!owner.trimmed().isEmpty())
            sql += QStringLiteral(" OWNER %1").arg(owner);
        return sql;
    }
    QString sql = QStringLiteral("CREATE DATABASE %1").arg(name);
    if (!charset.isEmpty() && charset != QStringLiteral("default"))
        sql += QStringLiteral(" DEFAULT CHARACTER SET ") + charset;
    if (!collate.isEmpty() && collate != QStringLiteral("default"))
        sql += QStringLiteral(" COLLATE ") + collate;
    return sql;
}

QString Metadata::dropDatabaseSql(DbType type, const QString &name)
{
    Q_UNUSED(type)
    // name 为调用方 quoteIdent 后的标识符
    return QStringLiteral("DROP DATABASE %1").arg(name);
}

static bool isNumericType(const QString &t)
{
    static const QSet<QString> nums = {
        QStringLiteral("INT"), QStringLiteral("BIGINT"), QStringLiteral("TINYINT"),
        QStringLiteral("SMALLINT"), QStringLiteral("MEDIUMINT"), QStringLiteral("DECIMAL"),
        QStringLiteral("FLOAT"), QStringLiteral("DOUBLE"), QStringLiteral("YEAR"),
        QStringLiteral("smallint"), QStringLiteral("integer"), QStringLiteral("bigint"),
        QStringLiteral("numeric"), QStringLiteral("real"), QStringLiteral("double precision"),
        QStringLiteral("boolean")};
    return nums.contains(t.trimmed());
}

static QString formatDefault(DbType type, const ColumnSpec &col)
{
    const QString d = col.defaultValue.trimmed();
    if (d.isEmpty())
        return {};
    if (isNumericType(col.type))
        return d;
    // 函数调用 / 关键字保持原样
    static const QSet<QString> keywords = {
        QStringLiteral("CURRENT_TIMESTAMP"), QStringLiteral("NOW()"),
        QStringLiteral("CURRENT_DATE"), QStringLiteral("CURRENT_TIME"),
        QStringLiteral("NULL"), QStringLiteral("TRUE"), QStringLiteral("FALSE"),
        QStringLiteral("gen_random_uuid()"), QStringLiteral("uuid_generate_v4()")};
    bool keywordMatched = false;
    for (const QString &k : keywords) {
        if (k.compare(d, Qt::CaseInsensitive) == 0) {
            keywordMatched = true;
            break;
        }
    }
    if (d.contains(QLatin1Char('(')) || keywordMatched)
        return d;
    return type == DbType::PostgreSQL
               ? QStringLiteral("'") + QString(d).replace(QLatin1Char('\''), QStringLiteral("''")) +
                     QStringLiteral("'")
               : QStringLiteral("'") + QString(d).replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
                     .replace(QLatin1Char('\''), QStringLiteral("\\'")) + QStringLiteral("'");
}

static QString columnType(DbType type, const ColumnSpec &col)
{
    QString t = col.type.trimmed();
    const bool upper = (type == DbType::MySQL);

    auto enc = [&t, upper] { if (upper) t = t.toUpper(); };
    enc();

    // ENUM / SET
    const QString tu = t.toUpper();
    if (tu == QStringLiteral("ENUM") || tu == QStringLiteral("SET")) {
        if (!col.enumValues.trimmed().isEmpty()) {
            QStringList parts = col.enumValues.split(QLatin1Char(','), Qt::SkipEmptyParts);
            QStringList quoted;
            for (QString p : parts)
                quoted << QStringLiteral("'%1'").arg(p.trimmed().replace(QLatin1Char('\\'),
                                                                         QStringLiteral("\\\\"))
                                                    .replace(QLatin1Char('\''), QStringLiteral("\\'")));
            t += QStringLiteral("(%1)").arg(quoted.join(QStringLiteral(",")));
        }
        return t;
    }

    // VARCHAR / CHAR 长度
    const bool hasLen = (tu == QStringLiteral("VARCHAR") || tu == QStringLiteral("CHAR")) &&
                        col.length > 0;
    if (hasLen)
        t += QStringLiteral("(%1)").arg(col.length);

    // DECIMAL / NUMERIC 精度
    else if (tu == QStringLiteral("DECIMAL") || tu == QStringLiteral("NUMERIC")) {
        const int m = col.length > 0 ? col.length : 10;
        const int d = col.scale > 0 ? col.scale : 0;
        t += QStringLiteral("(%1,%2)").arg(m).arg(d);
    }

    return t;
}

static QString quoteBacktick(const QString &s)
{
    return QStringLiteral("`") + QString(s).replace(QLatin1Char('`'), QStringLiteral("``")) +
           QStringLiteral("`");
}

static QString quoteDQuote(const QString &s)
{
    return QStringLiteral("\"") + QString(s).replace(QLatin1Char('"'), QStringLiteral("\"\"")) +
           QStringLiteral("\"");
}

static QString buildColumnDef(DbType type, const ColumnSpec &col, bool inlinePk)
{
    QStringList pieces;
    pieces << (type == DbType::MySQL ? quoteBacktick(col.name) : quoteDQuote(col.name));
    pieces << columnType(type, col);

    if (type == DbType::PostgreSQL && col.autoIncrement && col.primaryKey)
        pieces << QStringLiteral("GENERATED BY DEFAULT AS IDENTITY");
    if (col.notNull || col.primaryKey)
        pieces << QStringLiteral("NOT NULL");
    if (type == DbType::MySQL && col.autoIncrement)
        pieces << QStringLiteral("AUTO_INCREMENT");
    if (inlinePk && col.primaryKey)
        pieces << QStringLiteral("PRIMARY KEY");
    if (col.unique)
        pieces << QStringLiteral("UNIQUE");
    const QString def = formatDefault(type, col);
    if (!def.isEmpty())
        pieces << QStringLiteral("DEFAULT ") + def;
    if (type == DbType::MySQL && !col.comment.isEmpty())
        pieces << QStringLiteral("COMMENT '%1'")
                      .arg(QString(col.comment).replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
                               .replace(QLatin1Char('\''), QStringLiteral("\\'")));
    return pieces.join(QLatin1Char(' '));
}

QString Metadata::createTableSql(DbType type, const QString &database, const TableSpec &spec)
{
    QStringList defs;
    QStringList pkCols;
    QStringList indexes;    // 表级 INDEX
    QStringList fks;        // 表级 FOREIGN KEY
    int validCount = 0;
    for (const ColumnSpec &col : spec.columns)
        if (!col.name.trimmed().isEmpty())
            ++validCount;

    const auto q = [type](const QString &s) {
        return type == DbType::MySQL ? quoteBacktick(s) : quoteDQuote(s);
    };

    for (const ColumnSpec &col : spec.columns) {
        if (col.name.trimmed().isEmpty())
            continue;
        defs << buildColumnDef(type, col, col.primaryKey && validCount == 1);
        if (col.primaryKey && validCount > 1)
            pkCols << q(col.name);
        if (col.index && !col.primaryKey && !col.unique)
            indexes << QStringLiteral("INDEX %1 (%2)")
                        .arg(q(QStringLiteral("idx_%1").arg(col.name)), q(col.name));
        if (!col.fkTable.trimmed().isEmpty() && !col.fkColumn.trimmed().isEmpty()) {
            fks << QStringLiteral("FOREIGN KEY (%1) REFERENCES %2(%3)")
                       .arg(q(col.name), q(col.fkTable), q(col.fkColumn));
        }
    }
    if (!pkCols.isEmpty())
        defs << QStringLiteral("PRIMARY KEY (%1)").arg(pkCols.join(QStringLiteral(", ")));
    defs << indexes;
    defs << fks;

    QString head;
    if (type == DbType::PostgreSQL) {
        const QString schema = spec.schema.isEmpty() ? QStringLiteral("public") : spec.schema;
        head = QStringLiteral("CREATE TABLE %1.%2").arg(quoteDQuote(schema),
                                                        quoteDQuote(spec.name));
    } else {
        head = QStringLiteral("CREATE TABLE %1.%2")
                   .arg(database, quoteBacktick(spec.name));
    }
    QString sql = head + QStringLiteral(" (\n  ") + defs.join(QStringLiteral(",\n  ")) +
                  QStringLiteral("\n)");
    if (type == DbType::MySQL)
        sql += QStringLiteral(" ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
    return sql;
}

static QString pgAlterColumn(const QString &table, const QString &oldName, const ColumnSpec &col,
                             const ColumnInfo *old)
{
    QStringList stmts;
    if (!old || old->name != col.name)
        stmts << QStringLiteral("ALTER TABLE %1 RENAME COLUMN %2 TO %3")
                     .arg(table, quoteDQuote(old ? old->name : col.name), quoteDQuote(col.name));
    const QString quoted = quoteDQuote(col.name);
    if (!old || old->fullType.compare(columnType(DbType::PostgreSQL, col),
                                      Qt::CaseInsensitive) != 0) {
        stmts << QStringLiteral("ALTER TABLE %1 ALTER COLUMN %2 TYPE %3 USING %2::%3")
                     .arg(table, quoted, columnType(DbType::PostgreSQL, col));
    }
    const bool needNotNull = col.notNull || col.primaryKey;
    if (!old || old->nullable == needNotNull) {
        // 一致，跳过
    } else if (needNotNull) {
        stmts << QStringLiteral("ALTER TABLE %1 ALTER COLUMN %2 SET NOT NULL").arg(table, quoted);
    } else {
        stmts << QStringLiteral("ALTER TABLE %1 ALTER COLUMN %2 DROP NOT NULL").arg(table, quoted);
    }
    const QString def = formatDefault(DbType::PostgreSQL, col);
    const QString oldDef = old ? old->defaultValue : QString();
    if (def != oldDef) {
        if (def.isEmpty())
            stmts << QStringLiteral("ALTER TABLE %1 ALTER COLUMN %2 DROP DEFAULT").arg(table, quoted);
        else
            stmts << QStringLiteral("ALTER TABLE %1 ALTER COLUMN %2 SET DEFAULT %3")
                         .arg(table, quoted, def);
    }
    return stmts.join(QStringLiteral(";\n"));
}

QStringList Metadata::alterTableSqls(DbType type, const QString &database,
                                     const QString &schema, const QString &table,
                                     const QList<ColumnInfo> &oldColumns,
                                     const TableSpec &spec)
{
    QStringList stmts;
    const QString tableRef = type == DbType::PostgreSQL
                                 ? quoteDQuote(schema.isEmpty() ? QStringLiteral("public")
                                                               : schema) +
                                       QStringLiteral(".") + quoteDQuote(table)
                                 : quoteBacktick(database) + QStringLiteral(".") +
                                       quoteBacktick(table);

    QSet<QString> newNames;
    for (const ColumnSpec &c : spec.columns)
        if (!c.name.trimmed().isEmpty())
            newNames.insert(c.name);

    // 删除字段
    for (const ColumnInfo &old : oldColumns) {
        if (!newNames.contains(old.name))
            stmts << QStringLiteral("ALTER TABLE %1 DROP COLUMN %2")
                         .arg(tableRef, type == DbType::MySQL ? quoteBacktick(old.name)
                                                              : quoteDQuote(old.name));
    }

    // 新增 / 修改字段
    for (const ColumnSpec &col : spec.columns) {
        if (col.name.trimmed().isEmpty())
            continue;
        const ColumnInfo *match = nullptr;
        for (const ColumnInfo &old : oldColumns) {
            if (old.name == col.name) {
                match = &old;
                break;
            }
        }
        if (!match) {
            stmts << QStringLiteral("ALTER TABLE %1 ADD COLUMN %2")
                         .arg(tableRef, buildColumnDef(type, col, false));
            continue;
        }
        if (type == DbType::MySQL) {
            // CHANGE 一次完成重命名/类型/约束调整
            stmts << QStringLiteral("ALTER TABLE %1 CHANGE COLUMN %2 %3")
                         .arg(tableRef, quoteBacktick(match->name),
                              buildColumnDef(type, col, false));
        } else {
            const QString s = pgAlterColumn(tableRef, match->name, col, match);
            if (!s.isEmpty())
                stmts << s;
        }
    }
    return stmts;
}

QString Metadata::dropTableSql(DbType type, const QString &database,
                               const QString &schema, const QString &table)
{
    Q_UNUSED(database)
    if (type == DbType::PostgreSQL)
        return QStringLiteral("DROP TABLE IF EXISTS %1.%2")
            .arg(QStringLiteral("\"%1\"").arg(schema.isEmpty() ? QStringLiteral("public") : schema),
                 QStringLiteral("\"%1\"").arg(table));
    return QStringLiteral("DROP TABLE IF EXISTS `%1`.`%2`").arg(database, table);
}
