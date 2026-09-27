/****************************************************************************
**                                                                         **
** This file is part of the Algorithm Flowchart Editor project.            **
**                                                                         **
** This file may be used under the terms of the GNU                        **
** General Public License versions 2.0 or 3.0 as published by the Free     **
** Software Foundation and appearing in the file LICENSE included in       **
** the packaging of this file.                                             **
** You can find license at http://www.gnu.org/licenses/gpl.html            **
**                                                                         **
****************************************************************************/

#include "appicons.h"

#include <QFile>
#include <QGuiApplication>
#include <QHash>
#include <QIconEngine>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace {

QByteArray svgSource(const QString &name)
{
    static QHash<QString, QByteArray> cache;
    auto it = cache.constFind(name);
    if (it != cache.constEnd())
        return it.value();
    QFile file(QStringLiteral(":/images/icons/%1.svg").arg(name));
    QByteArray data;
    if (file.open(QIODevice::ReadOnly))
        data = file.readAll();
    cache.insert(name, data);
    return data;
}

class TintedSvgIconEngine : public QIconEngine
{
public:
    explicit TintedSvgIconEngine(const QString &name) : fName(name) { }

    QIconEngine *clone() const override { return new TintedSvgIconEngine(fName); }
    QString key() const override { return QStringLiteral("afce-tinted-svg"); }
    QString iconName() override { return fName; }
    bool isNull() override { return svgSource(fName).isEmpty(); }

    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override { return size; }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatio() : qreal(1);
        painter->drawPixmap(rect, render(rect.size(), dpr, mode));
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State) override
    {
        return render(size, 1, mode);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State, qreal scale) override
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        // size is in device independent pixels
        return render(size, scale, mode);
#else
        // size is in device pixels already
        return render(size / scale, scale, mode);
#endif
    }

private:
    static QColor tint(QIcon::Mode mode)
    {
        const QPalette palette = QGuiApplication::palette();
        switch (mode) {
        case QIcon::Disabled: {
            QColor c = palette.color(QPalette::Active, QPalette::WindowText);
            c.setAlphaF(0.32f);
            return c;
        }
        case QIcon::Selected:
            return palette.color(QPalette::Active, QPalette::HighlightedText);
        default:
            return palette.color(QPalette::Active, QPalette::WindowText);
        }
    }

    QPixmap render(const QSize &size, qreal scale, QIcon::Mode mode) const
    {
        if (size.isEmpty())
            return QPixmap();
        const QColor color = tint(mode);
        const QSize deviceSize = (QSizeF(size) * scale).toSize();
        const QString cacheKey = QStringLiteral("afce-icon:%1:%2x%3:%4")
                                     .arg(fName).arg(deviceSize.width()).arg(deviceSize.height())
                                     .arg(color.rgba(), 8, 16, QLatin1Char('0'));
        QPixmap result;
        if (!QPixmapCache::find(cacheKey, &result)) {
            QByteArray svg = svgSource(fName);
            svg.replace("#000000", color.name(QColor::HexRgb).toLatin1());
            QSvgRenderer renderer(svg);
            QImage image(deviceSize, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            if (renderer.isValid()) {
                QPainter painter(&image);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setOpacity(color.alphaF());
                // keep the square aspect ratio of the drawing
                const int side = qMin(deviceSize.width(), deviceSize.height());
                const QRectF target((deviceSize.width() - side) / 2.0, (deviceSize.height() - side) / 2.0, side, side);
                renderer.render(&painter, target);
            }
            result = QPixmap::fromImage(image);
            QPixmapCache::insert(cacheKey, result);
        }
        result.setDevicePixelRatio(scale);
        return result;
    }

    QString fName;
};

} // namespace

namespace afce {

QIcon uiIcon(const QString &name)
{
    return QIcon(new TintedSvgIconEngine(name));
}

QIcon applicationIcon()
{
    static const QIcon icon = []() {
        QIcon result;
        // the simplified drawing for small sizes, the detailed one from 48 px
        const QList<QPair<int, QString>> sources = {
            {16, QStringLiteral(":/images/appicon-small.svg")},  {24, QStringLiteral(":/images/appicon-small.svg")},
            {32, QStringLiteral(":/images/appicon-small.svg")},  {48, QStringLiteral(":/images/appicon.svg")},
            {64, QStringLiteral(":/images/appicon.svg")},        {128, QStringLiteral(":/images/appicon.svg")},
            {256, QStringLiteral(":/images/appicon.svg")},       {512, QStringLiteral(":/images/appicon.svg")}};
        for (const auto &source : sources) {
            QSvgRenderer renderer(source.second);
            if (!renderer.isValid())
                continue;
            QPixmap pixmap(source.first, source.first);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing, true);
            renderer.render(&painter);
            painter.end();
            result.addPixmap(pixmap);
        }
        return result;
    }();
    return icon;
}

} // namespace afce
