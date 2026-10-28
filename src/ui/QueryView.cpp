#include "QueryView.h"

#include "core/ConnectionStore.h"
#include "ErDialog.h"
#include "IconManager.h"
#include "core/DbClient.h"
#include "core/SqlHighlighter.h"

#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTableView>
#include <QTextDocument>
#include <QVBoxLayout>

QueryView::QueryView(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 10);
    root->setSpacing(10);

    auto *bar = new QHBoxLayout;
    bar->setSpacing(8);
    auto *runBtn = new QPushButton(IconManager::icon(QStringLiteral("play"), QColor("white"),
                                                     QSize(14, 14)),
                                   tr("运行 (F5)"));
    runBtn->setObjectName(QStringLiteral("btnPrimary"));
    runBtn->setMinimumHeight(32);
    runBtn->setCursor(Qt::PointingHandCursor);
    connect(runBtn, &QPushButton::clicked, this, &QueryView::runQuery);

    auto *fmtBtn = new QPushButton(IconManager::icon(QStringLiteral("edit"), QColor("#5A6478"),
                                                     QSize(14, 14)),
                                   tr("格式化"));
    fmtBtn->setObjectName(QStringLiteral("btnGhost"));
    fmtBtn->setMinimumHeight(32);
    fmtBtn->setCursor(Qt::PointingHandCursor);
    connect(fmtBtn, &QPushButton::clicked, this, &QueryView::formatSql);

    auto *explainBtn = new QPushButton(IconManager::icon(QStringLiteral("terminal"), QColor("#5A6478"),
                                                         QSize(14, 14)),
                                       tr("Explain"));
    explainBtn->setObjectName(QStringLiteral("btnGhost"));
    explainBtn->setMinimumHeight(32);
    explainBtn->setCursor(Qt::PointingHandCursor);
    connect(explainBtn, &QPushButton::clicked, this, &QueryView::runExplain);

    auto *procBtn = new QPushButton(IconManager::icon(QStringLiteral("plug"), QColor("#5A6478"),
                                                      QSize(14, 14)),
                                    tr("进程"));
    procBtn->setObjectName(QStringLiteral("btnGhost"));
    procBtn->setMinimumHeight(32);
    procBtn->setCursor(Qt::PointingHandCursor);
    connect(procBtn, &QPushButton::clicked, this, &QueryView::showProcessList);

    auto *erBtn = new QPushButton(IconManager::icon(QStringLiteral("table"), QColor("#5A6478"),
                                                    QSize(14, 14)),
                                  tr("ER图"));
    erBtn->setObjectName(QStringLiteral("btnGhost"));
    erBtn->setMinimumHeight(32);
    erBtn->setCursor(Qt::PointingHandCursor);
    connect(erBtn, &QPushButton::clicked, this, &QueryView::showErDiagram);

    auto *histBtn = new QPushButton(IconManager::icon(QStringLiteral("search"), QColor("#5A6478"),
                                                      QSize(14, 14)),
                                    tr("历史"));
    histBtn->setObjectName(QStringLiteral("btnGhost"));
    histBtn->setMinimumHeight(32);
    histBtn->setCursor(Qt::PointingHandCursor);
    histBtn->setCheckable(true);
    connect(histBtn, &QPushButton::clicked, this, [this, histBtn] {
        m_historyPanel->setVisible(histBtn->isChecked());
        if (histBtn->isChecked())
            loadHistory();
    });

    m_dbLabel = new QLabel;
    m_dbLabel->setObjectName(QStringLiteral("queryDbLabel"));
    m_message = new QLabel;
    m_message->setStyleSheet(QStringLiteral("color:#7A8499;"));

    bar->addWidget(runBtn);
    bar->addWidget(fmtBtn);
    bar->addWidget(explainBtn);
    bar->addWidget(procBtn);
    bar->addWidget(erBtn);
    bar->addWidget(histBtn);
    bar->addSpacing(10);
    bar->addWidget(m_dbLabel);
    bar->addStretch();
    bar->addWidget(m_message);

    m_editor = new QPlainTextEdit;
    QFont monoFont(QStringLiteral("Consolas"));
    monoFont.setStyleHint(QFont::Monospace);
    monoFont.setPointSize(11);
    m_editor->setFont(monoFont);
    m_editor->setPlaceholderText(QStringLiteral(
        "-- 在此输入 SQL，按 F5 或点击“运行”\nSELECT * FROM your_table LIMIT 100;"));
    new SqlHighlighter(m_editor->document());

    auto *f5 = new QShortcut(QKeySequence(Qt::Key_F5), m_editor);
    connect(f5, &QShortcut::activated, this, &QueryView::runQuery);

    m_model = new QStandardItemModel(this);
    m_view = new QTableView;
    m_view->setModel(m_model);
    m_view->setAlternatingRowColors(true);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->horizontalHeader()->setHighlightSections(false);
    m_view->verticalHeader()->setDefaultSectionSize(28);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 右侧历史面板
    m_historyPanel = new QWidget;
    m_historyPanel->setVisible(false);
    auto *histLayout = new QVBoxLayout(m_historyPanel);
    histLayout->setContentsMargins(0, 0, 0, 0);
    histLayout->setSpacing(6);
    auto *histHeader = new QHBoxLayout;
    histHeader->addWidget(new QLabel(tr("查询历史")));
    auto *clearHistBtn = new QPushButton(tr("清空"));
    clearHistBtn->setObjectName(QStringLiteral("btnGhost"));
    clearHistBtn->setCursor(Qt::PointingHandCursor);
    connect(clearHistBtn, &QPushButton::clicked, this, &QueryView::clearHistory);
    histHeader->addStretch();
    histHeader->addWidget(clearHistBtn);
    histLayout->addLayout(histHeader);
    m_historyList = new QListWidget;
    m_historyList->setAlternatingRowColors(true);
    connect(m_historyList, &QListWidget::currentRowChanged,
            this, &QueryView::onHistorySelected);
    histLayout->addWidget(m_historyList, 1);

    m_splitter = new QSplitter(Qt::Horizontal);
    auto *vSplitter = new QSplitter(Qt::Vertical);
    vSplitter->addWidget(m_editor);
    vSplitter->addWidget(m_view);
    vSplitter->setStretchFactor(0, 1);
    vSplitter->setStretchFactor(1, 2);
    vSplitter->setSizes({220, 380});
    m_splitter->addWidget(vSplitter);
    m_splitter->addWidget(m_historyPanel);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setSizes({500, 0});

    root->addLayout(bar);
    root->addWidget(m_splitter, 1);
}

