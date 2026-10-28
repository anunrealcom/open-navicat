#pragma once

#include <QDialog>

class QVBoxLayout;
class QLabel;

/// 无边框圆角对话框基类（与主窗口统一的 QQ 风格）
class FramelessDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FramelessDialog(const QString &title, QWidget *parent = nullptr);

    void setDialogSize(int w, int h);
    QVBoxLayout *bodyLayout() const { return m_bodyLayout; }

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    QPoint m_dragPos;
    bool m_dragging = false;
    QVBoxLayout *m_bodyLayout = nullptr;
    QLabel *m_titleLabel = nullptr;
};
