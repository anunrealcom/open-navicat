#include "ErDialog.h"

#include "IconManager.h"
#include "core/DbClient.h"
#include "core/Metadata.h"

#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {

struct FkRelation
{
    QString fromTable;
    QString fromColumn;
    QString toTable;
    QString toColumn;
};

/// 轻量缩放视图
class ZoomView : public QGraphicsView
{
public:
    explicit ZoomView(QGraphicsScene *s) : QGraphicsView(s)
    {
        setRenderHint(QPainter::Antialiasing);
        setDragMode(QGraphicsView::RubberBandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    }

protected:
    void wheelEvent(QWheelEvent *e) override
    {
        const qreal factor = e->angleDelta().y() > 0 ? 1.15 : 0.87;
        scale(factor, factor);
    }
};

} // namespace

ErDialog::ErDialog(DbClient *client, const QString &database, QWidget *parent)
    : FramelessDialog(tr("ER 图 - %1").arg(database), parent)
{
    setDialogSize(1100, 720);

    if (!client) return;

    auto *root = bodyLayout();
    root->setContentsMargins(14, 10, 14, 14);
    root->setSpacing(8);

    // 工具栏
    auto *toolBar = new QHBoxLayout;
    auto *refreshBtn = new QPushButton(IconManager::icon(QStringLiteral("refresh"),
                                                          QColor("#1B94FF"), QSize(14, 14)),
                                       tr("刷新"));
    refreshBtn->setObjectName(QStringLiteral("btnGhost"));
    refreshBtn->setMinimumHeight(30);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    auto *fitBtn = new QPushButton(tr("适应窗口"));
    fitBtn->setObjectName(QStringLiteral("btnGhost"));
    fitBtn->setMinimumHeight(30);
    fitBtn->setCursor(Qt::PointingHandCursor);
    auto *hint = new QLabel(tr("滚轮缩放，拖动平移，鼠标框选"));
    hint->setStyleSheet(QStringLiteral("color:#9AA3B5; font-size:12px;"));
    toolBar->addWidget(refreshBtn);
    toolBar->addWidget(fitBtn);
    toolBar->addStretch();
    toolBar->addWidget(hint);
    root->addLayout(toolBar);

    auto *scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    auto *view = new ZoomView(scene);
    view->setSceneRect(0, 0, 4000, 4000);
    view->setBackgroundBrush(QColor("#F7F8FA"));
    root->addWidget(view, 1);

    auto load = [=] {
        scene->clear();
        QString err;

        // 1. 列所有表
        const QList<TableInfo> tables = Metadata::tables(client, database, &err);
        if (tables.isEmpty()) {
            scene->addText(err.isEmpty() ? tr("当前数据库没有表") : err);
            return;
        }

        // 2. 读每个表的列
        QMap<QString, QList<ColumnInfo>> tableColumns;
        for (const TableInfo &t : tables) {
            auto cols = Metadata::columns(client, database, t.schema, t.name, &err);
            tableColumns[t.name] = cols;
        }

        // 3. 读外键关系
        QList<FkRelation> fks;
        {
            QString sql;
            switch (client->type()) {
            case DbType::MySQL:
                sql = QStringLiteral(
                    "SELECT TABLE_NAME, COLUMN_NAME, REFERENCED_TABLE_NAME, "
                    "REFERENCED_COLUMN_NAME "
                    "FROM information_schema.KEY_COLUMN_USAGE "
                    "WHERE TABLE_SCHEMA = '%1' AND REFERENCED_TABLE_NAME IS NOT NULL")
                          .arg(database);
                break;
            case DbType::PostgreSQL:
                sql = QStringLiteral(
                    "SELECT kcu.table_name, kcu.column_name, "
                    "kcu.referenced_table_name, kcu.referenced_column_name "
                    "FROM information_schema.key_column_usage kcu "
                    "WHERE kcu.table_catalog = '%1' "
                    "AND kcu.referenced_table_name IS NOT NULL")
                          .arg(database);
                break;
            case DbType::SQLite:
                // SQLite: 对每张表跑 PRAGMA foreign_key_list
                for (const TableInfo &t : tables) {
                    const QString pragma = QStringLiteral("PRAGMA foreign_key_list(%1)")
                                               .arg(client->quoteIdent(t.name));
                    DbResult res;
                    if (client->execute(pragma, {}, &res, nullptr) && res.hasResult) {
                        for (int r = 0; r < res.rows.size(); ++r) {
                            const auto &row = res.rows.at(r);
                            if (row.size() >= 5) {
                                FkRelation fk;
                                fk.fromTable = t.name;
                                fk.fromColumn = row.at(3); // from
                                fk.toTable = row.at(2);    // table
                                fk.toColumn = row.at(4);   // to
                                fks << fk;
                            }
                        }
                    }
                }
                goto after_fk;
            }
            DbResult res;
            if (client->execute(sql, {}, &res, nullptr) && res.hasResult) {
                for (int r = 0; r < res.rows.size(); ++r) {
                    const auto &row = res.rows.at(r);
                    if (row.size() >= 4) {
                        FkRelation fk;
                        fk.fromTable = row.at(0);
                        fk.fromColumn = row.at(1);
                        fk.toTable = row.at(2);
                        fk.toColumn = row.at(3);
                        fks << fk;
                    }
                }
            }
        }
        after_fk:

        // 4. 按 FK 关系数决定布局位置（简单网格 + 关联紧密的靠近）
        const int boxW = 220;
        const int rowH = 24;
        const int headerH = 32;
        const int gapX = 60;
        const int gapY = 50;
        const int cols = qMax(1, int(qCeil(qSqrt(tables.size()))));

        // 表矩形
        QMap<QString, QGraphicsRectItem *> rects;
        int idx = 0;
        for (const TableInfo &t : tables) {
            const int colCount = tableColumns.value(t.name).size();
            const int boxH = headerH + qMax(3, colCount) * rowH + 8;
            const int gx = (idx % cols) * (boxW + gapX);
            const int gy = (idx / cols) * (boxH + gapY) + 40;
            ++idx;

            auto *rect = scene->addRect(QRectF(gx, gy, boxW, boxH),
                                         QPen(QColor("#D7DDE5")),
                                         QBrush(QColor("#FFFFFF")));
            rect->setFlag(QGraphicsItem::ItemIsMovable);
            rect->setFlag(QGraphicsItem::ItemIsSelectable);
            rects[t.name] = rect;

            // 表头
            auto *titleBg = scene->addRect(QRectF(gx, gy, boxW, headerH),
                                           Qt::NoPen, QBrush(QColor("#1B94FF")));
            titleBg->setParentItem(rect);
            auto *titleText = scene->addSimpleText(t.name);
            titleText->setBrush(QBrush(QColor("#FFFFFF")));
            QFont tf; tf.setBold(true); tf.setPointSize(10);
            titleText->setFont(tf);
            titleText->setParentItem(rect);
            titleText->setPos(gx + 10, gy + 7);

            // 列
            int ly = gy + headerH + 4;
            for (const ColumnInfo &c : tableColumns.value(t.name)) {
                QColor colColor("#1F2937");
                QString prefix;
                if (c.primaryKey) { prefix = QStringLiteral("🔑 "); colColor = QColor("#B45309"); }
                else if (!c.nullable) { colColor = QColor("#0F766E"); }
                auto *colText = scene->addSimpleText(prefix + c.name);
                colText->setBrush(QBrush(colColor));
                colText->setParentItem(rect);
                colText->setPos(gx + 10, ly);
                ly += rowH;
            }
        }

        // 5. 画 FK 线
        QPen fkPen(QColor("#1B94FF"));
        fkPen.setWidthF(1.5);
        fkPen.setStyle(Qt::DashLine);
        for (const FkRelation &fk : fks) {
            auto *from = rects.value(fk.fromTable);
            auto *to = rects.value(fk.toTable);
            if (!from || !to) continue;
            QPointF p1 = from->mapToScene(from->boundingRect().center());
            QPointF p2 = to->mapToScene(to->boundingRect().center());

            // 从右出 / 从左入
            if (p1.x() < p2.x()) {
                p1.setX(from->sceneBoundingRect().right());
                p2.setX(to->sceneBoundingRect().left());
            } else {
                p1.setX(from->sceneBoundingRect().left());
                p2.setX(to->sceneBoundingRect().right());
            }

            // 画一条带中间拐点的折线
            const qreal midX = (p1.x() + p2.x()) / 2;
            QPainterPath path(p1);
            path.lineTo(midX, p1.y());
            path.lineTo(midX, p2.y());
            path.lineTo(p2);
            auto *line = scene->addPath(path, fkPen);
            line->setZValue(-1);

            // 箭头终点
            QPolygonF arrow;
            arrow << QPointF(0, 0) << QPointF(-8, -5) << QPointF(-8, 5);
            auto *arrowItem = scene->addPolygon(arrow, fkPen, QBrush(QColor("#1B94FF")));
            arrowItem->setPos(p2);
            arrowItem->setRotation(qAtan2(p2.y() - midX, p2.x() - midX) * 180 / M_PI);
            arrowItem->setZValue(-1);

            // 关系标签
            auto *label = scene->addSimpleText(QStringLiteral("%1 → %2")
                                                   .arg(fk.fromColumn, fk.toColumn));
            label->setBrush(QBrush(QColor("#5A6478")));
            QFont lf; lf.setPointSize(8);
            label->setFont(lf);
            label->setPos((p1.x() + p2.x()) / 2 - 30, (p1.y() + p2.y()) / 2 - 8);
        }

        scene->setSceneRect(scene->itemsBoundingRect().adjusted(-80, -80, 80, 80));
        view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    };

    connect(refreshBtn, &QPushButton::clicked, this, load);
    connect(fitBtn, &QPushButton::clicked, this, [=] {
        view->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    });

    load();
}