void QueryView::setSource(DbClient *client, const QString &profileId,
                          const QString &profileName, const QString &database)
{
    m_client = client;
    m_profileId = profileId;
    m_profileName = profileName;
    m_database = database;
    m_dbLabel->setText(QStringLiteral("%1  /  %2").arg(profileName, database));
    if (m_historyList->isVisible())
        loadHistory();
}

QString QueryView::tabKey() const
{
    return QStringLiteral("query:%1:%2").arg(m_profileId, m_database);
}

QString QueryView::tabTitle() const
{
    return tr("查询") + QStringLiteral(" - ") + m_database;
}

// ========== SQL 格式化（状态机分句 + 关键字大写 + 子句换行 + SELECT 列对齐） ==========
static QString formatSqlSimple(const QString &raw)
{
    QString s = raw;
    s.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));

    // 步骤 1: 分句（分号分割，跳过字符串/注释内的分号）
    QStringList stmts;
    QString cur;
    enum State { Normal, SingleQuote, DoubleQuote, LineComment, BlockComment };
    State st = Normal;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (st == Normal) {
            if (c == QLatin1Char(';')) {
                QString t = cur.trimmed();
                if (!t.isEmpty()) stmts << t;
                cur.clear();
            } else if (c == QLatin1Char('\'')) { cur += c; st = SingleQuote; }
            else if (c == QLatin1Char('"'))    { cur += c; st = DoubleQuote; }
            else if (c == QLatin1Char('-') && i + 1 < s.size() && s.at(i + 1) == QLatin1Char('-')) { cur += c; st = LineComment; }
            else if (c == QLatin1Char('/') && i + 1 < s.size() && s.at(i + 1) == QLatin1Char('*')) { cur += c; st = BlockComment; }
            else cur += c;
        } else if (st == SingleQuote) {
            cur += c;
            if (c == QLatin1Char('\'')) st = Normal;
        } else if (st == DoubleQuote) {
            cur += c;
            if (c == QLatin1Char('"')) st = Normal;
        } else if (st == LineComment) {
            cur += c;
            if (c == QLatin1Char('\n')) st = Normal;
        } else if (st == BlockComment) {
            cur += c;
            if (c == QLatin1Char('*') && i + 1 < s.size() && s.at(i + 1) == QLatin1Char('/')) {
                cur += s.at(++i); st = Normal;
            }
        }
    }
    QString tail = cur.trimmed();
    if (!tail.isEmpty()) stmts << tail;

    // 步骤 2: 关键字大写 + 单行化 + 子句换行 + SELECT 列垂直对齐
    static const QStringList allKeywords = {
        QStringLiteral("select"), QStringLiteral("from"), QStringLiteral("where"),
        QStringLiteral("insert"), QStringLiteral("into"), QStringLiteral("values"),
        QStringLiteral("update"), QStringLiteral("set"), QStringLiteral("delete"),
        QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("not"),
        QStringLiteral("join"), QStringLiteral("inner"), QStringLiteral("left"),
        QStringLiteral("right"), QStringLiteral("outer"), QStringLiteral("cross"),
        QStringLiteral("on"), QStringLiteral("group"), QStringLiteral("by"),
        QStringLiteral("having"), QStringLiteral("order"), QStringLiteral("limit"),
        QStringLiteral("offset"), QStringLiteral("union"), QStringLiteral("all"),
        QStringLiteral("distinct"), QStringLiteral("as"), QStringLiteral("is"),
        QStringLiteral("null"), QStringLiteral("in"), QStringLiteral("like"),
        QStringLiteral("between"), QStringLiteral("case"), QStringLiteral("when"),
        QStringLiteral("then"), QStringLiteral("else"), QStringLiteral("end"),
        QStringLiteral("exists"), QStringLiteral("true"), QStringLiteral("false")
    };

    static const QStringList clauseKeywords = {
        // 这些关键字前加换行（多行 SQL 子句）
        QStringLiteral("from"), QStringLiteral("where"),
        QStringLiteral("join"), QStringLiteral("inner join"), QStringLiteral("left join"),
        QStringLiteral("right join"), QStringLiteral("outer join"), QStringLiteral("cross join"),
        QStringLiteral("group by"), QStringLiteral("having"), QStringLiteral("order by"),
        QStringLiteral("limit"), QStringLiteral("offset"), QStringLiteral("union"),
        QStringLiteral("values"), QStringLiteral("set")
    };

    QStringList formatted;
    for (QString stmt : stmts) {
        // 关键字大写
        for (const QString &kw : allKeywords)
            stmt.replace(QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw),
                                            QRegularExpression::CaseInsensitiveOption),
                         kw.toUpper());

        // 合并成单行（去掉现有换行），字符串/注释内容里的 \n 也去掉
        QString oneLine;
        State st2 = Normal;
        for (int i = 0; i < stmt.size(); ++i) {
            const QChar c = stmt.at(i);
            if (st2 == Normal) {
                if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) oneLine += QLatin1Char(' ');
                else if (c == QLatin1Char('\'')) { oneLine += c; st2 = SingleQuote; }
                else if (c == QLatin1Char('"'))    { oneLine += c; st2 = DoubleQuote; }
                else if (c == QLatin1Char('-') && i + 1 < stmt.size() && stmt.at(i + 1) == QLatin1Char('-')) { oneLine += c; st2 = LineComment; }
                else if (c == QLatin1Char('/') && i + 1 < stmt.size() && stmt.at(i + 1) == QLatin1Char('*')) { oneLine += c; st2 = BlockComment; }
                else oneLine += c;
            } else if (st2 == SingleQuote) {
                oneLine += c;
                if (c == QLatin1Char('\'')) st2 = Normal;
            } else if (st2 == DoubleQuote) {
                oneLine += c;
                if (c == QLatin1Char('"')) st2 = Normal;
            } else if (st2 == LineComment) {
                oneLine += QLatin1Char(' ');
                if (c == QLatin1Char('\n')) st2 = Normal;
            } else if (st2 == BlockComment) {
                oneLine += c;
                if (c == QLatin1Char('*') && i + 1 < stmt.size() && stmt.at(i + 1) == QLatin1Char('/')) {
                    oneLine += stmt.at(++i); st2 = Normal;
                }
            }
        }
        stmt = oneLine.simplified();

        // 子句关键字前加换行
        for (const QString &clause : clauseKeywords) {
            stmt.replace(QRegularExpression(QStringLiteral("\\s+\\b%1\\b").arg(clause),
                                            QRegularExpression::CaseInsensitiveOption),
                         QStringLiteral("\n%1").arg(clause.toUpper()));
        }

        // SELECT 内部：逗号后跟换行 + 缩进
        // 需要先定位第一个 FROM 出现的位置（字符串/注释感知）
        int fromPos = -1;
        {
            State st3 = Normal;
            for (int i = 0; i < stmt.size() - 3; ++i) {
                const QChar c = stmt.at(i);
                if (st3 == Normal) {
                    if (c == QLatin1Char('\'')) st3 = SingleQuote;
                    else if (c == QLatin1Char('"')) st3 = DoubleQuote;
                    else if (c == QLatin1Char('F') || c == QLatin1Char('f')) {
                        if (i > 0 && stmt.at(i - 1).isLetterOrNumber()) continue;
                        if (stmt.mid(i, 4).compare(QLatin1String("FROM"), Qt::CaseInsensitive) == 0
                            && (i + 4 >= stmt.size() || !stmt.at(i + 4).isLetter())) {
                            fromPos = i; break;
                        }
                    }
                } else if (st3 == SingleQuote) {
                    if (c == QLatin1Char('\'')) st3 = Normal;
                } else if (st3 == DoubleQuote) {
                    if (c == QLatin1Char('"')) st3 = Normal;
                }
            }
        }
        if (fromPos > 0) {
            // 只处理 FROM 之前的 SELECT 列表区域
            QString head = stmt.left(fromPos);
            QString tail = stmt.mid(fromPos);
            // 括号嵌套感知：只在 depth==0 时替换逗号
            QString newHead;
            int depth = 0;
            State st4 = Normal;
            for (int i = 0; i < head.size(); ++i) {
                const QChar c = head.at(i);
                if (st4 == Normal) {
                    if (c == QLatin1Char('\'')) { newHead += c; st4 = SingleQuote; }
                    else if (c == QLatin1Char('"')) { newHead += c; st4 = DoubleQuote; }
                    else if (c == QLatin1Char('(')) { ++depth; newHead += c; }
                    else if (c == QLatin1Char(')')) { --depth; newHead += c; }
                    else if (c == QLatin1Char(',') && depth == 0)
                        newHead += QLatin1String(",\n    ");
                    else newHead += c;
                } else if (st4 == SingleQuote) {
                    newHead += c;
                    if (c == QLatin1Char('\'')) st4 = Normal;
                } else if (st4 == DoubleQuote) {
                    newHead += c;
                    if (c == QLatin1Char('"')) st4 = Normal;
                }
            }
            stmt = newHead + tail;
        }

        formatted << stmt;
    }

    return formatted.join(QStringLiteral(";\n\n")) +
           (formatted.isEmpty() ? QString() : QStringLiteral(";"));
}

