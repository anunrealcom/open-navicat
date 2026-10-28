#include "DataView.h"

#include "IconManager.h"
#include "core/DbClient.h"
#include "core/EditableTableModel.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QKeySequence>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>

static QPushButton *toolButton(const QString &icon, const QColor &color, const QString &tip)
{
    auto *b = new QPushButton;
    b->setObjectName(QStringLiteral("iconButton"));
    b->setFixedSize(32, 32);
    b->setIcon(IconManager::icon(icon, color, QSize(17, 17)));
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

DataView::DataView(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 10);
    root->setSpacing(10);

    // 工具栏
    auto *bar = new QHBoxLayout;
    bar->setSpacing(8);
    m_title = new QLabel;
    QFont titleFont = m_title->font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleFont.setBold(true);
    m_title->setFont(titleFont);

    auto *addBtn = toolButton(QStringLiteral("plus"), QColor("#1B94FF"), tr("新增一行"));
    auto *delBtn = toolButton(QStringLiteral("minus"), QColor("#E5484D"), tr("删除选中行"));
    auto *copyBtn = toolButton(QStringLiteral("edit"), QColor("#7A8499"), tr("复制选中行 (Ctrl+C)"));
    auto *importBtn = toolButton(QStringLiteral("database_add"), QColor("#8B5CF6"), tr("导入 CSV"));
    auto *exportBtn = toolButton(QStringLiteral("save"), QColor("#16A34A"), tr("导出 CSV"));
    auto *refreshBtn = toolButton(QStringLiteral("refresh"), QColor("#5A6478"), tr("刷新"));
    connect(addBtn, &QPushButton::clicked, this, &DataView::addRow);
    connect(delBtn, &QPushButton::clicked, this, &DataView::deleteSelectedRows);
    connect(copyBtn, &QPushButton::clicked, this, &DataView::copySelected);
    connect(importBtn, &QPushButton::clicked, this, [this] {
        if (!m_client) return;
        const QString path = QFileDialog::getOpenFileName(
            this, tr("导入 CSV"), QString(),
            tr("CSV 文件 (*.csv);;所有文件 (*.*)"));
        if (path.isEmpty()) return;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("导入失败"),
                                 tr("无法读取文件：%1").arg(file.errorString()));
            return;
        }
        QTextStream ts(&file);
        ts.setEncoding(QStringConverter::Utf8);

        // 简单 CSV 行解析（支持引号包裹内的逗号和换行）
        auto parseLine = [](const QString &in) -> QStringList {
            QStringList out; QString cur; bool inQuote = false;
            for (int i = 0; i < in.size(); ++i) {
                const QChar c = in.at(i);
                if (inQuote) {
                    if (c == QLatin1Char('"') && i + 1 < in.size() && in.at(i + 1) == QLatin1Char('"')) {
                        cur += QLatin1Char('"'); ++i;
                    } else if (c == QLatin1Char('"')) {
                        inQuote = false;
                    } else cur += c;
                } else {
                    if (c == QLatin1Char(',')) { out << cur; cur.clear(); }
                    else if (c == QLatin1Char('"')) inQuote = true;
                    else cur += c;
                }
            }
            out << cur;
            return out;
        };

        // 读取表头
        const QStringList header = parseLine(ts.readLine());
        if (header.isEmpty()) { file.close(); return; }

        // 匹配表头到列索引
        const int colCount = m_model->columnCount();
        QList<int> colIndex;
        for (const QString &h : header) {
            const QString trimmed = h.trimmed();
            int found = -1;
            for (int c = 0; c < colCount; ++c) {
                if (m_model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString()
                        .compare(trimmed, Qt::CaseInsensitive) == 0) {
                    found = c; break;
                }
            }
            colIndex << found;
        }

        int ok = 0, fail = 0;
        QString err;
        while (!ts.atEnd()) {
            const QString raw = ts.readLine();
            if (raw.trimmed().isEmpty()) continue;
            const QStringList cells = parseLine(raw);

            QStringList cols, vals;
            for (int i = 0; i < cells.size() && i < colIndex.size(); ++i) {
                const int c = colIndex.at(i);
                if (c < 0) continue;
                const QString &val = cells.at(i);
                if (val.compare(QStringLiteral("null"), Qt::CaseInsensitive) == 0)
                    continue;
                cols << m_client->quoteIdent(
                    m_model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString());
                QString esc = val;
                esc.replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
                   .replace(QLatin1Char('\''), QStringLiteral("\\'"));
                vals << QLatin1Char('\'') + esc + QLatin1Char('\'');
            }
            if (cols.isEmpty()) continue;
            const QString sql = QStringLiteral("INSERT INTO %1.%2 (%3) VALUES (%4)")
                                    .arg(m_client->quoteIdent(m_database),
                                         m_client->quoteIdent(m_table),
                                         cols.join(QStringLiteral(", ")),
                                         vals.join(QStringLiteral(", ")));
            DbResult res;
            if (m_client->execute(sql, {}, &res, &err)) ++ok;
            else ++fail;
        }
        file.close();
        refresh();
        QMessageBox::information(this, tr("导入完成"),
                                 tr("成功 %1 行，失败 %2 行").arg(ok).arg(fail));
    });
    connect(exportBtn, &QPushButton::clicked, this, &DataView::exportCsv);
    connect(refreshBtn, &QPushButton::clicked, this, &DataView::refresh);

    // Ctrl+C 复制快捷键
    auto *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copyShortcut, &QShortcut::activated, this, &DataView::copySelected);

    m_where = new QLineEdit;
    m_where->setObjectName(QStringLiteral("searchBox"));
    m_where->setPlaceholderText(tr("筛选条件（WHERE），如：id > 10 AND name LIKE '%a%'"));
    m_where->setMinimumHeight(30);
    m_where->setClearButtonEnabled(true);
    m_where->addAction(IconManager::icon(QStringLiteral("filter"), QColor("#9AA3B5"),
                                         QSize(15, 15)),
                       QLineEdit::LeadingPosition);
    connect(m_where, &QLineEdit::returnPressed, this, [this] {
        m_page = 0;
        refresh();
    });

    bar->addWidget(m_title);
    bar->addSpacing(8);
    bar->addWidget(addBtn);
    bar->addWidget(delBtn);
    bar->addWidget(copyBtn);
    bar->addWidget(importBtn);
    bar->addWidget(exportBtn);
    bar->addStretch();
    bar->addWidget(m_where, 1);
    bar->addWidget(refreshBtn);

    // 数据表格
    m_model = new EditableTableModel(this);
    connect(m_model, &EditableTableModel::editFailed, this, [this](const QString &reason) {
        QMessageBox::warning(this, tr("编辑失败"), reason);
    });
    m_view = new QTableView;
    m_view->setModel(m_model);
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setEditTriggers(QAbstractItemView::DoubleClicked |
                            QAbstractItemView::EditKeyPressed |
                            QAbstractItemView::SelectedClicked);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->horizontalHeader()->setHighlightSections(false);
    m_view->horizontalHeader()->setSectionsClickable(true);
    m_view->horizontalHeader()->setSortIndicatorShown(true);
    m_view->verticalHeader()->setDefaultSectionSize(30);
    m_view->setItemDelegate(new QStyledItemDelegate(m_view));
    connect(m_view, &QTableView::customContextMenuRequested, this, &DataView::onContextMenu);
    connect(m_view->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &DataView::onHeaderClicked);

    // 分页栏
    auto *pager = new QHBoxLayout;
    pager->setSpacing(6);
    auto *firstBtn = toolButton(QStringLiteral("step_first"), QColor("#5A6478"), tr("第一页"));
    auto *prevBtn = toolButton(QStringLiteral("chevron_left"), QColor("#5A6478"), tr("上一页"));
    auto *nextBtn = toolButton(QStringLiteral("chevron_right"), QColor("#5A6478"), tr("下一页"));
    auto *lastBtn = toolButton(QStringLiteral("step_last"), QColor("#5A6478"), tr("最后一页"));
    connect(firstBtn, &QPushButton::clicked, this, &DataView::firstPage);
    connect(prevBtn, &QPushButton::clicked, this, &DataView::prevPage);
    connect(nextBtn, &QPushButton::clicked, this, &DataView::nextPage);
    connect(lastBtn, &QPushButton::clicked, this, &DataView::lastPage);

    m_pageInfo = new QLabel;
    m_pageSize = new QComboBox;
    m_pageSize->addItems({QStringLiteral("50"), QStringLiteral("100"), QStringLiteral("200"),
                          QStringLiteral("500")});
    m_pageSize->setCurrentText(QStringLiteral("100"));
    m_pageSize->setMinimumHeight(28);
    connect(m_pageSize, &QComboBox::currentTextChanged, this, [this] {
        m_page = 0;
        refresh();
    });

    pager->addWidget(new QLabel(tr("每页")));
    pager->addWidget(m_pageSize);
    pager->addWidget(new QLabel(tr("条")));
    pager->addStretch();
    pager->addWidget(firstBtn);
    pager->addWidget(prevBtn);
    pager->addWidget(m_pageInfo);
    pager->addWidget(nextBtn);
    pager->addWidget(lastBtn);

    root->addLayout(bar);
    root->addWidget(m_view, 1);
    root->addLayout(pager);
}

