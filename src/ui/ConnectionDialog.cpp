#include "ConnectionDialog.h"

#include "IconManager.h"
#include "core/DbClient.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

static QLineEdit *makeLine(const QString &placeholder, bool password = false)
{
    auto *e = new QLineEdit;
    e->setPlaceholderText(placeholder);
    e->setMinimumHeight(32);
    if (password)
        e->setEchoMode(QLineEdit::Password);
    return e;
}

ConnectionDialog::ConnectionDialog(QWidget *parent)
    : FramelessDialog(tr("新建连接"), parent)
{
    setDialogSize(480, 620);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(14);

    m_name = makeLine(tr("例如：本地开发库"));
    m_type = new QComboBox;
    m_type->setMinimumHeight(32);
    m_type->addItem(IconManager::icon(QStringLiteral("database"), QColor("#1B94FF")),
                    QStringLiteral("MySQL"), static_cast<int>(DbType::MySQL));
    m_type->addItem(IconManager::icon(QStringLiteral("database"), QColor("#336791")),
                    QStringLiteral("PostgreSQL"), static_cast<int>(DbType::PostgreSQL));
    m_type->addItem(IconManager::icon(QStringLiteral("database"), QColor("#003B57")),
                    QStringLiteral("SQLite"), static_cast<int>(DbType::SQLite));

    m_host = makeLine(QStringLiteral("127.0.0.1"));
    m_port = new QSpinBox;
    m_port->setRange(1, 65535);
    m_port->setValue(3306);
    m_port->setMinimumHeight(32);

    m_user = makeLine(tr("root"));
    m_password = makeLine(tr("请输入密码"), true);
    m_eyeBtn = new QPushButton;
    m_eyeBtn->setObjectName(QStringLiteral("iconButton"));
    m_eyeBtn->setFixedSize(34, 32);
    m_eyeBtn->setIcon(IconManager::icon(QStringLiteral("eye"), QColor("#6B7280"),
                                        QSize(17, 17)));
    m_eyeBtn->setCursor(Qt::PointingHandCursor);
    m_eyeBtn->setCheckable(true);
    connect(m_eyeBtn, &QPushButton::clicked, this, &ConnectionDialog::togglePasswordVisible);
    auto *pwRow = new QHBoxLayout;
    pwRow->setContentsMargins(0, 0, 0, 0);
    pwRow->setSpacing(6);
    pwRow->addWidget(m_password);
    pwRow->addWidget(m_eyeBtn);
    m_rememberPwd = new QCheckBox(tr("记住密码"));
    m_rememberPwd->setCursor(Qt::PointingHandCursor);
    m_rememberPwd->setChecked(true);
    pwRow->addWidget(m_rememberPwd);
    auto *pwRowW = new QWidget;
    pwRowW->setLayout(pwRow);

    m_database = makeLine(tr("可选，不填则打开连接后选择"));
    m_databaseBrowse = new QPushButton(tr("浏览…"));
    m_databaseBrowse->setMinimumHeight(32);
    m_databaseBrowse->setCursor(Qt::PointingHandCursor);
    m_databaseBrowse->setObjectName(QStringLiteral("btnGhost"));
    m_databaseBrowse->hide();
    connect(m_databaseBrowse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("选择 SQLite 数据库文件"), QString(),
            tr("SQLite 数据库 (*.db *.sqlite *.sqlite3);;所有文件 (*.*)"));
        if (!path.isEmpty())
            m_database->setText(path);
    });
    auto *dbRow = new QHBoxLayout;
    dbRow->setContentsMargins(0, 0, 0, 0);
    dbRow->setSpacing(6);
    dbRow->addWidget(m_database, 1);
    dbRow->addWidget(m_databaseBrowse);
    auto *dbRowW = new QWidget;
    dbRowW->setLayout(dbRow);

    m_labelDb = new QLabel(tr("默认数据库"));

    form->addRow(tr("连接名称"), m_name);
    form->addRow(tr("数据库类型"), m_type);
    form->addRow(tr("主机地址"), m_host);
    form->addRow(tr("端口"), m_port);
    form->addRow(tr("用户名"), m_user);
    form->addRow(tr("密码"), pwRowW);
    form->addRow(m_labelDb, dbRowW);

    auto *testBtn = new QPushButton(IconManager::icon(QStringLiteral("check"),
                                                       QColor("#1B94FF"), QSize(16, 16)),
                                    tr("测试连接"));
    testBtn->setMinimumHeight(34);
    testBtn->setCursor(Qt::PointingHandCursor);
    connect(testBtn, &QPushButton::clicked, this, &ConnectionDialog::onTestConnection);

    m_resultLabel = new QLabel;
    m_resultLabel->setWordWrap(true);
    m_resultLabel->setVisible(false);
    m_resultLabel->setObjectName(QStringLiteral("testResult"));

    auto *saveBtn = new QPushButton(IconManager::icon(QStringLiteral("save"), QColor("white"),
                                                      QSize(16, 16)),
                                    tr("保存连接"));
    saveBtn->setObjectName(QStringLiteral("btnPrimary"));
    saveBtn->setMinimumHeight(36);
    saveBtn->setCursor(Qt::PointingHandCursor);
    auto *cancelBtn = new QPushButton(tr("取消"));
    cancelBtn->setMinimumHeight(36);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(saveBtn, &QPushButton::clicked, this, &ConnectionDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(10);
    btnRow->addWidget(cancelBtn, 1);
    btnRow->addWidget(saveBtn, 1);

    bodyLayout()->addLayout(form);
    bodyLayout()->addWidget(testBtn);
    bodyLayout()->addWidget(m_resultLabel);
    bodyLayout()->addStretch();
    bodyLayout()->addLayout(btnRow);

    connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionDialog::onTypeChanged);

    m_host->setText(QStringLiteral("127.0.0.1"));
    m_user->setText(QStringLiteral("root"));
    onTypeChanged(m_type->currentIndex());
}

