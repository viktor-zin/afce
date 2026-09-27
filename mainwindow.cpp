/****************************************************************************
**                                                                         **
** Copyright (C) 2009-2014 Victor Zinkevich. All rights reserved.          **
** Contact: vicking@yandex.ru                                              **
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

#include "mainwindow.h"

#include "afceutil.h"
#include "appicons.h"
#include "blockcatalog.h"
#include "blockeditdialog.h"
#include "codeeditor.h"
#include "codeimporter.h"
#include "flowchartexport.h"
#include "importdialog.h"
#include "qflowchartstyle.h"
#include "sourcecodegenerator.h"
#include "zvflowchart.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLibraryInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QNativeGestureEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QStringDecoder>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>

#include <cmath>

namespace {

const int kZoomLevels[] = {25, 33, 50, 67, 75, 90, 100, 110, 125, 150, 175, 200, 250, 300, 400, 500};
const int kMinZoom = 25;
const int kMaxZoom = 500;
const int kSliderMax = 1000;   // the zoom slider is logarithmic: 0 = 25 %, kSliderMax = 500 %
const int kMaxRecentFiles = 10;
const int kStateVersion = 2;   // version of the saved dock / toolbar layout
const int kCanvasMargin = 24;  // space around the flowchart in the editor

int sliderFromPercent(int percent)
{
    return qRound(kSliderMax * std::log(double(percent) / kMinZoom) / std::log(double(kMaxZoom) / kMinZoom));
}

int percentFromSlider(int value)
{
    const int percent = qRound(kMinZoom * std::pow(double(kMaxZoom) / kMinZoom, double(value) / kSliderMax));
    return qAbs(percent - 100) <= 3 ? 100 : percent; // snap to 100 %
}

int nextZoomLevel(int percent, int steps)
{
    int result = percent;
    for (; steps > 0; --steps) {
        for (int level : kZoomLevels) {
            if (level > result) {
                result = level;
                break;
            }
        }
    }
    for (; steps < 0; ++steps) {
        int previous = kMinZoom;
        for (int level : kZoomLevels) {
            if (level >= result)
                break;
            previous = level;
        }
        result = previous;
    }
    return qBound(kMinZoom, result, kMaxZoom);
}

// Source files are usually UTF-8; older Russian/Ukrainian sources are often
// in the Windows code page: fall back to the system codec, then windows-1251.
QString decodeSourceText(const QByteArray &data)
{
    QStringDecoder utf8(QStringDecoder::Utf8);
    const QString text = utf8.decode(data);
    if (!utf8.hasError())
        return text;
    QStringDecoder system(QStringDecoder::System);
    const QString local = system.decode(data);
    if (!system.hasError() && system.isValid() && local != text)
        return local;
    QStringDecoder cp1251("windows-1251");
    if (cp1251.isValid()) {
        const QString cyrillic = cp1251.decode(data);
        if (!cp1251.hasError())
            return cyrillic;
    }
    return text;
}

// File suffix of the code of a generator (for "Save as...").
QString generatorSuffix(const QString &id)
{
    static const QMap<QString, QString> suffixes = {
        {QStringLiteral("c"), QStringLiteral("c")},        {QStringLiteral("cpp"), QStringLiteral("cpp")},
        {QStringLiteral("py"), QStringLiteral("py")},      {QStringLiteral("pas"), QStringLiteral("pas")},
        {QStringLiteral("js"), QStringLiteral("js")},      {QStringLiteral("php"), QStringLiteral("php")},
        {QStringLiteral("perl"), QStringLiteral("pl")},    {QStringLiteral("ruby"), QStringLiteral("rb")},
        {QStringLiteral("vbs"), QStringLiteral("vbs")},    {QStringLiteral("autoit"), QStringLiteral("au3")},
        {QStringLiteral("bas256"), QStringLiteral("kbs")}, {QStringLiteral("freebasic"), QStringLiteral("bas")},
    };
    return suffixes.value(id, QStringLiteral("txt"));
}

// "Save (Ctrl+S)" tooltips.
void setActionTexts(QAction *action, const QString &text, const QString &statusTip)
{
    action->setText(text);
    action->setStatusTip(statusTip);
    QString plain = text;
    plain.remove(QRegularExpression(QStringLiteral("&(?!&)")));
    if (plain.endsWith(QLatin1String("...")))
        plain.chop(3);
    else if (plain.endsWith(QChar(0x2026)))
        plain.chop(1);
    const QKeySequence shortcut = action->shortcut();
    action->setToolTip(shortcut.isEmpty() ? plain
                                          : QStringLiteral("%1 (%2)").arg(plain, shortcut.toString(QKeySequence::NativeText)));
}

QList<QKeySequence> shortcutList(std::initializer_list<QKeySequence> keys)
{
    QList<QKeySequence> result;
    for (const QKeySequence &key : keys) {
        if (!key.isEmpty() && !result.contains(key))
            result << key;
    }
    return result;
}

QList<QKeySequence> shortcutList(QKeySequence::StandardKey standard, std::initializer_list<QKeySequence> extra = {})
{
    QList<QKeySequence> result;
    const QList<QKeySequence> keys = QKeySequence::keyBindings(standard);
    for (const QKeySequence &key : keys) {
        if (!key.isEmpty() && !result.contains(key))
            result << key;
    }
    for (const QKeySequence &key : extra) {
        if (!key.isEmpty() && !result.contains(key))
            result << key;
    }
    return result;
}

// A tool panel button: a click selects the tool (insertion by clicking a point),
// pressing and dragging drags the block onto the flowchart.
class PaletteButton : public QToolButton
{
public:
    using QToolButton::QToolButton;

    QString fragment; // insertion buffer; empty for "Select"

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            m_pressPos = event->position().toPoint();
        QToolButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!fragment.isEmpty() && (event->buttons() & Qt::LeftButton)
            && (event->position().toPoint() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            startBlockDrag();
            return;
        }
        QToolButton::mouseMoveEvent(event);
    }

private:
    QPoint m_pressPos;

    // A "chip" with the block icon and its name follows the mouse.
    QPixmap dragPixmap() const
    {
        const qreal dpr = devicePixelRatioF();
        QFont f = font();
        f.setBold(true);
        const QFontMetrics fm(f);
        const QSize iconSize(40, 26);
        const int pad = 8;
        const QSize size(pad + iconSize.width() + 6 + fm.horizontalAdvance(text()) + pad,
                         qMax(iconSize.height(), fm.height()) + 2 * 6);
        QPixmap pm(size * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QColor bg = palette().color(QPalette::Base);
        bg.setAlpha(235);
        QColor border = palette().color(QPalette::Highlight);
        p.setPen(QPen(border, 1.5));
        p.setBrush(bg);
        p.drawRoundedRect(QRectF(0.75, 0.75, size.width() - 1.5, size.height() - 1.5), 7, 7);
        icon().paint(&p, QRect(QPoint(pad, (size.height() - iconSize.height()) / 2), iconSize));
        p.setFont(f);
        p.setPen(palette().color(QPalette::Text));
        p.drawText(QRect(pad + iconSize.width() + 6, 0, size.width(), size.height()),
                   Qt::AlignVCenter | Qt::AlignLeft, text());
        return pm;
    }

    void startBlockDrag()
    {
        setDown(false); // no click when the button is released
        QMimeData *mime = new QMimeData;
        mime->setData(QFlowChart::fragmentMimeType(), fragment.toUtf8());
        QDrag *drag = new QDrag(this);
        drag->setMimeData(mime);
        const QPixmap pm = dragPixmap();
        drag->setPixmap(pm);
        drag->setHotSpot(QPoint(20, int(pm.deviceIndependentSize().height() / 2)));
        drag->exec(Qt::CopyAction, Qt::CopyAction);
    }
};

} // namespace

QString afceVersion()
{
    return afce::programVersion();
}


// ===========================================================================
// AfcScrollArea

void AfcScrollArea::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    emit mouseDown();
}

void AfcScrollArea::wheelEvent(QWheelEvent *event)
{
    if((event->modifiers() & Qt::ControlModifier) != 0) {
        // Ctrl+wheel zooms; one notch (120) = one zoom step
        fZoomDelta += event->angleDelta().y();
        const int steps = fZoomDelta / 120;
        fZoomDelta -= steps * 120;
        if (steps != 0) {
            emit zoomStepped(steps);
            emit zoomSteppedAt(steps, event->position().toPoint());
        }
        event->accept();
    }
    else if ((event->modifiers() & Qt::ShiftModifier) != 0 && event->angleDelta().x() == 0
             && event->angleDelta().y() != 0) {
        // Shift+wheel scrolls horizontally (macOS already delivers such events as horizontal)
        fZoomDelta = 0;
        QWheelEvent horizontal(event->position(), event->globalPosition(),
                               QPoint(event->pixelDelta().y(), 0), QPoint(event->angleDelta().y(), 0),
                               event->buttons(), event->modifiers() & ~Qt::ShiftModifier,
                               event->phase(), event->inverted(), event->source());
        QScrollArea::wheelEvent(&horizontal);
        event->setAccepted(horizontal.isAccepted());
    }
    else {
        // plain wheel / trackpad: the standard scrolling of QScrollArea
        fZoomDelta = 0;
        QScrollArea::wheelEvent(event);
    }
}


// ===========================================================================
// DocumentTab

DocumentTab::DocumentTab(QWidget *parent)
    : AfcScrollArea(parent), fCanvas(new QWidget), fChart(new QFlowChart(fCanvas))
{
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    viewport()->setAutoFillBackground(true);
    fCanvas->setObjectName(QStringLiteral("canvas"));
    fChart->move(kCanvasMargin, kCanvasMargin);
    setWidget(fCanvas);
    syncCanvas();
    fChart->setStatus(QFlowChart::Selectable);
    fChart->installEventFilter(this);
    fCanvas->installEventFilter(this);
    connect(fChart, &QFlowChart::changed, this, &DocumentTab::syncCanvas);
    connect(fChart, &QFlowChart::zoomChanged, this, &DocumentTab::syncCanvas);
    connect(this, &AfcScrollArea::mouseDown, fChart, [this]() {
        if (fChart->status() == QFlowChart::Selectable)
            fChart->deselectAll();
    });
}

void DocumentTab::syncCanvas()
{
    const QSize size = fChart->size() + QSize(2 * kCanvasMargin, 2 * kCanvasMargin);
    if (fCanvas->size() != size)
        fCanvas->resize(size);
}

void DocumentTab::setFileName(const QString &fileName)
{
    fFileName = fileName;
}

void DocumentTab::setTitle(const QString &title)
{
    fTitle = title;
}

QString DocumentTab::displayName() const
{
    return fFileName.isEmpty() ? fTitle : QFileInfo(fFileName).fileName();
}

void DocumentTab::setModified(bool modified)
{
    if (fModified == modified)
        return;
    fModified = modified;
    emit modificationChanged(modified);
}

bool DocumentTab::isPristine() const
{
    if (!fFileName.isEmpty() || fModified)
        return false;
    const QBlock *root = fChart->root();
    return !root || root->items.isEmpty() || root->item(0)->items.isEmpty();
}

double DocumentTab::zoom() const
{
    return fChart->zoom();
}

void DocumentTab::setZoom(double zoom, const QPoint &anchor)
{
    zoom = qBound(kMinZoom / 100.0, zoom, kMaxZoom / 100.0);
    const double old = fChart->zoom();
    if (qAbs(old - zoom) < 1e-6)
        return;
    const QPoint a = anchor.x() < 0 || anchor.y() < 0 ? viewport()->rect().center() : anchor;
    const QPointF base = QPointF(fChart->mapFrom(viewport(), a)) / old;
    fChart->setZoom(zoom);
    syncCanvas();
    // the scroll area has moved the resized chart: keep the anchor point in place
    const QPoint moved = fChart->mapTo(viewport(), (base * zoom).toPoint());
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + moved.x() - a.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + moved.y() - a.y());
}

double DocumentTab::fitZoom() const
{
    const double zoom = fChart->zoom();
    const QBlock *root = fChart->root();
    if (!root || zoom <= 0)
        return 1;
    const double width = root->width / zoom;
    const double height = root->height / zoom;
    const QSize available = maximumViewportSize() - QSize(2 * kCanvasMargin + 8, 2 * kCanvasMargin + 8);
    if (width <= 0 || height <= 0 || available.isEmpty())
        return 1;
    return qBound(kMinZoom / 100.0, qMin(available.width() / width, available.height() / height), 2.0);
}

bool DocumentTab::handleGesture(QEvent *event, QWidget *receiver)
{
    if (event->type() != QEvent::NativeGesture)
        return false;
    auto *gesture = static_cast<QNativeGestureEvent *>(event);
    const QPoint position = receiver->mapTo(viewport(), gesture->position().toPoint());
    switch (gesture->gestureType()) {
    case Qt::ZoomNativeGesture:
        emit pinchZoomed(1.0 + gesture->value(), position);
        return true;
    case Qt::SmartZoomNativeGesture:
        emit smartZoomRequested();
        return true;
    default:
        return false;
    }
}

bool DocumentTab::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == fChart && event->type() == QEvent::Resize)
        syncCanvas();
    if (watched == fCanvas) {
        // the margin around the chart behaves like the empty viewport
        if (handleGesture(event, fCanvas)) {
            event->accept();
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            mousePressEvent(static_cast<QMouseEvent *>(event));
            return true;
        }
    }
    if (watched == fChart) {
        if (handleGesture(event, fChart)) {
            event->accept();
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress
            && static_cast<QMouseEvent *>(event)->button() == Qt::RightButton
            && fChart->status() == QFlowChart::Insertion) {
            // right-click cancels the insertion mode
            fChart->setStatus(QFlowChart::Selectable);
            return true;
        }
    }
    return AfcScrollArea::eventFilter(watched, event);
}

bool DocumentTab::viewportEvent(QEvent *event)
{
    if (handleGesture(event, viewport())) {
        event->accept();
        return true;
    }
    return AfcScrollArea::viewportEvent(event);
}

void DocumentTab::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton && fChart->status() == QFlowChart::Insertion) {
        fChart->setStatus(QFlowChart::Selectable);
        event->accept();
        return;
    }
    AfcScrollArea::mousePressEvent(event);
}


// ===========================================================================
// MainWindow

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
{
    QApplication::setWindowIcon(afce::applicationIcon());
    fAssignSymbol = QStringLiteral(":=");
    setAcceptDrops(true);
    setUnifiedTitleAndToolBarOnMac(true);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);

    tabs = new QTabWidget;
    tabs->setObjectName(QStringLiteral("document_tabs"));
    tabs->setDocumentMode(true);
    tabs->setTabsClosable(false); // own close buttons (installCloseButton): tinted icon, all styles
    tabs->setMovable(true);
    tabs->setUsesScrollButtons(true);
    tabs->setElideMode(Qt::ElideMiddle);
    setCentralWidget(tabs);

    codeTimer = new QTimer(this);
    codeTimer->setSingleShot(true);
    codeTimer->setInterval(150);
    connect(codeTimer, &QTimer::timeout, this, &MainWindow::generateCode);

    createActions();
    createMenus();
    createToolBar();
    createToolPanel();
    createCodePanel();
    resizeDocks({dockTools, dockCode}, {200, 340}, Qt::Horizontal);

    helpWindow = new THelpWindow(this);
    helpWindow->setObjectName(QStringLiteral("help_window"));
    helpWindow->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::RightDockWidgetArea, helpWindow);
    helpWindow->hide();
    connect(actHelp, &QAction::triggered, helpWindow, &QDockWidget::setVisible);
    connect(helpWindow, &QDockWidget::visibilityChanged, actHelp, &QAction::setChecked);

    createStatusBar();

    connect(tabs, &QTabWidget::currentChanged, this, &MainWindow::currentTabChanged);
    connect(tabs, &QTabWidget::tabCloseRequested, this, [this](int index) { closeTab(index); });
    // middle click closes a tab
    tabs->tabBar()->installEventFilter(this);

    retranslateUi();
    newDocument();
    updateRecentMenu();
    updateToolPanelStyle();
    updateActions();
    generateCode();
}

MainWindow::~MainWindow()
{
    // Children are destroyed after this destructor has run; make sure none of
    // them can call back into the (then half-destroyed) main window.
    codeTimer->stop();
    const QList<QObject *> children = findChildren<QObject *>();
    for (QObject *child : children)
        child->disconnect(this);
}

// ---------------------------------------------------------------------------
// construction

void MainWindow::createActions()
{
    auto make = [this](const char *objectName, const QString &icon, const QList<QKeySequence> &keys = {}) {
        QAction *action = new QAction(this);
        action->setObjectName(QLatin1String(objectName));
        if (!icon.isEmpty())
            action->setIcon(afce::uiIcon(icon));
        if (!keys.isEmpty())
            action->setShortcuts(keys);
        return action;
    };

    actNew = make("action_new", QStringLiteral("new"), shortcutList(QKeySequence::New));
    actOpen = make("action_open", QStringLiteral("open"), shortcutList(QKeySequence::Open));
    actImport = make("action_import", QStringLiteral("import-code"), shortcutList({QKeySequence(Qt::CTRL | Qt::Key_I)}));
    actSave = make("action_save", QStringLiteral("save"), shortcutList(QKeySequence::Save));
    actSaveAs = make("action_save_as", QStringLiteral("save-as"), shortcutList(QKeySequence::SaveAs));
    actExportPng = make("action_export_png", QString());
    actExportSvg = make("action_export_svg", QString());
    actExportPdf = make("action_export_pdf", QString());
    actCopyImage = make("action_copy_image", QStringLiteral("copy-image"));
    actPrint = make("action_print", QStringLiteral("print"), shortcutList(QKeySequence::Print));
    actCloseTab = make("action_close_tab", QStringLiteral("close"),
                       shortcutList({QKeySequence(Qt::CTRL | Qt::Key_W), QKeySequence(QKeySequence::Close)}));
    actExit = make("action_exit", QString(), shortcutList(QKeySequence::Quit, {QKeySequence(Qt::CTRL | Qt::Key_Q)}));
    actUndo = make("action_undo", QStringLiteral("undo"), shortcutList(QKeySequence::Undo));
    actRedo = make("action_redo", QStringLiteral("redo"),
                   shortcutList(QKeySequence::Redo, {QKeySequence(Qt::CTRL | Qt::Key_Y)}));
    actCut = make("action_cut", QStringLiteral("cut"), shortcutList(QKeySequence::Cut));
    actCopy = make("action_copy", QStringLiteral("copy"), shortcutList(QKeySequence::Copy));
    actPaste = make("action_paste", QStringLiteral("paste"), shortcutList(QKeySequence::Paste));
#ifdef Q_OS_MACOS
    actDelete = make("action_delete", QStringLiteral("delete"),
                     shortcutList({QKeySequence(Qt::Key_Backspace), QKeySequence(QKeySequence::Delete)}));
#else
    actDelete = make("action_delete", QStringLiteral("delete"), shortcutList(QKeySequence::Delete));
#endif
    actSelectAll = make("action_select_all", QString(), shortcutList(QKeySequence::SelectAll));
    actTools = make("action_tool_panel", QStringLiteral("tools-panel"), shortcutList({QKeySequence(Qt::Key_F2)}));
    actCode = make("action_code_panel", QStringLiteral("code-panel"), shortcutList({QKeySequence(Qt::Key_F3)}));
    actZoomIn = make("action_zoom_in", QStringLiteral("zoom-in"),
                     shortcutList(QKeySequence::ZoomIn, {QKeySequence(Qt::CTRL | Qt::Key_Equal)}));
    actZoomOut = make("action_zoom_out", QStringLiteral("zoom-out"), shortcutList(QKeySequence::ZoomOut));
    actZoomReset = make("action_zoom_reset", QStringLiteral("zoom-reset"), shortcutList({QKeySequence(Qt::CTRL | Qt::Key_0)}));
    actZoomFit = make("action_zoom_fit", QStringLiteral("zoom-fit"), shortcutList({QKeySequence(Qt::CTRL | Qt::Key_9)}));
    actMonochrome = make("action_monochrome", QString());
    actHelp = make("action_help", QStringLiteral("help"),
                   shortcutList(QKeySequence::HelpContents, {QKeySequence(Qt::Key_F1)}));
    actAbout = make("action_about", QStringLiteral("about"));
    actAboutQt = make("action_about_qt", QString());
    actClearRecent = make("action_clear_recent", QString());
    actCancelInsertion = make("action_cancel_insertion", QString(), shortcutList({QKeySequence(Qt::Key_Escape)}));
    addAction(actCancelInsertion); // not in a menu: Esc works while the window is active

    actExit->setMenuRole(QAction::QuitRole);
    actAbout->setMenuRole(QAction::AboutRole);
    actAboutQt->setMenuRole(QAction::AboutQtRole);
    actTools->setCheckable(true);
    actCode->setCheckable(true);
    actHelp->setCheckable(true);
    actMonochrome->setCheckable(true);

    connect(actNew, &QAction::triggered, this, &MainWindow::slotFileNew);
    connect(actOpen, &QAction::triggered, this, &MainWindow::slotFileOpen);
    connect(actImport, &QAction::triggered, this, &MainWindow::slotFileImport);
    connect(actSave, &QAction::triggered, this, &MainWindow::slotFileSave);
    connect(actSaveAs, &QAction::triggered, this, &MainWindow::slotFileSaveAs);
    connect(actExportPng, &QAction::triggered, this, &MainWindow::slotFileExportPng);
    connect(actExportSvg, &QAction::triggered, this, &MainWindow::slotFileExportSvg);
    connect(actExportPdf, &QAction::triggered, this, &MainWindow::slotFileExportPdf);
    connect(actCopyImage, &QAction::triggered, this, &MainWindow::slotFileCopyImage);
    connect(actPrint, &QAction::triggered, this, &MainWindow::slotFilePrint);
    connect(actCloseTab, &QAction::triggered, this, &MainWindow::slotFileCloseTab);
    connect(actExit, &QAction::triggered, this, &MainWindow::close);
    connect(actUndo, &QAction::triggered, this, [this]() { if (document()) document()->undo(); });
    connect(actRedo, &QAction::triggered, this, [this]() { if (document()) document()->redo(); });
    connect(actCut, &QAction::triggered, this, &MainWindow::slotEditCut);
    connect(actCopy, &QAction::triggered, this, &MainWindow::slotEditCopy);
    connect(actPaste, &QAction::triggered, this, &MainWindow::slotEditPaste);
    connect(actDelete, &QAction::triggered, this, &MainWindow::slotEditDelete);
    connect(actSelectAll, &QAction::triggered, this, &MainWindow::slotEditSelectAll);
    connect(actZoomIn, &QAction::triggered, this, &MainWindow::zoomIn);
    connect(actZoomOut, &QAction::triggered, this, &MainWindow::zoomOut);
    connect(actZoomReset, &QAction::triggered, this, &MainWindow::zoomReset);
    connect(actZoomFit, &QAction::triggered, this, &MainWindow::zoomToFit);
    connect(actMonochrome, &QAction::toggled, this, &MainWindow::setMonochrome);
    connect(actAbout, &QAction::triggered, this, &MainWindow::slotHelpAbout);
    connect(actAboutQt, &QAction::triggered, this, &MainWindow::slotHelpAboutQt);
    connect(actClearRecent, &QAction::triggered, this, &MainWindow::clearRecentFiles);
    connect(actCancelInsertion, &QAction::triggered, this, &MainWindow::slotCancelInsertion);

    assignGroup = new QActionGroup(this);
    assignGroup->setExclusive(true);
    const QStringList symbols = {QStringLiteral(":="), QStringLiteral("="), QString(QChar(0x2190))};
    for (const QString &symbol : symbols) {
        QAction *act = new QAction(this);
        act->setCheckable(true);
        act->setData(symbol);
        act->setChecked(symbol == fAssignSymbol);
        assignGroup->addAction(act);
        actAssign << act;
        connect(act, &QAction::triggered, this, [this, symbol]() { setAssignSymbol(symbol); });
    }

    languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);
    const QMap<QString, QString> avlLangs = enumLanguages();
    for (auto it = avlLangs.cbegin(); it != avlLangs.cend(); ++it) {
        QAction *act = new QAction(this);
        act->setText(it.value());
        act->setData(it.key());
        act->setCheckable(true);
        languageGroup->addAction(act);
        actLanguages.append(act);
        connect(act, &QAction::triggered, this, &MainWindow::slotChangeLanguage);
    }
}

void MainWindow::createMenus()
{
    menuFile = menuBar()->addMenu(QString());
    menuFile->addAction(actNew);
    menuFile->addAction(actOpen);
    menuRecent = menuFile->addMenu(QString());
    menuRecent->setObjectName(QStringLiteral("menu_recent"));
    menuRecent->setIcon(afce::uiIcon(QStringLiteral("recent")));
    menuRecent->setToolTipsVisible(true);
    menuFile->addAction(actImport);
    menuFile->addSeparator();
    menuFile->addAction(actSave);
    menuFile->addAction(actSaveAs);
    menuFile->addSeparator();
    menuExport = menuFile->addMenu(QString());
    menuExport->setObjectName(QStringLiteral("menu_export"));
    menuExport->setIcon(afce::uiIcon(QStringLiteral("export")));
    menuExport->addAction(actExportPng);
    menuExport->addAction(actExportSvg);
    menuExport->addAction(actExportPdf);
    menuExport->addSeparator();
    menuExport->addAction(actCopyImage);
    menuFile->addAction(actPrint);
    menuFile->addSeparator();
    menuFile->addAction(actCloseTab);
    menuFile->addSeparator();
    menuFile->addAction(actExit);

    menuEdit = menuBar()->addMenu(QString());
    menuEdit->addAction(actUndo);
    menuEdit->addAction(actRedo);
    menuEdit->addSeparator();
    menuEdit->addAction(actCut);
    menuEdit->addAction(actCopy);
    menuEdit->addAction(actPaste);
    menuEdit->addAction(actDelete);
    menuEdit->addSeparator();
    menuEdit->addAction(actSelectAll);

    menuView = menuBar()->addMenu(QString());
    menuView->addAction(actTools);
    menuView->addAction(actCode);
    menuView->addSeparator();
    menuView->addAction(actZoomIn);
    menuView->addAction(actZoomOut);
    menuView->addAction(actZoomReset);
    menuView->addAction(actZoomFit);
    menuView->addSeparator();
    menuView->addAction(actMonochrome);
    menuAssign = menuView->addMenu(QString());
    for (QAction *act : std::as_const(actAssign))
        menuAssign->addAction(act);
    menuView->addSeparator();
    menuLanguage = menuView->addMenu(QString());
    for (QAction *act : std::as_const(actLanguages))
        menuLanguage->addAction(act);

    menuHelp = menuBar()->addMenu(QString());
    menuHelp->addAction(actHelp);
    menuHelp->addSeparator();
    menuHelp->addAction(actAbout);
    menuHelp->addAction(actAboutQt);
}

void MainWindow::createToolBar()
{
    toolBar = addToolBar(QString());
    toolBar->setObjectName(QStringLiteral("standard_toolbar"));
    toolBar->setIconSize(QSize(20, 20));
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolBar->setMovable(false);
    toolBar->setFloatable(false);

    toolBar->addAction(actNew);
    toolBar->addAction(actOpen);
    toolBar->addAction(actSave);
    toolBar->addAction(actImport);
    if (QToolButton *importButton = qobject_cast<QToolButton *>(toolBar->widgetForAction(actImport))) {
        importButton->setObjectName(QStringLiteral("toolbar_import"));
        importButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
    toolBar->addSeparator();
    toolBar->addAction(actUndo);
    toolBar->addAction(actRedo);
    toolBar->addSeparator();
    toolBar->addAction(actCut);
    toolBar->addAction(actCopy);
    toolBar->addAction(actPaste);
    toolBar->addAction(actDelete);
    toolBar->addSeparator();
    toolBar->addAction(actZoomOut);
    toolBar->addAction(actZoomIn);
    toolBar->addAction(actZoomFit);
    toolBar->addSeparator();
    toolBar->addAction(menuExport->menuAction());
    if (QToolButton *exportButton = qobject_cast<QToolButton *>(toolBar->widgetForAction(menuExport->menuAction())))
        exportButton->setPopupMode(QToolButton::InstantPopup);
    toolBar->addAction(actPrint);

    // panels and help at the right end
    QWidget *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);
    toolBar->addAction(actTools);
    toolBar->addAction(actCode);
    toolBar->addAction(actHelp);
}

void MainWindow::createToolPanel()
{
    dockTools = new QDockWidget(this);
    dockTools->setObjectName(QStringLiteral("dock_tools"));
    dockTools->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dockTools->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::LeftDockWidgetArea, dockTools);

    QScrollArea *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("tool_panel_scroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    toolsWidget = new QWidget;
    toolsWidget->setObjectName(QStringLiteral("tool_panel"));
    toolsLayout = new QVBoxLayout(toolsWidget);
    toolsLayout->setContentsMargins(8, 6, 8, 8);
    toolsLayout->setSpacing(1);
    scroll->setWidget(toolsWidget);
    dockTools->setWidget(scroll);

    toolGroup = new QButtonGroup(this);
    toolGroup->setExclusive(true);
    connect(toolGroup, &QButtonGroup::idClicked, this, &MainWindow::toolClicked);

    connect(actTools, &QAction::triggered, dockTools, &QDockWidget::setVisible);
    connect(dockTools, &QDockWidget::visibilityChanged, actTools, &QAction::setChecked);
    actTools->setChecked(true);
}

void MainWindow::rebuildToolPanel()
{
    const QList<QAbstractButton *> old = toolGroup->buttons();
    for (QAbstractButton *button : old)
        toolGroup->removeButton(button);
    while (QLayoutItem *item = toolsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    toolXml.clear();
    toolTitles.clear();

    auto addButton = [this](const QIcon &icon, const QString &text, const QString &tip) {
        PaletteButton *button = new PaletteButton;
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(34, 22));
        button->setIcon(icon);
        button->setText(text);
        button->setToolTip(tip);
        button->setStatusTip(tip);
        button->setAutoRaise(true);
        button->setCheckable(true);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setMinimumHeight(30);
        toolGroup->addButton(button, toolXml.size());
        toolsLayout->addWidget(button);
        return button;
    };

    QToolButton *select = addButton(afce::uiIcon(QStringLiteral("select")), tr("Select"),
                                    tr("Select blocks; double-click a block to edit it"));
    select->setObjectName(QStringLiteral("tool_select"));
    toolXml << QString();
    toolTitles << tr("Select");

    QString group;
    const QList<afce::BlockKind> kinds = afce::blockKinds();
    for (const afce::BlockKind &kind : kinds) {
        if (kind.group != group) {
            group = kind.group;
            QLabel *header = new QLabel(afce::blockGroupTitle(group));
            header->setObjectName(QStringLiteral("tool_group_header"));
            toolsLayout->addWidget(header);
        }
        PaletteButton *button = addButton(afce::blockIcon(kind.type, palette()), kind.title,
                                          (kind.description.isEmpty() ? kind.title : kind.description) + QLatin1Char('\n')
                                              + tr("Drag onto the flowchart, or click and then click a point."));
        button->fragment = kind.xmlTemplate;
        button->setObjectName(QStringLiteral("tool_") + kind.type);
        button->setProperty("blockType", kind.type);
        toolXml << kind.xmlTemplate;
        toolTitles << kind.title;
    }
    toolsLayout->addStretch(1);
    // wide enough for the longest title: never a horizontal scroll bar
    if (QScrollArea *scroll = qobject_cast<QScrollArea *>(dockTools->widget())) {
        toolsWidget->adjustSize();
        scroll->setMinimumWidth(toolsLayout->sizeHint().width()
                                + scroll->verticalScrollBar()->sizeHint().width());
    }
    chartStatusChanged();
}

void MainWindow::updateToolPanelStyle()
{
    const QPalette pal = palette();
    QColor hover = pal.color(QPalette::WindowText);
    hover.setAlphaF(0.08f);
    QColor checked = pal.color(QPalette::Highlight);
    checked.setAlphaF(0.22f);
    QColor checkedBorder = pal.color(QPalette::Highlight);
    checkedBorder.setAlphaF(0.55f);
    QColor header = pal.color(QPalette::WindowText);
    header.setAlphaF(0.55f);
    auto css = [](const QColor &c) {
        return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alphaF(), 0, 'f', 2);
    };
    toolsWidget->setStyleSheet(QStringLiteral(
        "QToolButton { border: 1px solid transparent; border-radius: 6px; padding: 3px 6px; }"
        "QToolButton:hover { background: %1; }"
        "QToolButton:checked { background: %2; border-color: %3; }"
        "QLabel#tool_group_header { color: %4; font-weight: 600; padding: 10px 4px 3px 4px; }")
        .arg(css(hover), css(checked), css(checkedBorder), css(header)));
}

void MainWindow::createCodePanel()
{
    dockCode = new QDockWidget(this);
    dockCode->setObjectName(QStringLiteral("dock_code"));
    dockCode->setAllowedAreas(Qt::AllDockWidgetAreas);
    dockCode->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
                          | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, dockCode);

    QWidget *panel = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 4, 6, 6);
    layout->setSpacing(4);

    codeLanguage = new QComboBox;
    codeLanguage->setObjectName(QStringLiteral("code_language"));
    codeLanguage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    codeLanguage->setMinimumContentsLength(10);
    btnCodeCopy = new QToolButton;
    btnCodeCopy->setObjectName(QStringLiteral("code_copy"));
    btnCodeCopy->setIcon(afce::uiIcon(QStringLiteral("copy")));
    btnCodeCopy->setAutoRaise(true);
    btnCodeCopy->setIconSize(QSize(18, 18));
    btnCodeSave = new QToolButton;
    btnCodeSave->setObjectName(QStringLiteral("code_save"));
    btnCodeSave->setIcon(afce::uiIcon(QStringLiteral("save-as")));
    btnCodeSave->setAutoRaise(true);
    btnCodeSave->setIconSize(QSize(18, 18));
    QHBoxLayout *header = new QHBoxLayout;
    header->setSpacing(2);
    header->addWidget(codeLanguage, 1);
    header->addWidget(btnCodeCopy);
    header->addWidget(btnCodeSave);
    layout->addLayout(header);

    codeText = new CodeEditor;
    codeText->setObjectName(QStringLiteral("code_text"));
    codeText->setReadOnly(true);
    codeText->setLineWrapMode(QPlainTextEdit::NoWrap);
    codeText->setLineNumbersVisible(true);
    codeText->setCurrentLineHighlight(false); // a read-only viewer: no band on line 1
    codeText->setMinimumWidth(240);
    codeHighlighter = new CodeHighlighter(codeText->document());
    layout->addWidget(codeText, 1);

    // C / C++: the code can be edited and turned back into the flowchart
    codeApplyRow = new QWidget;
    QHBoxLayout *applyLayout = new QHBoxLayout(codeApplyRow);
    applyLayout->setContentsMargins(0, 0, 0, 0);
    applyLayout->setSpacing(4);
    codeHint = new QLabel;
    codeHint->setObjectName(QStringLiteral("code_hint"));
    codeHint->setWordWrap(true);
    codeHint->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnCodeRevert = new QToolButton;
    btnCodeRevert->setObjectName(QStringLiteral("code_revert"));
    btnCodeRevert->setIcon(afce::uiIcon(QStringLiteral("undo")));
    btnCodeRevert->setAutoRaise(true);
    btnCodeRevert->setIconSize(QSize(18, 18));
    btnCodeApply = new QToolButton;
    btnCodeApply->setObjectName(QStringLiteral("code_apply"));
    btnCodeApply->setIcon(afce::uiIcon(QStringLiteral("import-code")));
    btnCodeApply->setIconSize(QSize(18, 18));
    btnCodeApply->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    applyLayout->addWidget(codeHint, 1);
    applyLayout->addWidget(btnCodeRevert);
    applyLayout->addWidget(btnCodeApply);
    layout->addWidget(codeApplyRow);
    dockCode->setWidget(panel);

    for (const QKeySequence &key : {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}) {
        QShortcut *apply = new QShortcut(key, codeText);
        apply->setContext(Qt::WidgetShortcut);
        connect(apply, &QShortcut::activated, this, &MainWindow::codeToFlowchart);
    }
    connect(codeText, &QPlainTextEdit::textChanged, this, &MainWindow::codeTextChanged);
    connect(btnCodeApply, &QToolButton::clicked, this, &MainWindow::codeToFlowchart);
    connect(btnCodeRevert, &QToolButton::clicked, this, &MainWindow::codeRevert);

    connect(codeLanguage, qOverload<int>(&QComboBox::activated), this, &MainWindow::codeLangChanged);
    connect(btnCodeCopy, &QToolButton::clicked, this, &MainWindow::codeCopy);
    connect(btnCodeSave, &QToolButton::clicked, this, &MainWindow::codeSaveAs);
    connect(actCode, &QAction::triggered, dockCode, &QDockWidget::setVisible);
    connect(dockCode, &QDockWidget::visibilityChanged, actCode, &QAction::setChecked);
    connect(dockCode, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (visible)
            generateCode();
    });
    actCode->setChecked(true);
}

void MainWindow::createStatusBar()
{
    statusLabel = new QLabel;
    statusLabel->setObjectName(QStringLiteral("status_hint"));
    statusBar()->addWidget(statusLabel, 1);
    statusBar()->setSizeGripEnabled(false);

    QWidget *zoom = new QWidget;
    zoom->setObjectName(QStringLiteral("zoom_panel"));
    QHBoxLayout *layout = new QHBoxLayout(zoom);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(2);

    btnZoomOut = new QToolButton;
    btnZoomOut->setDefaultAction(actZoomOut);
    btnZoomOut->setAutoRaise(true);
    btnZoomOut->setIconSize(QSize(16, 16));
    zoomSlider = new QSlider(Qt::Horizontal);
    zoomSlider->setObjectName(QStringLiteral("zoom_slider"));
    zoomSlider->setRange(0, kSliderMax);
    zoomSlider->setPageStep(kSliderMax / 10);
    zoomSlider->setFixedWidth(120);
    zoomSlider->setFocusPolicy(Qt::NoFocus);
    btnZoomIn = new QToolButton;
    btnZoomIn->setDefaultAction(actZoomIn);
    btnZoomIn->setAutoRaise(true);
    btnZoomIn->setIconSize(QSize(16, 16));

    // the zoom value; click: a menu of presets
    btnZoomValue = new QToolButton;
    btnZoomValue->setObjectName(QStringLiteral("zoom_value"));
    btnZoomValue->setAutoRaise(true);
    btnZoomValue->setPopupMode(QToolButton::InstantPopup);
    btnZoomValue->setToolButtonStyle(Qt::ToolButtonTextOnly);
    btnZoomValue->setMinimumWidth(btnZoomValue->fontMetrics().horizontalAdvance(QStringLiteral("500 %")) + 16);
    btnZoomValue->setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; width: 0px; }"));
    QMenu *presets = new QMenu(btnZoomValue);
    for (int percent : {50, 75, 100, 150, 200, 300, 400}) {
        QAction *act = presets->addAction(QStringLiteral("%1 %").arg(percent));
        connect(act, &QAction::triggered, this, [this, percent]() { setZoomPercent(percent); });
    }
    presets->addSeparator();
    presets->addAction(actZoomFit);
    btnZoomValue->setMenu(presets);

    layout->addWidget(btnZoomOut);
    layout->addWidget(zoomSlider);
    layout->addWidget(btnZoomIn);
    layout->addWidget(btnZoomValue);
    statusBar()->addPermanentWidget(zoom);

    connect(zoomSlider, &QSlider::valueChanged, this, [this](int value) { setZoomPercent(percentFromSlider(value)); });
}

// ---------------------------------------------------------------------------
// texts

void MainWindow::retranslateUi()
{
    setActionTexts(actNew, tr("&New"), tr("Create a new flowchart in a new tab"));
    setActionTexts(actOpen, tr("&Open..."), tr("Open a saved flowchart"));
    setActionTexts(actImport, tr("&Import from Source Code..."), tr("Build flowcharts from C or C++ source code"));
    actImport->setIconText(tr("Code \u2192 Flowchart"));
    setActionTexts(actSave, tr("&Save"), tr("Save changes"));
    setActionTexts(actSaveAs, tr("Save &As..."), tr("Save the flowchart in a new file"));
    setActionTexts(actExportPng, tr("&PNG Image..."), tr("Save the flowchart as a PNG picture"));
    setActionTexts(actExportSvg, tr("&SVG Image..."), tr("Save the flowchart in the SVG vector format"));
    setActionTexts(actExportPdf, tr("P&DF Document..."), tr("Save the flowchart as a PDF document"));
    setActionTexts(actCopyImage, tr("&Copy as Image"), tr("Copy the flowchart to the clipboard as a picture"));
    setActionTexts(actPrint, tr("&Print..."), tr("Print the flowchart"));
    setActionTexts(actCloseTab, tr("&Close Tab"), tr("Close the current flowchart"));
    setActionTexts(actExit, tr("E&xit"), tr("Exit from program"));
    setActionTexts(actUndo, tr("&Undo"), tr("Undo the last operation"));
    setActionTexts(actRedo, tr("&Redo"), tr("Restore the last undone action"));
    setActionTexts(actCut, tr("Cu&t"), tr("Cut the current selection"));
    setActionTexts(actCopy, tr("&Copy"), tr("Copy the current selection"));
    setActionTexts(actPaste, tr("&Paste"), tr("Paste blocks from the clipboard"));
    setActionTexts(actDelete, tr("&Delete"), tr("Delete the current selection"));
    setActionTexts(actSelectAll, tr("Select &All"), tr("Select the whole algorithm"));
    setActionTexts(actTools, tr("&Tool Panel"), tr("Toggle the tool panel"));
    setActionTexts(actCode, tr("&Source Code Panel"), tr("Toggle the source code panel"));
    setActionTexts(actZoomIn, tr("Zoom &In"), tr("Enlarge the flowchart"));
    setActionTexts(actZoomOut, tr("Zoom &Out"), tr("Reduce the flowchart"));
    setActionTexts(actZoomReset, tr("&Actual Size"), tr("Show the flowchart at 100 %"));
    setActionTexts(actZoomFit, tr("Zoom to &Fit"), tr("Show the whole flowchart"));
    setActionTexts(actMonochrome, tr("Classic &Black-and-White Blocks"),
                   tr("Draw the blocks black on white, without colours"));
    setActionTexts(actHelp, tr("&Help"), tr("Toggle Help window"));
    setActionTexts(actAbout, tr("&About AFCE"), tr("Information about authors"));
    setActionTexts(actAboutQt, tr("About &Qt"), tr("Information about Qt"));
    setActionTexts(actClearRecent, tr("&Clear List"), tr("Forget the recently opened files"));
    setActionTexts(actCancelInsertion, tr("Cancel Insertion"), tr("Return to the selection mode"));
    const QStringList assignNames = {tr("x := y (Pascal)"), tr("x = y (C, Python)"), tr("x ← y (arrow)")};
    for (int i = 0; i < actAssign.size(); ++i)
        actAssign.at(i)->setText(assignNames.value(i));

    menuFile->setTitle(tr("&File"));
    menuRecent->setTitle(tr("Open &Recent"));
    menuExport->setTitle(tr("&Export"));
    menuExport->menuAction()->setToolTip(tr("Export the flowchart as a picture"));
    menuExport->menuAction()->setStatusTip(tr("Export the flowchart as a picture or copy it to the clipboard"));
    menuEdit->setTitle(tr("&Edit"));
    menuView->setTitle(tr("&View"));
    menuAssign->setTitle(tr("A&ssignment Symbol"));
    menuLanguage->setTitle(tr("&Language"));
    menuHelp->setTitle(tr("&Help"));

    toolBar->setWindowTitle(tr("Standard"));
    dockTools->setWindowTitle(tr("Tools"));
    dockCode->setWindowTitle(tr("Source code"));
    codeLanguage->setToolTip(tr("Programming language of the generated code"));
    btnCodeCopy->setToolTip(tr("Copy the code to the clipboard"));
    btnCodeSave->setToolTip(tr("Save the code to a file..."));
    btnCodeApply->setText(tr("To Flowchart"));
    btnCodeApply->setToolTip(tr("Build the flowchart of the current tab from this code (Ctrl+Enter)"));
    btnCodeRevert->setToolTip(tr("Discard the changes in the code"));
    updateCodePanelState();
    zoomSlider->setToolTip(tr("Zoom"));
    btnZoomValue->setToolTip(tr("Zoom presets"));

    const QString currentLocale = QLocale().name();
    for (QAction *act : std::as_const(actLanguages))
        act->setChecked(act->data().toString() == currentLocale);

    slotReloadGenerators();
    rebuildToolPanel();
    updateRecentMenu();
    for (int i = 0; i < tabCount(); ++i)
        updateTabText(tab(i));
    updateZoomWidgets();
    updateTitle();
    showStatusHint();
    helpWindow->setWindowTitle(tr("Help window"));
    helpWindow->retranslateUi();
}

void MainWindow::updateTitle()
{
    DocumentTab *t = currentTab();
    if (!t) {
        setWindowTitle(tr("Algorithm Flowchart Editor"));
        return;
    }
    setWindowTitle(tr("%1[*] - Algorithm Flowchart Editor").arg(t->displayName()));
    setWindowModified(t->isModified());
    setWindowFilePath(t->fileName());
}

void MainWindow::updateTabText(DocumentTab *t)
{
    const int index = tabs->indexOf(t);
    if (index < 0)
        return;
    QString text = t->displayName();
    text.replace(QLatin1Char('&'), QLatin1String("&&"));
    tabs->setTabText(index, t->isModified() ? text + QLatin1Char('*') : text);
    tabs->setTabToolTip(index, t->fileName().isEmpty() ? t->displayName() : QDir::toNativeSeparators(t->fileName()));
}

void MainWindow::showStatusHint()
{
    QFlowChart *doc = document();
    if (!doc) {
        statusLabel->clear();
        return;
    }
    if (doc->status() == QFlowChart::Insertion) {
        const int id = toolGroup->checkedId();
        if (id > 0 && id < toolTitles.size())
            statusLabel->setText(tr("Click a highlighted point to insert “%1”. Esc or right-click cancels.")
                                     .arg(toolTitles.at(id)));
        else
            statusLabel->setText(tr("Click a highlighted point to paste the blocks. Esc or right-click cancels."));
    } else {
        statusLabel->setText(tr("Double-click a block to edit it. Ctrl+wheel zooms."));
    }
}

// ---------------------------------------------------------------------------
// settings

void MainWindow::saveSetting(const QString &key, const QVariant &value)
{
    if (!fPersistSettings)
        return;
    QSettings settings("afce", "application");
    settings.setValue(key, value);
}

void MainWindow::writeSettings()
{
    if (!fPersistSettings)
        return;
    QSettings settings("afce", "application");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState(kStateVersion));
    settings.setValue("codeLanguage", codeLanguage->currentData().toString());
    settings.setValue("recentFiles", fRecentFiles);
    settings.setValue("view/monochrome", fMonochrome);
    settings.setValue("view/assignSymbol", fAssignSymbol);
    settings.setValue("lastDir", fLastDir);
    settings.setValue("lastExportDir", fLastExportDir);
}

void MainWindow::readSettings()
{
    QSettings settings("afce", "application");

    const QVariant geometry = settings.value("geometry");
    if (geometry.typeId() == QMetaType::QByteArray)
        restoreGeometry(geometry.toByteArray());
    else if (geometry.typeId() == QMetaType::QRect)
        setGeometry(geometry.toRect()); // written by old versions
    else
        resize(1100, 720);
    restoreState(settings.value("windowState").toByteArray(), kStateVersion);

    const int lang = codeLanguage->findData(settings.value("codeLanguage").toString());
    if (lang >= 0)
        codeLanguage->setCurrentIndex(lang);

    fRecentFiles = settings.value("recentFiles").toStringList();
    while (fRecentFiles.size() > kMaxRecentFiles)
        fRecentFiles.removeLast();
    fLastDir = settings.value("lastDir").toString();
    fLastExportDir = settings.value("lastExportDir").toString();
    {
        const QSignalBlocker blocker(actMonochrome);
        fMonochrome = settings.value("view/monochrome", false).toBool();
        actMonochrome->setChecked(fMonochrome);
    }
    const QString symbol = settings.value("view/assignSymbol", QStringLiteral(":=")).toString();
    fAssignSymbol = QStringLiteral(":=");
    for (QAction *act : std::as_const(actAssign)) {
        if (act->data().toString() == symbol)
            fAssignSymbol = symbol;
    }
    for (QAction *act : std::as_const(actAssign))
        act->setChecked(act->data().toString() == fAssignSymbol);

    updateRecentMenu();
    applyChartStyle();
    generateCode();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // ask about every modified document, starting with the current one
    QList<DocumentTab *> order;
    if (currentTab())
        order << currentTab();
    for (int i = 0; i < tabCount(); ++i) {
        if (!order.contains(tab(i)))
            order << tab(i);
    }
    for (DocumentTab *t : std::as_const(order)) {
        if (!maybeSave(t)) {
            event->ignore();
            return;
        }
    }
    writeSettings();
    event->accept();
}

void MainWindow::changeEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    case QEvent::PaletteChange:
    case QEvent::ThemeChange:
        // e.g. the system switched between light and dark mode
        if (tabs) {
            applyChartStyle();
            updateIcons();
            rebuildToolPanel();
            updateToolPanelStyle();
        }
        break;
    default:
        break;
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::updateIcons()
{
    // The icons are tinted when painted; set them again so that native menus
    // (which convert them once) pick up the new colours as well.
    const QList<QAction *> actions = findChildren<QAction *>();
    for (QAction *action : actions) {
        if (!action->icon().isNull())
            action->setIcon(action->icon());
    }
    btnCodeCopy->setIcon(btnCodeCopy->icon());
    btnCodeSave->setIcon(btnCodeSave->icon());
}

// ---------------------------------------------------------------------------
// tabs

QFlowChart *MainWindow::document() const
{
    DocumentTab *t = currentTab();
    return t ? t->flowChart() : nullptr;
}

DocumentTab *MainWindow::currentTab() const
{
    return tabs ? qobject_cast<DocumentTab *>(tabs->currentWidget()) : nullptr;
}

DocumentTab *MainWindow::tab(int index) const
{
    return qobject_cast<DocumentTab *>(tabs->widget(index));
}

int MainWindow::tabCount() const
{
    return tabs->count();
}

DocumentTab *MainWindow::tabOf(QObject *chart) const
{
    for (int i = 0; i < tabs->count(); ++i) {
        DocumentTab *t = tab(i);
        if (t && t->flowChart() == chart)
            return t;
    }
    return nullptr;
}

int MainWindow::findTab(const QString &fileName) const
{
    const QString wanted = QFileInfo(fileName).canonicalFilePath();
    if (wanted.isEmpty())
        return -1;
    for (int i = 0; i < tabs->count(); ++i) {
        DocumentTab *t = tab(i);
        if (t && !t->fileName().isEmpty() && QFileInfo(t->fileName()).canonicalFilePath() == wanted)
            return i;
    }
    return -1;
}

void MainWindow::connectTab(DocumentTab *t)
{
    QFlowChart *chart = t->flowChart();
    connect(chart, &QFlowChart::statusChanged, this, &MainWindow::chartStatusChanged);
    connect(chart, &QFlowChart::changed, this, &MainWindow::chartChanged);
    connect(chart, &QFlowChart::modified, this, &MainWindow::chartModified);
    connect(chart, &QFlowChart::editBlock, this, &MainWindow::chartEditBlock);
    connect(t, &DocumentTab::modificationChanged, this, [this, t]() {
        updateTabText(t);
        if (t == currentTab())
            updateTitle();
    });
    connect(t, &AfcScrollArea::zoomSteppedAt, this, [this, t](int steps, const QPoint &position) {
        const int percent = nextZoomLevel(qRound(t->zoom() * 100), steps);
        t->setZoom(percent / 100.0, position);
        updateZoomWidgets();
    });
    connect(t, &DocumentTab::pinchZoomed, this, [this, t](double factor, const QPoint &position) {
        t->setZoom(t->zoom() * factor, position);
        updateZoomWidgets();
    });
    connect(t, &DocumentTab::smartZoomRequested, this, [this, t]() {
        if (qAbs(t->zoom() - 1.0) < 0.01)
            zoomToFit();
        else
            zoomReset();
    });
}

DocumentTab *MainWindow::addTab(DocumentTab *t, int index)
{
    connectTab(t);
    applyChartStyle(t);
    // an untouched empty document is replaced by the opened one
    DocumentTab *pristine = currentTab() && currentTab()->isPristine() ? currentTab() : nullptr;
    if (index < 0)
        index = pristine ? tabs->indexOf(pristine) + 1 : tabs->count();
    index = tabs->insertTab(index, t, QString());
    installCloseButton(t);
    updateTabText(t);
    tabs->setCurrentIndex(index);
    if (pristine) {
        pristine->flowChart()->disconnect(this);
        pristine->disconnect(this);
        tabs->removeTab(tabs->indexOf(pristine));
        pristine->deleteLater();
    }
    return t;
}

void MainWindow::installCloseButton(DocumentTab *t)
{
    QTabBar *bar = tabs->tabBar();
    QToolButton *close = new QToolButton;
    close->setObjectName(QStringLiteral("tab_close"));
    close->setIcon(afce::uiIcon(QStringLiteral("close")));
    close->setIconSize(QSize(12, 12));
    close->setFixedSize(18, 18);
    close->setAutoRaise(true);
    close->setFocusPolicy(Qt::NoFocus);
    close->setCursor(Qt::ArrowCursor);
    close->setToolTip(tr("Close Tab"));
    connect(close, &QToolButton::clicked, this, [this, t]() { closeTab(tabs->indexOf(t)); });
    const auto side = static_cast<QTabBar::ButtonPosition>(
        bar->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, bar));
    bar->setTabButton(tabs->indexOf(t), side, close);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == tabs->tabBar() && event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::MiddleButton) {
            const int index = tabs->tabBar()->tabAt(mouse->position().toPoint());
            if (index >= 0) {
                closeTab(index);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

DocumentTab *MainWindow::newDocument()
{
    DocumentTab *t = new DocumentTab;
    ++fUntitledCounter;
    t->setTitle(fUntitledCounter == 1 ? tr("Untitled") : tr("Untitled %1").arg(fUntitledCounter));
    connectTab(t);
    applyChartStyle(t);
    const int index = tabs->addTab(t, QString());
    installCloseButton(t);
    updateTabText(t);
    tabs->setCurrentIndex(index);
    return t;
}

DocumentTab *MainWindow::newDocument(const QDomDocument &doc, const QString &title, QString *errorMessage)
{
    DocumentTab *t = new DocumentTab;
    if (!t->flowChart()->loadDocument(doc, errorMessage)) {
        delete t;
        return nullptr;
    }
    if (title.isEmpty()) {
        ++fUntitledCounter;
        t->setTitle(fUntitledCounter == 1 ? tr("Untitled") : tr("Untitled %1").arg(fUntitledCounter));
    } else {
        t->setTitle(title);
    }
    t->setModified(true); // a new, unsaved document
    return addTab(t);
}

bool MainWindow::closeTab(int index)
{
    DocumentTab *t = tab(index);
    if (!t)
        return false;
    if (!maybeSave(t))
        return false;
    t->flowChart()->disconnect(this);
    t->disconnect(this);
    tabs->removeTab(tabs->indexOf(t));
    t->deleteLater();
    if (tabs->count() == 0)
        newDocument();
    return true;
}

void MainWindow::slotFileCloseTab()
{
    closeTab(tabs->currentIndex());
}

void MainWindow::currentTabChanged(int)
{
    // only the current document can be in the insertion mode
    for (int i = 0; i < tabs->count(); ++i) {
        DocumentTab *t = tab(i);
        if (t && t != currentTab() && t->flowChart()->status() == QFlowChart::Insertion)
            t->flowChart()->setStatus(QFlowChart::Selectable);
    }
    chartStatusChanged();
    updateTitle();
    updateZoomWidgets();
    updateActions();
    fCodeEdited = false; // the code panel follows the current tab
    updateCodePanelState();
    generateCode();
}

bool MainWindow::maybeSave(DocumentTab *t)
{
    if (!t || !t->isModified())
        return true;
    tabs->setCurrentWidget(t);
    QMessageBox::StandardButton answer;
    if (fSavePromptHook) {
        answer = fSavePromptHook(t->displayName());
    } else {
        QMessageBox box(QMessageBox::Warning, tr("Save changes"),
                        tr("Do you want to save the changes to “%1”?").arg(t->displayName()),
                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
        box.setInformativeText(tr("Your changes will be lost if you don't save them."));
        box.setDefaultButton(QMessageBox::Save);
        box.setWindowModality(Qt::WindowModal);
        answer = static_cast<QMessageBox::StandardButton>(box.exec());
    }
    if (answer == QMessageBox::Save)
        return saveTab(t);
    return answer == QMessageBox::Discard;
}

bool MainWindow::saveTab(DocumentTab *t, bool saveAs)
{
    if (!t)
        return false;
    QString fn = t->fileName();
    if (saveAs || fn.isEmpty()) {
        QString dir = fn.isEmpty() ? fLastDir : QFileInfo(fn).absolutePath();
        if (dir.isEmpty())
            dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        QString base = fn.isEmpty() ? t->displayName() : QFileInfo(fn).fileName();
        base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
        if (!base.endsWith(QLatin1String(".afc"), Qt::CaseInsensitive))
            base += QStringLiteral(".afc");
        const QString suggested = QDir(dir).filePath(base);
        fn = fSaveFileNameHook ? fSaveFileNameHook(suggested)
                               : QFileDialog::getSaveFileName(this, tr("Select a file to save"), suggested,
                                                              tr("Algorithm flowcharts (*.afc)"));
        if (fn.isEmpty())
            return false;
        if (!fn.endsWith(QLatin1String(".afc"), Qt::CaseInsensitive))
            fn += QStringLiteral(".afc");
    }
    QString error;
    if (!t->flowChart()->saveFile(fn, &error)) {
        QMessageBox::critical(this, tr("Failed to save a file"), error);
        return false;
    }
    t->setFileName(QFileInfo(fn).absoluteFilePath());
    t->setModified(false);
    fLastDir = QFileInfo(fn).absolutePath();
    saveSetting("lastDir", fLastDir);
    addRecentFile(t->fileName());
    updateTabText(t);
    if (t == currentTab())
        updateTitle();
    return true;
}

// ---------------------------------------------------------------------------
// chart signals

void MainWindow::chartStatusChanged()
{
    QFlowChart *doc = document();
    if (sender() && sender() != doc && qobject_cast<QFlowChart *>(sender()))
        return;
    if (doc && doc->status() != QFlowChart::Insertion) {
        if (QAbstractButton *select = toolGroup->button(0))
            select->setChecked(true);
    } else if (doc && doc->buffer().isEmpty() == false && toolGroup->checkedId() == 0) {
        // pasting: no tool is active
        toolGroup->setExclusive(false);
        if (QAbstractButton *select = toolGroup->button(0))
            select->setChecked(false);
        toolGroup->setExclusive(true);
    }
    updateActions();
    showStatusHint();
}

void MainWindow::chartChanged()
{
    if (sender() != document())
        return;
    updateActions();
    codeTimer->start();
}

void MainWindow::chartModified()
{
    if (DocumentTab *t = tabOf(sender()))
        t->setModified(true);
}

void MainWindow::chartEditBlock(QBlock *block)
{
    if (block)
        BlockEditDialog::edit(block, this);
}

void MainWindow::toolClicked(int id)
{
    if (id <= 0 || id >= toolXml.size()) {
        slotCancelInsertion();
        return;
    }
    const bool multi = (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;
    insertTool(toolXml.at(id), multi);
}

void MainWindow::insertTool(const QString &xml, bool multi)
{
    QFlowChart *doc = document();
    if (!doc)
        return;
    doc->setBuffer(xml);
    if (!doc->buffer().isEmpty()) {
        doc->setMultiInsert(multi);
        doc->setStatus(QFlowChart::Insertion);
    } else {
        doc->setStatus(QFlowChart::Selectable);
    }
}

void MainWindow::slotCancelInsertion()
{
    if (document() && document()->status() == QFlowChart::Insertion)
        document()->setStatus(QFlowChart::Selectable);
    else
        chartStatusChanged();
}

void MainWindow::updateActions()
{
    QFlowChart *doc = document();
    const bool inserting = doc && doc->status() == QFlowChart::Insertion;
    const bool selecting = doc && doc->status() == QFlowChart::Selectable;
    const bool hasBlock = selecting && doc->activeBlock();
    actUndo->setEnabled(doc && doc->canUndo() && !inserting);
    actRedo->setEnabled(doc && doc->canRedo() && !inserting);
    actSave->setEnabled(doc && !inserting);
    actSaveAs->setEnabled(doc && !inserting);
    actPrint->setEnabled(doc && !inserting);
    menuExport->setEnabled(doc && !inserting);
    actExportPng->setEnabled(doc && !inserting);
    actExportSvg->setEnabled(doc && !inserting);
    actExportPdf->setEnabled(doc && !inserting);
    actCopyImage->setEnabled(doc && !inserting);
    actCopy->setEnabled(hasBlock);
    actCut->setEnabled(hasBlock);
    actDelete->setEnabled(hasBlock);
    actPaste->setEnabled(selecting && doc->canPaste());
    actSelectAll->setEnabled(selecting);
    actCancelInsertion->setEnabled(inserting);
    actCloseTab->setEnabled(doc != nullptr);
}

// ---------------------------------------------------------------------------
// zoom

int MainWindow::zoomPercent() const
{
    return document() ? qRound(document()->zoom() * 100) : 100;
}

void MainWindow::setZoomPercent(int percent)
{
    DocumentTab *t = currentTab();
    if (!t)
        return;
    t->setZoom(qBound(kMinZoom, percent, kMaxZoom) / 100.0);
    updateZoomWidgets();
}

void MainWindow::zoomIn()
{
    setZoomPercent(nextZoomLevel(zoomPercent(), 1));
}

void MainWindow::zoomOut()
{
    setZoomPercent(nextZoomLevel(zoomPercent(), -1));
}

void MainWindow::zoomReset()
{
    setZoomPercent(100);
}

void MainWindow::zoomToFit()
{
    DocumentTab *t = currentTab();
    if (!t)
        return;
    // the size of the blocks does not scale exactly: refine once
    t->setZoom(t->fitZoom());
    t->setZoom(t->fitZoom());
    t->horizontalScrollBar()->setValue(0);
    t->verticalScrollBar()->setValue(0);
    updateZoomWidgets();
}

void MainWindow::updateZoomWidgets()
{
    const int percent = zoomPercent();
    {
        const QSignalBlocker blocker(zoomSlider);
        zoomSlider->setValue(sliderFromPercent(percent));
    }
    btnZoomValue->setText(tr("%1 %").arg(percent));
    actZoomIn->setEnabled(percent < kMaxZoom);
    actZoomOut->setEnabled(percent > kMinZoom);
}

// ---------------------------------------------------------------------------
// style

void MainWindow::applyChartStyle()
{
    for (int i = 0; i < tabs->count(); ++i)
        applyChartStyle(tab(i));
}

void MainWindow::applyChartStyle(DocumentTab *t)
{
    if (!t)
        return;
    QFlowChartStyle style = QFlowChartStyle::uiStyle(palette(), fMonochrome);
    style.setAssignSymbol(fAssignSymbol);
    t->flowChart()->setChartStyle(style);
    QPalette canvas = t->viewport()->palette();
    const QColor background = style.canvasColor().isValid() ? style.canvasColor() : palette().color(QPalette::Base);
    canvas.setColor(QPalette::Window, background);
    canvas.setColor(QPalette::Base, background);
    t->viewport()->setPalette(canvas);
    t->viewport()->setBackgroundRole(QPalette::Base);
    if (QWidget *c = t->widget()) {
        c->setPalette(canvas);
        c->setBackgroundRole(QPalette::Base);
    }
    t->flowChart()->update();
}

void MainWindow::setMonochrome(bool monochrome)
{
    fMonochrome = monochrome;
    saveSetting("view/monochrome", monochrome);
    applyChartStyle();
}

void MainWindow::setAssignSymbol(const QString &symbol)
{
    fAssignSymbol = symbol;
    for (QAction *act : std::as_const(actAssign))
        act->setChecked(act->data().toString() == symbol);
    saveSetting("view/assignSymbol", symbol);
    applyChartStyle();
}

// ---------------------------------------------------------------------------
// files

void MainWindow::slotFileNew()
{
    newDocument();
}

void MainWindow::slotFileOpen()
{
    QString dir = fLastDir;
    if (currentTab() && !currentTab()->fileName().isEmpty())
        dir = QFileInfo(currentTab()->fileName()).absolutePath();
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Select a file to open"), dir,
                                                            tr("Algorithm flowcharts (*.afc)"));
    for (const QString &fn : files)
        openDocument(fn);
}

bool MainWindow::requestOpenDocument(const QString &fn)
{
    if (isSourceFile(fn)) {
        showImportDialog(fn);
        return true;
    }
    return openDocument(fn);
}

bool MainWindow::openDocument(const QString &fn)
{
    QString error;
    if (!openDocument(fn, &error)) {
        // the open documents are left untouched
        QMessageBox::critical(this, tr("Failed to open a file"), error);
        return false;
    }
    return true;
}

bool MainWindow::openDocument(const QString &fn, QString *errorMessage)
{
    const QString path = QFileInfo(fn).absoluteFilePath();
    const int existing = findTab(path);
    if (existing >= 0) {
        tabs->setCurrentIndex(existing);
        addRecentFile(path);
        return true;
    }
    DocumentTab *t = new DocumentTab;
    if (!t->flowChart()->loadFile(fn, errorMessage)) {
        delete t;
        if (!QFileInfo::exists(fn) && fRecentFiles.removeAll(QDir::cleanPath(path)) > 0) {
            saveSetting("recentFiles", fRecentFiles);
            updateRecentMenu();
        }
        return false;
    }
    t->setFileName(path);
    t->setModified(false);
    addTab(t);
    fLastDir = QFileInfo(path).absolutePath();
    saveSetting("lastDir", fLastDir);
    addRecentFile(path);
    return true;
}

bool MainWindow::isSourceFile(const QString &fileName)
{
    static const QStringList suffixes = {QStringLiteral("c"),   QStringLiteral("cpp"), QStringLiteral("cc"),
                                         QStringLiteral("cxx"), QStringLiteral("c++"), QStringLiteral("h"),
                                         QStringLiteral("hpp"), QStringLiteral("hh"),  QStringLiteral("hxx")};
    return suffixes.contains(QFileInfo(fileName).suffix().toLower());
}

bool MainWindow::importFromSource(const QString &sourceFile, const QString &functionName,
                                  QDomDocument *doc, QString *errorMessage)
{
    QFile file(sourceFile);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = tr("Unable to open file '%1': %2").arg(QDir::toNativeSeparators(sourceFile), file.errorString());
        return false;
    }
    const QString source = decodeSourceText(file.readAll());

    afce::CodeImporter importer;
    const bool parsed = importer.parse(source, sourceFile);
    const QList<afce::FunctionInfo> functions = importer.functions();

    auto diagnosticsText = [&importer]() {
        QStringList lines;
        const QList<afce::Diagnostic> diagnostics = importer.diagnostics();
        for (const afce::Diagnostic &d : diagnostics) {
            if (d.severity == afce::Diagnostic::Info)
                continue;
            lines << (d.line > 0 ? tr("Line %1: %2").arg(d.line).arg(d.message) : d.message);
        }
        return lines.isEmpty() ? QString() : QStringLiteral("\n\n") + lines.join(QLatin1Char('\n'));
    };

    if (!parsed || functions.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("No function definitions were found in '%1'.").arg(QDir::toNativeSeparators(sourceFile))
                            + diagnosticsText();
        return false;
    }

    int index = importer.defaultFunctionIndex();
    if (!functionName.isEmpty()) {
        index = -1;
        for (int i = 0; i < functions.size() && index < 0; ++i) {
            if (functions.at(i).name == functionName)
                index = i;
        }
        // "bar" also matches "Foo::bar" if that is unambiguous
        for (int i = 0; i < functions.size() && index < 0; ++i) {
            if (functions.at(i).name.endsWith(QStringLiteral("::") + functionName))
                index = i;
        }
        if (index < 0) {
            QStringList names;
            for (const afce::FunctionInfo &f : functions)
                names << f.name;
            if (errorMessage)
                *errorMessage = tr("Function '%1' was not found in '%2'. Available functions: %3.")
                                    .arg(functionName, QDir::toNativeSeparators(sourceFile),
                                         names.join(QStringLiteral(", ")));
            return false;
        }
    }
    if (index < 0 || index >= functions.size())
        index = 0;

    const QDomDocument result = importer.flowchart(index);
    QString error;
    if (!QFlowChart::isValidDocument(result, &error)) {
        if (errorMessage)
            *errorMessage = tr("Unable to build the flowchart of '%1': %2").arg(functions.at(index).name, error)
                            + diagnosticsText();
        return false;
    }
    if (doc)
        *doc = result;
    return true;
}

bool MainWindow::importSource(const QString &sourceFile, const QString &functionName)
{
    QString error;
    if (!importSource(sourceFile, functionName, &error)) {
        QMessageBox::critical(this, tr("Import from source code"), error);
        return false;
    }
    return true;
}

bool MainWindow::importSource(const QString &sourceFile, const QString &functionName, QString *errorMessage)
{
    QDomDocument doc;
    if (!importFromSource(sourceFile, functionName, &doc, errorMessage))
        return false;
    QString title = doc.documentElement().attribute(QStringLiteral("name"));
    if (title.isEmpty())
        title = QFileInfo(sourceFile).completeBaseName();
    return newDocument(doc, title, errorMessage) != nullptr;
}

void MainWindow::showImportDialog(const QString &sourceFile)
{
    ImportDialog dialog(this);
    if (!sourceFile.isEmpty() && !dialog.loadFile(sourceFile))
        return;
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QList<QDomDocument> documents = dialog.importedDocuments();
    const QStringList titles = dialog.importedTitles();
    for (int i = 0; i < documents.size(); ++i) {
        QString error;
        if (!newDocument(documents.at(i), titles.value(i), &error)) {
            QMessageBox::critical(this, tr("Import from source code"), error);
            break;
        }
    }
}

void MainWindow::slotFileImport()
{
    showImportDialog();
}

void MainWindow::slotFileSave()
{
    saveTab(currentTab());
}

void MainWindow::slotFileSaveAs()
{
    saveTab(currentTab(), true);
}

void MainWindow::addRecentFile(const QString &fileName)
{
    const QString path = QDir::cleanPath(QFileInfo(fileName).absoluteFilePath());
    fRecentFiles.removeAll(path);
    fRecentFiles.prepend(path);
    while (fRecentFiles.size() > kMaxRecentFiles)
        fRecentFiles.removeLast();
    saveSetting("recentFiles", fRecentFiles);
    updateRecentMenu();
}

void MainWindow::clearRecentFiles()
{
    fRecentFiles.clear();
    saveSetting("recentFiles", fRecentFiles);
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    menuRecent->clear();
    for (int i = 0; i < fRecentFiles.size(); ++i) {
        const QString path = fRecentFiles.at(i);
        QString name = QFileInfo(path).fileName();
        name.replace(QLatin1Char('&'), QLatin1String("&&"));
        QAction *act = menuRecent->addAction(i < 9 ? QStringLiteral("&%1  %2").arg(i + 1).arg(name)
                                                   : QStringLiteral("1&0  %1").arg(name));
        act->setData(path);
        act->setToolTip(QDir::toNativeSeparators(path));
        act->setStatusTip(QDir::toNativeSeparators(path));
        connect(act, &QAction::triggered, this, [this, path]() { openDocument(path); });
    }
    if (fRecentFiles.isEmpty()) {
        QAction *none = menuRecent->addAction(tr("No Recent Files"));
        none->setEnabled(false);
    }
    menuRecent->addSeparator();
    menuRecent->addAction(actClearRecent);
    actClearRecent->setEnabled(!fRecentFiles.isEmpty());
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls())
        return;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        const QString fn = url.toLocalFile();
        if (!fn.isEmpty() && (fn.endsWith(QLatin1String(".afc"), Qt::CaseInsensitive) || isSourceFile(fn))) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList flowcharts;
    QStringList sources;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        const QString fn = url.toLocalFile();
        if (fn.endsWith(QLatin1String(".afc"), Qt::CaseInsensitive))
            flowcharts << fn;
        else if (isSourceFile(fn))
            sources << fn;
    }
    if (flowcharts.isEmpty() && sources.isEmpty())
        return;
    event->acceptProposedAction();
    // not inside the drop handler: the drag source (e.g. the Finder) would wait for the dialogs
    QTimer::singleShot(0, this, [this, flowcharts, sources]() {
        for (const QString &fn : flowcharts)
            openDocument(fn);
        if (!sources.isEmpty())
            showImportDialog(sources.constFirst());
    });
}

// ---------------------------------------------------------------------------
// export / print

QString MainWindow::getFilterFor(const QString & fileExt)
{
    return tr("%1 image (*.%2)").arg(fileExt.toUpper(), fileExt);
}

QString MainWindow::getWriteFormatFilter()
{
    QString result;
    const QList<QByteArray> formats = QImageWriter::supportedImageFormats();
    for(const QByteArray &format : formats)
    {
        const QString ext = QString::fromLatin1(format);
        if (!afce::isRasterExportFormat(ext))
            continue;
        if(!result.isEmpty()) result.append(";;");
        result.append(getFilterFor(ext));
    }
    return result;
}

QString MainWindow::exportFileName(const QString &suffix, const QString &filter)
{
    DocumentTab *t = currentTab();
    if (!t)
        return QString();
    QString dir = fLastExportDir;
    if (dir.isEmpty())
        dir = t->fileName().isEmpty() ? fLastDir : QFileInfo(t->fileName()).absolutePath();
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    QString base = t->fileName().isEmpty() ? t->displayName() : QFileInfo(t->fileName()).completeBaseName();
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    QString fn = QFileDialog::getSaveFileName(this, tr("Select a file to export"),
                                              QDir(dir).filePath(base + QLatin1Char('.') + suffix), filter);
    if (fn.isEmpty())
        return QString();
    if (!fn.endsWith(QLatin1Char('.') + suffix, Qt::CaseInsensitive))
        fn += QLatin1Char('.') + suffix;
    fLastExportDir = QFileInfo(fn).absolutePath();
    saveSetting("lastExportDir", fLastExportDir);
    return fn;
}

bool MainWindow::exportTo(const QString &fn, const QByteArray &format)
{
    if (!document())
        return false;
    QString error;
    bool ok = false;
    const QDomDocument doc = document()->document();
    afce::ExportOptions options;
    options.monochrome = fMonochrome;
    options.assignSymbol = fAssignSymbol;
    if (format == "svg") {
        ok = afce::exportSvg(doc, fn, options, &error);
    } else if (format == "pdf") {
        ok = afce::exportPdf(doc, fn, options, &error);
    } else {
        // twice the screen resolution: sharp in documents and on HiDPI screens
        options.zoom = 2.0;
        QImage image = afce::renderFlowchartImage(doc, options);
        if (image.isNull()) {
            error = tr("The flowchart is too large to be exported as a picture.");
        } else {
            image.setDotsPerMeterX(qRound(192 / 0.0254));
            image.setDotsPerMeterY(qRound(192 / 0.0254));
            QImageWriter writer(fn, format);
            ok = writer.write(image);
            if (!ok)
                error = tr("Unable to write file '%1': %2").arg(QDir::toNativeSeparators(fn), writer.errorString());
        }
    }
    if (!ok)
        QMessageBox::critical(this, tr("Export"), error);
    else
        statusBar()->showMessage(tr("Exported to %1").arg(QDir::toNativeSeparators(fn)), 4000);
    return ok;
}

void MainWindow::slotFileExportPng()
{
    const QString fn = exportFileName(QStringLiteral("png"), getFilterFor(QStringLiteral("png")));
    if (!fn.isEmpty())
        exportTo(fn, "png");
}

void MainWindow::slotFileExportSvg()
{
    const QString fn = exportFileName(QStringLiteral("svg"), getFilterFor(QStringLiteral("svg")));
    if (!fn.isEmpty())
        exportTo(fn, "svg");
}

void MainWindow::slotFileExportPdf()
{
    const QString fn = exportFileName(QStringLiteral("pdf"), tr("PDF document (*.pdf)"));
    if (!fn.isEmpty())
        exportTo(fn, "pdf");
}

void MainWindow::slotFileCopyImage()
{
    if (!document())
        return;
    afce::ExportOptions options;
    options.monochrome = fMonochrome;
    options.assignSymbol = fAssignSymbol;
    options.zoom = 2.0;
    QImage image = afce::renderFlowchartImage(document()->document(), options);
    if (image.isNull()) {
        QMessageBox::critical(this, tr("Copy as Image"), tr("The flowchart is too large to be copied as a picture."));
        return;
    }
    image.setDotsPerMeterX(qRound(192 / 0.0254));
    image.setDotsPerMeterY(qRound(192 / 0.0254));
    QApplication::clipboard()->setImage(image);
    statusBar()->showMessage(tr("The flowchart has been copied to the clipboard"), 3000);
}

void MainWindow::slotFilePrint()
{
    if (!document())
        return;
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog pd(&printer, this);
    if (pd.exec() == QDialog::Accepted)
    {
        QString error;
        afce::ExportOptions options;
        options.monochrome = fMonochrome;
        options.assignSymbol = fAssignSymbol;
        if (!afce::printFlowchart(document()->document(), &printer, options, &error))
            QMessageBox::critical(this, tr("Print"), error);
    }
}

// ---------------------------------------------------------------------------
// edit

void MainWindow::slotEditCut()
{
    slotEditCopy();
    slotEditDelete();
}

void MainWindow::slotEditCopy()
{
    if(document() && document()->activeBlock())
    {
        QApplication::clipboard()->setText(document()->activeBlockXml());
        updateActions();
    }
}

void MainWindow::slotEditPaste()
{
    QFlowChart *doc = document();
    if (!doc)
        return;
    doc->setBuffer(QApplication::clipboard()->text());
    if (!doc->buffer().isEmpty()) {
        doc->setMultiInsert(false);
        toolGroup->setExclusive(false);
        const QList<QAbstractButton *> buttons = toolGroup->buttons();
        for (QAbstractButton *button : buttons)
            button->setChecked(false);
        toolGroup->setExclusive(true);
        doc->setStatus(QFlowChart::Insertion);
    }
}

void MainWindow::slotEditDelete()
{
    if (document() && document()->status() == QFlowChart::Selectable)
        document()->deleteActiveBlock();
}

void MainWindow::slotEditSelectAll()
{
    if (document() && document()->status() == QFlowChart::Selectable)
        document()->selectAll();
}

// ---------------------------------------------------------------------------
// source code panel

void MainWindow::slotReloadGenerators()
{
    QString current = codeLanguage->currentData().toString();
    const QSignalBlocker blocker(codeLanguage);
    codeLanguage->clear();
    const QString loc = QLocale().name();
    const QStringList files = afce::generatorFiles();
    for (const QString &path : files) {
        const QString id = QFileInfo(path).completeBaseName();
        QString langName;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject names = QJsonDocument::fromJson(f.readAll()).object().value("name").toObject();
            langName = names.value(loc).toString();
            if (langName.isEmpty())
                langName = names.value("en_US").toString();
        }
        if (langName.isEmpty())
            langName = id;
        codeLanguage->addItem(langName, id);
    }

    int index = codeLanguage->findData(current);
    if (index < 0)
        index = codeLanguage->findData(QStringLiteral("cpp")); // default: C++
    if (index < 0)
        index = codeLanguage->count() > 0 ? 0 : -1;
    codeLanguage->setCurrentIndex(index);
}

void MainWindow::codeLangChanged(int)
{
    saveSetting("codeLanguage", codeLanguage->currentData().toString());
    fCodeEdited = false; // the edits were in the other language
    updateCodePanelState();
    generateCode();
}

void MainWindow::generateCode()
{
    codeTimer->stop();
    QFlowChart *doc = document();
    if (!doc || codeLanguage->currentIndex() < 0) {
        codeText->clear();
        return;
    }
    if (fCodeEdited)
        return; // keep the user's edits until they are applied or discarded
    const QString language = codeLanguage->currentData().toString();
    SourceCodeGenerator gen;
    gen.loadRule("generators:" + language + ".json");
    const QString code = gen.applyRule(doc->document());
    if (codeHighlighter->language() != language)
        codeHighlighter->setLanguage(language);
    if (codeText->toPlainText() != code) {
        const int v = codeText->verticalScrollBar()->value();
        const int h = codeText->horizontalScrollBar()->value();
        fSettingCode = true;
        codeText->setPlainText(code);
        fSettingCode = false;
        codeText->verticalScrollBar()->setValue(v);
        codeText->horizontalScrollBar()->setValue(h);
    }
}

bool MainWindow::codeIsImportable() const
{
    const QString language = codeLanguage->currentData().toString();
    return language == QLatin1String("c") || language == QLatin1String("cpp");
}

void MainWindow::updateCodePanelState()
{
    const bool importable = codeIsImportable();
    codeText->setReadOnly(!importable);
    codeText->setCurrentLineHighlight(importable);
    btnCodeApply->setVisible(importable);
    btnCodeApply->setEnabled(importable && document() != nullptr);
    btnCodeRevert->setVisible(importable && fCodeEdited);
    if (!importable)
        codeHint->setText(tr("Choose C or C++ to edit the code and build the flowchart from it."));
    else if (fCodeEdited)
        codeHint->setText(tr("The code has been changed."));
    else
        codeHint->setText(tr("Edit or paste code, then build the flowchart."));
    QFont hintFont = codeHint->font();
    hintFont.setItalic(!importable || !fCodeEdited);
    codeHint->setFont(hintFont);
}

void MainWindow::codeTextChanged()
{
    if (fSettingCode || codeText->isReadOnly() || fCodeEdited)
        return;
    fCodeEdited = true;
    updateCodePanelState();
}

void MainWindow::codeRevert()
{
    fCodeEdited = false;
    updateCodePanelState();
    generateCode();
}

void MainWindow::codeToFlowchart()
{
    QFlowChart *doc = document();
    DocumentTab *target = currentTab();
    if (!doc || !target || !codeIsImportable())
        return;
    const QString title = tr("Code to Flowchart");
    const bool isC = codeLanguage->currentData().toString() == QLatin1String("c");
    afce::ImportOptions options;
    options.language = isC ? afce::ImportOptions::Language::C : afce::ImportOptions::Language::Cpp;
    afce::CodeImporter importer(options);
    if (!importer.parse(codeText->toPlainText()) || importer.functions().isEmpty()) {
        QMessageBox::warning(this, title, tr("No function definitions or statements were found in the code."));
        return;
    }
    const QList<afce::FunctionInfo> functions = importer.functions();
    auto functionTitle = [](const afce::FunctionInfo &f) {
        return f.name.isEmpty() ? tr("Fragment") : f.name;
    };

    // the function of the current chart (by name), otherwise main
    int index = importer.defaultFunctionIndex();
    const QString chartName = doc->root()->attributes.value(QStringLiteral("name"));
    for (int i = 0; i < functions.size(); ++i) {
        if (!chartName.isEmpty() && functions.at(i).name == chartName)
            index = i;
    }
    bool all = false;
    if (functions.size() > 1) {
        QStringList items;
        for (const afce::FunctionInfo &f : functions)
            items << (f.name.isEmpty() ? tr("Fragment") : QStringLiteral("%1(%2)").arg(f.name, f.parameters));
        items << tr("All functions (the others in new tabs)");
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, title,
                                                     tr("The code contains several functions. Which one should this flowchart show?"),
                                                     items, qMax(0, index), false, &ok);
        if (!ok)
            return;
        const int chosen = int(items.indexOf(choice));
        if (chosen == items.size() - 1)
            all = true;
        else if (chosen >= 0)
            index = chosen;
    }
    if (index < 0 || index >= functions.size())
        index = 0;

    const QDomDocument result = importer.flowchart(index);
    QString error;
    if (!QFlowChart::isValidDocument(result, &error)) {
        QMessageBox::critical(this, title, error);
        return;
    }
    QStringList warnings;
    const QList<afce::Diagnostic> diagnostics = importer.diagnostics();
    for (const afce::Diagnostic &d : diagnostics) {
        if (d.severity != afce::Diagnostic::Info)
            warnings << (d.line > 0 ? tr("Line %1: %2").arg(d.line).arg(d.message) : d.message);
    }

    doc->makeUndo(); // one undo step: Ctrl+Z returns the previous flowchart
    doc->fromString(result.toString());
    doc->deselectAll();
    fCodeEdited = false;
    updateCodePanelState();
    generateCode();

    if (all) {
        for (int i = 0; i < functions.size(); ++i) {
            if (i != index)
                newDocument(importer.flowchart(i), functionTitle(functions.at(i)), nullptr);
        }
        tabs->setCurrentWidget(target);
    }
    statusBar()->showMessage(tr("The flowchart has been built from the code"), 4000);
    warnings.removeDuplicates();
    if (!warnings.isEmpty()) {
        const int shown = 12;
        QString text = warnings.mid(0, shown).join(QLatin1Char('\n'));
        if (warnings.size() > shown)
            text += QStringLiteral("\n...");
        QMessageBox::information(this, title,
                                 tr("The flowchart has been built, but some parts of the code cannot be shown exactly:")
                                     + QStringLiteral("\n\n") + text);
    }
}

void MainWindow::codeCopy()
{
    QApplication::clipboard()->setText(codeText->toPlainText());
    statusBar()->showMessage(tr("The source code has been copied to the clipboard"), 3000);
}

void MainWindow::codeSaveAs()
{
    DocumentTab *t = currentTab();
    if (!t)
        return;
    const QString language = codeLanguage->currentData().toString();
    const QString suffix = generatorSuffix(language);
    QString dir = t->fileName().isEmpty() ? fLastDir : QFileInfo(t->fileName()).absolutePath();
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString base = t->fileName().isEmpty() ? t->displayName() : QFileInfo(t->fileName()).completeBaseName();
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const QString filter = tr("%1 source code (*.%2)").arg(codeLanguage->currentText(), suffix)
                           + QStringLiteral(";;") + tr("All files (*)");
    const QString fn = QFileDialog::getSaveFileName(this, tr("Save the source code"),
                                                    QDir(dir).filePath(base + QLatin1Char('.') + suffix), filter);
    if (fn.isEmpty())
        return;
    QSaveFile file(fn);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Failed to save a file"), file.errorString());
        return;
    }
    file.write(codeText->toPlainText().toUtf8());
    if (!file.commit()) {
        QMessageBox::critical(this, tr("Failed to save a file"), file.errorString());
        return;
    }
    statusBar()->showMessage(tr("Saved %1").arg(QDir::toNativeSeparators(fn)), 3000);
}

// ---------------------------------------------------------------------------
// help

void MainWindow::slotHelpAbout()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("About AFCE"));
    QLabel *icon = new QLabel;
    icon->setPixmap(QApplication::windowIcon().pixmap(QSize(48, 48)));
    icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    const QString buildYear = QStringLiteral(__DATE__).right(4);
    QLabel *text = new QLabel(tr(
        "<h2 style='margin-bottom:2px'>AFCE</h2>"
        "<p style='margin-top:0'>Algorithm Flowchart Editor<br>Version %1</p>"
        "<p>Copyright &copy; 2008&ndash;%2 Victor Zinkevich and AFCE contributors.<br>"
        "Contributors: Sergey Ryabenko, Alexey Loginov and others.</p>"
        "<p>This program is free software: you can redistribute it and/or modify it<br>"
        "under the terms of the GNU General Public License, version 2 or 3.</p>"
        "<p>The program is provided AS IS with NO WARRANTY OF ANY KIND,<br>"
        "INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND<br>"
        "FITNESS FOR A PARTICULAR PURPOSE.</p>"
        "<p><a href='%3'>%3</a></p>"
        "<p style='color:gray'>Qt %4</p>")
        .arg(afce::programVersion(), buildYear, QStringLiteral("https://github.com/Heisenberg4441/afce-reimagined"),
             QString::fromLatin1(qVersion())));
    text->setTextFormat(Qt::RichText);
    text->setOpenExternalLinks(true);
    text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QGridLayout *layout = new QGridLayout(&dlg);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setHorizontalSpacing(16);
    layout->addWidget(icon, 0, 0);
    layout->addWidget(text, 0, 1);
    layout->addWidget(buttons, 1, 0, 1, 2);
    dlg.exec();
}

void MainWindow::slotHelpAboutQt()
{
    QMessageBox::aboutQt(this);
}

// ---------------------------------------------------------------------------
// languages

void MainWindow::slotChangeLanguage()
{
    QAction * action = qobject_cast<QAction *>(sender());
    if (!action)
        return;
    QString localeName = action->data().toString();
    // installing the translators sends QEvent::LanguageChange to every
    // widget, which calls retranslateUi()
    setApplicationLocale(localeName);
    retranslateUi();
    saveSetting("locale", localeName);
}

QMap<QString, QString> MainWindow::enumLanguages()
{
    QMap<QString, QString> result;
    const QDir dir(QStringLiteral(":/i18n"));
    const QStringList qms = dir.entryList(QStringList() << "afce_*.qm", QDir::Files, QDir::Name);

    static const QRegularExpression rx(QStringLiteral("^afce_([-_a-zA-Z]+)\\.qm$"));
    for (const QString &qm : qms) {
        const QRegularExpressionMatch match = rx.match(qm);
        if(match.hasMatch()) {
            const QString localeName = match.captured(1);
            QLocale loc(localeName);
            QString name = loc.nativeLanguageName();
            if (!name.isEmpty())
                name[0] = name.at(0).toUpper();
            result.insert(localeName, name.isEmpty() ? localeName : name);
        }
    }
    return result;
}

QString resolveApplicationLocale(const QString &preferred)
{
    const QStringList available = MainWindow::enumLanguages().keys();
    if (available.contains(preferred))
        return preferred;
    // e.g. "ru" or "ru-RU" -> ru_RU
    QStringList candidates;
    if (!preferred.isEmpty())
        candidates << preferred;
    candidates << QLocale::system().uiLanguages();
    for (const QString &candidate : std::as_const(candidates)) {
        const QLocale loc(candidate);
        if (available.contains(loc.name()))
            return loc.name();
        for (const QString &name : available) {
            if (QLocale(name).language() == loc.language())
                return name;
        }
    }
    return QStringLiteral("en_US");
}

void setApplicationLocale(const QString &localeName)
{
    QLocale locale(localeName);
    QLocale::setDefault(locale);
    static QTranslator *qtTranslator = nullptr;
    static QTranslator *myappTranslator = nullptr;

    // replace the translators of the previous language
    if (qtTranslator) {
        QCoreApplication::removeTranslator(qtTranslator);
        delete qtTranslator;
        qtTranslator = nullptr;
    }
    if (myappTranslator) {
        QCoreApplication::removeTranslator(myappTranslator);
        delete myappTranslator;
        myappTranslator = nullptr;
    }

    // Qt's own strings (standard dialogs, buttons): the translations of the Qt
    // installation, deployed ones (windeployqt), then the copies embedded at build time
    QStringList qtDirs;
    qtDirs << QLibraryInfo::path(QLibraryInfo::TranslationsPath)
           << QCoreApplication::applicationDirPath() + "/translations"
           << QCoreApplication::applicationDirPath() + "/../Resources/translations"
           << QStringLiteral(":/i18n");
    qtTranslator = new QTranslator(qApp);
    bool qtLoaded = false;
    for (const QString &dir : std::as_const(qtDirs)) {
        if (qtTranslator->load(locale, "qtbase", "_", dir) || qtTranslator->load(locale, "qt", "_", dir)) {
            qtLoaded = true;
            break;
        }
    }
    if (qtLoaded) {
        QCoreApplication::installTranslator(qtTranslator);
    } else {
        delete qtTranslator;
        qtTranslator = nullptr;
    }

    // AFCE strings, embedded as :/i18n/afce_<locale>.qm
    myappTranslator = new QTranslator(qApp);
    if (myappTranslator->load(locale, "afce", "_", ":/i18n")) {
        QCoreApplication::installTranslator(myappTranslator);
    } else {
        delete myappTranslator;
        myappTranslator = nullptr;
    }

    const QWidgetList wl = QApplication::allWidgets();
    for (QWidget *w : wl) {
        w->setLocale(locale);
    }
}