void DataView::setSource(DbClient *client, const QString &profileId,
                         const QString &profileName,
                         const QString &database, const QString &schema, const QString &table)
{
    m_client = client;
    m_profileId = profileId;
    m_profileName = profileName;
    m_database = database;
    m_schema = schema;
    m_table = table;
    m_page = 0;
    m_model->bind(client, database, schema, table);
    const QString label = schema.isEmpty() || schema == QLatin1String("public")
                              ? table
                              : schema + QLatin1Char('.') + table;
    m_title->setText(QStringLiteral("%1  ·  %2").arg(profileName, label));
    refresh();
}

QString DataView::tabKey() const
{
    return QStringLiteral("data:%1:%2:%3:%4").arg(m_profileId, m_database, m_schema, m_table);
}

QString DataView::tabTitle() const
{
    return m_schema.isEmpty() || m_schema == QLatin1String("public")
               ? m_table
               : m_schema + QLatin1Char('.') + m_table;
}

void DataView::refresh()
{
    if (!m_client)
        return;
    QString err;
    if (!m_model->loadPage(m_where->text().trimmed(), currentOrderBy(),
                           m_page, m_pageSize->currentText().toInt(), &err)) {
        QMessageBox::warning(this, tr("查询失败"), err);
        return;
    }
    m_page = m_model->page();
    m_view->resizeColumnsToContents();
    updatePager();
}

