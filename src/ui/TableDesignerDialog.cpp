#include "TableDesignerDialog.h"

#include "IconManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

enum Col
{
    COL_NAME = 0,
    COL_TYPE,
    COL_LENGTH,
    COL_SCALE,
    COL_ENUM,
    COL_PK,
    COL_AUTO,
    COL_NOTNULL,
    COL_UNIQUE,
    COL_INDEX,
    COL_FKTABLE,
    COL_FKCOL,
    COL_DEFAULT,
    COL_COMMENT,
    COL_COUNT
};

namespace {
/// 根据类型字符串决定哪些列隐藏
void applyTypeVisibility(QTableWidget *grid, int row, const QString &typeStr)
{
    const QString t = typeStr.trimmed().toUpper();
    // 所有类型先显示
    grid->setColumnHidden(COL_LENGTH, false);
    grid->setColumnHidden(COL_SCALE, false);
    grid->setColumnHidden(COL_ENUM, false);

    if (t == QStringLiteral("ENUM") || t == QStringLiteral("SET")) {
        grid->setColumnHidden(COL_LENGTH, true);
        grid->setColumnHidden(COL_SCALE, true);
    } else if (t == QStringLiteral("DECIMAL") || t == QStringLiteral("NUMERIC")) {
        grid->setColumnHidden(COL_ENUM, true);
    } else if (t == QStringLiteral("VARCHAR") || t == QStringLiteral("CHAR") ||
               t == QStringLiteral("BINARY") || t == QStringLiteral("VARBINARY")) {
        grid->setColumnHidden(COL_SCALE, true);
        grid->setColumnHidden(COL_ENUM, true);
    } else {
        // INT / BIGINT / TEXT / DATE / DATETIME / FLOAT / DOUBLE / JSON / BLOB / BOOL 等
        grid->setColumnHidden(COL_LENGTH, true);
        grid->setColumnHidden(COL_SCALE, true);
        grid->setColumnHidden(COL_ENUM, true);
    }

    // 外键两列始终显示（MySQL/SQLite/PG 都支持 FK）
    grid->setColumnHidden(COL_FKTABLE, false);
    grid->setColumnHidden(COL_FKCOL, false);
}
} // namespace

