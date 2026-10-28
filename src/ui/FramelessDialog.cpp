#include "FramelessDialog.h"

#include "IconManager.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

FramelessDialog::FramelessDialog(const QString &title, QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);

    auto *root = new QFrame(this);
    root->setObjectName(QStringLiteral("dialogFrame"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(root);

    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 标题栏
    auto *header = new QFrame(root);
    header->setObjectName(QStringLiteral("dialogHeader"));
    header->setFixedHeight(46);
    auto *headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(18, 0, 10, 0);
    headerLay->setSpacing(8);

    auto *logo = new QLabel(header);
    logo->setPixmap(QPixmap(QStringLiteral(":/images/logo_24.png")));
    logo->setFixedSize(20, 20);
    logo->setScaledContents(true);

    m_titleLabel = new QLabel(title, header);
    m_titleLabel->setObjectName(QStringLiteral("dialogTitle"));

    auto *closeBtn = new QPushButton(header);
    closeBtn->setObjectName(QStringLiteral("winCloseBtn"));
    closeBtn->setFixedSize(34, 30);
    closeBtn->setIcon(IconManager::icon(QStringLiteral("close"), QColor(QStringLiteral("#6B7280")),
                                        QSize(16, 16)));
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    headerLay->addWidget(logo);
    headerLay->addWidget(m_titleLabel);
    headerLay->addStretch();
    headerLay->addWidget(closeBtn);

    auto *body = new QFrame(root);
    body->setObjectName(QStringLiteral("dialogBody"));
    m_bodyLayout = new QVBoxLayout(body);
    m_bodyLayout->setContentsMargins(22, 18, 22, 20);
    m_bodyLayout->setSpacing(14);

    layout->addWidget(header);
    layout->addWidget(body, 1);
}

void FramelessDialog::setDialogSize(int w, int h)
{
    resize(w, h);
    setMinimumSize(w, h);
}

void FramelessDialog::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && e->position().y() <= 46) {
        m_dragging = true;
        m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QDialog::mousePressEvent(e);
}

void FramelessDialog::mouseMoveEvent(QMouseEvent *e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton))
        move(e->globalPosition().toPoint() - m_dragPos);
    QDialog::mouseMoveEvent(e);
}

void FramelessDialog::mouseReleaseEvent(QMouseEvent *e)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(e);
}
