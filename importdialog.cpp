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

#include "importdialog.h"

#include "appicons.h"
#include "codeeditor.h"
#include "qflowchartstyle.h"
#include "zvflowchart.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringDecoder>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>

namespace {

const int kDebounceMs = 300;
const int kMinZoomPercent = 20;
const int kMaxZoomPercent = 200;
const qint64 kMaxFileSize = 8 * 1024 * 1024;

bool gQuietMode = false;

// Windows-1251, bytes 0x80..0xBF (0xC0..0xFF are U+0410..U+044F).
const char16_t kCp1251High[64] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
};

QString decodeCp1251(const QByteArray &data)
{
    QString result;
    result.reserve(data.size());
    for (const char ch : data) {
        const uchar c = uchar(ch);
        if (c < 0x80)
            result.append(QChar(c));
        else if (c >= 0xC0)
            result.append(QChar(char16_t(0x0410 + (c - 0xC0))));
        else
            result.append(QChar(kCp1251High[c - 0x80]));
    }
    return result;
}

QIcon severityIcon(afce::Diagnostic::Severity severity, const QPalette &palette)
{
    CodeEditor::LineMarker::Severity s = CodeEditor::LineMarker::Warning;
    QString glyph = QStringLiteral("!");
    if (severity == afce::Diagnostic::Error) {
        s = CodeEditor::LineMarker::Error;
        glyph = QStringLiteral("×");
    } else if (severity == afce::Diagnostic::Info) {
        s = CodeEditor::LineMarker::Info;
        glyph = QStringLiteral("i");
    }
    QIcon icon;
    for (int size : {16, 32, 48}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(CodeEditor::markerColor(s, palette));
        const double m = size / 16.0;
        p.drawEllipse(QRectF(m, m, size - 2 * m, size - 2 * m));
        QFont font = QApplication::font();
        font.setBold(true);
        font.setPixelSize(qRound(size * (severity == afce::Diagnostic::Error ? 0.8 : 0.66)));
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, glyph);
        p.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

QString droppedSourceFile(const QMimeData *mime)
{
    if (!mime || !mime->hasUrls())
        return QString();
    const QList<QUrl> urls = mime->urls();
    for (const QUrl &url : urls)
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile())
            return url.toLocalFile();
    return QString();
}

} // namespace

// ---------------------------------------------------------------------------

class ImportDialogPrivate
{
public:
    explicit ImportDialogPrivate(ImportDialog *dialog) : q(dialog) {}

    void setupUi();
    void readSettings();
    void writeSettings() const;
    void scheduleUpdate(bool sourceChanged);
    void parse();
    void rebuildFlowchart();
    void showEmptyState(const QString &title, const QString &hint = QString());
    void showDiagnostics(const QList<afce::Diagnostic> &diagnostics);
    void updateButtons();
    void updateTitle();
    void setZoom(double zoom, bool fromUser);
    void applyFitZoom();
    void applyPalette();
    void importFunctions(bool all);
    int currentFunctionIndex() const;
    QString functionTitle(int index) const;
    void reportError(const QString &message);
    void openFileDialog();
    void setOptionsVisible(bool visible);

    ImportDialog *q;

    CodeEditor *editor = nullptr;
    CodeHighlighter *highlighter = nullptr;
    QPushButton *openButton = nullptr;
    QComboBox *languageCombo = nullptr;
    QLabel *infoLabel = nullptr;
    QComboBox *functionCombo = nullptr;
    QSplitter *mainSplitter = nullptr;
    QSplitter *leftSplitter = nullptr;
    QLabel *diagnosticsHeader = nullptr;
    QListWidget *diagnosticsList = nullptr;
    QStackedWidget *previewStack = nullptr;
    QLabel *emptyTitle = nullptr;
    QLabel *emptyHint = nullptr;
    QScrollArea *previewArea = nullptr;
    QFlowChart *chart = nullptr;
    QToolButton *zoomOutButton = nullptr;
    QToolButton *zoomInButton = nullptr;
    QToolButton *fitButton = nullptr;
    QSlider *zoomSlider = nullptr;
    QLabel *zoomLabel = nullptr;
    QToolButton *optionsToggle = nullptr;
    QFrame *optionsPanel = nullptr;
    QComboBox *forStyleCombo = nullptr;
    QCheckBox *keepDeclarations = nullptr;
    QCheckBox *detectIO = nullptr;
    QCheckBox *callsAsSubroutine = nullptr;
    QCheckBox *expandCompoundAssign = nullptr;
    QCheckBox *keepMainReturn = nullptr;
    QPushButton *importButton = nullptr;
    QPushButton *importAllButton = nullptr;
    QPushButton *cancelButton = nullptr;

    QTimer updateTimer;
    afce::CodeImporter importer;
    QList<afce::FunctionInfo> functions;
    bool sourceDirty = true;
    bool updatingFunctions = false;
    bool updatingOptions = false;
    bool updatingZoom = false;
    bool fitMode = true;
    bool previewValid = false;
    QString fileName;
    QString wantedFunction;
    QString lastError;
    QString lastDirectory;
    QList<QDomDocument> documents;
    QStringList titles;
};

