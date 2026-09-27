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

// Flowchart model / rendering / export tests (run with QT_QPA_PLATFORM=offscreen).

#include "afceutil.h"
#include "blockcatalog.h"
#include "flowchartexport.h"
#include "qflowchartstyle.h"
#include "zvflowchart.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QPaintEngine>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QPrinter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <climits>
#include <functional>
#include <memory>

#ifndef AFCE_TEST_DATA_DIR
#error "AFCE_TEST_DATA_DIR must be defined"
#endif

namespace {

QString dataDir()
{
    return QStringLiteral(AFCE_TEST_DATA_DIR);
}

QStringList sampleFiles()
{
    const QDir dir(dataDir() + QStringLiteral("/samples"));
    QStringList result;
    const QStringList names = dir.entryList(QStringList() << QStringLiteral("*.afc"), QDir::Files, QDir::Name);
    for (const QString &name : names)
        result << dir.absoluteFilePath(name);
    return result;
}

QString sample(const QString &name)
{
    return dataDir() + QStringLiteral("/samples/") + name;
}

QString robustness(const QString &name)
{
    return dataDir() + QStringLiteral("/robustness/") + name;
}

QDomDocument parse(const QString &xml)
{
    QDomDocument doc;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const QDomDocument::ParseResult result = doc.setContent(xml);
    if (!result)
        qWarning() << "XML error" << result.errorMessage << result.errorLine;
#else
    QString error;
    int line = 0;
    if (!doc.setContent(xml, &error, &line))
        qWarning() << "XML error" << error << line;
#endif
    return doc;
}

QDomDocument readXml(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return QDomDocument();
    QDomDocument doc;
    if (!doc.setContent(file.readAll()))
        return QDomDocument();
    return doc;
}

// Canonical text of an element tree: element names, sorted attributes
// (the algorithm "version" and empty t1..t8 attributes ignored), children.
QString canonical(const QDomElement &element)
{
    QStringList attrs;
    const QDomNamedNodeMap map = element.attributes();
    for (int i = 0; i < map.count(); ++i) {
        const QDomAttr attr = map.item(i).toAttr();
        if (element.tagName() == QLatin1String("algorithm") && attr.name() == QLatin1String("version"))
            continue;
        attrs << attr.name() + QLatin1Char('=') + attr.value();
    }
    attrs.sort();
    QString result = QLatin1Char('<') + element.tagName();
    if (!attrs.isEmpty())
        result += QLatin1Char(' ') + attrs.join(QLatin1Char(' '));
    result += QLatin1Char('>');
    for (QDomElement child = element.firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
        result += canonical(child);
    result += QStringLiteral("</") + element.tagName() + QLatin1Char('>');
    return result;
}

int countBlocks(const QBlock *block)
{
    int n = 1;
    for (const QBlock *child : block->items)
        n += countBlocks(child);
    return n;
}

// Expected number of insertion points: one before every item of a branch plus one at its end.
int expectedInsertionPoints(const QBlock *block)
{
    int n = block->isBranch ? int(block->items.size()) + 1 : 0;
    for (const QBlock *child : block->items)
        n += expectedInsertionPoints(child);
    return n;
}

QBlock *findBlock(QBlock *block, const std::function<bool(QBlock *)> &predicate)
{
    if (predicate(block))
        return block;
    for (QBlock *child : block->items) {
        if (QBlock *found = findBlock(child, predicate))
            return found;
    }
    return nullptr;
}

QBlock *body(QFlowChart &chart)
{
    return chart.root()->item(0);
}

// Share of pixels that differ from white.
double inkRatio(const QImage &image)
{
    if (image.isNull())
        return 0;
    qint64 ink = 0;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qGray(line[x]) < 200)
                ++ink;
        }
    }
    return double(ink) / (qint64(image.width()) * image.height());
}

// Size of the first page (MediaBox, in points) of a PDF written by QPdfWriter.
QSizeF pdfPageSize(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return QSizeF();
    static const QRegularExpression rx(QStringLiteral("/MediaBox\\s*\\[\\s*0\\s+0\\s+([0-9.]+)\\s+([0-9.]+)\\s*\\]"));
    const QRegularExpressionMatch match = rx.match(QString::fromLatin1(file.readAll()));
    if (!match.hasMatch())
        return QSizeF();
    return QSizeF(match.captured(1).toDouble(), match.captured(2).toDouble());
}

// A paint device that records the text lines QPainter draws (after line
// breaking) and their positions; everything else is ignored.
class TextRecorder : public QPaintDevice
{
public:
    struct Line
    {
        QString text;
        QPointF position;
    };

    TextRecorder() : m_engine(this) { }
    ~TextRecorder() override = default;
    QPaintEngine *paintEngine() const override { return &m_engine; }
    QList<Line> lines;

protected:
    int metric(PaintDeviceMetric metric) const override
    {
        switch (metric) {
        case PdmWidth:
        case PdmHeight:
            return 1000000;
        case PdmWidthMM:
        case PdmHeightMM:
            return 1000000 * 254 / 960;
        case PdmNumColors:
            return INT_MAX;
        case PdmDepth:
            return 32;
        case PdmDpiX:
        case PdmDpiY:
        case PdmPhysicalDpiX:
        case PdmPhysicalDpiY:
            return 96;
        case PdmDevicePixelRatio:
            return 1;
        case PdmDevicePixelRatioScaled:
            return int(QPaintDevice::devicePixelRatioFScale());
        default:
            return QPaintDevice::metric(metric);
        }
    }

private:
    class Engine : public QPaintEngine
    {
    public:
        explicit Engine(TextRecorder *device) : QPaintEngine(QPaintEngine::AllFeatures), m_device(device) { }
        bool begin(QPaintDevice *) override { return true; }
        bool end() override { return true; }
        void updateState(const QPaintEngineState &state) override
        {
            if (state.state() & QPaintEngine::DirtyTransform)
                m_transform = state.transform();
        }
        Type type() const override { return QPaintEngine::User; }
        void drawTextItem(const QPointF &p, const QTextItem &textItem) override
        {
            m_device->lines.append({textItem.text(), m_transform.map(p)});
        }
        void drawPixmap(const QRectF &, const QPixmap &, const QRectF &) override { }
        void drawImage(const QRectF &, const QImage &, const QRectF &, Qt::ImageConversionFlags) override { }
        void drawTiledPixmap(const QRectF &, const QPixmap &, const QPointF &) override { }
        void drawPath(const QPainterPath &) override { }
        void drawPolygon(const QPointF *, int, PolygonDrawMode) override { }
        void drawPolygon(const QPoint *, int, PolygonDrawMode) override { }
        void drawLines(const QLineF *, int) override { }
        void drawLines(const QLine *, int) override { }
        void drawRects(const QRectF *, int) override { }
        void drawRects(const QRect *, int) override { }
        void drawEllipse(const QRectF &) override { }
        void drawEllipse(const QRect &) override { }
        void drawPoints(const QPointF *, int) override { }
        void drawPoints(const QPoint *, int) override { }

    private:
        TextRecorder *m_device;
        QTransform m_transform;
    };
    mutable Engine m_engine;
};

// Moves the mouse over a widget without buttons (QTest::mouseMove only moves
// the cursor then, which sends no event on the offscreen platform).
void hoverAt(QWidget *widget, const QPoint &pos)
{
    QMouseEvent event(QEvent::MouseMove, pos, widget->mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

// The text lines of the chart painted at the given zoom.
QList<TextRecorder::Line> paintedText(QFlowChart &chart, double zoom)
{
    chart.setZoom(zoom);
    TextRecorder device;
    QPainter painter(&device);
    chart.paintTo(&painter);
    painter.end();
    return device.lines;
}

} // namespace

class Test_flowchart : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();

    void loadSamples_data();
    void loadSamples();
    void xmlRoundTrip_data();
    void xmlRoundTrip();
    void saveAndLoadFile();
    void saveErrors();
    void backwardCompatibility();
    void invalidFiles_data();
    void invalidFiles();
    void render_data();
    void render();
    void exportDocumentStyle();
    void exportFiles_data();
    void exportFiles();
    void exportErrors();
    void printToPdf();
    void pdfPageSizeLimit_data();
    void pdfPageSizeLimit();
    void zoom();
    void textLayoutIndependentOfZoom_data();
    void textLayoutIndependentOfZoom();
    void userDataDirIndependentOfApplicationName();
    void undoRedo();
    void loadClearsHistory();
    void insertionPoints_data();
    void insertionPoints();
    void insertWithMouse();
    void deleteAndCopyPaste();
    void deleteParentOfSelection();
    void insertableBuffer();
    void unknownElements();
    void malformedBlocks();
    void blockOwnership();
    void chartDeletesAllBlocks();
    void noSignalsFromDestructor();

    // new blocks, layout and editor interaction
    void newBlockTypes();
    void layoutSanity_data();
    void layoutSanity();
    void terminates_data();
    void terminates();
    void endNotDrawnAfterReturn();
    void captions();
    void blockCatalog();
    void everyBlockRenders_data();
    void everyBlockRenders();
    void caseBranches();
    void dragMoveIsOneUndoStep();
    void dropFragmentFromOutside();
    void dragCopy();
    void dropIntoItselfRejected();
    void hoverAndEscape();
    void doubleClickBeginEditsAlgorithm();
    void styles();
};

void Test_flowchart::initTestCase()
{
    QVERIFY2(QFileInfo(dataDir()).isDir(), qPrintable(dataDir()));
    QVERIFY(sampleFiles().size() >= 5);
}

void Test_flowchart::loadSamples_data()
{
    QTest::addColumn<QString>("fileName");
    for (const QString &file : sampleFiles())
        QTest::newRow(qPrintable(QFileInfo(file).fileName())) << file;
}