void DataView::updatePager()
{
    const qint64 total = m_model->totalCount();
    const int size = m_pageSize->currentText().toInt();
    const int pages = total <= 0 ? 1 : int((total + size - 1) / size);
    m_pageInfo->setText(QStringLiteral("  %1 / %2 页 · 共 %3 行  ")
                            .arg(m_page + 1)
                            .arg(pages)
                            .arg(total));
}

void DataView::firstPage()
{
    m_page = 0;
    refresh();
}

void DataView::prevPage()
{
    if (m_page > 0)
        --m_page;
    refresh();
}

void DataView::nextPage()
{
    const int size = m_pageSize->currentText().toInt();
    const int pages = int((m_model->totalCount() + size - 1) / size);
    if (m_page + 1 < pages)
        ++m_page;
    refresh();
}

void DataView::lastPage()
{
    const int size = m_pageSize->currentText().toInt();
    m_page = qMax(0, int((m_model->totalCount() + size - 1) / size) - 1);
    refresh();
}

void DataView::addRow()
{
    QString err;
    if (!m_model->insertEmptyRow(&err)) {
        QMessageBox::warning(this, tr("新增失败"), err);
        return;
    }
    refresh();
}

void DataView::deleteSelectedRows()
{
    const QModelIndexList sel = m_view->selectionModel()->selectedRows();
    if (sel.isEmpty())
        return;
    if (QMessageBox::question(this, tr("删除行"),
                              tr("确定删除选中的 %1 行吗？").arg(sel.size())) != QMessageBox::Yes)
        return;
    QList<int> rows;
    for (const QModelIndex &idx : sel)
        rows << idx.row();
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    QString err;
    if (!m_model->deleteRows(rows, &err)) {
        QMessageBox::warning(this, tr("删除失败"), err);
        return;
    }
    refresh();
}

