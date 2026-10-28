#include "MainWindow.h"

#include "ConnectionDialog.h"
#include "DataView.h"
#include "IconManager.h"
#include "ObjectTree.h"
#include "QueryView.h"
#include "TitleBar.h"
#include "core/ConnectionStore.h"
#include "core/DbClient.h"
#include "core/SessionManager.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#if defined(Q_OS_WIN)
#include <windowsx.h>

#include <windows.h>
#endif

namespace {
constexpr int kResizeBorder = 6;

QPushButton *toolButton(const QString &icon, const QColor &color,
                        const QString &text, const QString &tip)
{
    auto *b = new QPushButton(IconManager::icon(icon, color, QSize(17, 17)), text);
    b->setObjectName(QStringLiteral("sideToolButton"));
    b->setToolTip(tip);
    b->setMinimumHeight(34);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QPushButton *iconToolButton(const QString &icon, const QColor &color, const QString &tip)
{
    auto *b = new QPushButton;
    b->setIcon(IconManager::icon(icon, color, QSize(17, 17)));
    b->setObjectName(QStringLiteral("sideIconButton"));
    b->setFixedSize(36, 34);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("DataMG - 数据库管理工具"));
    setWindowIcon(IconManager::appIcon());
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(1280, 820);
    setMinimumSize(960, 600);

    buildUi();
    showWelcome(true);
}

void MainWindow::buildUi()
{
    auto *frame = new QWidget;
    frame->setObjectName(QStringLiteral("appFrame"));
    frame->setProperty("maximized", false);
    auto *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    m_titleBar = new TitleBar(this);
    frameLayout->addWidget(m_titleBar);

    // ---- 主体：左侧对象树 + 右侧工作区 ----
    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);

    // 左侧面板
    auto *leftPanel = new QFrame;
    leftPanel->setObjectName(QStringLiteral("leftPanel"));
    leftPanel->setMinimumWidth(260);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(10, 10, 10, 10);
    leftLayout->setSpacing(8);

    auto *btnConn = toolButton(QStringLiteral("plug"), QColor("#1B94FF"), tr("新建连接"),
                               tr("创建一个新的数据库连接"));
    auto *btnQuery = iconToolButton(QStringLiteral("terminal"), QColor("#5A6478"),
                                    tr("新建查询（对当前数据库）"));
    auto *btnDb = iconToolButton(QStringLiteral("database_add"), QColor("#5A6478"),
                                 tr("新建数据库"));
    auto *btnTable = iconToolButton(QStringLiteral("table_add"), QColor("#5A6478"),
                                    tr("新建表"));
    auto *btnRefresh = iconToolButton(QStringLiteral("refresh"), QColor("#5A6478"),
                                      tr("刷新连接列表"));
    connect(btnConn, &QPushButton::clicked, this, &MainWindow::onAddConnection);

    auto *toolGrid = new QHBoxLayout;
    toolGrid->setSpacing(6);
    toolGrid->addWidget(btnQuery);
    toolGrid->addWidget(btnDb);
    toolGrid->addWidget(btnTable);
    toolGrid->addWidget(btnRefresh);
    toolGrid->addStretch();

    m_tree = new ObjectTree;
    connect(m_tree, &ObjectTree::addConnectionRequested, this, &MainWindow::onAddConnection);
    connect(m_tree, &ObjectTree::editConnectionRequested, this,
            &MainWindow::onEditConnection);
    connect(m_tree, &ObjectTree::openDataTableRequested, this,
            &MainWindow::onOpenDataTable);
    connect(m_tree, &ObjectTree::newQueryRequested, this, &MainWindow::onNewQuery);
    connect(m_tree, &ObjectTree::connectionClosed, this, &MainWindow::onConnectionClosed);
    connect(m_tree, &ObjectTree::statusMessage, this, &MainWindow::onStatusMessage);

    connect(btnQuery, &QPushButton::clicked, m_tree, &ObjectTree::newQueryForSelection);
    connect(btnDb, &QPushButton::clicked, m_tree, &ObjectTree::newDatabaseForSelection);
    connect(btnTable, &QPushButton::clicked, m_tree, &ObjectTree::newTableForSelection);
    connect(btnRefresh, &QPushButton::clicked, m_tree, [this] {
        m_tree->reloadProfiles();
        onStatusMessage(tr("连接列表已刷新"));
    });

    auto *treeTitle = new QLabel(tr("连接"));
    QFont titleFont = treeTitle->font();
    titleFont.setBold(true);
    treeTitle->setFont(titleFont);
    treeTitle->setStyleSheet(QStringLiteral("color:#5A6478;"));

