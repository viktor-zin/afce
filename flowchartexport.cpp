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

#include "flowchartexport.h"

#include "afceutil.h"
#include "qflowchartstyle.h"
#include "zvflowchart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImageWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPrinter>
#include <QSvgGenerator>
#include <QtMath>

namespace afce {

namespace {

constexpr double kScreenDpi = 96.0;
constexpr double kMaxZoom = 50.0;
constexpr int kPdfResolution = 1200;
// PDF implementation limit of the page size (200 inches, Adobe PDF reference, appendix C):
// Acrobat and other viewers refuse or crop larger pages.
constexpr double kMaxPdfPagePoints = 14400.0;

// A hidden flowchart used only for rendering, so that the chart shown in the
// editor (its zoom, selection and UI colours) is never touched.
class OffscreenChart
{
public:
    OffscreenChart(const QDomDocument &doc, const ExportOptions &options)
    {
        m_valid = m_chart.loadDocument(doc, &m_error);
        m_chart.setStatus(QFlowChart::Display);
        QFlowChartStyle style = QFlowChartStyle::documentStyle(options.monochrome);
        style.setAssignSymbol(options.assignSymbol);
        m_chart.setChartStyle(style);
        setZoom(options.zoom);
    }

    bool isValid() const { return m_valid; }
    QString errorString() const { return m_error; }
    void setZoom(double zoom) { m_chart.setZoom(zoom); }
    QSize size() const
    {
        return QSize(qMax(1, qCeil(m_chart.root()->width)), qMax(1, qCeil(m_chart.root()->height)));
    }
    void paint(QPainter *painter)
    {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        m_chart.paintTo(painter);
    }
    QString title() const
    {
        const QString name = m_chart.root()->attributes.value(QStringLiteral("name"));
        return name.isEmpty() ? QCoreApplication::translate("FlowchartExport", "Algorithm flowchart") : name;
    }

private:
    QFlowChart m_chart;
    bool m_valid = false;
    QString m_error;
};

bool checkOptions(const ExportOptions &options, QString *errorMessage)
{
    if (!(options.zoom > 0.0) || options.zoom > kMaxZoom) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("FlowchartExport", "Invalid zoom factor %1 (allowed: greater than 0 and at most %2).")
                                .arg(options.zoom).arg(kMaxZoom);
        return false;
    }
    return true;
}

QString nativeName(const QString &fileName)
{
    return QDir::toNativeSeparators(fileName);
}

// White background, document style.
QImage renderImage(OffscreenChart &chart, double zoom)
{
    const QSize size = chart.size();
    if (qint64(size.width()) * size.height() > qint64(16384) * 16384)
        return QImage();
    QImage image(size, QImage::Format_RGB32);
    if (image.isNull())
        return QImage();
    image.setDotsPerMeterX(qRound(kScreenDpi * zoom / 0.0254));
    image.setDotsPerMeterY(qRound(kScreenDpi * zoom / 0.0254));
    image.fill(Qt::white);
    QPainter painter(&image);
    chart.paint(&painter);
    painter.end();
    return image;
}

} // namespace

QStringList exportFormats()
{
    QStringList result;
    const QList<QByteArray> formats = QImageWriter::supportedImageFormats();
    for (const QByteArray &format : formats) {
        const QString suffix = QString::fromLatin1(format).toLower();
        if (!result.contains(suffix))
            result << suffix;
    }
    for (const QString &vector : {QStringLiteral("svg"), QStringLiteral("pdf")}) {
        if (!result.contains(vector))
            result << vector;
    }
    return result;
}

bool isRasterExportFormat(const QString &suffix)
{
    const QString s = suffix.toLower();
    if (s == QLatin1String("svg") || s == QLatin1String("pdf"))
        return false;
    return QImageWriter::supportedImageFormats().contains(s.toLatin1());
}

QImage renderFlowchartImage(const QDomDocument &doc, const ExportOptions &options)
{
    if (!checkOptions(options, nullptr))
        return QImage();
    OffscreenChart chart(doc, options);
    if (!chart.isValid())
        return QImage();
    return renderImage(chart, options.zoom);
}

bool exportRaster(const QDomDocument &doc, const QString &fileName, const QByteArray &format,
                  const ExportOptions &options, QString *errorMessage)
{
    if (!checkOptions(options, errorMessage))
        return false;
    OffscreenChart chart(doc, options);
    if (!chart.isValid()) {
        if (errorMessage)
            *errorMessage = chart.errorString();
        return false;
    }
    const QImage image = renderImage(chart, options.zoom);
    if (image.isNull()) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("FlowchartExport", "The picture is too large. Try a smaller zoom factor.");
        return false;
    }
    QImageWriter writer(fileName, format);
    if (!writer.write(image)) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("FlowchartExport", "Unable to write file '%1': %2").arg(nativeName(fileName), writer.errorString());
        return false;
    }
    return true;
}