void QueryView::formatSql()
{
    m_editor->setPlainText(formatSqlSimple(m_editor->toPlainText()));
}

// ========== 历史面板 ==========
void QueryView::toggleHistory()
{
    const bool show = !m_historyPanel->isVisible();
    m_historyPanel->setVisible(show);
    if (show)
        loadHistory();
}

void QueryView::loadHistory()
{
    m_historyList->clear();
    if (m_profileId.isEmpty()) return;
    const auto items = ConnectionStore::instance().queryHistory(m_profileId, 200);
    for (const QueryHistoryItem &it : items) {
        const QString timeStr = QDateTime::fromMSecsSinceEpoch(it.timestamp)
                                    .toString(QStringLiteral("MM-dd HH:mm"));
        const QString preview = it.sql.left(120).replace(QLatin1Char('\n'),
                                                         QLatin1Char(' '));
        auto *item = new QListWidgetItem;
        item->setText(QStringLiteral("%1  %2  [%3 ms] %4")
                          .arg(timeStr, it.success ? QStringLiteral("✓") : QStringLiteral("✕"),
                               QString::number(it.elapsedMs), preview));
        item->setData(Qt::UserRole, it.sql);
        item->setToolTip(it.sql);
        if (!it.success)
            item->setForeground(QColor("#E5484D"));
        m_historyList->addItem(item);
    }
}