void Test_flowchart::loadSamples()
{
    QFETCH(QString, fileName);
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(fileName, &error), qPrintable(error));
    QVERIFY(error.isEmpty());
    QCOMPARE(chart.root()->type(), QStringLiteral("algorithm"));
    QCOMPARE(chart.root()->attributes.value(QStringLiteral("version")), QStringLiteral(AFC_VERSION));
    QVERIFY(chart.root()->items.size() >= 1);
    QVERIFY(body(chart)->isBranch);
    QVERIFY(chart.root()->width > 0);
    QVERIFY(chart.root()->height > 0);
    QCOMPARE(chart.size(), QSize(qCeil(chart.root()->width), qCeil(chart.root()->height)));
    QVERIFY(!chart.canUndo());
    QVERIFY(!chart.canRedo());
    // every block knows its chart
    QVERIFY(!findBlock(chart.root(), [&chart](QBlock *b) { return b->flowChart() != &chart; }));
}

void Test_flowchart::xmlRoundTrip_data()
{
    loadSamples_data();
    QTest::newRow("unknown_elements") << robustness(QStringLiteral("unknown_elements.afc"));
}

void Test_flowchart::xmlRoundTrip()
{
    QFETCH(QString, fileName);
    QFlowChart first;
    QString error;
    QVERIFY2(first.loadFile(fileName, &error), qPrintable(error));
    const QString xml1 = first.document().toString(2);

    QFlowChart second;
    QVERIFY2(second.loadDocument(parse(xml1), &error), qPrintable(error));
    const QString xml2 = second.document().toString(2);
    QCOMPARE(xml2, xml1);

    // nothing of the original file is lost (old files are converted, see backwardCompatibility())
    if (!fileName.endsWith(QLatin1String("old_format.afc"))) {
        const QDomDocument original = readXml(fileName);
        QVERIFY(!original.isNull());
        QCOMPARE(canonical(parse(xml1).documentElement()), canonical(original.documentElement()));
    }
    QCOMPARE(parse(xml1).documentElement().attribute(QStringLiteral("version")), QStringLiteral(AFC_VERSION));
    QVERIFY(xml1.startsWith(QLatin1String("<!DOCTYPE AFC>")));
}

void Test_flowchart::saveAndLoadFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("all_blocks.afc")), &error), qPrintable(error));
    // non-ASCII text survives (files are UTF-8)
    QBlock *process = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("process"); });
    QVERIFY(process);
    process->attributes[QStringLiteral("text")] = QStringLiteral("Привет, мир — \"quotes\" & <tags>");
    const QString path = dir.filePath(QStringLiteral("saved.afc"));
    QVERIFY2(chart.saveFile(path, &error), qPrintable(error));

    QFlowChart loaded;
    QVERIFY2(loaded.loadFile(path, &error), qPrintable(error));
    QCOMPARE(loaded.document().toString(2), chart.document().toString(2));
    QBlock *loadedProcess = findBlock(loaded.root(), [](QBlock *b) { return b->type() == QLatin1String("process"); });
    QVERIFY(loadedProcess);
    QCOMPARE(loadedProcess->attributes.value(QStringLiteral("text")), QStringLiteral("Привет, мир — \"quotes\" & <tags>"));
}

void Test_flowchart::saveErrors()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFlowChart chart;
    QString error;
    // a directory that does not exist
    QVERIFY(!chart.saveFile(dir.filePath(QStringLiteral("no/such/dir/x.afc")), &error));
    QVERIFY(!error.isEmpty());
    // a path that is a directory
    error.clear();
    QVERIFY(!chart.saveFile(dir.path(), &error));
    QVERIFY(!error.isEmpty());
}

void Test_flowchart::backwardCompatibility()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("old_format.afc")), &error), qPrintable(error));
    QCOMPARE(chart.root()->attributes.value(QStringLiteral("version")), QStringLiteral(AFC_VERSION));

    QList<QBlock *> io;
    findBlock(chart.root(), [&io](QBlock *b) {
        if (b->type() == QLatin1String("io") || b->type() == QLatin1String("ou"))
            io << b;
        return false;
    });
    QCOMPARE(io.size(), 4);
    QCOMPARE(io.at(0)->type(), QStringLiteral("io"));
    QCOMPARE(io.at(0)->attributes.value(QStringLiteral("vars")), QStringLiteral("a,b"));
    QCOMPARE(io.at(1)->attributes.value(QStringLiteral("vars")), QStringLiteral("a"));
    QCOMPARE(io.at(2)->attributes.value(QStringLiteral("vars")), QStringLiteral("b"));
    QCOMPARE(io.at(3)->attributes.value(QStringLiteral("vars")), QStringLiteral("a,b,a + b"));
    for (QBlock *b : std::as_const(io)) {
        for (int i = 1; i <= 8; ++i)
            QVERIFY(!b->attributes.contains(QStringLiteral("t%1").arg(i)));
    }
    const QString xml = chart.document().toString();
    QVERIFY(!xml.contains(QLatin1String(" t1=")));
    QVERIFY(xml.contains(QLatin1String("version=\"" AFC_VERSION "\"")));
}

void Test_flowchart::invalidFiles_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::newRow("broken xml") << robustness(QStringLiteral("broken.afc"));
    QTest::newRow("not afc") << robustness(QStringLiteral("not_afc.xml"));
    QTest::newRow("empty file") << robustness(QStringLiteral("empty_file.afc"));
    QTest::newRow("no body") << robustness(QStringLiteral("no_body.afc"));
    QTest::newRow("missing file") << robustness(QStringLiteral("does_not_exist.afc"));
    QTest::newRow("directory") << robustness(QString());
}

void Test_flowchart::invalidFiles()
{
    QFETCH(QString, fileName);
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("max_of_two.afc")), &error), qPrintable(error));
    const QString before = chart.document().toString();

    QVERIFY(!chart.loadFile(fileName, &error));
    QVERIFY(!error.isEmpty());
    // the current document is untouched
    QCOMPARE(chart.document().toString(), before);
}

void Test_flowchart::render_data()
{
    loadSamples_data();
}

void Test_flowchart::render()
{
    QFETCH(QString, fileName);
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(fileName, &error), qPrintable(error));

    const QImage image = afce::renderFlowchartImage(chart.document());
    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), QSize(qCeil(chart.root()->width), qCeil(chart.root()->height)));
    const double ink = inkRatio(image);
    QVERIFY2(ink > 0.005, qPrintable(QStringLiteral("ink ratio %1").arg(ink)));
    QVERIFY2(ink < 0.5, qPrintable(QStringLiteral("ink ratio %1").arg(ink)));
    // white background in the corners
    QCOMPARE(image.pixel(0, 0), qRgb(255, 255, 255));
    QCOMPARE(image.pixel(image.width() - 1, image.height() - 1), qRgb(255, 255, 255));

    // zoom scales the picture
    afce::ExportOptions options;
    options.zoom = 2;
    const QImage big = afce::renderFlowchartImage(chart.document(), options);
    QVERIFY(qAbs(big.width() - 2 * image.width()) <= 2);
    QVERIFY(qAbs(big.height() - 2 * image.height()) <= 2);

    // the widget paints too
    chart.resize(chart.sizeHint());
    const QPixmap grabbed = chart.grab();
    QVERIFY(!grabbed.isNull());
}

void Test_flowchart::exportDocumentStyle()
{
    // a dark UI palette must not leak into exports
    QPalette dark;
    dark.setColor(QPalette::Base, QColor(30, 30, 30));
    dark.setColor(QPalette::Text, QColor(230, 230, 230));
    dark.setColor(QPalette::Highlight, QColor(40, 80, 160));
    dark.setColor(QPalette::HighlightedText, Qt::white);
    QFlowChart chart;
    chart.setChartStyle(QFlowChartStyle::uiStyle(dark));
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("max_of_two.afc")), &error), qPrintable(error));
    QCOMPARE(chart.chartStyle().normalBackground(), QColor(30, 30, 30));

    const QImage image = afce::renderFlowchartImage(chart.document());
    QVERIFY(!image.isNull());
    QCOMPARE(image.pixel(0, 0), qRgb(255, 255, 255));
    // the lines are dark
    bool black = false;
    for (int y = 0; y < image.height() && !black; ++y) {
        for (int x = 0; x < image.width() && !black; ++x)
            black = qGray(image.pixel(x, y)) < 40;
    }
    QVERIFY(black);
    // the chart shown in the editor keeps its style
    QCOMPARE(chart.chartStyle().normalBackground(), QColor(30, 30, 30));

    const QFlowChartStyle document = QFlowChartStyle::documentStyle(true);
    QCOMPARE(document.normalBackground(), QColor(Qt::white));
    QCOMPARE(document.normalForeground(), QColor(Qt::black));
    QVERIFY(document.monochrome());
}

void Test_flowchart::exportFiles_data()
{
    QTest::addColumn<QString>("suffix");
    QTest::addColumn<QByteArray>("magic");
    QTest::newRow("png") << QStringLiteral("png") << QByteArray("\x89PNG");
    QTest::newRow("jpg") << QStringLiteral("jpg") << QByteArray("\xFF\xD8");
    QTest::newRow("bmp") << QStringLiteral("bmp") << QByteArray("BM");
    QTest::newRow("svg") << QStringLiteral("svg") << QByteArray("<?xml");
    QTest::newRow("pdf") << QStringLiteral("pdf") << QByteArray("%PDF");
}