void ImportDialogPrivate::setupUi()
{
    q->setWindowTitle(ImportDialog::tr("Import from Source Code"));
    q->setAcceptDrops(true);
    q->setSizeGripEnabled(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(q);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // --- left: source code ---------------------------------------------------
    QWidget *codePane = new QWidget;
    QVBoxLayout *codeLayout = new QVBoxLayout(codePane);
    codeLayout->setContentsMargins(0, 0, 0, 0);
    codeLayout->setSpacing(6);

    QHBoxLayout *codeBar = new QHBoxLayout;
    codeBar->setSpacing(6);
    openButton = new QPushButton(ImportDialog::tr("Open File..."));
    openButton->setObjectName(QStringLiteral("openFileButton"));
    openButton->setIcon(afce::uiIcon(QStringLiteral("open")));
    openButton->setToolTip(ImportDialog::tr("Open a C or C++ source file (%1)")
                               .arg(QKeySequence(QKeySequence::Open).toString(QKeySequence::NativeText)));
    openButton->setShortcut(QKeySequence::Open);
    openButton->setAutoDefault(false);
    codeBar->addWidget(openButton);
    codeBar->addSpacing(6);
    QLabel *languageLabel = new QLabel(ImportDialog::tr("&Language:"));
    languageCombo = new QComboBox;
    languageCombo->setObjectName(QStringLiteral("languageCombo"));
    languageCombo->addItem(ImportDialog::tr("Auto"), int(afce::ImportOptions::Language::Auto));
    languageCombo->addItem(QStringLiteral("C"), int(afce::ImportOptions::Language::C));
    languageCombo->addItem(QStringLiteral("C++"), int(afce::ImportOptions::Language::Cpp));
    languageCombo->setToolTip(ImportDialog::tr("Source language (Auto: by the file name extension, otherwise C++)"));
    languageLabel->setBuddy(languageCombo);
    codeBar->addWidget(languageLabel);
    codeBar->addWidget(languageCombo);
    codeBar->addStretch(1);
    infoLabel = new QLabel;
    infoLabel->setObjectName(QStringLiteral("infoLabel"));
    infoLabel->setForegroundRole(QPalette::PlaceholderText);
    codeBar->addWidget(infoLabel);
    codeLayout->addLayout(codeBar);

    editor = new CodeEditor;
    editor->setObjectName(QStringLiteral("sourceEditor"));
    editor->setPlaceholderText(ImportDialog::tr("Paste C or C++ code here or open a file"));
    highlighter = new CodeHighlighter(editor->document());
    highlighter->setLanguage(QStringLiteral("cpp"));

    QWidget *diagnosticsPane = new QWidget;
    QVBoxLayout *diagnosticsLayout = new QVBoxLayout(diagnosticsPane);
    diagnosticsLayout->setContentsMargins(0, 4, 0, 0);
    diagnosticsLayout->setSpacing(4);
    diagnosticsHeader = new QLabel;
    diagnosticsHeader->setObjectName(QStringLiteral("diagnosticsHeader"));
    diagnosticsLayout->addWidget(diagnosticsHeader);
    diagnosticsList = new QListWidget;
    diagnosticsList->setObjectName(QStringLiteral("diagnosticsList"));
    diagnosticsList->setIconSize(QSize(16, 16));
    diagnosticsList->setWordWrap(true);
    diagnosticsList->setResizeMode(QListView::Adjust);
    diagnosticsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    diagnosticsList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    diagnosticsList->setToolTip(ImportDialog::tr("Double-click a message to show its line"));
    diagnosticsLayout->addWidget(diagnosticsList);

    leftSplitter = new QSplitter(Qt::Vertical);
    leftSplitter->setObjectName(QStringLiteral("codeSplitter"));
    leftSplitter->setChildrenCollapsible(false);
    leftSplitter->addWidget(editor);
    leftSplitter->addWidget(diagnosticsPane);
    leftSplitter->setStretchFactor(0, 5);
    leftSplitter->setStretchFactor(1, 1);
    diagnosticsPane->setMinimumHeight(64);
    codeLayout->addWidget(leftSplitter, 1);

    // --- right: preview ---------------------------------------------------------
    QWidget *previewPane = new QWidget;
    QVBoxLayout *previewLayout = new QVBoxLayout(previewPane);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(6);

    QHBoxLayout *previewBar = new QHBoxLayout;
    previewBar->setSpacing(4);
    QLabel *functionLabel = new QLabel(ImportDialog::tr("&Function:"));
    functionCombo = new QComboBox;
    functionCombo->setObjectName(QStringLiteral("functionCombo"));
    functionCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    functionCombo->setMinimumContentsLength(16);
    functionCombo->setToolTip(ImportDialog::tr("Function shown in the preview and imported by \"Import\""));
    functionLabel->setBuddy(functionCombo);
    previewBar->addWidget(functionLabel);
    previewBar->addWidget(functionCombo, 1);
    previewBar->addSpacing(10);

    auto makeToolButton = [](const QString &text, const QString &toolTip) {
        QToolButton *b = new QToolButton;
        b->setText(text);
        b->setToolTip(toolTip);
        b->setAutoRaise(true);
        b->setFocusPolicy(Qt::TabFocus);
        return b;
    };
    zoomOutButton = makeToolButton(QStringLiteral("−"), ImportDialog::tr("Zoom out"));
    zoomInButton = makeToolButton(QStringLiteral("+"), ImportDialog::tr("Zoom in"));
    fitButton = makeToolButton(ImportDialog::tr("Fit"), ImportDialog::tr("Fit the flowchart into the preview"));
    fitButton->setObjectName(QStringLiteral("fitButton"));
    fitButton->setCheckable(true);
    fitButton->setChecked(true);
    zoomSlider = new QSlider(Qt::Horizontal);
    zoomSlider->setObjectName(QStringLiteral("zoomSlider"));
    zoomSlider->setRange(kMinZoomPercent, kMaxZoomPercent);
    zoomSlider->setValue(100);
    zoomSlider->setFixedWidth(90);
    zoomSlider->setToolTip(ImportDialog::tr("Zoom (Ctrl+wheel over the preview)"));
    zoomLabel = new QLabel(QStringLiteral("100%"));
    zoomLabel->setMinimumWidth(zoomLabel->fontMetrics().horizontalAdvance(QStringLiteral("200%")) + 4);
    zoomLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    zoomLabel->setForegroundRole(QPalette::PlaceholderText);
    previewBar->addWidget(zoomOutButton);
    previewBar->addWidget(zoomSlider);
    previewBar->addWidget(zoomInButton);
    previewBar->addWidget(zoomLabel);
    previewBar->addWidget(fitButton);
    previewLayout->addLayout(previewBar);

    previewStack = new QStackedWidget;
    previewStack->setObjectName(QStringLiteral("previewStack"));

    QFrame *emptyPage = new QFrame;
    emptyPage->setFrameShape(QFrame::StyledPanel);
    emptyPage->setBackgroundRole(QPalette::Base);
    emptyPage->setAutoFillBackground(true);
    QVBoxLayout *emptyLayout = new QVBoxLayout(emptyPage);
    emptyLayout->setContentsMargins(24, 24, 24, 24);
    emptyLayout->addStretch(1);
    emptyTitle = new QLabel;
    emptyTitle->setObjectName(QStringLiteral("emptyTitle"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyTitle->setWordWrap(true);
    QFont titleFont = emptyTitle->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.25);
    emptyTitle->setFont(titleFont);
    emptyHint = new QLabel;
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyHint->setWordWrap(true);
    emptyHint->setForegroundRole(QPalette::PlaceholderText);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addSpacing(6);
    emptyLayout->addWidget(emptyHint);
    emptyLayout->addStretch(1);
    previewStack->addWidget(emptyPage);

    previewArea = new QScrollArea;
    previewArea->setObjectName(QStringLiteral("previewArea"));
    previewArea->setWidgetResizable(false);
    previewArea->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    previewArea->setBackgroundRole(QPalette::Base);
    previewArea->viewport()->setBackgroundRole(QPalette::Base);
    previewArea->viewport()->setAutoFillBackground(true);
    chart = new QFlowChart;
    chart->setObjectName(QStringLiteral("previewChart"));
    chart->setStatus(QFlowChart::Display);
    chart->setChartStyle(QFlowChartStyle::uiStyle(q->palette()));
    previewArea->setWidget(chart);
    previewArea->viewport()->installEventFilter(q);
    previewStack->addWidget(previewArea);
    previewLayout->addWidget(previewStack, 1);

    mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->setObjectName(QStringLiteral("mainSplitter"));
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setHandleWidth(10);
    mainSplitter->addWidget(codePane);
    mainSplitter->addWidget(previewPane);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);
    codePane->setMinimumWidth(260);
    previewPane->setMinimumWidth(220);
    mainLayout->addWidget(mainSplitter, 1);

    // --- options ----------------------------------------------------------------
    optionsPanel = new QFrame;
    optionsPanel->setObjectName(QStringLiteral("optionsPanel"));
    optionsPanel->setFrameShape(QFrame::StyledPanel);
    QGridLayout *optionsLayout = new QGridLayout(optionsPanel);
    optionsLayout->setContentsMargins(10, 8, 10, 8);
    optionsLayout->setHorizontalSpacing(18);
    optionsLayout->setVerticalSpacing(6);

    QHBoxLayout *forLayout = new QHBoxLayout;
    forLayout->setSpacing(6);
    QLabel *forLabel = new QLabel(ImportDialog::tr("F&or loops as:"));
    forStyleCombo = new QComboBox;
    forStyleCombo->setObjectName(QStringLiteral("forStyleCombo"));
    forStyleCombo->addItem(ImportDialog::tr("C-style \"for\" block"), int(afce::ImportOptions::ForStyle::CStyle));
    forStyleCombo->addItem(ImportDialog::tr("\"while\" loop"), int(afce::ImportOptions::ForStyle::While));
    forStyleCombo->addItem(ImportDialog::tr("Counting loop where possible"), int(afce::ImportOptions::ForStyle::Arithmetic));
    forStyleCombo->setItemData(0, ImportDialog::tr("for (init; condition; step) becomes one loop block"), Qt::ToolTipRole);
    forStyleCombo->setItemData(1, ImportDialog::tr("Initialization, a pre-condition loop and the step at the end of its body"),
                               Qt::ToolTipRole);
    forStyleCombo->setItemData(2, ImportDialog::tr("for (i = a; i <= b; i++) becomes \"i from a to b\"; other loops stay C-style"),
                               Qt::ToolTipRole);
    forLabel->setBuddy(forStyleCombo);
    forLayout->addWidget(forLabel);
    forLayout->addWidget(forStyleCombo);
    forLayout->addStretch(1);

    detectIO = new QCheckBox(ImportDialog::tr("Recognize &input and output"));
    detectIO->setObjectName(QStringLiteral("detectIO"));
    detectIO->setToolTip(ImportDialog::tr("scanf, printf, cin, cout and similar become input / output blocks"));
    callsAsSubroutine = new QCheckBox(ImportDialog::tr("Function calls as &subroutine blocks"));
    callsAsSubroutine->setObjectName(QStringLiteral("callsAsSubroutine"));
    callsAsSubroutine->setToolTip(ImportDialog::tr("A statement that only calls a function becomes a \"predefined process\" block"));
    keepDeclarations = new QCheckBox(ImportDialog::tr("Keep variable &declarations"));
    keepDeclarations->setObjectName(QStringLiteral("keepDeclarations"));
    keepDeclarations->setToolTip(ImportDialog::tr("Every declaration becomes a process block with its original text"));
    expandCompoundAssign = new QCheckBox(ImportDialog::tr("E&xpand compound assignments"));
    expandCompoundAssign->setObjectName(QStringLiteral("expandCompoundAssign"));
    expandCompoundAssign->setToolTip(ImportDialog::tr("x += 2 becomes x := x + 2, i++ becomes i := i + 1"));
    keepMainReturn = new QCheckBox(ImportDialog::tr("Keep \"&return 0\" at the end of main"));
    keepMainReturn->setObjectName(QStringLiteral("keepMainReturn"));

    optionsLayout->addLayout(forLayout, 0, 0);
    optionsLayout->addWidget(keepDeclarations, 1, 0);
    optionsLayout->addWidget(detectIO, 0, 1);
    optionsLayout->addWidget(callsAsSubroutine, 1, 1);
    optionsLayout->addWidget(expandCompoundAssign, 0, 2);
    optionsLayout->addWidget(keepMainReturn, 1, 2);
    optionsLayout->setColumnStretch(3, 1);
    mainLayout->addWidget(optionsPanel);

    // --- buttons ----------------------------------------------------------------
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    optionsToggle = new QToolButton;
    optionsToggle->setObjectName(QStringLiteral("optionsToggle"));
    optionsToggle->setText(ImportDialog::tr("Options"));
    optionsToggle->setAutoRaise(true);
    optionsToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    optionsToggle->setArrowType(Qt::DownArrow);
    optionsToggle->setToolTip(ImportDialog::tr("Show or hide the import options"));
    buttonLayout->addWidget(optionsToggle);
    buttonLayout->addStretch(1);
    importAllButton = new QPushButton(ImportDialog::tr("Import &All Functions"));
    importAllButton->setObjectName(QStringLiteral("importAllButton"));
    importAllButton->setAutoDefault(false);
    importButton = new QPushButton(ImportDialog::tr("&Import"));
    importButton->setObjectName(QStringLiteral("importButton"));
    importButton->setDefault(true);
    importButton->setToolTip(ImportDialog::tr("Import the selected function as a new flowchart"));
    cancelButton = new QPushButton(ImportDialog::tr("Cancel"));
    cancelButton->setObjectName(QStringLiteral("cancelButton"));
    cancelButton->setAutoDefault(false);
    buttonLayout->addWidget(importAllButton);
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    // default geometry: a large part of the screen
    QSize size(1180, 760);
    if (const QScreen *screen = q->screen()) {
        const QSize available = screen->availableSize();
        size = size.boundedTo(QSize(available.width() * 9 / 10, available.height() * 9 / 10));
    }
    q->resize(size);
    mainSplitter->setSizes({size.width() / 2, size.width() / 2});
    leftSplitter->setSizes({size.height() * 4 / 5, size.height() / 5});

    // --- connections ------------------------------------------------------------
    updateTimer.setSingleShot(true);
    updateTimer.setInterval(kDebounceMs);
    QObject::connect(&updateTimer, &QTimer::timeout, q, [this]() { q->updatePreview(); });
    QObject::connect(editor, &QPlainTextEdit::textChanged, q, [this]() { scheduleUpdate(true); });
    QObject::connect(editor, &CodeEditor::fileDropped, q, [this](const QString &file) { q->loadFile(file); });
    QObject::connect(languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this]() {
        highlighter->setLanguage(languageCombo->currentData().toInt() == int(afce::ImportOptions::Language::C)
                                     ? QStringLiteral("c") : QStringLiteral("cpp"));
        scheduleUpdate(true);
    });
    QObject::connect(functionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this]() {
        if (updatingFunctions)
            return;
        const int index = currentFunctionIndex();
        if (index >= 0 && functions.at(index).line > 0)
            editor->scrollToLine(functions.at(index).line);
        scheduleUpdate(false);
    });
    QObject::connect(forStyleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), q, [this]() {
        if (!updatingOptions)
            scheduleUpdate(false);
    });
    for (QCheckBox *box : {keepDeclarations, detectIO, callsAsSubroutine, expandCompoundAssign, keepMainReturn}) {
        QObject::connect(box, &QCheckBox::toggled, q, [this]() {
            if (!updatingOptions)
                scheduleUpdate(false);
        });
    }
    QObject::connect(optionsToggle, &QToolButton::clicked, q, [this]() { setOptionsVisible(optionsPanel->isHidden()); });
    QObject::connect(openButton, &QPushButton::clicked, q, [this]() { openFileDialog(); });
    QObject::connect(importButton, &QPushButton::clicked, q, [this]() { importFunctions(false); });
    QObject::connect(importAllButton, &QPushButton::clicked, q, [this]() { importFunctions(true); });
    QObject::connect(cancelButton, &QPushButton::clicked, q, &QDialog::reject);

    auto jumpToDiagnostic = [this](QListWidgetItem *item, bool focus) {
        const int line = item ? item->data(Qt::UserRole).toInt() : 0;
        if (line > 0) {
            editor->highlightLine(line);
            if (focus)
                editor->setFocus();
        }
    };
    QObject::connect(diagnosticsList, &QListWidget::itemClicked, q,
                     [jumpToDiagnostic](QListWidgetItem *item) { jumpToDiagnostic(item, false); });
    QObject::connect(diagnosticsList, &QListWidget::itemActivated, q,
                     [jumpToDiagnostic](QListWidgetItem *item) { jumpToDiagnostic(item, true); });

    QObject::connect(zoomSlider, &QSlider::valueChanged, q, [this](int value) {
        if (!updatingZoom)
            setZoom(value / 100.0, true);
    });
    QObject::connect(zoomInButton, &QToolButton::clicked, q, [this]() {
        setZoom(qMin(kMaxZoomPercent / 100.0, chart->zoom() * 1.25), true);
    });
    QObject::connect(zoomOutButton, &QToolButton::clicked, q, [this]() {
        setZoom(qMax(kMinZoomPercent / 100.0, chart->zoom() / 1.25), true);
    });
    QObject::connect(fitButton, &QToolButton::toggled, q, [this](bool on) {
        fitMode = on;
        if (on)
            applyFitZoom();
    });

    // Ctrl+Enter imports from the editor as well
    QShortcut *importShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), q);
    QObject::connect(importShortcut, &QShortcut::activated, q, [this]() {
        if (importButton->isEnabled())
            importFunctions(false);
    });

    editor->setFocus();
}