    leftLayout->addWidget(btnConn);
    leftLayout->addLayout(toolGrid);
    leftLayout->addWidget(treeTitle);
    leftLayout->addWidget(m_tree, 1);

    // 右侧工作区（欢迎页 + 标签页）
    auto *rightPanel = new QFrame;
    rightPanel->setObjectName(QStringLiteral("rightPanel"));
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_stack = new QStackedWidget;
    m_welcomePage = new QWidget;
    auto *welcomeLayout = new QVBoxLayout(m_welcomePage);
    welcomeLayout->setAlignment(Qt::AlignCenter);
    welcomeLayout->setSpacing(14);
    auto *welcomeLogo = new QLabel;
    welcomeLogo->setAlignment(Qt::AlignCenter);
    welcomeLogo->setPixmap(QPixmap(QStringLiteral(":/images/logo_128.png"))
                               .scaled(96, 96, Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation));
    auto *welcomeName = new QLabel(QStringLiteral("DataMG"));
    welcomeName->setAlignment(Qt::AlignCenter);
    QFont bigFont = welcomeName->font();
    bigFont.setPointSize(26);
    bigFont.setBold(true);
    welcomeName->setFont(bigFont);
    welcomeName->setStyleSheet(QStringLiteral("color:#1F2937;"));
    auto *welcomeHint = new QLabel(
        tr("轻量级数据库管理工具 · 支持 MySQL / PostgreSQL<br><br>"
           "在左侧创建或打开一个连接，双击表可浏览数据，<br>"
           "右键连接 / 数据库 / 表可执行更多操作。"));
    welcomeHint->setAlignment(Qt::AlignCenter);
    welcomeHint->setStyleSheet(QStringLiteral("color:#9AA3B5; font-size:13px;"));
    welcomeLayout->addWidget(welcomeLogo);
    welcomeLayout->addWidget(welcomeName);
    welcomeLayout->addWidget(welcomeHint);

    m_tabs = new QTabWidget;
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *w = m_tabs->widget(index);
        m_tabs->removeTab(index);
        w->deleteLater();
        if (m_tabs->count() == 0)
            showWelcome(true);
    });

    m_stack->addWidget(m_welcomePage);
    m_stack->addWidget(m_tabs);
    rightLayout->addWidget(m_stack);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({300, 980});

    frameLayout->addWidget(splitter, 1);

    // ---- 底部状态条 ----
    auto *statusBar = new QFrame;
    statusBar->setObjectName(QStringLiteral("statusBar"));
    statusBar->setFixedHeight(26);
    auto *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(14, 0, 14, 0);
    m_status = new QLabel(tr("就绪"));
    m_status->setStyleSheet(QStringLiteral("color:#7A8499; font-size:12px;"));
    statusLayout->addWidget(m_status);
    statusLayout->addStretch();

    // 主题切换
    auto *themeBtn = new QPushButton(IconManager::icon(QStringLiteral("eye"), QColor("#7A8499"),
                                                       QSize(14, 14)),
                                     tr("浅色"));
    themeBtn->setObjectName(QStringLiteral("btnGhost"));
    themeBtn->setCursor(Qt::PointingHandCursor);
    themeBtn->setMinimumHeight(24);
    themeBtn->setStyleSheet(
        QStringLiteral("QPushButton {padding: 2px 10px; font-size:12px; color:#7A8499; "
                       "background:transparent; border:1px solid #3A404C; border-radius:4px;}"));
    QSettings ss;
    const bool darkInit = ss.value(QStringLiteral("theme/dark"), false).toBool();
    themeBtn->setText(darkInit ? tr("深色") : tr("浅色"));
    themeBtn->setCheckable(true);
    themeBtn->setChecked(darkInit);
    themeBtn->setText(darkInit ? tr("浅色模式") : tr("深色模式"));
    connect(themeBtn, &QPushButton::clicked, this, [themeBtn] {
        const bool dark = themeBtn->isChecked();
        extern void applyTheme(bool dark);
        applyTheme(dark);
        themeBtn->setText(dark ? tr("浅色模式") : tr("深色模式"));
    });
    statusLayout->addWidget(themeBtn);

    frameLayout->addWidget(statusBar);

    setCentralWidget(frame);
}

void MainWindow::showWelcome(bool welcome)
{
    m_stack->setCurrentIndex(welcome ? 0 : 1);
}

int MainWindow::findTab(const QString &key) const
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->widget(i)->property("tabKey").toString() == key)
            return i;
    }
    return -1;
}

void MainWindow::onAddConnection()
{
    ConnectionDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    ConnectionProfile p = dlg.profile();
    ConnectionStore::instance().save(p);
    m_tree->reloadProfiles();
    onStatusMessage(tr("连接“%1”已保存").arg(p.name));
}