void Test_flowchart::exportFiles()
{
    QFETCH(QString, suffix);
    QFETCH(QByteArray, magic);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("all_blocks.afc")), &error), qPrintable(error));
    const QString path = dir.filePath(QStringLiteral("out.") + suffix);
    QVERIFY(afce::exportFormats().contains(suffix));
    QVERIFY2(afce::exportFlowchart(chart.document(), path, afce::ExportOptions(), &error), qPrintable(error));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    QVERIFY(data.size() > 100);
    QVERIFY2(data.startsWith(magic), data.left(16).toHex().constData());
    if (suffix == QLatin1String("svg")) {
        QVERIFY(data.contains("<svg"));
        QVERIFY(data.contains("s := 0"));
    }
    if (afce::isRasterExportFormat(suffix)) {
        const QImage image(path);
        QVERIFY(!image.isNull());
        QCOMPARE(image.size(), QSize(qCeil(chart.root()->width), qCeil(chart.root()->height)));
        QCOMPARE(qGray(image.pixel(0, 0)), 255);
    }
}

void Test_flowchart::exportErrors()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFlowChart chart;
    QString error;
    QVERIFY(!afce::exportFlowchart(chart.document(), dir.filePath(QStringLiteral("x.unknown")), afce::ExportOptions(), &error));
    QVERIFY(!error.isEmpty());

    error.clear();
    afce::ExportOptions bad;
    bad.zoom = 0;
    QVERIFY(!afce::exportFlowchart(chart.document(), dir.filePath(QStringLiteral("x.png")), bad, &error));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!afce::exportFlowchart(chart.document(), dir.filePath(QStringLiteral("missing/dir/x.png")), afce::ExportOptions(), &error));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!afce::exportFlowchart(parse(QStringLiteral("<html/>")), dir.filePath(QStringLiteral("x.svg")), afce::ExportOptions(), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(afce::renderFlowchartImage(parse(QStringLiteral("<html/>"))).isNull());
}

void Test_flowchart::printToPdf()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("nested.afc")), &error), qPrintable(error));
    const QString path = dir.filePath(QStringLiteral("printed.pdf"));
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    QVERIFY2(afce::printFlowchart(chart.document(), &printer, afce::ExportOptions(), &error), qPrintable(error));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.readAll().startsWith("%PDF"));
    // the displayed chart is not affected by printing
    QCOMPARE(chart.zoom(), 1.0);
}

void Test_flowchart::zoom()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("nested.afc")), &error), qPrintable(error));
    chart.setZoom(1);
    const double w1 = chart.root()->width;
    const double h1 = chart.root()->height;
    chart.setZoom(2.5);
    QVERIFY(qAbs(chart.root()->width - 2.5 * w1) < 1);
    QVERIFY(qAbs(chart.root()->height - 2.5 * h1) < 1);
    QCOMPARE(chart.zoom(), 2.5);
}

void Test_flowchart::pdfPageSizeLimit_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<double>("zoom");
    QTest::newRow("nested 1") << QStringLiteral("nested.afc") << 1.0;
    QTest::newRow("nested 50") << QStringLiteral("nested.afc") << 50.0;
    QTest::newRow("all_blocks 12.5") << QStringLiteral("all_blocks.afc") << 12.5;
    QTest::newRow("max_of_two 50") << QStringLiteral("max_of_two.afc") << 50.0;
}

void Test_flowchart::pdfPageSizeLimit()
{
    // The PDF page is the chart at the requested zoom (1 px = 1/96 inch = 0.75 pt),
    // but never larger than 14400 pt (200 inches), the PDF implementation limit
    // that Acrobat and other viewers enforce: larger charts are scaled down.
    QFETCH(QString, fileName);
    QFETCH(double, zoom);
    constexpr double kLimit = 14400;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(fileName), &error), qPrintable(error));
    chart.setZoom(1);
    const QSizeF natural(chart.root()->width * 0.75, chart.root()->height * 0.75); // points at 100 %

    afce::ExportOptions options;
    options.zoom = zoom;
    const QString path = dir.filePath(QStringLiteral("out.pdf"));
    QVERIFY2(afce::exportFlowchart(chart.document(), path, options, &error), qPrintable(error));
    const QSizeF page = pdfPageSize(path);
    QVERIFY2(!page.isEmpty(), "no MediaBox in the PDF");
    QVERIFY2(page.width() <= kLimit && page.height() <= kLimit,
             qPrintable(QStringLiteral("page %1 x %2 pt").arg(page.width()).arg(page.height())));
    const double expectedScale = qMin(zoom, kLimit / qMax(natural.width(), natural.height()));
    const QSizeF expected = natural * expectedScale;
    QVERIFY2(qAbs(page.width() - expected.width()) <= 0.02 * expected.width() + 2
                 && qAbs(page.height() - expected.height()) <= 0.02 * expected.height() + 2,
             qPrintable(QStringLiteral("page %1 x %2 pt, expected about %3 x %4 pt")
                            .arg(page.width()).arg(page.height()).arg(expected.width()).arg(expected.height())));
}

void Test_flowchart::textLayoutIndependentOfZoom_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<double>("zoom");
    for (const char *name : {"max_of_two.afc", "all_blocks.afc", "nested.afc", "new_blocks.afc", "new_blocks_long.afc"}) {
        for (double zoom : {0.5, 2.0, 12.5}) // 12.5 = the zoom of PDF export (1200 dpi)
            QTest::addRow("%s %g", name, zoom) << QString::fromLatin1(name) << zoom;
    }
}

void Test_flowchart::textLayoutIndependentOfZoom()
{
    // The screen, PNG, SVG and PDF (zoom x12.5) must show the same text: line
    // breaks and relative positions must not depend on the zoom.
    QFETCH(QString, fileName);
    QFETCH(double, zoom);
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(fileName), &error), qPrintable(error));
    const QList<TextRecorder::Line> reference = paintedText(chart, 1);
    const QList<TextRecorder::Line> zoomed = paintedText(chart, zoom);
    QVERIFY(!reference.isEmpty());
    QStringList referenceText;
    QStringList zoomedText;
    for (const TextRecorder::Line &line : reference)
        referenceText << line.text;
    for (const TextRecorder::Line &line : zoomed)
        zoomedText << line.text;
    QCOMPARE(zoomedText, referenceText);
    // font sizes are whole pixels: allow a few pixels (at 100 %) of difference
    for (int i = 0; i < reference.size(); ++i) {
        const QPointF delta = zoomed.at(i).position / zoom - reference.at(i).position;
        QVERIFY2(qAbs(delta.x()) <= 6 && qAbs(delta.y()) <= 3,
                 qPrintable(QStringLiteral("'%1' is at (%2, %3) at 100 % but at (%4, %5) / %6")
                                .arg(reference.at(i).text)
                                .arg(reference.at(i).position.x()).arg(reference.at(i).position.y())
                                .arg(zoomed.at(i).position.x()).arg(zoomed.at(i).position.y()).arg(zoom)));
    }
}

void Test_flowchart::userDataDirIndependentOfApplicationName()
{
    // afce, afce-cli and the tests look for user generators / help in the same
    // directory (e.g. ~/.local/share/afce), whatever their application name is.
    const QString appName = QCoreApplication::applicationName();
    const QString orgName = QCoreApplication::organizationName();
    QCoreApplication::setApplicationName(QStringLiteral("afce"));
    QCoreApplication::setOrganizationName(QString());
    const QString expected = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                                             + QStringLiteral("/generators"));
    QVERIFY(!expected.isEmpty());

    QCoreApplication::setApplicationName(QStringLiteral("afce-cli"));
    QCoreApplication::setOrganizationName(QStringLiteral("Some Organization"));
    afce::setupSearchPaths();
    const QStringList paths = QDir::searchPaths(QStringLiteral("generators"));
    QCOMPARE(paths.value(0), expected);
    QCOMPARE(paths.value(paths.size() - 1), QStringLiteral(":/generators"));
    QCOMPARE(QDir::searchPaths(QStringLiteral("help")).value(0),
             QDir::cleanPath(QFileInfo(expected).path() + QStringLiteral("/help")));
    // the names are restored
    QCOMPARE(QCoreApplication::applicationName(), QStringLiteral("afce-cli"));
    QCOMPARE(QCoreApplication::organizationName(), QStringLiteral("Some Organization"));

    QCoreApplication::setApplicationName(appName);
    QCoreApplication::setOrganizationName(orgName);
    afce::setupSearchPaths();
}

void Test_flowchart::undoRedo()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("all_blocks.afc")), &error), qPrintable(error));
    const QString original = chart.toString();
    QSignalSpy modified(&chart, &QFlowChart::modified);

    chart.setActiveBlock(body(chart)->item(0));
    chart.deleteActiveBlock();
    QCOMPARE(modified.count(), 1);
    QVERIFY(!chart.activeBlock());
    const QString afterDelete = chart.toString();
    QVERIFY(afterDelete != original);
    QVERIFY(chart.canUndo());
    QVERIFY(!chart.canRedo());

    chart.undo();
    QCOMPARE(chart.toString(), original);
    QVERIFY(!chart.canUndo());
    QVERIFY(chart.canRedo());

    chart.redo();
    QCOMPARE(chart.toString(), afterDelete);
    QVERIFY(chart.canUndo());
    QVERIFY(!chart.canRedo());

    // a new change clears the redo stack
    chart.undo();
    QVERIFY(chart.canRedo());
    chart.makeUndo();
    body(chart)->deleteObject(0);
    chart.realignObjects();
    QVERIFY(!chart.canRedo());
    chart.undo();
    QCOMPARE(chart.toString(), original);
}

void Test_flowchart::loadClearsHistory()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("all_blocks.afc")), &error), qPrintable(error));
    chart.setActiveBlock(body(chart)->item(0));
    chart.deleteActiveBlock();
    QVERIFY(chart.canUndo());
    // undo after opening another file must not bring back the previous file
    QVERIFY2(chart.loadFile(sample(QStringLiteral("gcd.afc")), &error), qPrintable(error));
    QVERIFY(!chart.canUndo());
    QVERIFY(!chart.canRedo());
}