void QueryView::onHistorySelected(int row)
{
    if (row < 0) return;
    auto *item = m_historyList->item(row);
    if (!item) return;
    m_editor->setPlainText(item->data(Qt::UserRole).toString());
}

void QueryView::clearHistory()
{
    if (QMessageBox::question(this, tr("清空历史"),
                              tr("确定清空当前连接的查询历史？")) != QMessageBox::Yes)
        return;
    ConnectionStore::instance().clearQueryHistory(m_profileId);
    loadHistory();
}

// ========== SQL 执行 ==========
static QStringList splitSqlStatements(const QString &text)
{
    QStringList out;
    QString cur;
    cur.reserve(text.size());
    enum State { Normal, SingleQuote, DoubleQuote, LineComment, BlockComment };
    State st = Normal;
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (st == Normal) {
            if (c == QLatin1Char(';')) {
                QString t = cur.trimmed();
                if (!t.isEmpty())
                    out << t;
                cur.clear();
            } else if (c == QLatin1Char('\'')) { cur += c; st = SingleQuote; }
            else if (c == QLatin1Char('"'))    { cur += c; st = DoubleQuote; }
            else if (c == QLatin1Char('-') && i + 1 < text.size() && text.at(i + 1) == QLatin1Char('-')) { cur += c; st = LineComment; }
            else if (c == QLatin1Char('/') && i + 1 < text.size() && text.at(i + 1) == QLatin1Char('*')) { cur += c; st = BlockComment; }
            else cur += c;
        } else if (st == SingleQuote) {
            cur += c;
            if (c == QLatin1Char('\'')) st = Normal;
        } else if (st == DoubleQuote) {
            cur += c;
            if (c == QLatin1Char('"')) st = Normal;
        } else if (st == LineComment) {
            cur += c;
            if (c == QLatin1Char('\n')) st = Normal;
        } else if (st == BlockComment) {
            cur += c;
            if (c == QLatin1Char('*') && i + 1 < text.size() && text.at(i + 1) == QLatin1Char('/')) {
                cur += text.at(++i); st = Normal;
            }
        }
    }
    QString t = cur.trimmed();
    if (!t.isEmpty())
        out << t;
    return out;
}

