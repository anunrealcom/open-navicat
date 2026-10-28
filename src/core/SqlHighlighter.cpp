#include "SqlHighlighter.h"

#include <QBrush>
#include <QColor>

SqlHighlighter::SqlHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    auto makeRule = [this](const QString &pattern, const QColor &color, bool bold = false) {
        Rule r;
        r.pattern = QRegularExpression(pattern);
        r.format.setForeground(QBrush(color));
        if (bold)
            r.format.setFontWeight(QFont::Bold);
        m_rules << r;
    };

    // 关键字（MySQL/PostgreSQL 常用集合）
    static const QStringList keywords = {
        QStringLiteral("ADD"), QStringLiteral("ALL"), QStringLiteral("ALTER"), QStringLiteral("AND"),
        QStringLiteral("AS"), QStringLiteral("ASC"), QStringLiteral("AUTO_INCREMENT"),
        QStringLiteral("BEGIN"), QStringLiteral("BETWEEN"), QStringLiteral("BY"),
        QStringLiteral("CASE"), QStringLiteral("CHECK"), QStringLiteral("COLUMN"),
        QStringLiteral("COMMENT"), QStringLiteral("COMMIT"), QStringLiteral("CONSTRAINT"),
        QStringLiteral("CREATE"), QStringLiteral("CROSS"), QStringLiteral("DATABASE"),
        QStringLiteral("DEFAULT"), QStringLiteral("DELETE"), QStringLiteral("DESC"),
        QStringLiteral("DISTINCT"), QStringLiteral("DROP"), QStringLiteral("ELSE"),
        QStringLiteral("END"), QStringLiteral("ESCAPE"), QStringLiteral("EXISTS"),
        QStringLiteral("FOREIGN"), QStringLiteral("FROM"), QStringLiteral("GROUP"),
        QStringLiteral("HAVING"), QStringLiteral("IF"), QStringLiteral("IN"), QStringLiteral("INDEX"),
        QStringLiteral("INNER"), QStringLiteral("INSERT"), QStringLiteral("INTO"), QStringLiteral("IS"),
        QStringLiteral("JOIN"), QStringLiteral("KEY"), QStringLiteral("LEFT"), QStringLiteral("LIKE"),
        QStringLiteral("LIMIT"), QStringLiteral("NOT"), QStringLiteral("NULL"),
        QStringLiteral("OFFSET"), QStringLiteral("ON"), QStringLiteral("OR"), QStringLiteral("ORDER"),
        QStringLiteral("OUTER"), QStringLiteral("PRIMARY"), QStringLiteral("REFERENCES"),
        QStringLiteral("RETURNING"), QStringLiteral("RIGHT"), QStringLiteral("ROLLBACK"),
        QStringLiteral("SELECT"), QStringLiteral("SET"), QStringLiteral("TABLE"),
        QStringLiteral("THEN"), QStringLiteral("TRUNCATE"), QStringLiteral("UNION"),
        QStringLiteral("UNIQUE"), QStringLiteral("UPDATE"), QStringLiteral("USE"),
        QStringLiteral("VALUES"), QStringLiteral("VIEW"), QStringLiteral("WHEN"),
        QStringLiteral("WHERE"), QStringLiteral("WITH"), QStringLiteral("PRIMARY KEY"),
        QStringLiteral("GENERATED"), QStringLiteral("IDENTITY"), QStringLiteral("ENGINE"),
        QStringLiteral("VARCHAR"), QStringLiteral("SHOW"), QStringLiteral("DATABASES"),
        QStringLiteral("TABLES"), QStringLiteral("COLUMNS"), QStringLiteral("FULL")
    };
    for (const QString &kw : keywords)
        makeRule(QStringLiteral("\\b%1\\b").arg(kw), QColor("#1669D6"), true);

    makeRule(QStringLiteral("--[^\n]*"), QColor("#8A93A6"));               // 行注释
    makeRule(QStringLiteral("#[^\n]*"), QColor("#8A93A6"));                 // MySQL 行注释
    makeRule(QStringLiteral("'[^']*'"), QColor("#C2410C"));                 // 字符串
    makeRule(QStringLiteral("\"[^\"]*\""), QColor("#0F766E"));              // 标识符
    makeRule(QStringLiteral("`[^`]*`"), QColor("#0F766E"));
    makeRule(QStringLiteral("\\b\\d+(\\.\\d+)?\\b"), QColor("#7C3AED"));    // 数字
    makeRule(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()"),
             QColor("#0E7490"));                                            // 函数名

    m_commentFormat.setForeground(QBrush(QColor("#8A93A6")));
    m_commentStart = QRegularExpression(QStringLiteral("/\\*"));
    m_commentEnd = QRegularExpression(QStringLiteral("\\*/"));
}

void SqlHighlighter::highlightBlock(const QString &text)
{
    for (const Rule &r : std::as_const(m_rules)) {
        auto it = r.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), r.format);
        }
    }

    // 块注释
    setCurrentBlockState(0);
    int start = 0;
    if (previousBlockState() != 1)
        start = text.indexOf(m_commentStart);
    while (start >= 0) {
        const auto end = m_commentEnd.match(text, start + 2);
        int length;
        if (!end.hasMatch()) {
            setCurrentBlockState(1);
            length = text.length() - start;
        } else {
            length = end.capturedStart() - start + 2;
        }
        setFormat(start, length, m_commentFormat);
        start = text.indexOf(m_commentStart, start + length);
    }
}