void ImportDialogPrivate::readSettings()
{
    q->setOptions(afce::ImportOptions());
    if (gQuietMode)
        return;
    QSettings settings(QStringLiteral("afce"), QStringLiteral("application"));
    settings.beginGroup(QStringLiteral("importDialog"));
    const QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
    if (!geometry.isEmpty())
        q->restoreGeometry(geometry);
    const QByteArray mainState = settings.value(QStringLiteral("mainSplitter")).toByteArray();
    if (!mainState.isEmpty())
        mainSplitter->restoreState(mainState);
    const QByteArray codeState = settings.value(QStringLiteral("codeSplitter")).toByteArray();
    if (!codeState.isEmpty())
        leftSplitter->restoreState(codeState);
    setOptionsVisible(settings.value(QStringLiteral("optionsVisible"), true).toBool());
    const int language = languageCombo->findData(settings.value(QStringLiteral("language"), 0).toInt());
    if (language >= 0)
        languageCombo->setCurrentIndex(language);
    lastDirectory = settings.value(QStringLiteral("lastDirectory")).toString();

    afce::ImportOptions o;
    o.forStyle = afce::ImportOptions::ForStyle(settings.value(QStringLiteral("forStyle"), int(o.forStyle)).toInt());
    o.keepDeclarations = settings.value(QStringLiteral("keepDeclarations"), o.keepDeclarations).toBool();
    o.detectIO = settings.value(QStringLiteral("detectIO"), o.detectIO).toBool();
    o.callsAsSubroutine = settings.value(QStringLiteral("callsAsSubroutine"), o.callsAsSubroutine).toBool();
    o.expandCompoundAssign = settings.value(QStringLiteral("expandCompoundAssign"), o.expandCompoundAssign).toBool();
    o.omitMainReturn = !settings.value(QStringLiteral("keepMainReturn"), !o.omitMainReturn).toBool();
    o.language = afce::ImportOptions::Language(languageCombo->currentData().toInt());
    q->setOptions(o);
    const double zoom = settings.value(QStringLiteral("zoom"), 0.0).toDouble();
    if (zoom > 0) {
        fitMode = false;
        fitButton->setChecked(false);
        setZoom(zoom, false);
    }
    settings.endGroup();
}