void ConnectionDialog::setProfile(const ConnectionProfile &p)
{
    m_editing = p;
    m_name->setText(p.name);
    int idx = 0;
    if (p.type == DbType::PostgreSQL) idx = 1;
    else if (p.type == DbType::SQLite) idx = 2;
    m_type->setCurrentIndex(idx);
    m_host->setText(p.host);
    m_port->setValue(p.port > 0 ? p.port : ConnectionProfile::defaultPort(p.type));
    m_user->setText(p.username);
    m_password->setText(p.password);
    m_rememberPwd->setChecked(!p.password.isEmpty());
    m_database->setText(p.database);
}

void ConnectionDialog::onTypeChanged(int index)
{
    const auto t = static_cast<DbType>(m_type->itemData(index).toInt());
    m_port->setValue(ConnectionProfile::defaultPort(t));

    const bool isSqlite = (t == DbType::SQLite);
    m_host->setVisible(!isSqlite);
    m_port->setVisible(!isSqlite);
    m_user->setVisible(!isSqlite);
    m_password->setVisible(!isSqlite);
    m_eyeBtn->setVisible(!isSqlite);
    m_rememberPwd->setVisible(!isSqlite);
    m_databaseBrowse->setVisible(isSqlite);
    m_labelDb->setText(isSqlite ? tr("数据库文件") : tr("默认数据库"));
    m_database->setPlaceholderText(
        isSqlite ? tr("选择 .db 文件路径，或留空为 :memory:")
                 : tr("可选，不填则打开连接后选择"));
}

void ConnectionDialog::togglePasswordVisible()
{
    const bool visible = m_eyeBtn->isChecked();
    m_password->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
    m_eyeBtn->setIcon(IconManager::icon(visible ? QStringLiteral("eye_off")
                                                : QStringLiteral("eye"),
                                        QColor("#6B7280"), QSize(17, 17)));
}

void ConnectionDialog::onTestConnection()
{
    if (m_name->text().trimmed().isEmpty()) {
        m_resultLabel->setVisible(true);
        m_resultLabel->setStyleSheet(QStringLiteral("color:#E5484D;"));
        m_resultLabel->setText(tr("请先填写连接名称"));
        return;
    }

    m_resultLabel->setVisible(true);
    m_resultLabel->setStyleSheet(QStringLiteral("color:#7A8499;"));
    m_resultLabel->setText(tr("正在连接..."));

    DbClient *client = DbClient::create(static_cast<DbType>(m_type->currentData().toInt()));
    QString err;
    const bool ok = client->open(profile(), &err);
    if (ok) {
        m_resultLabel->setStyleSheet(QStringLiteral("color:#16A34A;"));
        m_resultLabel->setText(tr("✓ 连接成功，服务器可达。"));
        client->close();
    } else {
        m_resultLabel->setStyleSheet(QStringLiteral("color:#E5484D;"));
        m_resultLabel->setText(tr("✕ 连接失败：%1").arg(err));
    }
    delete client;
}

void ConnectionDialog::onSave()
{
    if (m_name->text().trimmed().isEmpty()) {
        m_resultLabel->setVisible(true);
        m_resultLabel->setStyleSheet(QStringLiteral("color:#E5484D;"));
        m_resultLabel->setText(tr("请填写连接名称"));
        return;
    }
    accept();
}

ConnectionProfile ConnectionDialog::profile() const
{
    ConnectionProfile p = m_editing;
    p.name = m_name->text().trimmed();
    p.type = static_cast<DbType>(m_type->currentData().toInt());
    p.host = m_host->text().trimmed();
    p.port = m_port->value();
    p.username = m_user->text().trimmed();
    p.password = m_rememberPwd && m_rememberPwd->isVisible() ? m_password->text() : QString();
    p.database = m_database->text().trimmed();
    return p;
}