void QueryView::runQuery()
{
    if (!m_client)
        return;
    QString sql = m_editor->textCursor().hasSelection()
                      ? m_editor->textCursor().selectedText()
                      : m_editor->toPlainText();
    sql.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    if (sql.trimmed().isEmpty())
        return;

    const QStringList stmts = splitSqlStatements(sql);
    if (stmts.isEmpty())
        return;

    int totalElapsed = 0;
    int affectedTotal = 0;
    bool allOk = true;
    QString firstError;
    int failedIndex = -1;
    DbResult lastRes;

    for (int i = 0; i < stmts.size(); ++i) {
        const QString &one = stmts.at(i);
        DbResult res;
        QString err;
        if (!m_client->execute(one, {}, &res, &err)) {
            if (firstError.isEmpty()) {
                firstError = err;
                failedIndex = i;
            }
            allOk = false;
            break;
        }
        totalElapsed += res.elapsedMs;
        if (!res.hasResult && res.affectedRows > 0)
            affectedTotal += static_cast<int>(res.affectedRows);
        lastRes = res;
    }

    // 保存查询历史（成功或失败都记录）
    ConnectionStore::instance().saveQuery(m_profileId, m_database, sql, totalElapsed, allOk);
    if (m_historyList->isVisible())
        loadHistory();

    m_model->clear();

    if (!allOk) {
        m_message->setStyleSheet(QStringLiteral("color:#E5484D;"));
        if (stmts.size() > 1) {
            m_message->setText(tr("✕ 第 %1/%2 条失败：%3")
                                   .arg(failedIndex + 1).arg(stmts.size()).arg(firstError));
        } else {
            m_message->setText(tr("✕ 执行失败：%1").arg(firstError));
        }
        return;
    }

    DbResult &res = lastRes;
    if (res.hasResult) {
        m_model->setColumnCount(res.columns.size());
        m_model->setHorizontalHeaderLabels(res.columns);
        for (int r = 0; r < res.rows.size(); ++r) {
            const QStringList &row = res.rows.at(r);
            QList<QStandardItem *> items;
            for (int c = 0; c < res.columns.size(); ++c) {
                const bool isNull = res.isNull(r, c);
                auto *it = new QStandardItem(isNull ? QStringLiteral("NULL") : row.value(c));
                if (isNull) {
                    it->setForeground(QColor("#9AA3B5"));
                    QFont f;
                    f.setItalic(true);
                    it->setFont(f);
                }
                it->setEditable(false);
                items << it;
            }
            m_model->appendRow(items);
        }
        m_message->setStyleSheet(QStringLiteral("color:#16A34A;"));
        if (stmts.size() > 1) {
            m_message->setText(tr("✓ 共 %1 条语句全部执行，耗时 %2 ms。最后一条返回 %3 行")
                                   .arg(stmts.size()).arg(totalElapsed).arg(res.rowCount()));
        } else {
            m_message->setText(tr("✓ 查询成功，返回 %1 行，耗时 %2 ms")
                                   .arg(res.rowCount()).arg(res.elapsedMs));
        }
        m_view->resizeColumnsToContents();
    } else {
        m_message->setStyleSheet(QStringLiteral("color:#16A34A;"));
        if (stmts.size() > 1) {
            m_message->setText(tr("✓ 共 %1 条语句全部执行，影响 %2 行，耗时 %3 ms")
                                   .arg(stmts.size()).arg(affectedTotal).arg(totalElapsed));
        } else {
            m_message->setText(tr("✓ 执行成功，影响 %1 行，耗时 %2 ms")
                                   .arg(res.affectedRows).arg(res.elapsedMs));
        }
    }
}

