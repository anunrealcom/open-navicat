#pragma once

#include <QWidget>

class DbClient;
class QPlainTextEdit;
class QTableView;
class QStandardItemModel;
class QLabel;
class QListWidget;
class QSplitter;

class QueryView : public QWidget
{
    Q_OBJECT
public:
    explicit QueryView(QWidget *parent = nullptr);

    void setSource(DbClient *client, const QString &profileId,
                   const QString &profileName, const QString &database);
    QString tabKey() const;
    QString tabTitle() const;

private slots:
    void runQuery();
    void runExplain();
    void showProcessList();
    void showErDiagram();
    void formatSql();
    void toggleHistory();
    void loadHistory();
    void onHistorySelected(int row);
    void clearHistory();

private:
    DbClient *m_client = nullptr;
    QString m_profileId;
    QString m_profileName;
    QString m_database;

    QPlainTextEdit *m_editor = nullptr;
    QStandardItemModel *m_model = nullptr;
    QTableView *m_view = nullptr;
    QLabel *m_dbLabel = nullptr;
    QLabel *m_message = nullptr;

    QSplitter *m_splitter = nullptr;
    QListWidget *m_historyList = nullptr;
    QWidget *m_historyPanel = nullptr;
};
