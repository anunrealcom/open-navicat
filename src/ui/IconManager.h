#pragma once

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QSize>

/// SVG 图标管理：将 :/icons/<name>.svg 中的 __COLOR__ 占位符替换后按需着色渲染
class IconManager
{
public:
    static QIcon icon(const QString &name,
                      const QColor &color = QColor(QStringLiteral("#5A6478")),
                      const QSize &size = QSize(18, 18));
    static QPixmap pixmap(const QString &name,
                          const QColor &color = QColor(QStringLiteral("#5A6478")),
                          const QSize &size = QSize(18, 18));
    static QIcon appIcon();

private:
    static QHash<QString, QIcon> s_cache;
};
