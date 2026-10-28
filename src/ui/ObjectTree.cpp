#include "ObjectTree.h"

#include "IconManager.h"
#include "core/ConnectionStore.h"
#include "core/DbClient.h"
#include "core/Metadata.h"
#include "core/SessionManager.h"
#include "ui/DatabaseDialog.h"
#include "ui/TableDesignerDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QMenu>
#include <QMessageBox>
#include <QTreeWidgetItem>

namespace {
const QColor kIconGrey("#9AA3B5");
const QColor kIconBlue("#1B94FF");
const QColor kIconAmber("#F59E0B");
const QColor kIconTable("#3B82F6");
const QColor kIconKey("#D97706");
}

ObjectTree::ObjectTree(QWidget *parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setIndentation(18);
    setAnimated(true);
    setRootIsDecorated(true);
    setUniformRowHeights(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setEditTriggers(NoEditTriggers);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setExpandsOnDoubleClick(false);

    connect(this, &QTreeWidget::itemExpanded, this, &ObjectTree::onItemExpanded);
    connect(this, &QTreeWidget::itemDoubleClicked, this, &ObjectTree::onItemDoubleClicked);
    connect(this, &QTreeWidget::customContextMenuRequested, this, &ObjectTree::onCustomContextMenu);

    reloadProfiles();
}

ObjectTree::NodeType ObjectTree::nodeType(QTreeWidgetItem *item)
{
    return static_cast<NodeType>(item->data(0, NodeTypeRole).toInt());
}

QTreeWidgetItem *ObjectTree::connectionOf(QTreeWidgetItem *item)
{
    while (item && nodeType(item) != NodeConnection)
        item = item->parent();
    return item;
}

QTreeWidgetItem *ObjectTree::databaseOf(QTreeWidgetItem *item)
{
    while (item && nodeType(item) != NodeDatabase)
        item = item->parent();
    return item;
}

DbClient *ObjectTree::clientOf(QTreeWidgetItem *item, bool requireOpen)
{
    QTreeWidgetItem *conn = connectionOf(item);
    if (!conn)
        return nullptr;
    const QString id = conn->data(0, ProfileIdRole).toString();
    DbClient *c = SessionManager::instance().client(id);
    if (requireOpen && (!c || !c->isOpen()))
        return nullptr;
    return c;
}

QTreeWidgetItem *ObjectTree::findConnectionItem(const QString &profileId) const
{
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem *it = topLevelItem(i);
        if (it->data(0, ProfileIdRole).toString() == profileId)
            return it;
    }
    return nullptr;
}

void ObjectTree::reloadProfiles()
{
    clear();
    const auto profiles = ConnectionStore::instance().loadAll();
    for (const ConnectionProfile &p : profiles) {
        auto *item = new QTreeWidgetItem(this);
        item->setData(0, NodeTypeRole, NodeConnection);
        item->setData(0, ProfileIdRole, p.id);
        item->setText(0, p.name);
        item->setToolTip(0, QStringLiteral("%1@%2:%3 (%4)")
                                   .arg(p.username, p.host).arg(p.port).arg(p.typeDisplay()));
        const bool open = SessionManager::instance().isOpen(p.id);
        item->setIcon(0, IconManager::icon(QStringLiteral("server"),
                                           open ? kIconBlue : kIconGrey, QSize(17, 17)));
        QFont f = item->font(0);
        f.setBold(open);
        item->setFont(0, f);
        if (open) {
            populateDatabases(item);
            item->setExpanded(true);
        }
    }
}

void ObjectTree::newDatabaseForSelection()
{
    QTreeWidgetItem *item = currentItem();
    QTreeWidgetItem *conn = item ? connectionOf(item) : nullptr;
    if (!conn) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个连接"));
        return;
    }
    if (!SessionManager::instance().isOpen(conn->data(0, ProfileIdRole).toString()) &&
        !openConnection(conn))
        return;
    newDatabase(conn);
}