TableDesignerDialog::TableDesignerDialog(DbType type, const QString &database,
                                         const QString &schema, QWidget *parent)
    : FramelessDialog(tr("表设计器"), parent),
      m_type(type),
      m_database(database),
      m_schema(schema)
{
    setDialogSize(1280, 680);

    auto *topForm = new QFormLayout;
    topForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_tableName = new QLineEdit;
    m_tableName->setPlaceholderText(tr("请输入表名"));
    m_tableName->setMinimumHeight(32);
    topForm->addRow(tr("表名"), m_tableName);

    auto *addBtn = new QPushButton(IconManager::icon(QStringLiteral("plus"),
                                                      QColor("#1B94FF"), QSize(16, 16)),
                                   tr("添加字段"));
    addBtn->setMinimumHeight(32);
    addBtn->setCursor(Qt::PointingHandCursor);
    auto *delBtn = new QPushButton(IconManager::icon(QStringLiteral("trash"),
                                                      QColor("#E5484D"), QSize(15, 15)),
                                   tr("删除字段"));
    delBtn->setMinimumHeight(32);
    delBtn->setCursor(Qt::PointingHandCursor);
    connect(addBtn, &QPushButton::clicked, this, &TableDesignerDialog::addColumn);
    connect(delBtn, &QPushButton::clicked, this, &TableDesignerDialog::removeColumn);

    auto *toolbarRow = new QHBoxLayout;
    toolbarRow->addWidget(new QLabel(tr("字段列表")));
    toolbarRow->addStretch();
    toolbarRow->addWidget(addBtn);
    toolbarRow->addWidget(delBtn);

    m_grid = new QTableWidget(0, COL_COUNT);
    m_grid->setHorizontalHeaderLabels({tr("字段名"), tr("类型"), tr("长度"), tr("精度"),
                                       tr("ENUM候选值"), tr("主键"), tr("自增"), tr("非空"),
                                       tr("唯一"), tr("索引"), tr("FK目标表"), tr("FK目标列"),
                                       tr("默认值"), tr("注释")});
    m_grid->verticalHeader()->setDefaultSectionSize(36);
    m_grid->verticalHeader()->setVisible(true);
    m_grid->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
    m_grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_grid->horizontalHeader()->setStretchLastSection(true);
    m_grid->setColumnWidth(COL_NAME, 140);
    m_grid->setColumnWidth(COL_TYPE, 140);
    m_grid->setColumnWidth(COL_LENGTH, 65);
    m_grid->setColumnWidth(COL_SCALE, 65);
    m_grid->setColumnWidth(COL_ENUM, 150);
    m_grid->setColumnWidth(COL_PK, 50);
    m_grid->setColumnWidth(COL_AUTO, 50);
    m_grid->setColumnWidth(COL_NOTNULL, 50);
    m_grid->setColumnWidth(COL_UNIQUE, 50);
    m_grid->setColumnWidth(COL_INDEX, 50);
    m_grid->setColumnWidth(COL_FKTABLE, 120);
    m_grid->setColumnWidth(COL_FKCOL, 100);
    m_grid->setColumnWidth(COL_DEFAULT, 120);
    m_colCount = COL_COUNT;

    auto *okBtn = new QPushButton(IconManager::icon(QStringLiteral("check"), QColor("white"),
                                                    QSize(16, 16)),
                                  tr("保存并创建"));
    okBtn->setObjectName(QStringLiteral("btnPrimary"));
    okBtn->setMinimumHeight(36);
    okBtn->setCursor(Qt::PointingHandCursor);
    auto *cancelBtn = new QPushButton(tr("取消"));
    cancelBtn->setMinimumHeight(36);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(okBtn, &QPushButton::clicked, this, [this] {
        if (!m_tableName->text().trimmed().isEmpty() && m_grid->rowCount() > 0)
            accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    auto *bottomRow = new QHBoxLayout;
    bottomRow->addStretch();
    bottomRow->addWidget(cancelBtn);
    bottomRow->addWidget(okBtn);

    bodyLayout()->addLayout(topForm);
    bodyLayout()->addLayout(toolbarRow);
    bodyLayout()->addWidget(m_grid, 1);
    bodyLayout()->addLayout(bottomRow);

    addColumn();
    m_grid->cellWidget(0, COL_NAME)->setFocus();
}

void TableDesignerDialog::setupRow(int row, const ColumnInfo *ci)
{
    auto *name = new QLineEdit;
    name->setPlaceholderText(tr("字段名"));
    name->setFrame(false);

    auto *type = new QComboBox;
    type->setEditable(true);
    type->addItems(Metadata::typeList(m_type));
    type->setFrame(false);

    auto *len = new QSpinBox;
    len->setRange(0, 65535);
    len->setFrame(false);
    len->setSpecialValueText(QStringLiteral("-"));

    auto *scale = new QSpinBox;
    scale->setRange(0, 30);
    scale->setFrame(false);
    scale->setSpecialValueText(QStringLiteral("-"));

    auto *enumEdit = new QLineEdit;
    enumEdit->setFrame(false);
    enumEdit->setPlaceholderText(tr("a,b,c"));

    auto *pk = new QCheckBox;
    auto *autoInc = new QCheckBox;
    auto *notNull = new QCheckBox;
    auto *unique = new QCheckBox;
    auto *indexBox = new QCheckBox;
    for (auto *cb : {pk, autoInc, notNull, unique, indexBox}) {
        cb->setLayoutDirection(Qt::RightToLeft);
        cb->setCursor(Qt::PointingHandCursor);
    }

    auto *fkTable = new QLineEdit;
    fkTable->setFrame(false);
    fkTable->setPlaceholderText(tr("目标表"));

    auto *fkCol = new QLineEdit;
    fkCol->setFrame(false);
    fkCol->setPlaceholderText(tr("目标列"));

    auto *def = new QLineEdit;
    def->setFrame(false);
    def->setPlaceholderText(tr("可选"));

    auto *comment = new QLineEdit;
    comment->setFrame(false);
    comment->setPlaceholderText(m_type == DbType::MySQL ? tr("可选") : tr("仅 MySQL"));
    comment->setEnabled(m_type == DbType::MySQL);

    m_grid->setCellWidget(row, COL_NAME, name);
    m_grid->setCellWidget(row, COL_TYPE, type);
    m_grid->setCellWidget(row, COL_LENGTH, len);
    m_grid->setCellWidget(row, COL_SCALE, scale);
    m_grid->setCellWidget(row, COL_ENUM, enumEdit);
    m_grid->setCellWidget(row, COL_PK, pk);
    m_grid->setCellWidget(row, COL_AUTO, autoInc);
    m_grid->setCellWidget(row, COL_NOTNULL, notNull);
    m_grid->setCellWidget(row, COL_UNIQUE, unique);
    m_grid->setCellWidget(row, COL_INDEX, indexBox);
    m_grid->setCellWidget(row, COL_FKTABLE, fkTable);
    m_grid->setCellWidget(row, COL_FKCOL, fkCol);
    m_grid->setCellWidget(row, COL_DEFAULT, def);
    m_grid->setCellWidget(row, COL_COMMENT, comment);

    // 类型变化时更新列可见性
    connect(type, &QComboBox::currentTextChanged, this, [this, row](const QString &t) {
        applyTypeVisibility(m_grid, row, t);
    });

    if (ci) {
        name->setText(ci->name);
        type->setCurrentText(ci->baseType);
        len->setValue(ci->length);
        pk->setChecked(ci->primaryKey);
        autoInc->setChecked(ci->autoIncrement);
        notNull->setChecked(!ci->nullable);
        if (ci->defaultValue.contains(QLatin1String("unique"), Qt::CaseInsensitive))
            unique->setChecked(true);
        def->setText(ci->defaultValue);
        comment->setText(ci->comment);
    }
}

void TableDesignerDialog::addColumn()
{
    const int row = m_grid->rowCount();
    m_grid->insertRow(row);
    setupRow(row);
}

void TableDesignerDialog::removeColumn()
{
    const int row = m_grid->currentRow();
    if (row >= 0 && m_grid->rowCount() > 1)
        m_grid->removeRow(row);
}

void TableDesignerDialog::loadColumns(const QList<ColumnInfo> &columns)
{
    m_grid->setRowCount(0);
    for (const ColumnInfo &ci : columns) {
        const int row = m_grid->rowCount();
        m_grid->insertRow(row);
        setupRow(row, &ci);
    }
}

void TableDesignerDialog::setTableName(const QString &name)
{
    m_tableName->setText(name);
}

QString TableDesignerDialog::tableName() const
{
    return m_tableName->text().trimmed();
}

TableSpec TableDesignerDialog::tableSpec() const
{
    TableSpec spec;
    spec.schema = m_schema;
    spec.name = m_tableName->text().trimmed();
    for (int r = 0; r < m_grid->rowCount(); ++r) {
        auto *name = qobject_cast<QLineEdit *>(m_grid->cellWidget(r, COL_NAME));
        auto *type = qobject_cast<QComboBox *>(m_grid->cellWidget(r, COL_TYPE));
        auto *len = qobject_cast<QSpinBox *>(m_grid->cellWidget(r, COL_LENGTH));
        auto *scale = qobject_cast<QSpinBox *>(m_grid->cellWidget(r, COL_SCALE));
        auto *enumEdit = qobject_cast<QLineEdit *>(m_grid->cellWidget(r, COL_ENUM));
        auto *pk = qobject_cast<QCheckBox *>(m_grid->cellWidget(r, COL_PK));
        auto *autoInc = qobject_cast<QCheckBox *>(m_grid->cellWidget(r, COL_AUTO));
        auto *notNull = qobject_cast<QCheckBox *>(m_grid->cellWidget(r, COL_NOTNULL));
        auto *unique = qobject_cast<QCheckBox *>(m_grid->cellWidget(r, COL_UNIQUE));
        auto *indexBox = qobject_cast<QCheckBox *>(m_grid->cellWidget(r, COL_INDEX));
        auto *fkTable = qobject_cast<QLineEdit *>(m_grid->cellWidget(r, COL_FKTABLE));
        auto *fkCol = qobject_cast<QLineEdit *>(m_grid->cellWidget(r, COL_FKCOL));
        auto *def = qobject_cast<QLineEdit *>(m_grid->cellWidget(r, COL_DEFAULT));
        auto *comment = qobject_cast<QLineEdit *>(m_grid->cellWidget(r, COL_COMMENT));
        if (!name || name->text().trimmed().isEmpty())
            continue;

        ColumnSpec c;
        c.name = name->text().trimmed();
        c.type = type ? type->currentText().trimmed() : QString();
        c.length = len ? len->value() : 0;
        c.scale = scale ? scale->value() : 0;
        c.enumValues = enumEdit ? enumEdit->text().trimmed() : QString();
        c.primaryKey = pk && pk->isChecked();
        c.autoIncrement = autoInc && autoInc->isChecked();
        c.notNull = notNull && notNull->isChecked();
        c.unique = unique && unique->isChecked();
        c.index = indexBox && indexBox->isChecked();
        c.fkTable = fkTable ? fkTable->text().trimmed() : QString();
        c.fkColumn = fkCol ? fkCol->text().trimmed() : QString();
        c.defaultValue = def ? def->text().trimmed() : QString();
        c.comment = comment ? comment->text().trimmed() : QString();
        spec.columns << c;
    }
    return spec;
}
