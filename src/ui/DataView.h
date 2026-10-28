#pragma once

#include <QWidget>

class DbClient;
class EditableTableModel;
class QTableView;
class QLabel;
class QLineEdit;
class QComboBox;
class QPushButton;

class DataView : public QWidget
{
    Q_OBJECT
public:
    explicit DataView(QWidget *parent = nullptr);

    void setSource(DbClient *client, const QString &profileId, const QString &profileName,
                   const QString &database, const QString &schema, const QString &table);
    QString tabKey() const;
    QString tabTitle() const;

private slots:
    void refresh();
    void firstPage();
    void prevPage();
    void nextPage();
    void lastPage();
    void addRow();
    void deleteSelectedRows();
    void onContextMenu(const QPoint &pos);
    void onHeaderClicked(int column);
    void exportCsv();
    void copySelected();

private:
    void updatePager();
    QString currentOrderBy() const;

    DbClient *m_client = nullptr;
    QString m_profileId;
    QString m_profileName;
    QString m_database;
    QString m_schema;
    QString m_table;

    EditableTableModel *m_model = nullptr;
    QTableView *m_view = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_pageInfo = nullptr;
    QLineEdit *m_where = nullptr;
    QComboBox *m_pageSize = nullptr;
    int m_page = 0;

    // 排序状态：-1=降序，0=无，1=升序。用于在表头显示 ▲/▼
    int m_sortColumn = -1;
    int m_sortOrder = 0;
};