void ObjectTree::newTableForSelection()
{
    QTreeWidgetItem *item = currentItem();
    if (!item) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个数据库"));
        return;
    }
    QTreeWidgetItem *conn = connectionOf(item);
    if (!conn) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个数据库"));
        return;
    }
    if (!SessionManager::instance().isOpen(conn->data(0, ProfileIdRole).toString()) &&
        !openConnection(conn))
        return;
    QTreeWidgetItem *db = databaseOf(item);
    if (!db) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个数据库"));
        return;
    }
    newTable(nodeType(item) == NodeTablesFolder ? item : db);
}

void ObjectTree::newQueryForSelection()
{
    QTreeWidgetItem *item = currentItem();
    QTreeWidgetItem *conn = item ? connectionOf(item) : nullptr;
    if (!conn) {
        QMessageBox::information(this, tr("提示"), tr("请先选择一个连接或数据库"));
        return;
    }
    if (!SessionManager::instance().isOpen(conn->data(0, ProfileIdRole).toString()) &&
        !openConnection(conn))
        return;
    QTreeWidgetItem *db = databaseOf(item);
    if (db)
        makeCurrentDatabase(db);
    emit newQueryRequested(conn->data(0, ProfileIdRole).toString(),
                           clientOf(conn)->currentDatabase());
}

bool ObjectTree::openConnection(QTreeWidgetItem *item)
{
    const QString id = item->data(0, ProfileIdRole).toString();
    const ConnectionProfile profile = ConnectionStore::instance().load(id);
    QString err;
    DbClient *c = SessionManager::instance().open(profile, &err);
    if (!c) {
        QMessageBox::warning(this, tr("连接失败"), err);
        return false;
    }
    item->setIcon(0, IconManager::icon(QStringLiteral("server"), kIconBlue, QSize(17, 17)));
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    populateDatabases(item);
    item->setExpanded(true);
    emit statusMessage(tr("已连接：%1").arg(profile.name));
    return true;
}

void ObjectTree::closeConnection(QTreeWidgetItem *item)
{
    const QString id = item->data(0, ProfileIdRole).toString();
    SessionManager::instance().close(id);
    emit connectionClosed(id);
    item->takeChildren();
    item->setIcon(0, IconManager::icon(QStringLiteral("server"), kIconGrey, QSize(17, 17)));
    QFont f = item->font(0);
    f.setBold(false);
    item->setFont(0, f);
    emit statusMessage(tr("连接已关闭"));
}

void ObjectTree::populateDatabases(QTreeWidgetItem *connItem)
{
    DbClient *c = clientOf(connItem);
    connItem->takeChildren();
    if (!c)
        return;

    QString err;
    const QStringList dbs = Metadata::databases(c, &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, tr("读取数据库列表失败"), err);
        return;
    }

    for (const QString &db : dbs) {
        auto *dbItem = new QTreeWidgetItem(connItem);
        dbItem->setData(0, NodeTypeRole, NodeDatabase);
        dbItem->setData(0, DbNameRole, db);
        dbItem->setData(0, LoadedRole, false);
        dbItem->setText(0, db);
        dbItem->setIcon(0, IconManager::icon(QStringLiteral("database"), kIconBlue,
                                             QSize(16, 16)));
        if (db.compare(c->currentDatabase(), Qt::CaseInsensitive) == 0) {
            QFont f = dbItem->font(0);
            f.setBold(true);
            dbItem->setFont(0, f);
        }
    }
}

