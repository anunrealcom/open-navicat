#pragma once

#include "FramelessDialog.h"

class DbClient;

class ErDialog : public FramelessDialog
{
    Q_OBJECT
public:
    ErDialog(DbClient *client, const QString &database,
             QWidget *parent = nullptr);
};