void Test_flowchart::insertionPoints_data()
{
    loadSamples_data();
}

void Test_flowchart::insertionPoints()
{
    QFETCH(QString, fileName);
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(fileName, &error), qPrintable(error));
    QVERIFY(chart.insertionPointList().isEmpty());
    chart.setBuffer(QStringLiteral("<algorithm><branch><process text=\"new\"/></branch></algorithm>"));
    chart.setStatus(QFlowChart::Insertion);
    const QList<QInsertionPoint> points = chart.insertionPointList();
    QCOMPARE(int(points.size()), expectedInsertionPoints(chart.root()));
    const QRectF bounds(0, 0, chart.root()->width, chart.root()->height);
    for (const QInsertionPoint &p : points) {
        QVERIFY(p.branch());
        QVERIFY(p.branch()->isBranch);
        QVERIFY(p.index() >= 0 && p.index() <= p.branch()->items.size());
        QVERIFY(bounds.contains(p.point()));
        // the nearest point of an insertion point is itself
        const QInsertionPoint nearest = chart.getNearistPoint(qRound(p.point().x()), qRound(p.point().y()));
        QCOMPARE(nearest.point(), p.point());
    }
    // zoom moves the points with the blocks
    chart.setZoom(2);
    QCOMPARE(int(chart.insertionPointList().size()), expectedInsertionPoints(chart.root()));
    chart.setStatus(QFlowChart::Selectable);
    QVERIFY(chart.insertionPointList().isEmpty());
}

void Test_flowchart::insertWithMouse()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("max_of_two.afc")), &error), qPrintable(error));
    chart.resize(chart.sizeHint());
    chart.show();
    QVERIFY(QTest::qWaitForWindowExposed(&chart));

    chart.setBuffer(QStringLiteral("<algorithm><branch><process text=\"inserted\"/></branch></algorithm>"));
    QVERIFY(!chart.buffer().isEmpty());
    chart.setStatus(QFlowChart::Insertion);
    // the point at the end of the algorithm body
    QInsertionPoint target;
    for (const QInsertionPoint &p : chart.insertionPointList()) {
        if (p.branch() == body(chart) && p.index() == body(chart)->items.size())
            target = p;
    }
    QVERIFY(!target.isNull());
    const int before = int(body(chart)->items.size());
    QTest::mouseClick(&chart, Qt::LeftButton, Qt::NoModifier, target.point().toPoint());
    QCOMPARE(int(body(chart)->items.size()), before + 1);
    QBlock *inserted = body(chart)->items.last();
    QCOMPARE(inserted->type(), QStringLiteral("process"));
    QCOMPARE(inserted->attributes.value(QStringLiteral("text")), QStringLiteral("inserted"));
    QCOMPARE(inserted->flowChart(), &chart);
    QCOMPARE(chart.status(), int(QFlowChart::Selectable)); // single insertion
    QVERIFY(chart.canUndo());
    chart.undo();
    QCOMPARE(int(body(chart)->items.size()), before);

    // selecting with the mouse
    QBlock *ifBlock = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("if"); });
    QVERIFY(ifBlock);
    const QPoint diamond(qRound(ifBlock->x + ifBlock->width / 2), qRound(ifBlock->y + 40));
    QTest::mouseClick(&chart, Qt::LeftButton, Qt::NoModifier, diamond);
    QCOMPARE(chart.activeBlock(), ifBlock);
    QSignalSpy edit(&chart, &QFlowChart::editBlock);
    QTest::mouseDClick(&chart, Qt::LeftButton, Qt::NoModifier, diamond);
    QCOMPARE(edit.count(), 1);
    QCOMPARE(edit.at(0).at(0).value<QBlock *>(), ifBlock);
}

void Test_flowchart::deleteAndCopyPaste()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("all_blocks.afc")), &error), qPrintable(error));
    const QString original = chart.toString();

    // copy the FOR loop, delete it, paste it back at the same place
    QBlock *forBlock = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("for"); });
    QVERIFY(forBlock);
    const int index = forBlock->index();
    chart.setActiveBlock(forBlock);
    const QString copied = chart.activeBlockXml();
    QVERIFY(QFlowChart::isInsertableBuffer(copied));
    QVERIFY(copied.contains(QLatin1String("<for ")));
    QVERIFY(copied.contains(QLatin1String("s + i")));

    QApplication::clipboard()->setText(copied);
    QVERIFY(chart.canPaste());

    chart.deleteActiveBlock();
    QVERIFY(!chart.activeBlock());
    QVERIFY(!findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("for"); }));

    chart.setBuffer(copied);
    chart.setStatus(QFlowChart::Insertion);
    QInsertionPoint target;
    for (const QInsertionPoint &p : chart.insertionPointList()) {
        if (p.branch() == body(chart) && p.index() == index)
            target = p;
    }
    QVERIFY(!target.isNull());
    chart.resize(chart.sizeHint());
    QTest::mouseClick(&chart, Qt::LeftButton, Qt::NoModifier, target.point().toPoint());
    QCOMPARE(chart.toString(), original);

    // copying a branch copies its content; copying the algorithm copies everything
    chart.setActiveBlock(body(chart));
    QVERIFY(QFlowChart::isInsertableBuffer(chart.activeBlockXml()));
    chart.setActiveBlock(chart.root());
    QVERIFY(QFlowChart::isInsertableBuffer(chart.activeBlockXml()));
    chart.deselectAll();
    QVERIFY(chart.activeBlockXml().isEmpty());

    // deleting the algorithm clears its body
    chart.deleteBlock(chart.root());
    QCOMPARE(body(chart)->items.size(), 0);
    QCOMPARE(chart.root()->items.size(), 1);
}

void Test_flowchart::deleteParentOfSelection()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("nested.afc")), &error), qPrintable(error));
    QBlock *outer = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("for"); });
    QVERIFY(outer);
    QBlock *inner = findBlock(outer, [](QBlock *b) { return b->type() == QLatin1String("post"); });
    QVERIFY(inner);
    chart.setActiveBlock(inner);
    QPointer<QBlock> watch(inner);
    chart.deleteBlock(outer);
    QVERIFY(watch.isNull());
    QVERIFY(!chart.activeBlock()); // no dangling selection
    chart.resize(chart.sizeHint());
    QVERIFY(!chart.grab().isNull());
}

void Test_flowchart::insertableBuffer()
{
    QVERIFY(QFlowChart::isInsertableBuffer(QStringLiteral("<algorithm><branch/></algorithm>")));
    QVERIFY(QFlowChart::isInsertableBuffer(QStringLiteral("<algorithm><branch><process text=\"x\"/></branch></algorithm>")));
    QVERIFY(!QFlowChart::isInsertableBuffer(QString()));
    QVERIFY(!QFlowChart::isInsertableBuffer(QStringLiteral("hello")));
    QVERIFY(!QFlowChart::isInsertableBuffer(QStringLiteral("<foo/>")));
    QVERIFY(!QFlowChart::isInsertableBuffer(QStringLiteral("<algorithm/>")));
    QFlowChart chart;
    chart.setBuffer(QStringLiteral("<html><body/></html>"));
    QVERIFY(chart.buffer().isEmpty());
    QApplication::clipboard()->setText(QStringLiteral("<p>not a flowchart</p>"));
    QVERIFY(!chart.canPaste());
}

void Test_flowchart::unknownElements()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(robustness(QStringLiteral("unknown_elements.afc")), &error), qPrintable(error));
    QBlock *mystery = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("mystery"); });
    QVERIFY(mystery);
    QCOMPARE(mystery->items.size(), 2);
    QVERIFY(mystery->width > 0 && mystery->height > 0);
    QVERIFY(!QBlock::isKnownType(QStringLiteral("mystery")));
    QVERIFY(QBlock::isKnownType(QStringLiteral("if")));

    // unknown elements are saved unchanged (compared canonically: before Qt 6.5
    // QDomDocument writes the attributes in hash order, which varies between runs)
    const QDomDocument saved = chart.document();
    QCOMPARE(canonical(saved.elementsByTagName(QStringLiteral("frobnicate")).item(0).toElement()),
             QStringLiteral("<frobnicate text=something new></frobnicate>"));
    QCOMPARE(canonical(saved.elementsByTagName(QStringLiteral("gizmo")).item(0).toElement()),
             QStringLiteral("<gizmo></gizmo>"));
    QCOMPARE(canonical(saved.elementsByTagName(QStringLiteral("whatever")).item(0).toElement()),
             QStringLiteral("<whatever a=1 b=2></whatever>"));

    const QImage image = afce::renderFlowchartImage(chart.document());
    QVERIFY(!image.isNull());
    QVERIFY(inkRatio(image) > 0.005);

    // insertion into the branches of an unknown element works
    chart.setStatus(QFlowChart::Insertion);
    QCOMPARE(int(chart.insertionPointList().size()), expectedInsertionPoints(chart.root()));

    // selecting and deleting an unknown block
    chart.setStatus(QFlowChart::Selectable);
    chart.setActiveBlock(mystery);
    chart.deleteActiveBlock();
    QVERIFY(!chart.document().toString().contains(QLatin1String("mystery")));
}