void ObjectTree::populateTables(QTreeWidgetItem *dbItem)
{
    DbClient *c = clientOf(dbItem);
    dbItem->takeChildren();
    if (!c)
        return;
    const QString db = dbItem->data(0, DbNameRole).toString();

    QString err;
    const QList<TableInfo> tables = Metadata::tables(c, db, &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, tr("读取表列表失败"), err);
        return;
    }

    auto *folder = new QTreeWidgetItem(dbItem);
    folder->setData(0, NodeTypeRole, NodeTablesFolder);
    folder->setData(0, DbNameRole, db);
    folder->setText(0, tr("表 (%1)").arg(tables.size()));
    folder->setIcon(0, IconManager::icon(QStringLiteral("folder"), kIconAmber, QSize(16, 16)));

    for (const TableInfo &t : tables) {
        auto *tbl = new QTreeWidgetItem(folder);
        tbl->setData(0, NodeTypeRole, NodeTable);
        tbl->setData(0, DbNameRole, db);
        tbl->setData(0, SchemaRole, t.schema);
        tbl->setData(0, TableNameRole, t.name);
        tbl->setData(0, LoadedRole, false);
        const QString label = t.schema.isEmpty() || t.schema == QLatin1String("public")
                                  ? t.name
                                  : t.schema + QLatin1Char('.') + t.name;
        tbl->setText(0, label);
        tbl->setIcon(0, IconManager::icon(QStringLiteral("table"), kIconTable, QSize(16, 16)));
    }
    folder->setExpanded(true);
    dbItem->setData(0, LoadedRole, true);
}

void ObjectTree::populateColumns(QTreeWidgetItem *tableItem)
{
    DbClient *c = clientOf(tableItem);
    tableItem->takeChildren();
    if (!c)
        return;
    const QString db = tableItem->data(0, DbNameRole).toString();
    const QString schema = tableItem->data(0, SchemaRole).toString();
    const QString table = tableItem->data(0, TableNameRole).toString();

    QString err;
    const QList<ColumnInfo> cols = Metadata::columns(c, db, schema, table, &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, tr("读取字段失败"), err);
        return;
    }
    for (const ColumnInfo &ci : cols) {
        auto *col = new QTreeWidgetItem(tableItem);
        col->setData(0, NodeTypeRole, NodeColumn);
        col->setText(0, QStringLiteral("%1  ·  %2").arg(ci.name, ci.fullType));
        col->setToolTip(0, QStringLiteral("%1 %2%3%4")
                                   .arg(ci.name, ci.fullType,
                                        ci.nullable ? QStringLiteral(" NULL") : QStringLiteral(" NOT NULL"),
                                        ci.primaryKey ? QStringLiteral(" PK") : QString()));
        col->setIcon(0, IconManager::icon(ci.primaryKey ? QStringLiteral("key")
                                                       : QStringLiteral("field"),
                                         ci.primaryKey ? kIconKey : kIconGrey, QSize(15, 15)));
    }
    tableItem->setData(0, LoadedRole, true);
}

void ObjectTree::makeCurrentDatabase(QTreeWidgetItem *dbItem)
{
    DbClient *c = clientOf(dbItem);
    if (!c)
        return;
    const QString db = dbItem->data(0, DbNameRole).toString();
    if (db.compare(c->currentDatabase(), Qt::CaseInsensitive) == 0)
        return;
    QString err;
    if (!c->setCurrentDatabase(db, &err)) {
        QMessageBox::warning(this, tr("切换数据库失败"), err);
        return;
    }
    QTreeWidgetItem *conn = connectionOf(dbItem);
    if (conn) {
        for (int i = 0; i < conn->childCount(); ++i) {
            QTreeWidgetItem *child = conn->child(i);
            QFont f = child->font(0);
            f.setBold(child == dbItem);
            child->setFont(0, f);
            child->setData(0, LoadedRole, false);
            child->takeChildren();
        }
    }
    emit statusMessage(tr("当前数据库：%1").arg(db));
}

void ObjectTree::onItemExpanded(QTreeWidgetItem *item)
{
    switch (nodeType(item)) {
    case NodeDatabase:
        if (!item->data(0, LoadedRole).toBool())
            populateTables(item);
        break;
    case NodeTable:
        if (!item->data(0, LoadedRole).toBool())
            populateColumns(item);
        break;
    default:
        break;
    }
}

void ObjectTree::onItemDoubleClicked(QTreeWidgetItem *item, int)
{
    switch (nodeType(item)) {
    case NodeConnection:
        if (SessionManager::instance().isOpen(item->data(0, ProfileIdRole).toString()))
            item->setExpanded(!item->isExpanded());
        else
            openConnection(item);
        break;
    case NodeDatabase:
        makeCurrentDatabase(item);
        item->setExpanded(true);
        break;
    case NodeTable: {
        QTreeWidgetItem *conn = connectionOf(item);
        if (conn)
            emit openDataTableRequested(conn->data(0, ProfileIdRole).toString(),
                                        item->data(0, DbNameRole).toString(),
                                        item->data(0, SchemaRole).toString(),
                                        item->data(0, TableNameRole).toString());
        break;
    }
    default:
        break;
    }
}

