#include "TitleBar.h"

#include "IconManager.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QMainWindow>
#include <QPushButton>

static QPushButton *winButton(const QString &objName, const QString &icon,
                              const QColor &color, const QString &tip)
{
    auto *b = new QPushButton;
    b->setObjectName(objName);
    b->setFixedSize(46, 34);
    b->setIcon(IconManager::icon(icon, color, QSize(15, 15)));
    b->setToolTip(tip);
    b->setCursor(Qt::ArrowCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

TitleBar::TitleBar(QMainWindow *window, QWidget *parent)
    : QWidget(parent)
    , m_window(window)
{
    setObjectName(QStringLiteral("titleBar"));
    setFixedHeight(38);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 0, 0);
    layout->setSpacing(8);

    auto *logo = new QLabel;
    logo->setPixmap(QPixmap(QStringLiteral(":/images/logo_20.png"))
                        .scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    auto *title = new QLabel(QStringLiteral("DataMG"));
    QFont f = title->font();
    f.setBold(true);
    f.setPointSize(10);
    title->setFont(f);
    title->setStyleSheet(QStringLiteral("color:#1F2937;"));

    auto *subtitle = new QLabel(QStringLiteral("数据库管理工具"));
    subtitle->setStyleSheet(QStringLiteral("color:#9AA3B5; font-size:11px;"));

    auto *minBtn = winButton(QStringLiteral("winMinBtn"), QStringLiteral("win_min"),
                             QColor("#5A6478"), tr("最小化"));
    m_maxBtn = winButton(QStringLiteral("winMaxBtn"), QStringLiteral("win_max"),
                         QColor("#5A6478"), tr("最大化"));
    auto *closeBtn = winButton(QStringLiteral("winCloseBtn"), QStringLiteral("close"),
                               QColor("#5A6478"), tr("关闭"));

    connect(minBtn, &QPushButton::clicked, m_window, &QWidget::showMinimized);
    connect(m_maxBtn, &QPushButton::clicked, this, [this] {
        if (m_window->isMaximized())
            m_window->showNormal();
        else
            m_window->showMaximized();
    });
    connect(closeBtn, &QPushButton::clicked, m_window, &QWidget::close);

    layout->addWidget(logo);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addStretch();
    layout->addWidget(minBtn);
    layout->addWidget(m_maxBtn);
    layout->addWidget(closeBtn);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_window->isMaximized()) {
            m_dragPos = QPoint(); // 最大化时拖动先还原
        } else {
            m_dragPos = event->globalPosition().toPoint() - m_window->frameGeometry().topLeft();
        }
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if (m_window->isMaximized()) {
        // 最大化状态下拖动标题栏：还原窗口并跟随鼠标
        m_window->showNormal();
        m_dragPos = QPoint(m_window->width() / 2, -10);
    }
    if (!m_dragPos.isNull())
        m_window->move(event->globalPosition().toPoint() - m_dragPos);
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_window->isMaximized())
            m_window->showNormal();
        else
            m_window->showMaximized();
    }
    QWidget::mouseDoubleClickEvent(event);
}