void Test_flowchart::malformedBlocks()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(robustness(QStringLiteral("malformed_blocks.afc")), &error), qPrintable(error));
    // missing branches are added so that every block can be drawn and edited
    findBlock(chart.root(), [](QBlock *b) {
        int branches = 0;
        for (const QBlock *child : b->items)
            branches += child->isBranch ? 1 : 0;
        if (branches < QBlock::requiredBranchCount(b->type()))
            qWarning() << "missing branches in" << b->type();
        return false;
    });
    QBlock *ifBlock = findBlock(chart.root(), [](QBlock *b) {
        return b->type() == QLatin1String("if") && b->attributes.value(QStringLiteral("cond")) == QLatin1String("no branches");
    });
    QVERIFY(ifBlock);
    QCOMPARE(ifBlock->items.size(), 2);
    QVERIFY(ifBlock->item(0)->isBranch && ifBlock->item(1)->isBranch);
    const QImage image = afce::renderFlowchartImage(chart.document());
    QVERIFY(!image.isNull());

    // and the result is stable
    QFlowChart again;
    QVERIFY2(again.loadDocument(chart.document(), &error), qPrintable(error));
    QCOMPARE(again.toString(), chart.toString());
}

void Test_flowchart::blockOwnership()
{
    int destroyed = 0;
    auto watch = [&destroyed](QBlock *b) {
        QObject::connect(b, &QObject::destroyed, [&destroyed]() { ++destroyed; });
    };

    // a parent deletes its items, even without a parent of its own
    QBlock *ifBlock = new QBlock(QStringLiteral("if"));
    QBlock *yes = new QBlock(QStringLiteral("branch"));
    QBlock *no = new QBlock(QStringLiteral("branch"));
    QBlock *inner = new QBlock(QStringLiteral("process"));
    QVERIFY(yes->isBranch);
    ifBlock->append(yes);
    ifBlock->append(no);
    yes->append(inner);
    QCOMPARE(inner->parent, yes);
    QCOMPARE(yes->parent, ifBlock);
    for (QBlock *b : {ifBlock, yes, no, inner})
        watch(b);
    delete ifBlock;
    QCOMPARE(destroyed, 4);

    // remove() hands the block over to the caller
    destroyed = 0;
    QBlock *branch = new QBlock(QStringLiteral("branch"));
    QBlock *a = new QBlock(QStringLiteral("process"));
    QBlock *b = new QBlock(QStringLiteral("process"));
    branch->append(a);
    branch->append(b);
    branch->remove(a);
    QVERIFY(!a->parent);
    QCOMPARE(branch->items.size(), 1);
    watch(a);
    watch(branch);
    watch(b);
    delete branch;
    QCOMPARE(destroyed, 2);
    QVERIFY(!a->parent);
    delete a;
    QCOMPARE(destroyed, 3);

    // deleting an item detaches it from its parent
    QBlock *list = new QBlock(QStringLiteral("branch"));
    QBlock *x = new QBlock(QStringLiteral("process"));
    QBlock *y = new QBlock(QStringLiteral("process"));
    list->append(x);
    list->append(y);
    delete x;
    QCOMPARE(list->items.size(), 1);
    QCOMPARE(list->item(0), y);

    // moving a block between parents
    QBlock *other = new QBlock(QStringLiteral("branch"));
    other->insert(0, y);
    QCOMPARE(list->items.size(), 0);
    QCOMPARE(y->parent, other);

    // setItem() replaces and deletes the old item
    QBlock *z = new QBlock(QStringLiteral("assign"));
    destroyed = 0;
    watch(y);
    other->setItem(0, z);
    QCOMPARE(destroyed, 1);
    QCOMPARE(other->item(0), z);
    QCOMPARE(z->parent, other);
    delete list;
    delete other;
}

void Test_flowchart::chartDeletesAllBlocks()
{
    auto chart = std::make_unique<QFlowChart>();
    QString error;
    QVERIFY2(chart->loadFile(sample(QStringLiteral("nested.afc")), &error), qPrintable(error));
    int destroyed = 0;
    QList<QBlock *> blocks;
    findBlock(chart->root(), [&blocks](QBlock *b) { blocks << b; return false; });
    QCOMPARE(int(blocks.size()), countBlocks(chart->root()));
    for (QBlock *b : std::as_const(blocks))
        QObject::connect(b, &QObject::destroyed, [&destroyed]() { ++destroyed; });
    chart.reset();
    QCOMPARE(destroyed, int(blocks.size())); // including the root, no double deletes

    // reloading frees the previous tree
    QFlowChart reused;
    QVERIFY2(reused.loadFile(sample(QStringLiteral("nested.afc")), &error), qPrintable(error));
    QPointer<QBlock> old = body(reused)->item(0);
    QVERIFY2(reused.loadFile(sample(QStringLiteral("gcd.afc")), &error), qPrintable(error));
    QVERIFY(old.isNull());
}

void Test_flowchart::noSignalsFromDestructor()
{
    QFlowChart *chart = new QFlowChart;
    QString error;
    QVERIFY2(chart->loadFile(sample(QStringLiteral("all_blocks.afc")), &error), qPrintable(error));
    chart->setActiveBlock(body(*chart)->item(0));
    int emitted = 0;
    QObject::connect(chart, &QFlowChart::changed, [&emitted]() { ++emitted; });
    QObject::connect(chart, &QFlowChart::modified, [&emitted]() { ++emitted; });
    QObject::connect(chart, &QFlowChart::statusChanged, [&emitted]() { ++emitted; });
    delete chart;
    QCOMPARE(emitted, 0);
}

void Test_flowchart::newBlockTypes()
{
    for (const char *type : {"forc", "foreach", "case", "call", "return", "break", "continue"})
        QVERIFY2(QBlock::isKnownType(QString::fromLatin1(type)), type);
    QCOMPARE(QBlock::requiredBranchCount(QStringLiteral("case")), 2);
    QCOMPARE(QBlock::requiredBranchCount(QStringLiteral("forc")), 1);
    QCOMPARE(QBlock::requiredBranchCount(QStringLiteral("foreach")), 1);
    for (const char *type : {"call", "return", "break", "continue"})
        QCOMPARE(QBlock::requiredBranchCount(QString::fromLatin1(type)), 0);
    QVERIFY(QBlock::isJumpType(QStringLiteral("break")));
    QVERIFY(!QBlock::isJumpType(QStringLiteral("call")));

    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("new_blocks.afc")), &error), qPrintable(error));
    QCOMPARE(chart.root()->attributes.value(QStringLiteral("name")), QStringLiteral("classify"));
    QCOMPARE(chart.root()->attributes.value(QStringLiteral("params")), QStringLiteral("int n"));
    QBlock *caseBlock = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("case"); });
    QVERIFY(caseBlock);
    QCOMPARE(caseBlock->items.size(), 4);
    QCOMPARE(caseBlock->item(1)->attributes.value(QStringLiteral("value")), QStringLiteral("1,3"));
    QCOMPARE(caseBlock->branchLabel(1), QStringLiteral("1, 3"));
    QCOMPARE(caseBlock->branchLabel(3), QStringLiteral("otherwise"));
    for (const char *type : {"forc", "foreach", "case", "call", "return", "break", "continue"}) {
        QBlock *block = findBlock(chart.root(), [type](QBlock *b) { return b->type() == QLatin1String(type); });
        QVERIFY2(block, type);
        QVERIFY2(block->isWellFormed(), type);
        QVERIFY(block->width > 0 && block->height > 0);
    }

    // a case without branches gets the two it needs (the last one is the default branch)
    QVERIFY(chart.loadDocument(parse(QStringLiteral("<algorithm><branch><case expr=\"x\"/></branch></algorithm>")), &error));
    caseBlock = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("case"); });
    QCOMPARE(caseBlock->items.size(), 2);
    QVERIFY(caseBlock->isWellFormed());
    // malformed: a case with a non-branch item is drawn as a generic box, nothing is lost
    QVERIFY(chart.loadDocument(parse(QStringLiteral("<algorithm><branch><case expr=\"x\"><process/><branch/><branch/></case></branch></algorithm>")), &error));
    caseBlock = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("case"); });
    QVERIFY(!caseBlock->isWellFormed());
    QVERIFY(!afce::renderFlowchartImage(chart.document()).isNull());
    QVERIFY(chart.document().toString().contains(QLatin1String("<process/>")));
}

void Test_flowchart::layoutSanity_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<double>("zoom");
    for (const QString &file : sampleFiles()) {
        for (double zoom : {1.0, 2.0})
            QTest::addRow("%s %g", qPrintable(QFileInfo(file).fileName()), zoom) << file << zoom;
    }
    QTest::addRow("unknown_elements 1") << robustness(QStringLiteral("unknown_elements.afc")) << 1.0;
    QTest::addRow("malformed_blocks 1") << robustness(QStringLiteral("malformed_blocks.afc")) << 1.0;
}

void Test_flowchart::layoutSanity()
{
    QFETCH(QString, fileName);
    QFETCH(double, zoom);
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(fileName, &error), qPrintable(error));
    chart.setZoom(zoom);
    const double eps = 0.01;
    QStringList problems;
    std::function<void(const QBlock *)> check = [&](const QBlock *block) {
        const QRectF rect(block->x, block->y, block->width, block->height);
        const QString name = block->type();
        for (int i = 0; i < block->items.size(); ++i) {
            const QBlock *child = block->item(i);
            const QRectF childRect(child->x, child->y, child->width, child->height);
            if (!rect.adjusted(-eps, -eps, eps, eps).contains(childRect))
                problems << QStringLiteral("%1 item %2 (%3) is outside its parent").arg(name).arg(i).arg(child->type());
            if (i > 0) {
                const QBlock *previous = block->item(i - 1);
                if (block->isBranch ? child->y < previous->y + previous->height - eps
                                    : child->x < previous->x + previous->width - eps)
                    problems << QStringLiteral("%1: items %2 and %3 overlap").arg(name).arg(i - 1).arg(i);
            }
        }
        if (!block->isBranch) {
            const QRectF symbol = block->symbolRect();
            if (symbol.isEmpty() || !rect.adjusted(-eps, -eps, eps, eps).contains(symbol))
                problems << QStringLiteral("%1: symbol outside the block").arg(name);
            // the symbol does not overlap the branches
            for (const QBlock *child : block->items) {
                const QRectF childRect(child->x, child->y, child->width, child->height);
                if (childRect.intersected(symbol).height() > eps && childRect.intersected(symbol).width() > eps)
                    problems << QStringLiteral("%1: symbol overlaps an item").arg(name);
            }
            // the text fits into the symbol
            const QRectF text = block->textRect();
            const QPainterPath path = block->symbolPath();
            if (text.isEmpty() && block->type() != QLatin1String("case"))
                problems << QStringLiteral("%1: no text").arg(name);
            if (!text.isEmpty()) {
                for (const QPointF &corner : {text.topLeft(), text.topRight(), text.bottomLeft(), text.bottomRight()}) {
                    if (!path.contains(corner))
                        problems << QStringLiteral("%1: text corner (%2, %3) outside the symbol").arg(name).arg(corner.x()).arg(corner.y());
                }
            }
        }
        for (const QBlock *child : block->items)
            check(child);
    };
    check(chart.root());
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
    // the widget has the size of the chart
    QCOMPARE(chart.size(), QSize(qCeil(chart.root()->width), qCeil(chart.root()->height)));
}

