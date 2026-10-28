#pragma once

#include <QMainWindow>

class TitleBar;
class ObjectTree;
class QTabWidget;
class QStackedWidget;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onAddConnection();
    void onEditConnection(const QString &profileId);
    void onOpenDataTable(const QString &profileId, const QString &db,
                         const QString &schema, const QString &table);
    void onNewQuery(const QString &profileId, const QString &db);
    void onConnectionClosed(const QString &profileId);
    void onStatusMessage(const QString &text);

private:
    void buildUi();
    void showWelcome(bool welcome);
    int findTab(const QString &key) const;
    void applyMaximizedStyle(bool maximized);

    TitleBar *m_titleBar = nullptr;
    ObjectTree *m_tree = nullptr;
    QTabWidget *m_tabs = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_welcomePage = nullptr;
    QLabel *m_status = nullptr;
};