void QueryView::runExplain()
{
    if (!m_client)
        return;
    QString sql = m_editor->toPlainText().trimmed();
    sql.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    if (sql.isEmpty())
        return;

    // 只取第一条语句做 Explain
    const int semi = sql.indexOf(QLatin1Char(';'));
    if (semi > 0)
        sql = sql.left(semi).trimmed();

    // SQLite: EXPLAIN QUERY PLAN
    // MySQL / PG: EXPLAIN
    QString explainSql;
    switch (m_client->type()) {
    case DbType::SQLite:
        explainSql = QStringLiteral("EXPLAIN QUERY PLAN %1").arg(sql);
        break;
    case DbType::MySQL:
    case DbType::PostgreSQL:
    default:
        explainSql = QStringLiteral("EXPLAIN %1").arg(sql);
        break;
    }

    DbResult res;
    QString err;
    m_model->clear();
    if (!m_client->execute(explainSql, {}, &res, &err)) {
        m_message->setStyleSheet(QStringLiteral("color:#E5484D;"));
        m_message->setText(tr("✕ Explain 失败：%1").arg(err));
        return;
    }
    if (!res.hasResult) {
        m_message->setStyleSheet(QStringLiteral("color:#E5484D;"));
        m_message->setText(tr("✕ Explain 无结果返回"));
        return;
    }

    m_model->setColumnCount(res.columns.size());
    m_model->setHorizontalHeaderLabels(res.columns);
    for (int r = 0; r < res.rows.size(); ++r) {
        const QStringList &row = res.rows.at(r);
        QList<QStandardItem *> items;
        for (int c = 0; c < res.columns.size(); ++c) {
            const bool isNull = res.isNull(r, c);
            auto *it = new QStandardItem(isNull ? QStringLiteral("NULL") : row.value(c));
            if (isNull) {
                it->setForeground(QColor("#9AA3B5"));
                QFont f; f.setItalic(true);
                it->setFont(f);
            }
            it->setEditable(false);
            items << it;
        }
        m_model->appendRow(items);
    }
    m_view->resizeColumnsToContents();
    m_message->setStyleSheet(QStringLiteral("color:#16A34A;"));
    m_message->setText(tr("✓ Explain 返回 %1 行，耗时 %2 ms").arg(res.rowCount()).arg(res.elapsedMs));
}

