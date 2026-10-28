#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>

class QTextDocument;

/// SQL 编辑器语法高亮
class SqlHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit SqlHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<Rule> m_rules;
    QTextCharFormat m_commentFormat;
    QRegularExpression m_commentStart;
    QRegularExpression m_commentEnd;
};