void MainWindow::onEditConnection(const QString &profileId)
{
    ConnectionProfile existing = ConnectionStore::instance().load(profileId);
    ConnectionDialog dlg(this);
    dlg.setProfile(existing);
    if (dlg.exec() != QDialog::Accepted)
        return;
    ConnectionProfile p = dlg.profile();
    p.id = profileId;
    ConnectionStore::instance().save(p);
    m_tree->reloadProfiles();
    onStatusMessage(tr("连接“%1”已更新").arg(p.name));
}

void MainWindow::onOpenDataTable(const QString &profileId, const QString &db,
                                 const QString &schema, const QString &table)
{
    const ConnectionProfile p = ConnectionStore::instance().load(profileId);
    QString err;
    DbClient *c = SessionManager::instance().client(profileId);
    if (!c || !c->isOpen())
        c = SessionManager::instance().open(p, &err);
    if (!c)
        return;

    const QString key =
        QStringLiteral("data:%1:%2:%3:%4").arg(profileId, db, schema, table);
    int idx = findTab(key);
    if (idx < 0) {
        auto *view = new DataView;
        view->setProperty("tabKey", key);
        view->setSource(c, profileId, p.name, db, schema, table);
        idx = m_tabs->addTab(view, IconManager::icon(QStringLiteral("table"),
                                                     QColor("#3B82F6"), QSize(15, 15)),
                             view->tabTitle());
    }
    m_tabs->setCurrentIndex(idx);
    showWelcome(false);
}

void MainWindow::onNewQuery(const QString &profileId, const QString &db)
{
    const ConnectionProfile p = ConnectionStore::instance().load(profileId);
    QString err;
    DbClient *c = SessionManager::instance().client(profileId);
    if (!c || !c->isOpen())
        c = SessionManager::instance().open(p, &err);
    if (!c)
        return;

    const QString currentDb = db.isEmpty() ? c->currentDatabase() : db;
    const QString key = QStringLiteral("query:%1:%2").arg(profileId, currentDb);
    int idx = findTab(key);
    if (idx < 0) {
        auto *view = new QueryView;
        view->setProperty("tabKey", key);
        view->setSource(c, profileId, p.name, currentDb);
        idx = m_tabs->addTab(view, IconManager::icon(QStringLiteral("terminal"),
                                                     QColor("#1B94FF"), QSize(15, 15)),
                             view->tabTitle());
    }
    m_tabs->setCurrentIndex(idx);
    showWelcome(false);
}

void MainWindow::onConnectionClosed(const QString &profileId)
{
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
        const QString key = m_tabs->widget(i)->property("tabKey").toString();
        if (key.startsWith(QStringLiteral("data:%1:").arg(profileId)) ||
            key.startsWith(QStringLiteral("query:%1:").arg(profileId))) {
            QWidget *w = m_tabs->widget(i);
            m_tabs->removeTab(i);
            w->deleteLater();
        }
    }
    if (m_tabs->count() == 0)
        showWelcome(true);
}

void MainWindow::onStatusMessage(const QString &text)
{
    m_status->setText(text);
}

void MainWindow::applyMaximizedStyle(bool maximized)
{
    centralWidget()->setProperty("maximized", maximized);
    centralWidget()->style()->unpolish(centralWidget());
    centralWidget()->style()->polish(centralWidget());
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
        applyMaximizedStyle(isMaximized());
    QMainWindow::changeEvent(event);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#if defined(Q_OS_WIN)
    if (!isMaximized()) {
        MSG *msg = reinterpret_cast<MSG *>(message);
        if (msg->message == WM_NCHITTEST) {
            RECT winrect;
            GetWindowRect(msg->hwnd, &winrect);
            const long x = GET_X_LPARAM(msg->lParam);
            const long y = GET_Y_LPARAM(msg->lParam);
            const bool left = x >= winrect.left && x < winrect.left + kResizeBorder;
            const bool right = x < winrect.right && x >= winrect.right - kResizeBorder;
            const bool top = y >= winrect.top && y < winrect.top + kResizeBorder;
            const bool bottom = y < winrect.bottom && y >= winrect.bottom - kResizeBorder;

            if (top && left)
                *result = HTTOPLEFT;
            else if (top && right)
                *result = HTTOPRIGHT;
            else if (bottom && left)
                *result = HTBOTTOMLEFT;
            else if (bottom && right)
                *result = HTBOTTOMRIGHT;
            else if (left)
                *result = HTLEFT;
            else if (right)
                *result = HTRIGHT;
            else if (top)
                *result = HTTOP;
            else if (bottom)
                *result = HTBOTTOM;
            else
                return QMainWindow::nativeEvent(eventType, message, result);
            return true;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}
