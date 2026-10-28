#pragma once

#include "FramelessDialog.h"
#include "core/ConnectionProfile.h"

class QLineEdit;
class QComboBox;
class QLabel;

class DatabaseDialog : public FramelessDialog
{
    Q_OBJECT
public:
    DatabaseDialog(DbType type, QWidget *parent = nullptr);

    QString databaseName() const;
    QString charset() const;
    QString collation() const;
    QString owner() const;

private:
    DbType m_type;
    QLineEdit *m_name = nullptr;
    QComboBox *m_charset = nullptr;
    QComboBox *m_collation = nullptr;
    QLineEdit *m_owner = nullptr;
};