void Test_flowchart::terminates_data()
{
    QTest::addColumn<QString>("branch");
    QTest::addColumn<bool>("expected");
    QTest::newRow("empty") << QString() << false;
    QTest::newRow("process") << QStringLiteral("<process text=\"x\"/>") << false;
    QTest::newRow("return") << QStringLiteral("<return value=\"1\"/>") << true;
    QTest::newRow("break") << QStringLiteral("<break/>") << true;
    QTest::newRow("continue") << QStringLiteral("<continue/>") << true;
    QTest::newRow("jump then block") << QStringLiteral("<break/><process text=\"x\"/>") << false;
    QTest::newRow("block then jump") << QStringLiteral("<process text=\"x\"/><return/>") << true;
    QTest::newRow("if both") << QStringLiteral("<if cond=\"c\"><branch><return/></branch><branch><break/></branch></if>") << true;
    QTest::newRow("if one") << QStringLiteral("<if cond=\"c\"><branch><return/></branch><branch/></if>") << false;
    QTest::newRow("case all") << QStringLiteral("<case expr=\"x\"><branch value=\"1\"><return/></branch><branch><break/></branch></case>") << true;
    QTest::newRow("case default open") << QStringLiteral("<case expr=\"x\"><branch value=\"1\"><return/></branch><branch/></case>") << false;
    QTest::newRow("nested if") << QStringLiteral("<if cond=\"a\"><branch><if cond=\"b\"><branch><return/></branch><branch><return/></branch></if></branch><branch><continue/></branch></if>") << true;
    QTest::newRow("loop with break") << QStringLiteral("<pre cond=\"c\"><branch><break/></branch></pre>") << false;
    QTest::newRow("forc with return") << QStringLiteral("<forc><branch><return/></branch></forc>") << false;
}

void Test_flowchart::terminates()
{
    QFETCH(QString, branch);
    QFETCH(bool, expected);
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadDocument(parse(QStringLiteral("<algorithm><branch>%1</branch></algorithm>").arg(branch)), &error),
             qPrintable(error));
    QCOMPARE(body(chart)->terminates(), expected);
    QCOMPARE(chart.root()->terminates(), expected);
    if (!body(chart)->items.isEmpty())
        QCOMPARE(body(chart)->items.last()->terminates(), expected || body(chart)->items.last()->terminates());
}

void Test_flowchart::endNotDrawnAfterReturn()
{
    auto texts = [](const QString &xml) {
        QFlowChart chart;
        chart.loadDocument(parse(xml));
        QStringList result;
        for (const TextRecorder::Line &line : paintedText(chart, 1))
            result << line.text;
        return result;
    };
    const QStringList open = texts(QStringLiteral("<algorithm><branch><process text=\"a\"/></branch></algorithm>"));
    QVERIFY2(open.contains(QStringLiteral("END")), qPrintable(open.join(QLatin1Char('|'))));
    QVERIFY(open.contains(QStringLiteral("BEGIN")));
    const QStringList closed = texts(QStringLiteral("<algorithm name=\"f\" params=\"int a\"><branch><return value=\"a\"/></branch></algorithm>"));
    QVERIFY2(!closed.contains(QStringLiteral("END")), qPrintable(closed.join(QLatin1Char('|'))));
    QVERIFY(closed.contains(QStringLiteral("f(int a)")));
    QVERIFY(closed.contains(QStringLiteral("return a")));

    // a chart ending with a return is shorter (no END terminator)
    QFlowChart a, b;
    a.loadDocument(parse(QStringLiteral("<algorithm><branch><process text=\"a\"/></branch></algorithm>")));
    b.loadDocument(parse(QStringLiteral("<algorithm><branch><return/></branch></algorithm>")));
    QVERIFY(b.root()->height < a.root()->height);
}

void Test_flowchart::captions()
{
    QFlowChart chart;
    QString error;
    QVERIFY(chart.loadDocument(parse(QStringLiteral(
        "<algorithm><branch><io vars=\"a,b\"/><ou vars=\"&quot;x, y = &quot;,f(a, b)\"/><assign dest=\"s\" src=\"0\"/>"
        "<forc init=\"\" cond=\"\" step=\"\"><branch/></forc><forc init=\"int i = 0\" cond=\"i &lt; n\" step=\"i++\"><branch/></forc>"
        "<foreach var=\"x\" range=\"v\"><branch/></foreach><for var=\"i\" from=\"1\" to=\"n\"><branch/></for>"
        "<call text=\"go()\"/><break/><continue/><return/></branch></algorithm>")), &error));
    QStringList lines;
    for (const TextRecorder::Line &line : paintedText(chart, 1))
        lines << line.text;
    for (const char *expected : {"Input: a, b", "Output: \"x, y = \", f(a, b)", "s := 0", "for (;;)", "int i = 0; i < n; i++",
                                 "x in v", "go()", "break", "continue", "return"})
        QVERIFY2(lines.contains(QString::fromUtf8(expected)), qPrintable(QString::fromUtf8(expected) + QStringLiteral(" in ") + lines.join(QLatin1Char('|'))));

    // the assignment symbol comes from the style
    QFlowChartStyle style = chart.chartStyle();
    style.setAssignSymbol(QStringLiteral("\u2190"));
    chart.setChartStyle(style);
    lines.clear();
    for (const TextRecorder::Line &line : paintedText(chart, 1))
        lines << line.text;
    // (a symbol from a fallback font is a text item of its own)
    QVERIFY2(lines.join(QString()).contains(QStringLiteral("s \u2190 0")), qPrintable(lines.join(QLatin1Char('|'))));

    // long texts are wrapped, overlong words broken
    QVERIFY(chart.loadDocument(parse(QStringLiteral("<algorithm><branch><process text=\"%1\"/></branch></algorithm>")
                                         .arg(QString(200, QLatin1Char('w')))), &error));
    const QBlock *process = body(chart)->item(0);
    QVERIFY(process->textRect().width() <= 361);
    QVERIFY(process->textRect().height() > 3 * 13);
    QVERIFY(process->symbolRect().contains(process->textRect()));
}

void Test_flowchart::blockCatalog()
{
    const QList<afce::BlockKind> kinds = afce::blockKinds();
    QCOMPARE(kinds.size(), 15);
    QStringList types;
    for (const afce::BlockKind &kind : kinds) {
        QVERIFY(!types.contains(kind.type));
        types << kind.type;
        QVERIFY2(QBlock::isKnownType(kind.type), qPrintable(kind.type));
        QVERIFY(!kind.title.isEmpty());
        QVERIFY(!kind.description.isEmpty());
        QVERIFY(QStringList({QStringLiteral("basic"), QStringLiteral("branching"), QStringLiteral("loops"), QStringLiteral("jumps")})
                    .contains(kind.group));
        QVERIFY(!afce::blockGroupTitle(kind.group).isEmpty());
        QVERIFY2(QFlowChart::isInsertableBuffer(kind.xmlTemplate), qPrintable(kind.xmlTemplate));
        const QDomDocument doc = parse(kind.xmlTemplate);
        QCOMPARE(doc.documentElement().firstChildElement().firstChildElement().tagName(), kind.type);

        // the icon is painted (not transparent) at several sizes and in dark mode
        QPalette dark;
        dark.setColor(QPalette::Window, QColor(40, 40, 40));
        dark.setColor(QPalette::WindowText, QColor(230, 230, 230));
        dark.setColor(QPalette::Base, QColor(30, 30, 30));
        for (const QPalette &palette : {QPalette(), dark}) {
            const QIcon icon = afce::blockIcon(kind.type, palette);
            QVERIFY(!icon.isNull());
            for (int size : {16, 32}) {
                const QImage image = icon.pixmap(QSize(size, size), 2.0).toImage();
                QVERIFY(!image.isNull());
                int opaque = 0;
                for (int y = 0; y < image.height(); ++y)
                    for (int x = 0; x < image.width(); ++x)
                        opaque += qAlpha(image.pixel(x, y)) > 128 ? 1 : 0;
                QVERIFY2(opaque > image.width() * image.height() / 20, qPrintable(kind.type));
            }
        }
    }
    // the templates insert a block of the kind
    QFlowChart chart;
    chart.setBuffer(kinds.at(6).xmlTemplate);
    QVERIFY(!chart.buffer().isEmpty());
}

void Test_flowchart::everyBlockRenders_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("xml");
    for (const afce::BlockKind &kind : afce::blockKinds())
        QTest::newRow(qPrintable(kind.type)) << kind.type << kind.xmlTemplate;
}