void ImportDialogPrivate::writeSettings() const
{
    if (gQuietMode)
        return;
    QSettings settings(QStringLiteral("afce"), QStringLiteral("application"));
    settings.beginGroup(QStringLiteral("importDialog"));
    settings.setValue(QStringLiteral("geometry"), q->saveGeometry());
    settings.setValue(QStringLiteral("mainSplitter"), mainSplitter->saveState());
    settings.setValue(QStringLiteral("codeSplitter"), leftSplitter->saveState());
    settings.setValue(QStringLiteral("optionsVisible"), !optionsPanel->isHidden());
    settings.setValue(QStringLiteral("language"), languageCombo->currentData().toInt());
    settings.setValue(QStringLiteral("lastDirectory"), lastDirectory);
    const afce::ImportOptions o = q->options();
    settings.setValue(QStringLiteral("forStyle"), int(o.forStyle));
    settings.setValue(QStringLiteral("keepDeclarations"), o.keepDeclarations);
    settings.setValue(QStringLiteral("detectIO"), o.detectIO);
    settings.setValue(QStringLiteral("callsAsSubroutine"), o.callsAsSubroutine);
    settings.setValue(QStringLiteral("expandCompoundAssign"), o.expandCompoundAssign);
    settings.setValue(QStringLiteral("keepMainReturn"), !o.omitMainReturn);
    settings.setValue(QStringLiteral("zoom"), fitMode ? 0.0 : chart->zoom());
    settings.endGroup();
}