void ObjectTree::onCustomContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = itemAt(pos);
    QMenu menu(this);

    if (!item) {
        menu.addAction(IconManager::icon(QStringLiteral("plug"), kIconBlue, QSize(16, 16)),
                       tr("新建连接"), this, &ObjectTree::addConnectionRequested);
        menu.addAction(IconManager::icon(QStringLiteral("refresh"), kIconGrey, QSize(15, 15)),
                       tr("刷新列表"), this, [this] { reloadProfiles(); });
        menu.exec(viewport()->mapToGlobal(pos));
        return;
    }

    switch (nodeType(item)) {
    case NodeConnection: {
        const bool opened = SessionManager::instance().isOpen(
            item->data(0, ProfileIdRole).toString());
        if (opened) {
            menu.addAction(IconManager::icon(QStringLiteral("terminal"), kIconBlue, QSize(15, 15)),
                           tr("新建查询"), this, [this, item] {
                               emit newQueryRequested(item->data(0, ProfileIdRole).toString(),
                                                      clientOf(item)->currentDatabase());
                           });
            menu.addAction(IconManager::icon(QStringLiteral("database_add"), kIconBlue,
                                             QSize(16, 16)),
                           tr("新建数据库"), this, [this, item] { newDatabase(item); });
            menu.addSeparator();
            menu.addAction(IconManager::icon(QStringLiteral("refresh"), kIconGrey, QSize(15, 15)),
                           tr("刷新"), this, [this, item] { refreshNode(item); });
            menu.addAction(IconManager::icon(QStringLiteral("power"), QColor("#E5484D"),
                                             QSize(15, 15)),
                           tr("关闭连接"), this, [this, item] { closeConnection(item); });
        } else {
            menu.addAction(IconManager::icon(QStringLiteral("plug"), kIconBlue, QSize(16, 16)),
                           tr("打开连接"), this, [this, item] { openConnection(item); });
        }
        menu.addSeparator();
        menu.addAction(IconManager::icon(QStringLiteral("edit"), kIconGrey, QSize(15, 15)),
                       tr("编辑连接"), this,
                       [this, item] { emit editConnectionRequested(item->data(0, ProfileIdRole)
                                                                        .toString()); });
        menu.addAction(IconManager::icon(QStringLiteral("trash"), QColor("#E5484D"),
                                         QSize(15, 15)),
                       tr("删除连接"), this, [this, item] {
                           const QString id = item->data(0, ProfileIdRole).toString();
                           if (QMessageBox::question(this, tr("删除连接"),
                                                     tr("确定删除连接“%1”吗？").arg(item->text(0))) !=
                               QMessageBox::Yes)
                               return;
                           SessionManager::instance().close(id);
                           ConnectionStore::instance().remove(id);
                           reloadProfiles();
                       });
        break;
    }
    case NodeDatabase: {
        menu.addAction(IconManager::icon(QStringLiteral("table_add"), kIconTable, QSize(16, 16)),
                       tr("新建表"), this, [this, item] { newTable(item); });
        menu.addAction(IconManager::icon(QStringLiteral("terminal"), kIconBlue, QSize(15, 15)),
                       tr("新建查询"), this, [this, item] {
                           QTreeWidgetItem *conn = connectionOf(item);
                           makeCurrentDatabase(item);
                           if (conn)
                               emit newQueryRequested(conn->data(0, ProfileIdRole).toString(),
                                                      item->data(0, DbNameRole).toString());
                       });
        menu.addAction(IconManager::icon(QStringLiteral("refresh"), kIconGrey, QSize(15, 15)),
                       tr("刷新"), this, [this, item] { refreshNode(item); });
        menu.addSeparator();
        menu.addAction(IconManager::icon(QStringLiteral("trash"), QColor("#E5484D"),
                                         QSize(15, 15)),
                       tr("删除数据库"), this, [this, item] { dropDatabase(item); });
        break;
    }
    case NodeTablesFolder:
        menu.addAction(IconManager::icon(QStringLiteral("table_add"), kIconTable, QSize(16, 16)),
                       tr("新建表"), this, [this, item] { newTable(item); });
        menu.addAction(IconManager::icon(QStringLiteral("refresh"), kIconGrey, QSize(15, 15)),
                       tr("刷新"), this, [this, item] { refreshNode(item); });
        break;
    case NodeTable: {
        menu.addAction(IconManager::icon(QStringLiteral("table"), kIconTable, QSize(15, 15)),
                       tr("打开表数据"), this, [this, item] {
                           QTreeWidgetItem *conn = connectionOf(item);
                           if (conn)
                               emit openDataTableRequested(conn->data(0, ProfileIdRole).toString(),
                                                           item->data(0, DbNameRole).toString(),
                                                           item->data(0, SchemaRole).toString(),
                                                           item->data(0, TableNameRole).toString());
                       });
        menu.addAction(IconManager::icon(QStringLiteral("edit"), kIconGrey, QSize(15, 15)),
                       tr("设计表"), this, [this, item] {
                           QTreeWidgetItem *conn = connectionOf(item);
                           if (!conn)
                               return;
                           DbClient *c = clientOf(item);
                           const QString db = item->data(0, DbNameRole).toString();
                           const QString schema = item->data(0, SchemaRole).toString();
                           const QString table = item->data(0, TableNameRole).toString();
                           QString err;
                           const QList<ColumnInfo> oldCols =
                               Metadata::columns(c, db, schema, table, &err);
                           if (!err.isEmpty() && oldCols.isEmpty()) {
                               QMessageBox::warning(this, tr("读取表结构失败"), err);
                               return;
                           }
                           TableDesignerDialog dlg(c->type(), db, schema, this);
                           dlg.setWindowTitle(tr("设计表 - %1").arg(table));
                           dlg.setTableName(table);
                           dlg.loadColumns(oldCols);
                           if (dlg.exec() != QDialog::Accepted)
                               return;
                           const QStringList sqls = Metadata::alterTableSqls(
                               c->type(), db, schema, table, oldCols, dlg.tableSpec());
                           for (const QString &s : sqls) {
                               if (s.trimmed().isEmpty())
                                   continue;
                               if (!c->execute(s, {}, nullptr, &err)) {
                                   QMessageBox::warning(this, tr("修改表失败"),
                                                        s + QStringLiteral("\n\n") + err);
                                   return;
                               }
                           }
                           item->setData(0, LoadedRole, false);
                           populateColumns(item);
                           emit statusMessage(tr("表 %1 结构已更新").arg(table));
                       });
        menu.addSeparator();
        menu.addAction(IconManager::icon(QStringLiteral("trash"), QColor("#E5484D"),
                                         QSize(15, 15)),
                       tr("删除表"), this, [this, item] { dropTable(item); });
        break;
    }
    case NodeColumn:
        menu.addAction(IconManager::icon(QStringLiteral("field"), kIconGrey, QSize(15, 15)),
                       tr("复制列名"), this, [item] {
                           QApplication::clipboard()->setText(item->text(0).section(
                               QStringLiteral("  ·  "), 0, 0));
                       });
        break;
    }
    menu.exec(viewport()->mapToGlobal(pos));
}