void Test_flowchart::everyBlockRenders()
{
    QFETCH(QString, type);
    QFETCH(QString, xml);
    QFlowChart empty;
    const QImage emptyImage = afce::renderFlowchartImage(empty.document());
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadDocument(parse(xml), &error), qPrintable(error));
    const QBlock *block = body(chart)->item(0);
    QCOMPARE(block->type(), type);
    QVERIFY(block->isWellFormed());
    for (bool monochrome : {false, true}) {
        afce::ExportOptions options;
        options.monochrome = monochrome;
        const QImage image = afce::renderFlowchartImage(chart.document(), options);
        QVERIFY(!image.isNull());
        // (a jump ends the algorithm: no END terminator)
        QVERIFY(image.height() > emptyImage.height() || QBlock::isJumpType(type));
        const double ink = inkRatio(image);
        QVERIFY2(ink > 0.01 && ink < 0.5, qPrintable(QStringLiteral("ink %1").arg(ink)));
        // the symbol area is painted: the pixel at the centre of the text box differs from the canvas or has text
        bool painted = false;
        const QRectF symbol = block->symbolRect();
        for (int y = int(symbol.top()) + 2; y < int(symbol.bottom()) - 1 && !painted; ++y) {
            for (int x = int(symbol.left()) + 2; x < int(symbol.right()) - 1 && !painted; ++x)
                painted = qGray(image.pixel(x, y)) < 128;
        }
        QVERIFY2(painted, "no text or lines inside the symbol");
    }
}

void Test_flowchart::caseBranches()
{
    QFlowChart chart;
    QString error;
    QVERIFY(chart.loadDocument(parse(afce::blockKinds().at(6).xmlTemplate), &error));
    QBlock *caseBlock = body(chart)->item(0);
    QCOMPARE(caseBlock->type(), QStringLiteral("case"));
    QCOMPARE(caseBlock->items.size(), 3);
    const double width3 = caseBlock->width;

    // add a branch before the default branch
    chart.makeUndo();
    QBlock *branch = new QBlock(QStringLiteral("branch"));
    branch->attributes.insert(QStringLiteral("value"), QStringLiteral("3,4"));
    caseBlock->insert(2, branch);
    branch->append(new QBlock(QStringLiteral("process")));
    chart.realignObjects();
    QCOMPARE(caseBlock->items.size(), 4);
    QCOMPARE(branch->flowChart(), &chart);
    QVERIFY(caseBlock->width > width3);
    QCOMPARE(caseBlock->branchLabel(2), QStringLiteral("3, 4"));
    QCOMPARE(caseBlock->branchLabel(3), QStringLiteral("otherwise"));
    chart.setStatus(QFlowChart::Insertion);
    QCOMPARE(int(chart.insertionPointList().size()), expectedInsertionPoints(chart.root()));
    int pointsInBranch = 0;
    for (const QInsertionPoint &p : chart.insertionPointList())
        pointsInBranch += p.branch() == branch ? 1 : 0;
    QCOMPARE(pointsInBranch, 2);
    chart.setStatus(QFlowChart::Selectable);

    // saved and loaded
    QFlowChart copy;
    QVERIFY(copy.loadDocument(chart.document(), &error));
    QCOMPARE(copy.toString(), chart.toString());

    // remove branches: at least the default branch and one more remain
    delete caseBlock->item(0);
    delete caseBlock->item(0);
    delete caseBlock->item(0);
    chart.realignObjects();
    QCOMPARE(caseBlock->items.size(), 2);
    QVERIFY(caseBlock->isWellFormed());
    chart.undo();
    caseBlock = body(chart)->item(0);
    QCOMPARE(caseBlock->items.size(), 3);
}

void Test_flowchart::dropFragmentFromOutside()
{
    // a block dragged from the tool panel (Qt drag and drop with a fragment)
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("gcd.afc")), &error), qPrintable(error));
    chart.setStatus(QFlowChart::Selectable);
    chart.resize(chart.sizeHint());
    chart.show();
    QVERIFY(QTest::qWaitForWindowExposed(&chart));
    const QString original = chart.toString();

    QBlock *body = chart.root()->item(0);
    const int count = int(body->items.size());
    QVERIFY(count > 0);
    // the insertion point at the end of the body
    const QPoint to(qRound(body->x + body->width / 2), qRound(body->y + body->height));
    QMimeData data;
    data.setData(QFlowChart::fragmentMimeType(), "<algorithm><branch><call text=\"hello()\"/></branch></algorithm>");

    QDragEnterEvent enter(to, Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&chart, &enter);
    QVERIFY(enter.isAccepted());
    QVERIFY(chart.isExternalDragActive());
    QVERIFY(!chart.insertionPointList().isEmpty());
    QVERIFY(!chart.targetPoint().isNull());
    QDragMoveEvent move(to, Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&chart, &move);
    QVERIFY(move.isAccepted());

    QSignalSpy modified(&chart, &QFlowChart::modified);
    QDropEvent drop(QPointF(to), Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&chart, &drop);
    QVERIFY(drop.isAccepted());
    QVERIFY(!chart.isExternalDragActive());
    QVERIFY(chart.insertionPointList().isEmpty());
    body = chart.root()->item(0);
    QCOMPARE(int(body->items.size()), count + 1);
    QCOMPARE(body->items.last()->type(), QStringLiteral("call"));
    QCOMPARE(chart.activeBlock(), body->items.last());
    QCOMPARE(modified.count(), 1);
    chart.undo();
    QCOMPARE(chart.toString(), original);

    // leaving the chart ends the drag without changes
    QDragEnterEvent enter2(to, Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&chart, &enter2);
    QVERIFY(chart.isExternalDragActive());
    QDragLeaveEvent leave;
    QApplication::sendEvent(&chart, &leave);
    QVERIFY(!chart.isExternalDragActive());
    QVERIFY(chart.insertionPointList().isEmpty());
    QCOMPARE(chart.toString(), original);

    // other data and the display mode are rejected
    QMimeData text;
    text.setText(QStringLiteral("hello"));
    QDragEnterEvent enterText(to, Qt::CopyAction, &text, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&chart, &enterText);
    QVERIFY(!enterText.isAccepted());
    QMimeData broken;
    broken.setData(QFlowChart::fragmentMimeType(), "<not-a-flowchart/>");
    QDragEnterEvent enterBroken(to, Qt::CopyAction, &broken, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&chart, &enterBroken);
    QVERIFY(!enterBroken.isAccepted());
    chart.setStatus(QFlowChart::Display);
    QDragEnterEvent enterDisplay(to, Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&chart, &enterDisplay);
    QVERIFY(!enterDisplay.isAccepted());
}

void Test_flowchart::dragMoveIsOneUndoStep()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("new_blocks.afc")), &error), qPrintable(error));
    chart.setStatus(QFlowChart::Selectable);
    chart.resize(chart.sizeHint());
    chart.show();
    QVERIFY(QTest::qWaitForWindowExposed(&chart));
    const QString original = chart.toString();

    QBlock *call = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("call"); });
    QVERIFY(call);
    QCOMPARE(call->index(), 2);
    const QPoint from = call->symbolRect().center().toPoint();
    // target: before the first item of the foreach body (the if)
    QBlock *foreachBlock = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("foreach"); });
    QBlock *target = foreachBlock->item(0);
    const QPoint to(qRound(target->x + target->width / 2), qRound(target->y + 1));

    QSignalSpy modified(&chart, &QFlowChart::modified);
    QTest::mousePress(&chart, Qt::LeftButton, Qt::NoModifier, from);
    QCOMPARE(chart.activeBlock(), call);
    QTest::mouseMove(&chart, from + QPoint(0, 30));
    QVERIFY(chart.isDragging());
    QCOMPARE(chart.draggedBlock(), call);
    QVERIFY(!chart.insertionPointList().isEmpty());
    // no insertion point inside the dragged block (it has no branches) and none in other charts
    for (const QInsertionPoint &p : chart.insertionPointList())
        QVERIFY(p.branch() && p.branch()->flowChart() == &chart);
    QTest::mouseMove(&chart, to);
    QVERIFY(!chart.targetPoint().isNull());
    QTest::mouseRelease(&chart, Qt::LeftButton, Qt::NoModifier, to);
    QVERIFY(!chart.isDragging());
    QVERIFY(chart.insertionPointList().isEmpty());

    QCOMPARE(call->parent, target);
    QCOMPARE(call->index(), 0);
    QCOMPARE(chart.activeBlock(), call);
    QCOMPARE(modified.count(), 1);
    QVERIFY(chart.toString() != original);
    chart.undo();
    QCOMPARE(chart.toString(), original);
    QVERIFY(!chart.canUndo());

    // a click without moving does not drag
    call = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("call"); });
    QTest::mouseClick(&chart, Qt::LeftButton, Qt::NoModifier, call->symbolRect().center().toPoint());
    QVERIFY(!chart.isDragging());
    QVERIFY(!chart.canUndo());
    QCOMPARE(chart.toString(), original);
}

