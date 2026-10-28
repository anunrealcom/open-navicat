#include "IconManager.h"

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>

QHash<QString, QIcon> IconManager::s_cache;

static QByteArray loadSvgBytes(const QString &name, const QColor &color)
{
    QFile f(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QByteArray data = f.readAll();
    data.replace("__COLOR__", color.name(QColor::HexRgb).toUtf8());
    return data;
}

QPixmap IconManager::pixmap(const QString &name, const QColor &color, const QSize &size)
{
    const QByteArray bytes = loadSvgBytes(name, color);
    if (bytes.isEmpty())
        return {};

    const qreal dpr = qMax(1.0, QGuiApplication::primaryScreen()
                                    ? QGuiApplication::primaryScreen()->devicePixelRatio()
                                    : 1.0);
    QImage image(size * dpr, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(bytes);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter, QRectF(0, 0, size.width() * dpr, size.height() * dpr));
    painter.end();
    QPixmap pm = QPixmap::fromImage(image);
    pm.setDevicePixelRatio(dpr);
    return pm;
}

QIcon IconManager::icon(const QString &name, const QColor &color, const QSize &size)
{
    const QString key = QStringLiteral("%1|%2|%3x%4")
                            .arg(name, color.name(), QString::number(size.width()),
                                 QString::number(size.height()));
    auto it = s_cache.constFind(key);
    if (it != s_cache.constEnd())
        return it.value();

    QIcon ic;
    const QColor disabledColor(QStringLiteral("#C2C8D5"));
    ic.addPixmap(pixmap(name, color, size), QIcon::Normal, QIcon::Off);
    if (color != disabledColor)
        ic.addPixmap(pixmap(name, disabledColor, size), QIcon::Disabled, QIcon::Off);
    s_cache.insert(key, ic);
    return ic;
}

QIcon IconManager::appIcon()
{
    QIcon icon;
    const QList<int> sizes = {16, 20, 24, 32, 40, 48, 64, 96, 128, 256};
    for (int s : sizes)
        icon.addFile(QStringLiteral(":/images/logo_%1.png").arg(s), QSize(s, s));
    return icon;
}