void ImportDialogPrivate::scheduleUpdate(bool sourceChanged)
{
    if (sourceChanged)
        sourceDirty = true;
    updateTimer.start();
    updateButtons();
}

void ImportDialogPrivate::parse()
{
    sourceDirty = false;
    const QString code = editor->toPlainText();
    importer.setOptions(q->options());
    functions.clear();
    if (!code.trimmed().isEmpty() && importer.parse(code, fileName))
        functions = importer.functions();

    // keep the selected function if it still exists
    QString keep = wantedFunction;
    if (keep.isEmpty() && functionCombo->currentIndex() >= 0)
        keep = functionCombo->currentData(Qt::UserRole + 1).toString();
    int select = -1;
    for (int i = 0; i < functions.size() && select < 0; ++i)
        if (!keep.isEmpty() && functions.at(i).name == keep)
            select = i;
    if (select < 0 && !wantedFunction.isEmpty()) {
        // "bar" selects "Foo::bar"
        for (int i = 0; i < functions.size() && select < 0; ++i)
            if (functions.at(i).name.endsWith(QLatin1String("::") + wantedFunction))
                select = i;
    }
    if (select >= 0 && functions.at(select).name == wantedFunction)
        wantedFunction.clear();
    if (select < 0)
        select = functions.isEmpty() ? -1 : qMax(0, importer.defaultFunctionIndex());

    updatingFunctions = true;
    functionCombo->clear();
    for (int i = 0; i < functions.size(); ++i) {
        const afce::FunctionInfo &f = functions.at(i);
        QString text;
        if (f.name.isEmpty())
            text = ImportDialog::tr("(whole fragment)");
        else
            text = f.name + QLatin1Char('(') + f.parameters + QLatin1Char(')');
        functionCombo->addItem(text, i);
        functionCombo->setItemData(i, f.name, Qt::UserRole + 1);
        QString tip = f.name.isEmpty() ? ImportDialog::tr("The code does not contain function definitions: "
                                                          "all statements are imported as one flowchart")
                                       : (f.returnType.isEmpty() ? QString() : f.returnType + QLatin1Char(' '))
                                             + f.name + QLatin1Char('(') + f.parameters + QLatin1Char(')');
        if (f.line > 0)
            tip += QLatin1Char('\n') + ImportDialog::tr("Line %1").arg(f.line);
        functionCombo->setItemData(i, tip, Qt::ToolTipRole);
    }
    functionCombo->setCurrentIndex(select);
    functionCombo->setEnabled(functions.size() > 1);
    updatingFunctions = false;

    // information line
    if (code.trimmed().isEmpty()) {
        infoLabel->clear();
    } else if (functions.isEmpty()) {
        infoLabel->setText(ImportDialog::tr("No functions found"));
    } else if (functions.size() == 1 && functions.first().name.isEmpty()) {
        infoLabel->setText(ImportDialog::tr("Code fragment"));
    } else {
        infoLabel->setText(functions.size() == 1 ? ImportDialog::tr("One function found")
                                                 : ImportDialog::tr("%n functions found", "", int(functions.size())));
    }
}

int ImportDialogPrivate::currentFunctionIndex() const
{
    if (functionCombo->currentIndex() < 0)
        return -1;
    const int index = functionCombo->currentData().toInt();
    return index >= 0 && index < functions.size() ? index : -1;
}

QString ImportDialogPrivate::functionTitle(int index) const
{
    const QString name = index >= 0 && index < functions.size() ? functions.at(index).name : QString();
    return name.isEmpty() ? ImportDialog::tr("Fragment") : name;
}

void ImportDialogPrivate::showEmptyState(const QString &title, const QString &hint)
{
    emptyTitle->setText(title);
    emptyHint->setText(hint);
    emptyHint->setVisible(!hint.isEmpty());
    previewStack->setCurrentIndex(0);
    previewValid = false;
    for (QWidget *w : std::initializer_list<QWidget *>{zoomInButton, zoomOutButton, zoomSlider, fitButton, zoomLabel})
        w->setEnabled(false);
}