void Test_flowchart::dragCopy()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("gcd.afc")), &error), qPrintable(error));
    chart.setStatus(QFlowChart::Selectable);
    chart.resize(chart.sizeHint());
    chart.show();
    QVERIFY(QTest::qWaitForWindowExposed(&chart));
    QBlock *io = body(chart)->item(0);
    const int before = int(body(chart)->items.size());
    const QPoint from = io->symbolRect().center().toPoint();
    const QPoint to(qRound(body(chart)->x + body(chart)->width / 2), qRound(body(chart)->y + body(chart)->height));
    QTest::mousePress(&chart, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(&chart, from + QPoint(0, 30));
    QTest::mouseMove(&chart, to);
    QTest::mouseRelease(&chart, Qt::LeftButton, Qt::AltModifier, to);
    QCOMPARE(int(body(chart)->items.size()), before + 1);
    QCOMPARE(body(chart)->item(0), io);
    QCOMPARE(body(chart)->items.last()->type(), QStringLiteral("io"));
    QCOMPARE(chart.activeBlock(), body(chart)->items.last());
    chart.undo();
    QCOMPARE(int(body(chart)->items.size()), before);

    // Esc cancels a drag
    io = body(chart)->item(0);
    QTest::mousePress(&chart, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(&chart, from + QPoint(0, 30));
    QVERIFY(chart.isDragging());
    QTest::keyClick(&chart, Qt::Key_Escape);
    QVERIFY(!chart.isDragging());
    QTest::mouseRelease(&chart, Qt::LeftButton, Qt::NoModifier, to);
    QVERIFY(!chart.canUndo());
}

void Test_flowchart::dropIntoItselfRejected()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("nested.afc")), &error), qPrintable(error));
    const QString original = chart.toString();
    QBlock *outer = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("for"); });
    QBlock *inner = findBlock(outer->item(0), [](QBlock *b) { return b->type() == QLatin1String("post"); });
    QInsertionPoint intoItself;
    intoItself.setBranch(inner->item(0));
    intoItself.setIndex(0);
    QVERIFY(!chart.dropBlock(outer, intoItself, false));
    QVERIFY(!chart.dropBlock(outer, intoItself, true));
    // the own place is not a move
    QInsertionPoint same;
    same.setBranch(outer->parent);
    same.setIndex(outer->index());
    QVERIFY(!chart.dropBlock(outer, same, false));
    same.setIndex(outer->index() + 1);
    QVERIFY(!chart.dropBlock(outer, same, false));
    // the algorithm and branches cannot be moved
    QVERIFY(!chart.dropBlock(chart.root(), same, false));
    QVERIFY(!chart.dropBlock(outer->item(0), same, false));
    QCOMPARE(chart.toString(), original);
    QVERIFY(!chart.canUndo());

    // a move to the front of the body
    QInsertionPoint front;
    front.setBranch(body(chart));
    front.setIndex(0);
    QVERIFY(chart.dropBlock(outer, front, false));
    QCOMPARE(body(chart)->item(0), outer);
    chart.undo();
    QCOMPARE(chart.toString(), original);
}

void Test_flowchart::hoverAndEscape()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("max_of_two.afc")), &error), qPrintable(error));
    chart.setStatus(QFlowChart::Selectable);
    chart.resize(chart.sizeHint());
    chart.show();
    QVERIFY(QTest::qWaitForWindowExposed(&chart));
    QBlock *io = body(chart)->item(0);
    hoverAt(&chart, io->symbolRect().center().toPoint());
    QCOMPARE(chart.hoverBlock(), io);
    // blank space of the body hovers nothing
    hoverAt(&chart, QPoint(1, qRound(io->y + io->height / 2)));
    QVERIFY(!chart.hoverBlock());
    // hovering and selection are painted (the picture changes)
    const QImage plain = chart.grab().toImage();
    hoverAt(&chart, io->symbolRect().center().toPoint());
    const QImage hovered = chart.grab().toImage();
    QVERIFY(plain != hovered);
    chart.setActiveBlock(io);
    const QImage selected = chart.grab().toImage();
    QVERIFY(selected != hovered);

    // Esc and the right button leave the insertion mode
    chart.setBuffer(afce::blockKinds().first().xmlTemplate);
    chart.setStatus(QFlowChart::Insertion);
    QTest::keyClick(&chart, Qt::Key_Escape);
    QCOMPARE(chart.status(), int(QFlowChart::Selectable));
    chart.setStatus(QFlowChart::Insertion);
    QTest::mouseClick(&chart, Qt::RightButton, Qt::NoModifier, io->symbolRect().center().toPoint());
    QCOMPARE(chart.status(), int(QFlowChart::Selectable));
    QVERIFY(!chart.canUndo());

    // a deleted hovered block is forgotten
    hoverAt(&chart, io->symbolRect().center().toPoint());
    QCOMPARE(chart.hoverBlock(), io);
    chart.deleteBlock(io);
    QVERIFY(!chart.hoverBlock());
}

void Test_flowchart::doubleClickBeginEditsAlgorithm()
{
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("gcd.afc")), &error), qPrintable(error));
    chart.setStatus(QFlowChart::Selectable);
    chart.resize(chart.sizeHint());
    chart.show();
    QVERIFY(QTest::qWaitForWindowExposed(&chart));
    QSignalSpy edit(&chart, &QFlowChart::editBlock);
    const QPoint begin = chart.root()->symbolRect().center().toPoint();
    QTest::mouseDClick(&chart, Qt::LeftButton, Qt::NoModifier, begin);
    QCOMPARE(edit.count(), 1);
    QCOMPARE(edit.at(0).at(0).value<QBlock *>(), chart.root());
    // a click on the blank canvas selects nothing
    QTest::mouseClick(&chart, Qt::LeftButton, Qt::NoModifier, QPoint(1, chart.height() / 2));
    QVERIFY(!chart.activeBlock());
    // a click on the empty area of a block (beside its branches) selects the block
    QBlock *loop = findBlock(chart.root(), [](QBlock *b) { return b->type() == QLatin1String("pre"); });
    const QPoint blank(qRound(loop->x + 3), qRound(loop->y + loop->height / 2));
    QCOMPARE(chart.root()->blockAt(blank.x(), blank.y()), loop);
    QTest::mouseClick(&chart, Qt::LeftButton, Qt::NoModifier, blank);
    QCOMPARE(chart.activeBlock(), loop);
    // and a click on an empty branch selects the block of the branch
    QVERIFY(chart.loadDocument(parse(QStringLiteral("<algorithm><branch><if cond=\"c\"><branch/><branch/></if></branch></algorithm>")), &error));
    chart.resize(chart.sizeHint());
    QBlock *ifBlock = body(chart)->item(0);
    QBlock *no = ifBlock->item(1);
    const QPoint inBranch(qRound(no->x + 3), qRound(no->y + no->height / 2));
    QCOMPARE(chart.root()->blockAt(inBranch.x(), inBranch.y()), no);
    QTest::mouseClick(&chart, Qt::LeftButton, Qt::NoModifier, inBranch);
    QCOMPARE(chart.activeBlock(), ifBlock);
}

void Test_flowchart::styles()
{
    const QFlowChartStyle document = QFlowChartStyle::documentStyle(false);
    const QFlowChartStyle classic = QFlowChartStyle::documentStyle(true);
    QCOMPARE(document.canvasColor(), QColor(Qt::white));
    QCOMPARE(classic.canvasColor(), QColor(Qt::white));
    QCOMPARE(document.assignSymbol(), QStringLiteral(":="));
    for (int c = 0; c < QFlowChartStyle::CategoryCount; ++c) {
        const QColor fill = document.fillColor(QFlowChartStyle::Category(c));
        QVERIFY(fill != Qt::white);                  // tinted
        QVERIFY(fill.lightnessF() > 0.75);           // but light: printable, dark text readable
        QCOMPARE(classic.fillColor(QFlowChartStyle::Category(c)), QColor(Qt::white));
    }
    QPalette dark;
    dark.setColor(QPalette::Base, QColor(30, 30, 30));
    dark.setColor(QPalette::Text, QColor(230, 230, 230));
    const QFlowChartStyle darkStyle = QFlowChartStyle::uiStyle(dark);
    QCOMPARE(darkStyle.canvasColor(), QColor(30, 30, 30));
    for (int c = 0; c < QFlowChartStyle::CategoryCount; ++c)
        QVERIFY(darkStyle.fillColor(QFlowChartStyle::Category(c)).lightnessF() < 0.45);
    QCOMPARE(QFlowChartStyle::uiStyle(dark, true).fillColor(QFlowChartStyle::Action), QColor(30, 30, 30));
    QCOMPARE(QBlock::category(QStringLiteral("if")), QFlowChartStyle::Decision);
    QCOMPARE(QBlock::category(QStringLiteral("case")), QFlowChartStyle::Decision);
    QCOMPARE(QBlock::category(QStringLiteral("forc")), QFlowChartStyle::Loop);
    QCOMPARE(QBlock::category(QStringLiteral("call")), QFlowChartStyle::Action);
    QCOMPARE(QBlock::category(QStringLiteral("ou")), QFlowChartStyle::InputOutput);
    QCOMPARE(QBlock::category(QStringLiteral("return")), QFlowChartStyle::Jump);
    QCOMPARE(QBlock::category(QStringLiteral("algorithm")), QFlowChartStyle::Terminator);

    // the export of a coloured chart has coloured symbols, the classic one has none
    QFlowChart chart;
    QString error;
    QVERIFY2(chart.loadFile(sample(QStringLiteral("gcd.afc")), &error), qPrintable(error));
    auto colourful = [](const QImage &image) {
        int n = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const QColor c = image.pixelColor(x, y);
                n += (qMax(c.red(), qMax(c.green(), c.blue())) - qMin(c.red(), qMin(c.green(), c.blue()))) > 12 ? 1 : 0;
            }
        return n;
    };
    afce::ExportOptions options;
    QVERIFY(colourful(afce::renderFlowchartImage(chart.document(), options)) > 1000);
    options.monochrome = true;
    QCOMPARE(colourful(afce::renderFlowchartImage(chart.document(), options)), 0);

    // fitting the zoom
    chart.setZoom(1);
    const QSize natural = chart.size();
    const double fit = chart.fitZoom(QSize(natural.width() / 2, natural.height()));
    QVERIFY(qAbs(fit - 0.5) < 0.01);
    chart.zoomToFit(QSize(natural.width() * 2, natural.height() * 3));
    QVERIFY(qAbs(chart.zoom() - 2.0) < 0.01);
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    Test_flowchart test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_flowchart.moc"