void QueryView::showProcessList()
{
    if (!m_client) return;

    // SQLite 不支持进程列表
    if (m_client->type() == DbType::SQLite) {
        QMessageBox::information(this, tr("进程列表"),
                                 tr("SQLite 为单连接模式，不支持进程列表。"));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(tr("进程列表 - %1").arg(m_profileName));
    dlg.resize(900, 500);

    auto *layout = new QVBoxLayout(&dlg);

    // 工具栏
    auto *toolBar = new QHBoxLayout;
    auto *refreshBtn = new QPushButton(IconManager::icon(QStringLiteral("refresh"),
                                                          QColor("#1B94FF"), QSize(14, 14)),
                                       tr("刷新"));
    refreshBtn->setObjectName(QStringLiteral("btnGhost"));
    refreshBtn->setCursor(Qt::PointingHandCursor);
    auto *killBtn = new QPushButton(IconManager::icon(QStringLiteral("trash"),
                                                       QColor("#E5484D"), QSize(14, 14)),
                                    tr("终止所选"));
    killBtn->setObjectName(QStringLiteral("btnGhost"));
    killBtn->setCursor(Qt::PointingHandCursor);
    killBtn->setEnabled(false);
    auto *tip = new QLabel(tr("双击进程行也会弹出终止确认"));
    tip->setStyleSheet(QStringLiteral("color:#9AA3B5; font-size:12px;"));
    toolBar->addWidget(refreshBtn);
    toolBar->addWidget(killBtn);
    toolBar->addStretch();
    toolBar->addWidget(tip);
    layout->addLayout(toolBar);

    auto *model = new QStandardItemModel(&dlg);
    auto *view = new QTableView(&dlg);
    view->setModel(model);
    view->setAlternatingRowColors(true);
    view->horizontalHeader()->setStretchLastSection(true);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(view, 1);

    auto load = [&] {
        model->clear();
        QString sql;
        switch (m_client->type()) {
        case DbType::MySQL:
            sql = QStringLiteral("SHOW FULL PROCESSLIST");
            break;
        case DbType::PostgreSQL:
            sql = QStringLiteral(
                "SELECT pid AS \"PID\", usename AS \"用户\", datname AS \"数据库\", "
                "client_addr AS \"客户端\", query_start AS \"开始时间\", "
                "state AS \"状态\", LEFT(query, 200) AS \"查询\" "
                "FROM pg_stat_activity ORDER BY query_start DESC");
            break;
        default:
            return;
        }
        DbResult res;
        QString err;
        if (!m_client->execute(sql, {}, &res, &err)) {
            QMessageBox::warning(&dlg, tr("进程列表"),
                                 tr("查询失败：%1").arg(err));
            return;
        }
        if (!res.hasResult) return;
        model->setColumnCount(res.columns.size());
        model->setHorizontalHeaderLabels(res.columns);
        for (int r = 0; r < res.rows.size(); ++r) {
            const QStringList &row = res.rows.at(r);
            QList<QStandardItem *> items;
            for (int c = 0; c < res.columns.size(); ++c) {
                auto *it = new QStandardItem(row.value(c));
                it->setEditable(false);
                items << it;
            }
            model->appendRow(items);
        }
        view->resizeColumnsToContents();
    };

    connect(refreshBtn, &QPushButton::clicked, &dlg, load);
    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged,
            &dlg, [&] { killBtn->setEnabled(view->currentIndex().isValid()); });

    auto killCurrent = [&] {
        if (!view->currentIndex().isValid()) return;
        const int row = view->currentIndex().row();
        QString idCol;
        if (m_client->type() == DbType::MySQL)
            idCol = QStringLiteral("Id");
        else if (m_client->type() == DbType::PostgreSQL)
            idCol = QStringLiteral("PID");
        int idIdx = -1;
        for (int c = 0; c < model->columnCount(); ++c)
            if (model->headerData(c, Qt::Horizontal).toString().compare(
                    idCol, Qt::CaseInsensitive) == 0) { idIdx = c; break; }
        if (idIdx < 0) return;
        const QString pid = model->item(row, idIdx)->text();
        if (QMessageBox::question(&dlg, tr("终止进程"),
                                  tr("确定终止进程 %1？").arg(pid)) != QMessageBox::Yes)
            return;
        QString killSql;
        if (m_client->type() == DbType::MySQL)
            killSql = QStringLiteral("KILL %1").arg(pid);
        else if (m_client->type() == DbType::PostgreSQL)
            killSql = QStringLiteral("SELECT pg_terminate_backend(%1)").arg(pid);
        DbResult res; QString err;
        if (!m_client->execute(killSql, {}, &res, &err))
            QMessageBox::warning(&dlg, tr("终止进程"), tr("失败：%1").arg(err));
        load();
    };
    connect(killBtn, &QPushButton::clicked, &dlg, killCurrent);
    connect(view, &QTableView::doubleClicked, &dlg, killCurrent);

    load();
    dlg.exec();
}

void QueryView::showErDiagram()
{
    if (!m_client) return;
    ErDialog dlg(m_client, m_database, this);
    dlg.exec();
}
