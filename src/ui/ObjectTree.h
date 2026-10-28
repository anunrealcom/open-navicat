#pragma once

#include <QTreeWidget>

class QTreeWidgetItem;

class ObjectTree : public QTreeWidget
{
    Q_OBJECT
public:
    enum NodeType
    {
        NodeConnection = 0,
        NodeDatabase,
        NodeTablesFolder,
        NodeTable,
        NodeColumn
    };

    enum ItemRole
    {
        NodeTypeRole = Qt::UserRole + 1,
        ProfileIdRole,
        DbNameRole,
        SchemaRole,
        TableNameRole,
        LoadedRole
    };

    explicit ObjectTree(QWidget *parent = nullptr);

    void reloadProfiles();

public slots:
    void newDatabaseForSelection();
    void newTableForSelection();
    void newQueryForSelection();

signals:
    void addConnectionRequested();
    void editConnectionRequested(const QString &profileId);
    void connectionClosed(const QString &profileId);
    void statusMessage(const QString &text);
    void openDataTableRequested(const QString &profileId, const QString &db,
                                const QString &schema, const QString &table);
    void designTableRequested(const QString &profileId, const QString &db,
                              const QString &schema, const QString &table);
    void newQueryRequested(const QString &profileId, const QString &db);

private slots:
    void onItemExpanded(QTreeWidgetItem *item);
    void onItemDoubleClicked(QTreeWidgetItem *item, int);
    void onCustomContextMenu(const QPoint &pos);

private:
    QTreeWidgetItem *findConnectionItem(const QString &profileId) const;
    bool openConnection(QTreeWidgetItem *item);
    void closeConnection(QTreeWidgetItem *item);
    void populateDatabases(QTreeWidgetItem *connItem);
    void populateTables(QTreeWidgetItem *dbItem);
    void populateColumns(QTreeWidgetItem *tableItem);
    void makeCurrentDatabase(QTreeWidgetItem *dbItem);
    void newDatabase(QTreeWidgetItem *connItem);
    void newTable(QTreeWidgetItem *dbOrFolderItem);
    void dropTable(QTreeWidgetItem *tableItem);
    void dropDatabase(QTreeWidgetItem *dbItem);
    void refreshNode(QTreeWidgetItem *item);

    class DbClient *clientOf(QTreeWidgetItem *item, bool requireOpen = true);
    static QTreeWidgetItem *connectionOf(QTreeWidgetItem *item);
    static QTreeWidgetItem *databaseOf(QTreeWidgetItem *item);
    static NodeType nodeType(QTreeWidgetItem *item);
};
