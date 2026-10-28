#include "ui/IconManager.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QSettings>

static QString loadQss(const QString &path)
{
    QFile qss(path);
    if (!qss.open(QFile::ReadOnly | QFile::Text))
        return QString();
    return QString::fromUtf8(qss.readAll());
}

void applyTheme(bool dark)
{
    qApp->setStyleSheet(loadQss(dark ? QStringLiteral(":/styles/dark.qss")
                                     : QStringLiteral(":/styles/app.qss")));
    QSettings s;
    s.setValue(QStringLiteral("theme/dark"), dark);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DataMG"));
    QApplication::setApplicationDisplayName(QStringLiteral("DataMG"));
    QApplication::setOrganizationName(QStringLiteral("YihuanTech"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QFont baseFont(QStringLiteral("Microsoft YaHei UI"));
    baseFont.setStyleStrategy(QFont::PreferAntialias);
    baseFont.setPointSize(9);
    app.setFont(baseFont);

    app.setWindowIcon(IconManager::appIcon());

    // 加载上次保存的主题，默认浅色
    QSettings s;
    const bool dark = s.value(QStringLiteral("theme/dark"), false).toBool();
    applyTheme(dark);

    MainWindow window;
    window.show();
    return app.exec();
}