void ImportDialogPrivate::rebuildFlowchart()
{
    const int index = currentFunctionIndex();
    if (index < 0) {
        chart->clear();
        if (editor->toPlainText().trimmed().isEmpty()) {
            showEmptyState(ImportDialog::tr("Paste C or C++ code here or open a file"),
                           ImportDialog::tr("You can also drop a .c or .cpp file onto this window. "
                                            "The flowchart preview appears here."));
            showDiagnostics({});
        } else {
            showEmptyState(ImportDialog::tr("Nothing to import"),
                           ImportDialog::tr("No function definitions or statements were found in the code."));
            showDiagnostics(importer.diagnostics());
        }
        return;
    }
    importer.setOptions(q->options());
    const QDomDocument doc = importer.flowchart(index);
    QString error;
    if (doc.isNull() || !chart->loadDocument(doc, &error)) {
        chart->clear();
        showEmptyState(ImportDialog::tr("The flowchart cannot be built"), error);
        showDiagnostics(importer.diagnostics());
        return;
    }
    chart->setStatus(QFlowChart::Display);
    previewStack->setCurrentIndex(1);
    previewValid = true;
    for (QWidget *w : std::initializer_list<QWidget *>{zoomInButton, zoomOutButton, zoomSlider, fitButton, zoomLabel})
        w->setEnabled(true);
    if (fitMode)
        applyFitZoom();
    showDiagnostics(importer.diagnostics());
}

void ImportDialogPrivate::showDiagnostics(const QList<afce::Diagnostic> &diagnostics)
{
    diagnosticsList->clear();
    QList<CodeEditor::LineMarker> markers;
    int errors = 0;
    int warnings = 0;
    const QPalette pal = q->palette();
    for (const afce::Diagnostic &diag : diagnostics) {
        const QString text = diag.line > 0 ? ImportDialog::tr("Line %1: %2").arg(diag.line).arg(diag.message)
                                           : diag.message;
        QListWidgetItem *item = new QListWidgetItem(severityIcon(diag.severity, pal), text);
        item->setData(Qt::UserRole, diag.line);
        item->setData(Qt::UserRole + 1, int(diag.severity));
        item->setToolTip(text);
        diagnosticsList->addItem(item);
        if (diag.severity == afce::Diagnostic::Error)
            ++errors;
        else if (diag.severity == afce::Diagnostic::Warning)
            ++warnings;
        if (diag.line > 0) {
            CodeEditor::LineMarker marker;
            marker.line = diag.line;
            marker.severity = diag.severity == afce::Diagnostic::Error ? CodeEditor::LineMarker::Error
                            : diag.severity == afce::Diagnostic::Warning ? CodeEditor::LineMarker::Warning
                                                                         : CodeEditor::LineMarker::Info;
            marker.toolTip = diag.message;
            markers.append(marker);
        }
    }
    editor->setLineMarkers(markers);
    if (diagnostics.isEmpty()) {
        if (previewValid) {
            QListWidgetItem *item = new QListWidgetItem(ImportDialog::tr("No problems: every statement was converted."));
            item->setFlags(Qt::NoItemFlags);
            item->setData(Qt::UserRole, 0);
            diagnosticsList->addItem(item);
        }
        diagnosticsHeader->setText(ImportDialog::tr("Messages"));
    } else {
        QStringList parts;
        if (errors > 0)
            parts << (errors == 1 ? ImportDialog::tr("1 error") : ImportDialog::tr("%n errors", "", errors));
        if (warnings > 0)
            parts << (warnings == 1 ? ImportDialog::tr("1 warning") : ImportDialog::tr("%n warnings", "", warnings));
        const int infos = int(diagnostics.size()) - errors - warnings;
        if (infos > 0)
            parts << (infos == 1 ? ImportDialog::tr("1 note") : ImportDialog::tr("%n notes", "", infos));
        diagnosticsHeader->setText(ImportDialog::tr("Messages (%1)").arg(parts.join(QStringLiteral(", "))));
    }
}

void ImportDialogPrivate::updateButtons()
{
    const bool pending = sourceDirty || updateTimer.isActive();
    const bool hasFunctions = !functions.isEmpty();
    importButton->setEnabled(hasFunctions || (pending && !editor->toPlainText().trimmed().isEmpty()));
    importAllButton->setEnabled(functions.size() > 1);
    importAllButton->setToolTip(functions.size() > 1
                                    ? ImportDialog::tr("Import every function as a separate flowchart (%n functions)", "",
                                                       int(functions.size()))
                                    : ImportDialog::tr("Import every function as a separate flowchart"));
}

void ImportDialogPrivate::updateTitle()
{
    QString title = ImportDialog::tr("Import from Source Code");
    if (!fileName.isEmpty())
        title += QStringLiteral(" — ") + QFileInfo(fileName).fileName();
    q->setWindowTitle(title);
}

void ImportDialogPrivate::setZoom(double zoom, bool fromUser)
{
    zoom = qBound(kMinZoomPercent / 100.0, zoom, kMaxZoomPercent / 100.0);
    if (fromUser && fitMode) {
        fitMode = false;
        const QSignalBlocker blocker(fitButton);
        fitButton->setChecked(false);
    }
    // keep the centre of the visible part in place
    QScrollBar *h = previewArea->horizontalScrollBar();
    QScrollBar *v = previewArea->verticalScrollBar();
    const double old = chart->zoom();
    const double cx = (h->value() + previewArea->viewport()->width() / 2.0) / old;
    const double cy = (v->value() + previewArea->viewport()->height() / 2.0) / old;
    if (!qFuzzyCompare(old, zoom))
        chart->setZoom(zoom);
    h->setValue(qRound(cx * zoom - previewArea->viewport()->width() / 2.0));
    v->setValue(qRound(cy * zoom - previewArea->viewport()->height() / 2.0));
    updatingZoom = true;
    zoomSlider->setValue(qRound(zoom * 100));
    updatingZoom = false;
    zoomLabel->setText(QStringLiteral("%1%").arg(qRound(zoom * 100)));
}