bool exportSvg(const QDomDocument &doc, const QString &fileName, const ExportOptions &options,
               QString *errorMessage)
{
    if (!checkOptions(options, errorMessage))
        return false;
    OffscreenChart chart(doc, options);
    if (!chart.isValid()) {
        if (errorMessage)
            *errorMessage = chart.errorString();
        return false;
    }
    const QSize size = chart.size();
    QSvgGenerator svg;
    svg.setFileName(fileName);
    svg.setSize(size);
    svg.setViewBox(QRect(QPoint(0, 0), size));
    svg.setResolution(qRound(kScreenDpi));
    svg.setTitle(chart.title());
    svg.setDescription(QCoreApplication::translate("FlowchartExport", "Created by AFCE %1").arg(programVersion()));
    QPainter painter;
    if (!painter.begin(&svg)) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("FlowchartExport", "Unable to write file '%1'.").arg(nativeName(fileName));
        return false;
    }
    chart.paint(&painter);
    if (!painter.end() || !QFileInfo::exists(fileName)) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("FlowchartExport", "Unable to write file '%1'.").arg(nativeName(fileName));
        return false;
    }
    return true;
}

bool exportPdf(const QDomDocument &doc, const QString &fileName, const ExportOptions &options,
               QString *errorMessage)
{
    if (!checkOptions(options, errorMessage))
        return false;
    OffscreenChart chart(doc, options);
    if (!chart.isValid()) {
        if (errorMessage)
            *errorMessage = chart.errorString();
        return false;
    }
    // Page = the chart at the requested zoom (1 px = 1/96 inch), no margins. The chart
    // is laid out at the PDF resolution (so that text is placed precisely) and the page
    // size is taken from that layout, so nothing is cut off at the page edges.
    // A page beyond the PDF size limit is scaled down (vector output: nothing is lost);
    // the layout is not exactly proportional to the zoom (text metrics), hence the loop.
    double renderZoom = options.zoom * kPdfResolution / kScreenDpi;
    QSizeF pagePoints;
    for (int attempt = 0; attempt < 10; ++attempt) {
        chart.setZoom(renderZoom);
        pagePoints = QSizeF(chart.size()) * (72.0 / kPdfResolution);
        const double longestSide = qMax(pagePoints.width(), pagePoints.height());
        if (longestSide <= kMaxPdfPagePoints)
            break;
        renderZoom *= kMaxPdfPagePoints / longestSide * (attempt == 0 ? 1.0 : 0.99);
    }
    // QPageSize works in whole points: round up so that the chart is not clipped
    pagePoints = QSizeF(qCeil(pagePoints.width() - 1e-6), qCeil(pagePoints.height() - 1e-6))
                     .boundedTo(QSizeF(kMaxPdfPagePoints, kMaxPdfPagePoints));
    QPdfWriter pdf(fileName);
    pdf.setCreator(QStringLiteral("AFCE %1").arg(programVersion()));
    pdf.setTitle(chart.title());
    pdf.setResolution(kPdfResolution);
    const QPageLayout layout(QPageSize(pagePoints, QPageSize::Point, QString(), QPageSize::ExactMatch),
                             QPageLayout::Portrait, QMarginsF(0, 0, 0, 0));
    pdf.setPageLayout(layout);
    QPainter painter;
    if (!painter.begin(&pdf)) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("FlowchartExport", "Unable to write file '%1'.").arg(nativeName(fileName));
        return false;
    }
    chart.paint(&painter);
    if (!painter.end() || !QFileInfo::exists(fileName)) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("FlowchartExport", "Unable to write file '%1'.").arg(nativeName(fileName));
        return false;
    }
    return true;
}

bool exportFlowchart(const QDomDocument &doc, const QString &fileName, const ExportOptions &options,
                     QString *errorMessage)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix == QLatin1String("svg"))
        return exportSvg(doc, fileName, options, errorMessage);
    if (suffix == QLatin1String("pdf"))
        return exportPdf(doc, fileName, options, errorMessage);
    if (isRasterExportFormat(suffix))
        return exportRaster(doc, fileName, suffix.toLatin1(), options, errorMessage);
    if (errorMessage) {
        *errorMessage = QCoreApplication::translate("FlowchartExport", "Unsupported export format '%1'. Supported formats: %2.")
                            .arg(suffix.isEmpty() ? nativeName(fileName) : suffix,
                                 exportFormats().join(QStringLiteral(", ")));
    }
    return false;
}

bool printFlowchart(const QDomDocument &doc, QPrinter *printer, const ExportOptions &options,
                    QString *errorMessage)
{
    if (!printer || !checkOptions(options, errorMessage))
        return false;
    OffscreenChart chart(doc, options);
    if (!chart.isValid()) {
        if (errorMessage)
            *errorMessage = chart.errorString();
        return false;
    }
    chart.setZoom(1);
    const QSize natural = chart.size();
    const QRect page = printer->pageLayout().paintRectPixels(printer->resolution());
    // 100 % = the size on a 96 dpi screen; shrink to fit the page if needed
    double z = options.zoom * printer->resolution() / kScreenDpi;
    if (natural.width() * z > page.width())
        z = page.width() / double(natural.width());
    if (natural.height() * z > page.height())
        z = page.height() / double(natural.height());
    chart.setZoom(z);
    QPainter painter;
    if (!painter.begin(printer)) {
        if (errorMessage)
            *errorMessage = QCoreApplication::translate("FlowchartExport", "Unable to start printing.");
        return false;
    }
    // centred horizontally at the top of the page
    painter.translate(qMax(0.0, (page.width() - chart.size().width()) / 2.0), 0);
    chart.paint(&painter);
    painter.end();
    return true;
}

} // namespace afce
