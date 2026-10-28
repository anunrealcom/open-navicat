#pragma once

#include "FramelessDialog.h"
#include "core/ConnectionProfile.h"

class QComboBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class QCheckBox;

class ConnectionDialog : public FramelessDialog
{
    Q_OBJECT
public:
    explicit ConnectionDialog(QWidget *parent = nullptr);

    void setProfile(const ConnectionProfile &p);
    ConnectionProfile profile() const;

private slots:
    void onTypeChanged(int index);
    void onTestConnection();
    void togglePasswordVisible();
    void onSave();

private:
    ConnectionProfile m_editing;

    QLineEdit *m_name = nullptr;
    QComboBox *m_type = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QLineEdit *m_user = nullptr;
    QLineEdit *m_password = nullptr;
    QPushButton *m_eyeBtn = nullptr;
    QCheckBox *m_rememberPwd = nullptr;
    QLineEdit *m_database = nullptr;
    QPushButton *m_databaseBrowse = nullptr;
    QLabel *m_labelDb = nullptr;
    QLabel *m_resultLabel = nullptr;
};
