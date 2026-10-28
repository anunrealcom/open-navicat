#include "DatabaseDialog.h"

#include "core/Metadata.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

DatabaseDialog::DatabaseDialog(DbType type, QWidget *parent)
    : FramelessDialog(type == DbType::PostgreSQL ? tr("新建 PostgreSQL 数据库")
                                                 : tr("新建 MySQL 数据库"),
                      parent),
      m_type(type)
{
    setDialogSize(440, 340);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(14);

    m_name = new QLineEdit;
    m_name->setPlaceholderText(tr("数据库名称"));
    m_name->setMinimumHeight(32);
    form->addRow(tr("数据库名"), m_name);

    if (m_type == DbType::MySQL) {
        m_charset = new QComboBox;
        m_charset->setMinimumHeight(32);
        m_charset->addItems(Metadata::charsetList());
        m_collation = new QComboBox;
        m_collation->setMinimumHeight(32);
        m_collation->addItems(Metadata::collationList(m_charset->currentText()));
        connect(m_charset, &QComboBox::currentTextChanged, this, [this](const QString &cs) {
            m_collation->clear();
            m_collation->addItems(Metadata::collationList(cs));
        });
        form->addRow(tr("字符集"), m_charset);
        form->addRow(tr("排序规则"), m_collation);
    } else {
        m_owner = new QLineEdit;
        m_owner->setPlaceholderText(tr("可选，默认为当前用户"));
        m_owner->setMinimumHeight(32);
        form->addRow(tr("所有者"), m_owner);
        auto *enc = new QLabel(tr("编码固定为 UTF8"));
        enc->setStyleSheet(QStringLiteral("color:#7A8499;"));
        form->addRow(QString(), enc);
    }

    auto *okBtn = new QPushButton(tr("创建"));
    okBtn->setObjectName(QStringLiteral("btnPrimary"));
    okBtn->setMinimumHeight(36);
    okBtn->setCursor(Qt::PointingHandCursor);
    auto *cancelBtn = new QPushButton(tr("取消"));
    cancelBtn->setMinimumHeight(36);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(okBtn, &QPushButton::clicked, this, [this] {
        if (!m_name->text().trimmed().isEmpty())
            accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_name, &QLineEdit::returnPressed, okBtn, &QPushButton::click);

    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    row->addWidget(cancelBtn, 1);
    row->addWidget(okBtn, 1);

    bodyLayout()->addLayout(form);
    bodyLayout()->addStretch();
    bodyLayout()->addLayout(row);
}

QString DatabaseDialog::databaseName() const { return m_name->text().trimmed(); }
QString DatabaseDialog::charset() const { return m_charset ? m_charset->currentText() : QString(); }
QString DatabaseDialog::collation() const { return m_collation ? m_collation->currentText() : QString(); }
QString DatabaseDialog::owner() const { return m_owner ? m_owner->text().trimmed() : QString(); }