void DataView::onContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_view->indexAt(pos);
    QMenu menu(this);
    menu.addAction(IconManager::icon(QStringLiteral("refresh"), QColor("#5A6478"),
                                     QSize(15, 15)),
                   tr("刷新"), this, &DataView::refresh);
    if (idx.isValid()) {
        menu.addSeparator();
        menu.addAction(IconManager::icon(QStringLiteral("field"), QColor("#7A8499"),
                                         QSize(15, 15)),
                       tr("将该单元格设为 NULL"), this, [this, idx] {
                           QString err;
                           if (!m_model->setCellNull(idx.row(), idx.column(), &err))
                               QMessageBox::warning(this, tr("设置失败"), err);
                       });
    }
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void DataView::onHeaderClicked(int column)
{
    if (!m_client || !m_model)
        return;
    if (column < 0 || column >= m_model->columnCount())
        return;

    if (m_sortColumn != column) {
        // 点击新列 → 默认升序
        m_sortColumn = column;
        m_sortOrder = 1;
    } else {
        // 同一列循环：升序 → 降序 → 无排序 → 升序
        if (m_sortOrder == 1)
            m_sortOrder = -1;
        else if (m_sortOrder == -1) {
            m_sortOrder = 0;
            m_sortColumn = -1;
        } else
            m_sortOrder = 1;
    }

    // 更新表头指示器
    if (m_sortOrder != 0)
        m_view->horizontalHeader()->setSortIndicator(column,
                                                     m_sortOrder == 1 ? Qt::AscendingOrder
                                                                      : Qt::DescendingOrder);
    else
        m_view->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);

    m_page = 0;
    refresh();
}

QString DataView::currentOrderBy() const
{
    if (m_sortColumn < 0 || m_sortColumn >= m_model->columnCount() || m_sortOrder == 0)
        return {};
    const QString col = m_model->headerData(m_sortColumn, Qt::Horizontal, Qt::DisplayRole)
                            .toString();
    if (col.isEmpty())
        return {};
    return m_client->quoteIdent(col) + (m_sortOrder == 1 ? QStringLiteral(" ASC")
                                                        : QStringLiteral(" DESC"));
}

static QString csvEscape(const QString &v)
{
    if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"')) ||
        v.contains(QLatin1Char('\n')) || v.contains(QLatin1Char('\r'))) {
        QString escaped = v;
        escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + escaped + QLatin1Char('"');
    }
    return v;
}

void DataView::exportCsv()
{
    if (!m_model || m_model->rowCount() == 0) {
        QMessageBox::information(this, tr("导出 CSV"), tr("当前没有数据可导出"));
        return;
    }
    const QString filename = m_table.isEmpty() ? QStringLiteral("export.csv")
                                                : m_table + QStringLiteral(".csv");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("导出 CSV"), filename,
        tr("CSV 文件 (*.csv);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"),
                             tr("无法写入文件：%1").arg(file.errorString()));
        return;
    }
    QTextStream ts(&file);
    ts.setEncoding(QStringConverter::Utf8);

    // 表头
    QStringList header;
    for (int c = 0; c < m_model->columnCount(); ++c)
        header << csvEscape(m_model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString());
    ts << header.join(QLatin1Char(',')) << QLatin1Char('\n');

    // 数据行
    for (int r = 0; r < m_model->rowCount(); ++r) {
        QStringList row;
        for (int c = 0; c < m_model->columnCount(); ++c) {
            const QModelIndex idx = m_model->index(r, c);
            row << csvEscape(m_model->data(idx, Qt::DisplayRole).toString());
        }
        ts << row.join(QLatin1Char(',')) << QLatin1Char('\n');
    }
    file.close();
    QMessageBox::information(this, tr("导出成功"),
                             tr("已导出 %1 行到：\n%2").arg(m_model->rowCount()).arg(path));
}

void DataView::copySelected()
{
    if (!m_model)
        return;
    const QModelIndexList sel = m_view->selectionModel()->selectedIndexes();
    if (sel.isEmpty())
        return;

    // 用 QSet 去重合并成矩形区域
    int minRow = m_model->rowCount(), maxRow = -1;
    int minCol = m_model->columnCount(), maxCol = -1;
    for (const QModelIndex &idx : sel) {
        minRow = qMin(minRow, idx.row());
        maxRow = qMax(maxRow, idx.row());
        minCol = qMin(minCol, idx.column());
        maxCol = qMax(maxCol, idx.column());
    }

    QStringList lines;
    for (int r = minRow; r <= maxRow; ++r) {
        QStringList cells;
        for (int c = minCol; c <= maxCol; ++c) {
            const QModelIndex idx = m_model->index(r, c);
            cells << m_model->data(idx, Qt::DisplayRole).toString();
        }
        lines << cells.join(QLatin1Char('\t'));
    }

    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}
