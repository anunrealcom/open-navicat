#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QMainWindow;

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QMainWindow *window, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QMainWindow *m_window;
    QPoint m_dragPos;
    QPushButton *m_maxBtn = nullptr;
};