void ObjectTree::newDatabase(QTreeWidgetItem *connItem)
{
    DbClient *c = clientOf(connItem);
    if (!c)
        return;
    DatabaseDialog dlg(c->type(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString owner = dlg.owner();
    const QString sql = Metadata::createDatabaseSql(
        c->type(), c->quoteIdent(dlg.databaseName()), dlg.charset(), dlg.collation(),
        owner.isEmpty() ? QString() : c->quoteIdent(owner));
    QString err;
    if (!c->execute(sql, {}, nullptr, &err)) {
        QMessageBox::warning(this, tr("创建数据库失败"), err);
        return;
    }
    populateDatabases(connItem);
    emit statusMessage(tr("数据库 %1 已创建").arg(dlg.databaseName()));
}

void ObjectTree::newTable(QTreeWidgetItem *dbOrFolderItem)
{
    QTreeWidgetItem *dbItem = nodeType(dbOrFolderItem) == NodeTablesFolder
                                  ? dbOrFolderItem->parent()
                                  : dbOrFolderItem;
    DbClient *c = clientOf(dbItem);
    if (!c)
        return;
    const QString db = dbItem->data(0, DbNameRole).toString();

    TableDesignerDialog dlg(c->type(), db, QString(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const TableSpec spec = dlg.tableSpec();
    if (spec.columns.isEmpty()) {
        QMessageBox::warning(this, tr("无法创建表"), tr("请至少添加一个字段"));
        return;
    }
    QString err;
    const QString sql = Metadata::createTableSql(c->type(), db, spec);
    if (!c->execute(sql, {}, nullptr, &err)) {
        QMessageBox::warning(this, tr("创建表失败"), sql + QStringLiteral("\n\n") + err);
        return;
    }
    dbItem->setData(0, LoadedRole, false);
    populateTables(dbItem);
    dbItem->setExpanded(true);
    emit statusMessage(tr("表 %1 已创建").arg(spec.name));
}

void ObjectTree::dropTable(QTreeWidgetItem *tableItem)
{
    DbClient *c = clientOf(tableItem);
    if (!c)
        return;
    const QString db = tableItem->data(0, DbNameRole).toString();
    const QString schema = tableItem->data(0, SchemaRole).toString();
    const QString table = tableItem->data(0, TableNameRole).toString();
    if (QMessageBox::question(this, tr("删除表"),
                              tr("确定删除表 %1 吗？该操作不可恢复！").arg(table)) != QMessageBox::Yes)
        return;
    QString err;
    if (!c->execute(Metadata::dropTableSql(c->type(), db, schema, table), {}, nullptr, &err)) {
        QMessageBox::warning(this, tr("删除表失败"), err);
        return;
    }
    QTreeWidgetItem *dbItem = databaseOf(tableItem);
    if (dbItem) {
        dbItem->setData(0, LoadedRole, false);
        populateTables(dbItem);
        dbItem->setExpanded(true);
    }
}

void ObjectTree::dropDatabase(QTreeWidgetItem *dbItem)
{
    DbClient *c = clientOf(dbItem);
    if (!c)
        return;
    const QString db = dbItem->data(0, DbNameRole).toString();
    if (db.compare(c->currentDatabase(), Qt::CaseInsensitive) == 0 &&
        c->type() == DbType::PostgreSQL) {
        QMessageBox::warning(this, tr("无法删除"),
                             tr("PostgreSQL 不能删除当前正在连接的数据库，请先切换到其他数据库。"));
        return;
    }
    if (QMessageBox::question(this, tr("删除数据库"),
                              tr("确定删除数据库 %1 吗？该操作不可恢复！").arg(db)) != QMessageBox::Yes)
        return;
    QString err;
    if (!c->execute(Metadata::dropDatabaseSql(c->type(), c->quoteIdent(db)), {}, nullptr, &err)) {
        QMessageBox::warning(this, tr("删除数据库失败"), err);
        return;
    }
    refreshNode(connectionOf(dbItem));
}

void ObjectTree::refreshNode(QTreeWidgetItem *item)
{
    if (!item)
        return;
    switch (nodeType(item)) {
    case NodeConnection:
        if (SessionManager::instance().isOpen(item->data(0, ProfileIdRole).toString()))
            populateDatabases(item);
        break;
    case NodeDatabase:
        item->setData(0, LoadedRole, false);
        item->takeChildren();
        populateTables(item);
        item->setExpanded(true);
        break;
    case NodeTablesFolder:
        if (item->parent())
            refreshNode(item->parent());
        break;
    case NodeTable:
        item->setData(0, LoadedRole, false);
        item->takeChildren();
        populateColumns(item);
        break;
    default:
        break;
    }
}