void ImportDialogPrivate::applyFitZoom()
{
    if (!previewValid || !chart->root())
        return;
    const double current = chart->zoom();
    const double width = chart->root()->width / current;
    const double height = chart->root()->height / current;
    if (width <= 0 || height <= 0)
        return;
    const QSize viewport = previewArea->maximumViewportSize() - QSize(8, 8);
    const double fitWidth = viewport.width() / width;
    const double fitAll = qMin(fitWidth, viewport.height() / height);
    // Show the whole chart when its text stays readable, otherwise fit the width and scroll.
    double zoom = fitAll >= 0.6 ? fitAll : fitWidth;
    zoom = qBound(0.35, qMin(zoom, 1.0), 1.0);
    zoom = std::floor(zoom * 100) / 100.0;
    setZoom(zoom, false);
}

void ImportDialogPrivate::applyPalette()
{
    chart->setChartStyle(QFlowChartStyle::uiStyle(q->palette()));
    highlighter->setPalette(editor->palette());
    if (previewValid || diagnosticsList->count() > 0) {
        // recolour the severity icons
        const QPalette pal = q->palette();
        for (int i = 0; i < diagnosticsList->count(); ++i) {
            QListWidgetItem *item = diagnosticsList->item(i);
            if (item->flags() != Qt::NoItemFlags)
                item->setIcon(severityIcon(afce::Diagnostic::Severity(item->data(Qt::UserRole + 1).toInt()), pal));
        }
    }
}

void ImportDialogPrivate::reportError(const QString &message)
{
    lastError = message;
    if (!gQuietMode)
        QMessageBox::warning(q, ImportDialog::tr("Import from Source Code"), message);
}

void ImportDialogPrivate::openFileDialog()
{
    QString dir = lastDirectory;
    if (!fileName.isEmpty())
        dir = QFileInfo(fileName).absolutePath();
    const QString filters = ImportDialog::tr("C/C++ sources (*.c *.cpp *.cc *.cxx *.c++ *.h *.hpp *.hh *.hxx *.inl *.ino)")
                          + QStringLiteral(";;") + ImportDialog::tr("C sources (*.c *.h)") + QStringLiteral(";;")
                          + ImportDialog::tr("C++ sources (*.cpp *.cc *.cxx *.c++ *.h *.hpp *.hh *.hxx)")
                          + QStringLiteral(";;") + ImportDialog::tr("All files (*)");
    const QString file = QFileDialog::getOpenFileName(q, ImportDialog::tr("Open Source File"), dir, filters);
    if (!file.isEmpty())
        q->loadFile(file);
}

void ImportDialogPrivate::setOptionsVisible(bool visible)
{
    optionsPanel->setVisible(visible);
    optionsToggle->setArrowType(visible ? Qt::DownArrow : Qt::RightArrow);
}

void ImportDialogPrivate::importFunctions(bool all)
{
    if (sourceDirty || updateTimer.isActive())
        q->updatePreview();
    documents.clear();
    titles.clear();
    QList<int> indexes;
    if (all) {
        for (int i = 0; i < functions.size(); ++i)
            indexes.append(i);
    } else if (currentFunctionIndex() >= 0) {
        indexes.append(currentFunctionIndex());
    }
    importer.setOptions(q->options());
    for (int index : std::as_const(indexes)) {
        const QDomDocument doc = importer.flowchart(index);
        if (doc.isNull() || !QFlowChart::isValidDocument(doc))
            continue;
        documents.append(doc);
        titles.append(functionTitle(index));
    }
    if (documents.isEmpty()) {
        QApplication::beep();
        return;
    }
    q->accept();
}

// ---------------------------------------------------------------------------

ImportDialog::ImportDialog(QWidget *parent) : QDialog(parent), d(new ImportDialogPrivate(this))
{
    d->setupUi();
    d->readSettings();
    updatePreview();
}

ImportDialog::~ImportDialog()
{
    // Destroy the widgets while d is still valid: they may emit signals
    // (text changed, current index changed, ...) or send filtered events.
    d->updateTimer.stop();
    d->previewArea->viewport()->removeEventFilter(this);
    const QList<QWidget *> widgets = findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *w : widgets)
        w->blockSignals(true);
    qDeleteAll(widgets);
    delete d;
    d = nullptr;
}

void ImportDialog::setSource(const QString &code, const QString &fileName)
{
    d->fileName = fileName;
    d->editor->setPlainText(code);
    d->editor->moveCursor(QTextCursor::Start);
    d->updateTitle();
    if (!fileName.isEmpty()) {
        const QString suffix = QFileInfo(fileName).suffix().toLower();
        const bool isC = suffix == QLatin1String("c");
        if (d->languageCombo->currentData().toInt() == int(afce::ImportOptions::Language::Auto))
            d->highlighter->setLanguage(isC ? QStringLiteral("c") : QStringLiteral("cpp"));
    }
    d->sourceDirty = true;
    updatePreview();
}

