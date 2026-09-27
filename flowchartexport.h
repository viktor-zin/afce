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

#ifndef FLOWCHARTEXPORT_H
#define FLOWCHARTEXPORT_H

#include <QDomDocument>
#include <QImage>
#include <QString>
#include <QStringList>

class QPrinter;

namespace afce {

// Export / print a flowchart. Everything is rendered with the light
// "document" style (QFlowChartStyle::documentStyle()): black lines and text
// on white, whatever the UI palette is. Requires a QApplication.
struct ExportOptions
{
    double zoom = 1.0;       // 1.0 = the size of the chart at 100 % zoom (96 dpi)
    bool monochrome = false; // classic black-and-white blocks
    QString assignSymbol = QStringLiteral(":="); // shown in assign blocks
};

// Lower-case file suffixes that exportFlowchart() accepts: "svg", "pdf" and
// every raster format Qt can write (png, jpg, bmp, ...).
QStringList exportFormats();
bool isRasterExportFormat(const QString &suffix);

// Renders the flowchart into an image with a white background.
// Returns a null image if doc is not a valid flowchart or the image would be too large.
QImage renderFlowchartImage(const QDomDocument &doc, const ExportOptions &options = ExportOptions());

// Writes the flowchart to fileName; the format is chosen by the file suffix.
// PDF: the page is the chart at options.zoom, but at most 200 x 200 inches
// (14400 pt, the PDF page size limit): larger charts are scaled down to fit.
bool exportFlowchart(const QDomDocument &doc, const QString &fileName,
                     const ExportOptions &options = ExportOptions(), QString *errorMessage = nullptr);
bool exportRaster(const QDomDocument &doc, const QString &fileName, const QByteArray &format = QByteArray(),
                  const ExportOptions &options = ExportOptions(), QString *errorMessage = nullptr);
bool exportSvg(const QDomDocument &doc, const QString &fileName,
               const ExportOptions &options = ExportOptions(), QString *errorMessage = nullptr);
bool exportPdf(const QDomDocument &doc, const QString &fileName,
               const ExportOptions &options = ExportOptions(), QString *errorMessage = nullptr);

// Prints the flowchart on one page, scaled down to fit the printable area
// (never enlarged beyond 100 %).
bool printFlowchart(const QDomDocument &doc, QPrinter *printer,
                    const ExportOptions &options = ExportOptions(), QString *errorMessage = nullptr);

} // namespace afce

#endif // FLOWCHARTEXPORT_H