bool ImportDialog::loadFile(const QString &fileName)
{
    QFile file(fileName);
    const QString shown = QDir::toNativeSeparators(fileName);
    if (!QFileInfo(fileName).isFile()) {
        d->reportError(tr("The file %1 does not exist.").arg(shown));
        return false;
    }
    if (file.size() > kMaxFileSize) {
        d->reportError(tr("The file %1 is too large for a source file (%2 MB).")
                           .arg(shown).arg(file.size() / (1024.0 * 1024.0), 0, 'f', 1));
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        d->reportError(tr("Cannot open the file %1:\n%2").arg(shown, file.errorString()));
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    // binary files (images, executables, ...) contain zero bytes; UTF-16/32 files have a BOM
    const bool hasBom = data.startsWith("\xff\xfe") || data.startsWith("\xfe\xff");
    if (!hasBom && data.left(8192).contains('\0')) {
        d->reportError(tr("The file %1 is not a text file.").arg(shown));
        return false;
    }
    d->lastError.clear();
    d->lastDirectory = QFileInfo(fileName).absolutePath();
    setSource(decodeSource(data), fileName);
    return true;
}

void ImportDialog::selectFunction(const QString &name)
{
    if (name.isEmpty())
        return;
    if (d->sourceDirty || d->updateTimer.isActive())
        updatePreview();
    int found = -1;
    for (int i = 0; i < d->functions.size() && found < 0; ++i)
        if (d->functions.at(i).name == name)
            found = i;
    for (int i = 0; i < d->functions.size() && found < 0; ++i)
        if (d->functions.at(i).name.endsWith(QLatin1String("::") + name))
            found = i;
    if (found < 0) {
        // remembered for the next parse (e.g. selectFunction() before setSource())
        d->wantedFunction = name;
        return;
    }
    d->wantedFunction.clear();
    const int comboIndex = d->functionCombo->findData(found);
    if (comboIndex >= 0 && comboIndex != d->functionCombo->currentIndex()) {
        d->functionCombo->setCurrentIndex(comboIndex);
        updatePreview();
    }
}

QList<QDomDocument> ImportDialog::importedDocuments() const
{
    return d->documents;
}

QStringList ImportDialog::importedTitles() const
{
    return d->titles;
}

QString ImportDialog::source() const
{
    return d->editor->toPlainText();
}

QString ImportDialog::fileName() const
{
    return d->fileName;
}

afce::ImportOptions ImportDialog::options() const
{
    afce::ImportOptions o;
    o.language = afce::ImportOptions::Language(d->languageCombo->currentData().toInt());
    o.forStyle = afce::ImportOptions::ForStyle(d->forStyleCombo->currentData().toInt());
    o.keepDeclarations = d->keepDeclarations->isChecked();
    o.detectIO = d->detectIO->isChecked();
    o.callsAsSubroutine = d->callsAsSubroutine->isChecked();
    o.expandCompoundAssign = d->expandCompoundAssign->isChecked();
    o.omitMainReturn = !d->keepMainReturn->isChecked();
    return o;
}

void ImportDialog::setOptions(const afce::ImportOptions &options)
{
    const afce::ImportOptions old = this->options();
    d->updatingOptions = true;
    const int language = d->languageCombo->findData(int(options.language));
    if (language >= 0)
        d->languageCombo->setCurrentIndex(language);
    const int forStyle = d->forStyleCombo->findData(int(options.forStyle));
    if (forStyle >= 0)
        d->forStyleCombo->setCurrentIndex(forStyle);
    d->keepDeclarations->setChecked(options.keepDeclarations);
    d->detectIO->setChecked(options.detectIO);
    d->callsAsSubroutine->setChecked(options.callsAsSubroutine);
    d->expandCompoundAssign->setChecked(options.expandCompoundAssign);
    d->keepMainReturn->setChecked(!options.omitMainReturn);
    d->updatingOptions = false;
    d->scheduleUpdate(old.language != options.language);
}

QString ImportDialog::currentFunction() const
{
    const int index = d->currentFunctionIndex();
    return index >= 0 ? d->functions.at(index).name : QString();
}

void ImportDialog::updatePreview()
{
    d->updateTimer.stop();
    if (d->sourceDirty)
        d->parse();
    d->rebuildFlowchart();
    d->updateButtons();
    emit previewUpdated();
}

QString ImportDialog::lastError() const
{
    return d->lastError;
}

QString ImportDialog::decodeSource(const QByteArray &data)
{
    // byte order marks (UTF-8, UTF-16, UTF-32)
    if (const std::optional<QStringConverter::Encoding> bom = QStringConverter::encodingForData(data)) {
        QStringDecoder decoder(*bom);
        return decoder.decode(data);
    }
    QStringDecoder utf8(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    const QString text = utf8.decode(data);
    if (!utf8.hasError())
        return text;
    // Not UTF-8: an 8-bit code page. Cyrillic text in Windows-1251 consists mostly of
    // letters in 0xC0..0xFF (and Ё/ё = 0xA8/0xB8); Latin-1 accented letters are rarer
    // and mixed with plain ASCII letters inside words.
    int cyrillic = 0;
    int other = 0;
    for (const char ch : data) {
        const uchar c = uchar(ch);
        if (c >= 0xC0 || c == 0xA8 || c == 0xB8)
            ++cyrillic;
        else if (c >= 0x80)
            ++other;
    }
    int mixedWords = 0; // ASCII letter directly next to a high byte: typical for Latin-1 (e.g. "Größe")
    for (int i = 1; i < data.size(); ++i) {
        const uchar a = uchar(data.at(i - 1));
        const uchar b = uchar(data.at(i));
        if ((a >= 0xC0 && ((b | 0x20) >= 'a' && (b | 0x20) <= 'z'))
                || (b >= 0xC0 && ((a | 0x20) >= 'a' && (a | 0x20) <= 'z')))
            ++mixedWords;
    }
    if (cyrillic > 0 && cyrillic >= 2 * other && mixedWords * 2 < cyrillic)
        return decodeCp1251(data);
    return QString::fromLatin1(data);
}

void ImportDialog::setQuietMode(bool quiet)
{
    gQuietMode = quiet;
}

void ImportDialog::done(int result)
{
    d->updateTimer.stop();
    d->writeSettings();
    if (result != QDialog::Accepted) {
        d->documents.clear();
        d->titles.clear();
    }
    QDialog::done(result);
}

void ImportDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (d && d->chart && d->highlighter
            && (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange))
        d->applyPalette();
}

void ImportDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (!droppedSourceFile(event->mimeData()).isEmpty())
        event->acceptProposedAction();
    else
        QDialog::dragEnterEvent(event);
}

void ImportDialog::dragMoveEvent(QDragMoveEvent *event)
{
    if (!droppedSourceFile(event->mimeData()).isEmpty())
        event->acceptProposedAction();
    else
        QDialog::dragMoveEvent(event);
}

void ImportDialog::dropEvent(QDropEvent *event)
{
    const QString file = droppedSourceFile(event->mimeData());
    if (file.isEmpty()) {
        QDialog::dropEvent(event);
        return;
    }
    event->acceptProposedAction();
    loadFile(file);
}

bool ImportDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (d && d->previewArea && watched == d->previewArea->viewport()) {
        if (event->type() == QEvent::Resize && d->fitMode) {
            // after the layout has settled
            QTimer::singleShot(0, this, [this]() {
                if (d->fitMode)
                    d->applyFitZoom();
            });
        } else if (event->type() == QEvent::Wheel) {
            QWheelEvent *wheel = static_cast<QWheelEvent *>(event);
            if (wheel->modifiers() & Qt::ControlModifier) {
                const int delta = wheel->angleDelta().y();
                if (delta != 0 && d->previewValid)
                    d->setZoom(d->chart->zoom() * std::pow(1.2, delta / 120.0), true);
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}
