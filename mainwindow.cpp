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
#include "ui_mainwindow.h"
#include "zvflowchart.h"
#include <QtGui>
#include <QtSvg>
#include <QSettings>
#include "qflowchartstyle.h"
#include "sourcecodegenerator.h"
#include <QWidgetList>

#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>

QString afceVersion()
{
    return PROGRAM_VERSION;
}




MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags), fDocument(0),
      ui(new Ui::MainWindow)
{

#if defined(Q_WS_X11) or defined(Q_OS_LINUX)
    QDir::setSearchPaths("generators", QStringList() << QString(PROGRAM_DATA_DIR) + "generators");
#else
    QDir::setSearchPaths("generators", QStringList() << qApp->applicationDirPath() + "/generators");
#endif

    ui->setupUi(this);
    setupUi();
    readSettings();
    retranslateUi();

    QFlowChart *fc = new QFlowChart(this);
    setDocument(fc);
    document()->setZoom(1);
    connect(document(), SIGNAL(statusChanged()), this, SLOT(slotStatusChanged()));
    connect(document(), SIGNAL(editBlock(QBlock *)), this, SLOT(slotEditBlock(QBlock *)));
    connect(ui->actUndo, SIGNAL(triggered()), document(), SLOT(undo()));
    connect(ui->actRedo, SIGNAL(triggered()), document(), SLOT(redo()));
    connect(document(), SIGNAL(changed()), this, SLOT(updateActions()));
    connect(document(), SIGNAL(changed()), this, SLOT(generateCode()));
    document()->setStatus(QFlowChart::Selectable);
    connect(saScheme, SIGNAL(mouseDown()), document(), SLOT(deselectAll()));
    connect(codeLanguage, SIGNAL(activated(int)), this, SLOT(codeLangChanged(int)));

    QFlowChartStyle st;
    QPalette pal = palette();
    st.setLineWidth(2);
    st.setNormalBackground(pal.color(QPalette::Base));
    st.setNormalForeground(pal.color(QPalette::WindowText));
    st.setNormalMarker(Qt::red);
    st.setSelectedBackground(pal.color(QPalette::Highlight));
    st.setSelectedForeground(pal.color(QPalette::HighlightedText));
    st.setNormalMarker(Qt::green);
    st.setFontSize(10);
    document()->setChartStyle(st);

    labelMenu = new QLabel(statusBar());

    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(labelMenu);
    labelMenu->setAlignment(Qt::AlignCenter);


    isSaved = true; //Let's allow to close application if no modification were made in empty document
    connect(document(), SIGNAL(modified()), SLOT(slotDocumentChanged()));
    connect(this, SIGNAL(documentLoaded()), SLOT(slotDocumentLoaded()));
    connect(this, SIGNAL(documentSaved()), SLOT(slotDocumentSaved()));

    if (qApp->arguments().size() > 1) {
        QFile test(qApp->arguments().at(1));
        if(test.exists()) {
            slotOpenDocument(qApp->arguments().at(1));
        }
        else {
            QMessageBox::critical(this, tr("Failed to open a file"), tr("Unable to open file '%1'.").arg(qApp->arguments().at(1)));
        }
    }
}


void MainWindow::writeSettings()
{
    QSettings settings("afce", "application");

    settings.setValue("geometry", geometry());
    settings.setValue("windowState", saveState());
}
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (okToContinue()) {
        writeSettings();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::readSettings()

{
    QSettings settings("afce", "application");

    setGeometry(settings.value("geometry", QRect(100, 100, 800, 600)).toRect());
    restoreState(settings.value("windowState").toByteArray());

}
void MainWindow::setupUi()
{
    QApplication::setWindowIcon(QIcon(":/images/icon.png"));
    createActions();
    createMenu();
    createToolBar();
    QWidget *body = new QWidget;
    QWidget *zoomPanel = new QWidget;
    zoomPanel->setMinimumHeight(18);
    saScheme = new AfcScrollArea();
    QPalette pal = saScheme->palette();
    pal.setColor(QPalette::Window, pal.color(QPalette::Base));
    saScheme->setPalette(pal);

    setCentralWidget(body);
    QVBoxLayout *bodyLayout = new QVBoxLayout;
    bodyLayout->addWidget(saScheme);
    bodyLayout->addWidget(zoomPanel);
    body->setLayout(bodyLayout);
    zoomSlider = new QSlider(Qt::Horizontal, zoomPanel);
    zoomLabel = new QLabel;
    QHBoxLayout *zoomLayout= new QHBoxLayout;
    zoomLayout->addStretch();
    zoomLayout->addWidget(zoomLabel);
    zoomLayout->addWidget(zoomSlider);
    zoomPanel->setLayout(zoomLayout);
    zoomSlider->setRange(1, 20);
    zoomSlider->setSingleStep(1);
    zoomSlider->setPageStep(10);
    connect(zoomSlider, SIGNAL(valueChanged(int)),this, SLOT(setZoom(int)));
    zoomSlider->setValue(4);
    connect(saScheme, SIGNAL(zoomStepped(int)), this, SLOT(shiftZoom(int)));

    createToolbox();

    dockCode = new QDockWidget(this);
    dockCode->setObjectName("dock_code");
    dockCode->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::RightDockWidgetArea, dockCode);
    codeWidget = new QFrame;
    codeLanguage = new QComboBox;
    codeText = new QTextEdit;
    codeLabel = new QLabel;
    ui->actCode->setCheckable(true);
    ui->actCode->setChecked(dockCode->isVisible());
    connect(ui->actCode, SIGNAL(triggered(bool)), dockCode, SLOT(setVisible(bool)));
    connect(dockCode, SIGNAL(visibilityChanged(bool)), ui->actCode, SLOT(setChecked(bool)));

    //connect(dockCode, SIGNAL(visibilityChanged(bool)), this, SLOT(docCodeVisibilityChanged(bool)));

    codeWidget->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);

    codeLabel->setBuddy(codeLanguage);

    codeText->setFont(QFont("Courier New", 12));
    codeText->setLineWrapMode(QTextEdit::NoWrap);
    codeText->setReadOnly(true);


    QVBoxLayout * vbl = new QVBoxLayout;
    vbl->addWidget(codeLabel);
    vbl->addWidget(codeLanguage);
    vbl->addWidget(codeText);
    codeWidget->setLayout(vbl);
    dockCode->setWidget(codeWidget);

    helpWindow = new THelpWindow();
    helpWindow->setObjectName("help_window");
    helpWindow->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::RightDockWidgetArea, helpWindow);
    helpWindow->hide();

    ui->actHelp->setCheckable(true);
    ui->actHelp->setChecked(helpWindow->isVisible());

    connect(ui->actHelp, SIGNAL(triggered(bool)), helpWindow, SLOT(setVisible(bool)));
    connect(helpWindow, SIGNAL(visibilityChanged(bool)), ui->actHelp, SLOT(setChecked(bool)));


}

QToolButton * createToolButton(const QString & fileName)
{
    QToolButton *Result = new QToolButton;
    Result->setIconSize(QSize(32, 32));
    Result->setIcon(QIcon(fileName));
    Result->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Result->setAutoRaise(true);
    Result->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    Result->setCheckable(true);
    Result->setAutoExclusive(true);
    return Result;
}

void MainWindow::createToolbox()
{
    dockTools = new QDockWidget(this);
    dockTools->setObjectName("dock_tools");
    dockTools->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dockTools->setMinimumWidth(150);
    addDockWidget(Qt::LeftDockWidgetArea, dockTools);
    connect(dockTools, SIGNAL(visibilityChanged(bool)), this, SLOT(docToolsVisibilityChanged(bool)));

    tbArrow = createToolButton(":/images/arrow.png");
    tbArrow->setChecked(true);
    tbProcess = createToolButton(":/images/simple.png");
    tbAssign = createToolButton(":/images/assign.png");
    tbIf = createToolButton(":/images/if.png");
    tbFor = createToolButton(":/images/for.png");
    tbWhilePre = createToolButton(":/images/while.png");
    tbWhilePost = createToolButton(":/images/until.png");
    tbIo = createToolButton(":/images/io.png");
    tbOu = createToolButton(":/images/ou.png");
    tbForCStyle = createToolButton(":/images/forc.png");

    connect(tbArrow, SIGNAL(pressed()), this, SLOT(slotToolArrow()));
    connect(tbProcess, SIGNAL(pressed()), this, SLOT(slotToolProcess()));
    connect(tbAssign, SIGNAL(pressed()), this, SLOT(slotToolAssign()));
    connect(tbIf, SIGNAL(pressed()), this, SLOT(slotToolIf()));
    connect(tbFor, SIGNAL(pressed()), this, SLOT(slotToolFor()));
    connect(tbWhilePre, SIGNAL(pressed()), this, SLOT(slotToolWhilePre()));
    connect(tbWhilePost, SIGNAL(pressed()), this, SLOT(slotToolWhilePost()));
    connect(tbIo, SIGNAL(pressed()), this, SLOT(slotToolIo()));
    connect(tbOu, SIGNAL(pressed()), this, SLOT(slotToolOu()));
    connect(tbForCStyle, SIGNAL(pressed()), this, SLOT(slotToolForCStyle()));

    toolsWidget = new QFrame;
    toolsWidget->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    QVBoxLayout *tl = new QVBoxLayout;
    tl->setSpacing(2);
    tl->addWidget(tbArrow);
    tl->addWidget(tbIo);
    tl->addWidget(tbOu);
    tl->addWidget(tbProcess);
    tl->addWidget(tbAssign);
    tl->addWidget(tbIf);
    tl->addWidget(tbFor);
    tl->addWidget(tbWhilePre);
    tl->addWidget(tbWhilePost);
    tl->addWidget(tbForCStyle);
    tl->addStretch();
    toolsWidget->setLayout(tl);
    dockTools->setWidget(toolsWidget);

    ui->actTools->setCheckable(true);
    ui->actTools->setChecked(dockTools->isVisible());
    connect(ui->actTools, SIGNAL(triggered(bool)), dockTools, SLOT(setVisible(bool)));
    connect(dockTools, SIGNAL(visibilityChanged(bool)), ui->actTools, SLOT(setChecked(bool)));


}

void MainWindow::retranslateUi()
{
    ui->retranslateUi(this);
    dockTools->setWindowTitle(tr("Tools"));
    tbArrow->setText(tr("Select"));
    tbProcess->setText(tr("Process"));
    tbAssign->setText(tr("Assign"));
    tbIf->setText(tr("If...then...else"));
    tbFor->setText(tr("FOR loop"));
    tbWhilePre->setText(tr("loop with pre-condition"));
    tbWhilePost->setText(tr("loop with post-condition"));
    tbIo->setText(tr("Input"));
    tbOu->setText(tr("Output"));
    tbForCStyle->setText(tr("FOR loop (C/C++)"));
//    actExit->setText(tr("E&xit"));
//    actExit->setStatusTip(tr("Exit from program"));
//    actOpen->setText(tr("&Open..."));
//    actOpen->setStatusTip(tr("Open saved file"));
//    actSave->setText(tr("&Save"));
//    actSave->setStatusTip(tr("Save changes"));
//    actSaveAs->setText(tr("Save &as..."));
//    actSaveAs->setStatusTip(tr("Save changes in a new file"));
//    actExport->setText(tr("&Export to raster..."));
//    actExport->setStatusTip(tr("Save the flowchart in a raster picture format"));
//    actExportSVG->setText(tr("&Export to SVG..."));
//    actExportSVG->setStatusTip(tr("Save the flowchart in a vector picture format"));
//    actPrint->setText(tr("&Print..."));
//    actPrint->setStatusTip(tr("To print"));
//    actNew->setText(tr("&New"));
//    actNew->setStatusTip(tr("Create a new project"));
//    actUndo->setText(tr("&Undo"));
//    actUndo->setStatusTip(tr("Undo the last operation"));
//    actRedo->setText(tr("&Redo"));
//    actRedo->setStatusTip(tr("Restore the last undone action"));
//    actCut->setText(tr("Cu&t"));
//    actCut->setStatusTip(tr("Cut the current selection"));
//    actCopy->setText(tr("&Copy"));
//    actCopy->setStatusTip(tr("Copy the current selection"));
//    actPaste->setText(tr("&Paste"));
//    actPaste->setStatusTip(tr("Paste"));
//    actDelete->setText(tr("&Delete"));
//    actDelete->setStatusTip(tr("Delete the current selection"));
//    actHelp->setText(tr("&Help"));
//    actHelp->setStatusTip(tr("Toggle Help window"));
//    actAbout->setText(tr("&About"));
//    actAbout->setStatusTip(tr("Information about authors"));
//    actAboutQt->setText(tr("About &Qt"));
//    actAboutQt->setStatusTip(tr("Information about Qt"));
//    actTools->setText(tr("&Tools"));
//    actTools->setStatusTip(tr("Toggle the tool panel"));
//    actCode->setText(tr("&Source code"));
//    actCode->setStatusTip(tr("Toggle the source code panel"));



//    actExit->setShortcut(tr("Alt+X"));
//    actOpen->setShortcut(tr("Ctrl+O"));
//    actSave->setShortcut(tr("Ctrl+S"));
//    actNew->setShortcut(tr("Ctrl+N"));
//    actUndo->setShortcut(tr("Ctrl+Z"));
//    actRedo->setShortcut(tr("Ctrl+Y"));
//    actCut->setShortcut(tr("Ctrl+X"));
//    actCopy->setShortcut(tr("Ctrl+C"));
//    actPaste->setShortcut(tr("Ctrl+V"));
//    actDelete->setShortcut(tr("Del"));
//    actHelp->setShortcut(tr("F1"));
//    actPrint->setShortcut(tr("Ctrl+P"));
//    actTools->setShortcut(tr("F2"));
//    actCode->setShortcut(tr("F3"));

    menuFile->setTitle(tr("&File"));
    menuEdit->setTitle(tr("&Edit"));
    menuHelp->setTitle(tr("&Help"));
    menuWindow->setTitle(tr("&View"));
    menuLanguage->setTitle(tr("&Language"));

    toolBar->setWindowTitle(tr("Standard"));
    dockCode->setWindowTitle(tr("Source code"));
    

    slotReloadGenerators();

    codeLabel->setText(tr("&Select programming language:"));

    if (!fileName.isEmpty())
        setWindowTitle(tr("%1 - Algorithm Flowchart Editor").arg(fileName));
    else
        setWindowTitle(tr("Algorithm Flowchart Editor"));
    helpWindow->setWindowTitle(tr("Help window"));
    helpWindow->textBrowser->setSearchPaths(QStringList() << "./help/"+QLocale().name() << "./help/en_US");
  #if defined(Q_WS_X11) or defined(Q_OS_LINUX)
    helpWindow->textBrowser->setSearchPaths(QStringList() << QString(PROGRAM_DATA_DIR) + "help/"+QLocale().name() << QString(PROGRAM_DATA_DIR) + "help/en_US");
  #endif
    helpWindow->textBrowser->reload();


}

void MainWindow::createMenu()
{
    menuFile = menuBar()->addMenu("");
    menuFile->addAction(ui->actNew);
    menuFile->addAction(ui->actOpen);
    menuFile->addSeparator();
    menuFile->addAction(ui->actSave);
    menuFile->addAction(ui->actSaveAs);
    menuFile->addSeparator();
    menuFile->addAction(ui->actExport);
    menuFile->addAction(ui->actExportSVG);
    menuFile->addSeparator();
    menuFile->addAction(ui->actPrint);
    menuFile->addSeparator();
    menuFile->addSeparator();
    menuFile->addAction(ui->actExit);
    ui->actAbout->isChecked();

    menuEdit = menuBar()->addMenu("");
    menuEdit->addAction(ui->actUndo);
    menuEdit->addAction(ui->actRedo);
    menuEdit->addSeparator();
    menuEdit->addAction(ui->actCut);
    menuEdit->addAction(ui->actCopy);
    menuEdit->addAction(ui->actPaste);
    menuEdit->addSeparator();
    menuEdit->addAction(ui->actDelete);

    menuWindow = menuBar()->addMenu("");
    menuWindow->addAction(ui->actTools);
    menuWindow->addAction(ui->actCode);
    menuWindow->addSeparator();
    menuLanguage = menuWindow->addMenu(tr("&Language"));
    for (int i = 0; i < actLanguages.size(); ++i) {
        menuLanguage->addAction(actLanguages[i]);
    }

    menuHelp = menuBar()->addMenu("");
    menuHelp->addAction(ui->actHelp);
    menuHelp->addSeparator();
    menuHelp->addAction(ui->actAbout);
    menuHelp->addAction(ui->actAboutQt);
}

void MainWindow::createToolBar()
{
    toolBar = addToolBar("");
    toolBar->setObjectName("standard_toolbar");
    toolBar->setIconSize(QSize(32,32));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    toolBar->addAction(ui->actNew);
    toolBar->addAction(ui->actOpen);
    toolBar->addAction(ui->actSave);
    toolBar->addSeparator();
    toolBar->addAction(ui->actUndo);
    toolBar->addAction(ui->actRedo);
    toolBar->addSeparator();
    toolBar->addAction(ui->actCut);
    toolBar->addAction(ui->actCopy);
    toolBar->addAction(ui->actPaste);
    toolBar->addSeparator();
    toolBar->addAction(ui->actHelp);
    toolBar->addSeparator();
    toolBar->addAction(ui->actTools);
    toolBar->addAction(ui->actCode);
}


void MainWindow::docToolsVisibilityChanged(bool visible)
{
    ui->actTools->setChecked(visible);
}

void MainWindow::docCodeVisibilityChanged(bool visible)
{
    ui->actCode->setChecked(visible);
}

void MainWindow::createActions()
{
//    actExit = new QAction(QIcon(":/images/exit.png"), "", this);
//    actOpen = new QAction(QIcon(":/images/open_document_32_h.png"), "", this);
//    actNew = new QAction(QIcon(":/images/new_document_32_h.png"), "", this);
//    actSave = new QAction(QIcon(":/images/save_32_h.png"), "", this);
//    actSaveAs = new QAction(this);
//    actUndo = new QAction(QIcon(":/images/restart-3.png"), "", this);
//    actRedo = new QAction(QIcon(":/images/restart-4.png"), "", this);
//    actCut = new QAction(QIcon(":/images/cut_clipboard_32_h.png"), "", this);
//    actCopy = new QAction(QIcon(":/images/copy_clipboard_32_h.png"), "", this);
//    actPaste = new QAction(QIcon(":/images/paste_clipboard_32_h.png"), "", this);
//    actDelete = new QAction(QIcon(":/images/delete_x_32_h.png"), "", this);
//    actExport = new QAction(this);
//    actExportSVG = new QAction(this);
//    actHelp = new QAction(QIcon(":/images/help-icon.png"), "", this);
//    actAbout = new QAction(this);
//    actAboutQt = new QAction(this);
//    actPrint = new QAction(QIcon(":/images/print_32_h.png"), "", this);
//    actTools = new QAction(QIcon(":/images/toolbar.png"), "", this);
//    actCode = new QAction(QIcon(":/images/source-code.png"), "", this);


    //connect(ui->actExit, SIGNAL(triggered()), this, SLOT(close()));
//    connect(ui->actNew, SIGNAL(triggered()), this, SLOT(on_actNew_triggered()));
//    connect(ui->actOpen, SIGNAL(triggered()), this, SLOT(on_actOpen_triggered()));
//    connect(ui->actSave, SIGNAL(triggered()), this, SLOT(on_actSave_triggered()));
//    connect(ui->actSaveAs, SIGNAL(triggered()), this, SLOT(on_actSaveAs_triggered()));
//    connect(actExport, SIGNAL(triggered()), this, SLOT(on_actExport_triggered()));
//    connect(actExportSVG, SIGNAL(triggered()), this, SLOT(on_actExportSVG_triggered()));
//    connect(actPrint, SIGNAL(triggered()), this, SLOT(on_actPrint_triggered()));
//    connect(actCut, SIGNAL(triggered()), this, SLOT(slotEditCut()));
//    connect(actCopy, SIGNAL(triggered()), this, SLOT(slotEditCopy()));
//    connect(actPaste, SIGNAL(triggered()), this, SLOT(slotEditPaste()));
//    connect(actDelete, SIGNAL(triggered()), this, SLOT(slotEditDelete()));

//    connect(actHelp, SIGNAL(triggered()), this, SLOT(slotHelpHelp()));
//    connect(actAbout, SIGNAL(triggered()), this, SLOT(slotHelpAbout()));
//    connect(actAboutQt, SIGNAL(triggered()), this, SLOT(slotHelpAboutQt()));

    /* ... */
//    connect(actTools, SIGNAL(triggered()), this, SLOT(slotTools()));


    QHash<QString, QString> avlLangs = enumLanguages();
    QList<QString> locales = avlLangs.keys();
    for (int i = 0; i < locales.size(); ++i) {
        QAction *act = new QAction(this);
        act->setText(avlLangs[locales[i]]);
        act->setData(locales[i]);
        actLanguages.append(act);
        connect(act, SIGNAL(triggered()), this, SLOT(slotChangeLanguage()));
    }

}


MainWindow::~MainWindow()
{
}

bool MainWindow::okToContinue()
{
    int r;
    if (!isSaved) {
        r = QMessageBox::warning(this,
                                 tr("Afce"), tr("There are unsaved changes. Do you really want to close afce?"),
                                 QMessageBox::Yes | QMessageBox::Default,
                                 QMessageBox::No | QMessageBox::Escape);

        if (r != QMessageBox::Yes) {
            return false;
        }
    }
    return true;
}

void MainWindow::on_actOpen_triggered()
{
    QString fn = QFileDialog::getOpenFileName ( this,
                                                tr("Select a file to open"), "", tr("Algorithm flowcharts (*.afc)"));
    if(!fn.isEmpty())
    {

        if (!isSaved)
            if(QMessageBox::warning(this, tr("Unsaved changes"), tr("You are about to open another document. It will discard all unsaved changes in the current document."),
                                    QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
                return;
        slotOpenDocument(fn);

    }
}

void MainWindow::slotOpenDocument(const QString &fn) {
    emit documentUnloaded();
    fileName = fn;
    setWindowTitle(tr("%1 - Algorithm Flowchart Editor").arg(fileName));
    QFile xml(fileName);
    if (xml.exists())
    {
        xml.open(QIODevice::ReadOnly | QIODevice::Text);
        QDomDocument doc;
        if (doc.setContent(&xml, false))
        {
            document()->root()->setXmlNode(doc.firstChildElement());
            document()->setZoom(1);
        }
    }
    emit documentLoaded();

}

void MainWindow::on_actPrint_triggered()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog pd(&printer, this);
    if (pd.exec() == QDialog::Accepted)
    {
        double oldZoom = document()->zoom();
        document()->setZoom(1);
        document()->setStatus(QFlowChart::Display);
        QBlock *r = document()->root();
        r->adjustSize(1);
        r->adjustPosition(0,0);
        QRect page = printer.pageRect();
        double z =  page.width()  / (double) r->width;
        if (r->height * z > page.height())
        {
            z = page.height() / (double) r->height;
        }
        if (z > (printer.resolution()/96.0)) z = printer.resolution()/96.0;
        document()->setZoom(z);
        r->adjustSize(z);
        r->adjustPosition(0, 0);
        QPainter canvas;
        canvas.begin(&printer);
        document()->paintTo(&canvas);
        canvas.end();
        document()->setZoom(oldZoom);
        document()->setStatus(QFlowChart::Selectable);
    }
}

void MainWindow::on_actNew_triggered()
{
    QProcess::startDetached(QApplication::applicationFilePath());
}

void MainWindow::on_actSave_triggered()
{
    if (fileName.isEmpty())
    {
        on_actSaveAs_triggered();
    }
    else
    {
        QDomDocument doc = document()->document();
        QString xmlString = doc.toString(2);
        QFile xml(fileName);
        xml.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Truncate);
        QTextStream stream(&xml);
        stream.setCodec(QTextCodec::codecForName("utf-8"));
        stream << xmlString;
        xml.close();
        emit documentSaved();
    }
}

void MainWindow::on_actSaveAs_triggered()
{
    QString fn = QFileDialog::getSaveFileName(this, tr("Select a file to save"), "", tr("Algorithm flowcharts (*.afc)"));
    if (!fn.isEmpty())
    {
        if(fn.right(4).toLower() != ".afc") fn += ".afc";
        fileName = fn;
        setWindowTitle(tr("%1 - Algorithm Flowchart Editor").arg(fileName));
        on_actSave_triggered();
    }
}

void MainWindow::slotDocumentSaved() {
    isSaved = true;
}

void MainWindow::slotDocumentChanged() {
    isSaved = false;
}

void MainWindow::slotDocumentLoaded() {
    isSaved = true;
}

void MainWindow::slotChangeLanguage()
{
    QAction * action = (QAction *)sender();
    QString localeName = action->data().toString();
    setApplicationLocale(localeName);
    retranslateUi();

    QSettings settings("afce", "application");
    settings.setValue("locale", localeName);
}

void MainWindow::slotReloadGenerators()
{
    int i = codeLanguage->currentIndex();
    codeLanguage->clear();
    QDir gd("generators:");
    QStringList gens = gd.entryList(QStringList() << "*.json", QDir::Files, QDir::Name);

    for (int g = 0; g < gens.size(); ++g) {
        QFile f(gd.absoluteFilePath(gens[g]));
        QString lang_name = QFileInfo(f.fileName()).baseName();
        if(f.open(QIODevice::ReadOnly|QIODevice::Text)) {
            QJsonDocument json = QJsonDocument::fromJson(f.readAll());
            f.close();
            QJsonObject obj = json.object();
            QString loc = QLocale().name();
            if(obj.contains("name")) {
                QJsonObject jn = obj.value("name").toObject();
                if(jn.contains(loc)) {
                    lang_name = jn.value(loc).toString();
                }
                else if (jn.contains("en_US")) {
                    lang_name = jn.value("en_US").toString();
                }
            }
            codeLanguage->addItem(lang_name, QFileInfo(f.fileName()).baseName());
        }
    }

    if (i!=-1)
        codeLanguage->setCurrentIndex(i);
    else
        codeLanguage->setCurrentIndex(0);

}


void MainWindow::on_actExport_triggered()
{
    QString filter = getWriteFormatFilter();
    QString sf = getFilterFor("png");
    QString fn = QFileDialog::getSaveFileName(this, tr("Select a file to export"), "", filter, &sf);
    if(!fn.isEmpty())
    {
        qDebug() << "Selected filter: " << sf;
        QRegExp rx("\\(\\*([^\\)]+)\\)$");
        QStringList masks;
        if(rx.indexIn(sf)!= -1) {
            // each filter may content several masks splitted by a spaces
            masks = rx.cap(1).split(" ", QString::SkipEmptyParts);

            bool matches = false;
            for(int i = 0; i < masks.size(); ++i) {
                QString ex = masks.at(i).toLower();
                if(fn.toLower().endsWith(ex)) {
                    matches = true;
                    break;
                }
            }

            // if no extension or a wrong extension is added then the correct extension will be appended
            if(!matches && !masks.empty()) {
                fn += masks.first().toLower();
            }
        }
        double oldZoom = document()->zoom();
        document()->setZoom(1);
        document()->setStatus(QFlowChart::Display);
        QBlock *r = document()->root();
        r->adjustSize(1);
        r->adjustPosition(0,0);
        QImage img(r->width, r->height, QImage::Format_ARGB32_Premultiplied);
        img.fill(0);
        QPainter canvas(&img);
        canvas.setRenderHint(QPainter::Antialiasing);
        document()->paintTo(&canvas);
        img.save(fn);
        document()->setZoom(oldZoom);
        document()->setStatus(QFlowChart::Selectable);
    }
}

void MainWindow::on_actExportSVG_triggered()
{
    QString filter = getWriteFormatFilter();
    QString fn = QFileDialog::getSaveFileName(this, tr("Select a file to export"), "", getFilterFor("svg"));
    if(!fn.isEmpty())
    {
        if(fn.right(4).toLower() != ".svg") fn += ".svg";
        double oldZoom = document()->zoom();
        document()->setZoom(1);
        document()->setStatus(QFlowChart::Display);
        QBlock *r = document()->root();
        r->adjustSize(1);
        r->adjustPosition(0,0);
        QSvgGenerator svg;
        svg.setSize(QSize(r->width, r->height));
        svg.setResolution(90);
        svg.setFileName(fn);
        QPainter canvas(&svg);
        canvas.setRenderHint(QPainter::Antialiasing);
        r->paint(&canvas, true);

        document()->setZoom(oldZoom);
        document()->setStatus(QFlowChart::Selectable);
    }
}


void MainWindow::on_actCut_triggered()
{
    on_actCopy_triggered();
    on_actDelete_triggered();
}

void MainWindow::on_actCopy_triggered()
{
    if(document())
    {
        if(document()->activeBlock())
        {
            QDomDocument doc("AFC"); // do not localize!
            QDomElement block = document()->activeBlock()->xmlNode(doc);
            if (document()->activeBlock()->isBranch)
            {
                QDomElement alg = doc.createElement("algorithm");
                alg.appendChild(block);
                doc.appendChild(alg);
            }
            else
            {
                if(block.nodeName() != "algorithm")
                {
                    QDomElement alg = doc.createElement("algorithm");
                    QDomElement branch = doc.createElement("branch");
                    alg.appendChild(branch);
                    branch.appendChild(block);
                    doc.appendChild(alg);
                }
                else
                    doc.appendChild(block);
            }
            QClipboard *clipbrd = QApplication::clipboard();
            clipbrd->setText(doc.toString(2));
            updateActions();
        }
    }
}

void MainWindow::on_actPaste_triggered()
{
    if(document())
    {
        QClipboard *clipbrd = QApplication::clipboard();
        document()->setBuffer(clipbrd->text());
        if(!document()->buffer().isEmpty())
        {
            document()->setStatus(QFlowChart::Insertion);
            document()->setMultiInsert(false);
        }
    }
}
void MainWindow::on_actDelete_triggered()
{
    if(document())
    {
        if(document()->status() == QFlowChart::Selectable)
        {
            document()->deleteActiveBlock();
        }
    }
}


void MainWindow::on_actAbout_triggered()
{
    QDialog dlg;
    QPushButton *ok = new QPushButton(tr("&OK"));
    QLabel *text = new QLabel(tr("<html><h1>AFCE</h1><p>Algorithm Flowchart Editor. Version %0</p><p>Copyright 2008-2014 Viktor Zinkevich. All rights reserved.</p> \
<p>Contributors:  Sergey Ryabenko, Alexey Loginov</p> \
<p>The program is provided AS IS with NO WARRANTY OF ANY KIND,<br> INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND<br> \
FITNESS FOR A PARTICULAR PURPOSE.</p></html>").arg(PROGRAM_VERSION));
    QLabel *ico = new QLabel();
    ico->setPixmap(QPixmap(":/images/icon.png"));
    QGridLayout *layout = new QGridLayout;
    QHBoxLayout *bl = new QHBoxLayout;
    bl->addStretch();
    bl->addWidget(ok);
    layout->addWidget(ico, 0, 0, 2, 1, Qt::AlignTop);
    layout->addWidget(text, 0, 1);
    layout->addLayout(bl, 1, 1);
    dlg.setLayout(layout);
    connect(ok, SIGNAL(clicked()), &dlg, SLOT(accept()));
    dlg.exec();
}

void MainWindow::on_actAboutQt_triggered()
{
    QMessageBox::aboutQt(this);
}

void MainWindow::slotStatusChanged()
{
    if(document())
    {
        if(document()->status() == QFlowChart::Selectable)
        {
            tbArrow->setChecked(true);
        }
    }
}

void MainWindow::updateActions()
{
    if(document())
    {
        if (ui->actTools->isVisible())
        {
            ui->actTools->setEnabled(false);
        }



        ui->actCode->setEnabled(document()->status() != QFlowChart::Insertion);
        ui->actTools->setEnabled(document()->status() != QFlowChart::Insertion);
        ui->actHelp->setEnabled(document()->status() != QFlowChart::Insertion);
        ui->actUndo->setEnabled(document()->canUndo() && document()->status() != QFlowChart::Insertion);
        ui->actRedo->setEnabled(document()->canRedo() && document()->status() != QFlowChart::Insertion);
        ui->actOpen->setEnabled(document()->status() != QFlowChart::Insertion);
        ui->actSave->setEnabled(document()->status() != QFlowChart::Insertion);
        ui->actSaveAs->setEnabled(document()->status() != QFlowChart::Insertion);
        ui->actPrint->setEnabled(document()->status() != QFlowChart::Insertion);
        ui->actExport->setEnabled(document()->status() != QFlowChart::Insertion);
        ui->actExportSVG->setEnabled(document()->status() != QFlowChart::Insertion);
        ui->actCopy->setEnabled(document()->status() == QFlowChart::Selectable && document()->activeBlock());
        ui->actCut->setEnabled(document()->status() == QFlowChart::Selectable && document()->activeBlock());
        ui->actPaste->setEnabled(document()->status() == QFlowChart::Selectable && document()->canPaste());
        ui->actDelete->setEnabled(document()->status() == QFlowChart::Selectable && document()->activeBlock());
    }
    else
    {
        ui->actUndo->setEnabled(false);
        ui->actRedo->setEnabled(false);
        ui->actOpen->setEnabled(false);
        ui->actSave->setEnabled(false);
        ui->actSaveAs->setEnabled(false);
        ui->actPrint->setEnabled(false);
        ui->actExport->setEnabled(false);
        ui->actExportSVG->setEnabled(false);
        ui->actCopy->setEnabled(false);
        ui->actCut->setEnabled(false);
        ui->actPaste->setEnabled(false);
        ui->actDelete->setEnabled(false);
    }
}

void MainWindow::codeLangChanged(int )
{
    generateCode();
}

void MainWindow::generateCode()
{
    if (document() && codeLanguage->currentIndex() >= 0)
    {
        SourceCodeGenerator gen;
        gen.loadRule("generators:" + codeLanguage->itemData(codeLanguage->currentIndex()).toString() + ".json");
        codeText->setText(gen.applyRule(document()->document()));
    }
}

void MainWindow::slotEditBlock(QBlock *aBlock)
{
    if(aBlock)
    {
        QDialog dlg;
        QVBoxLayout *mainLayout = new QVBoxLayout();
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        QPushButton *btnOk = new QPushButton(tr("&OK"));
        QPushButton *btnCancel = new QPushButton(tr("&Cancel"));
        connect(btnOk, SIGNAL(clicked()), &dlg, SLOT(accept()));
        connect(btnCancel, SIGNAL(clicked()), &dlg, SLOT(reject()));
        buttonLayout->addStretch();
        buttonLayout->addWidget(btnOk);
        buttonLayout->addWidget(btnCancel);
        dlg.setLayout(mainLayout);
        if(aBlock->type() == "process" || aBlock->type() == "if" || aBlock->type() == "pre" || aBlock->type() == "post" )
        {

            QLineEdit *text = new QLineEdit();
            QLabel *lab = new QLabel(tr("&Content:"));
            QString attr = "text";
            if (aBlock->type() == "process")
            {
                dlg.setWindowTitle(tr("Process"));
            }
            else if (aBlock->type() == "if")
            {
                dlg.setWindowTitle(tr("Branching"));
                lab->setText(tr("&Condition:"));
                attr = "cond";
            }
            else if (aBlock->type() == "pre")
            {
                dlg.setWindowTitle(tr("WHILE loop"));
                lab->setText(tr("&Condition:"));
                attr = "cond";
            }
            else if (aBlock->type() == "post")
            {
                dlg.setWindowTitle(tr("Post-condition loop"));
                lab->setText(tr("&Condition:"));
                attr = "cond";
            }
            text->setText(aBlock->attributes.value(attr, ""));
            lab->setBuddy(text);
            QHBoxLayout *box = new QHBoxLayout;
            box->addWidget(lab);
            box->addWidget(text);
            mainLayout->addLayout(box);
            mainLayout->addLayout(buttonLayout);

            if (dlg.exec() == QDialog::Accepted)
            {
                if(aBlock->flowChart())
                {
                    aBlock->flowChart()->makeUndo();
                    aBlock->attributes[attr] = text->text();
                    aBlock->flowChart()->update();
                    aBlock->flowChart()->makeChanged();
                }
            }
        }
        else if(aBlock->type() == "for")
        {
            dlg.setWindowTitle(tr("FOR loop"));
            QGridLayout *gl = new QGridLayout();
            QLineEdit *teVar = new QLineEdit();
            QLineEdit *teFrom = new QLineEdit();
            QLineEdit *teTo = new QLineEdit();
            QLabel *labVar = new QLabel(tr("&Variable:"));
            QLabel *labFrom = new QLabel(tr("&Start value:"));
            QLabel *labTo = new QLabel(tr("&End value:"));
            gl->addWidget(labVar, 0,0);
            gl->addWidget(teVar, 0,1);
            gl->addWidget(labFrom, 1,0);
            gl->addWidget(teFrom, 1,1);
            gl->addWidget(labTo, 2,0);
            gl->addWidget(teTo, 2,1);
            labFrom->setBuddy(teFrom);
            labTo->setBuddy(teTo);
            labVar->setBuddy(teVar);

            mainLayout->addLayout(gl);
            mainLayout->addLayout(buttonLayout);
            teVar->setText(aBlock->attributes.value("var", ""));
            teFrom->setText(aBlock->attributes.value("from", ""));
            teTo->setText(aBlock->attributes.value("to", ""));
            if (dlg.exec() == QDialog::Accepted)
            {
                if(aBlock->flowChart())
                {
                    aBlock->flowChart()->makeUndo();
                    aBlock->attributes["var"] = teVar->text();
                    aBlock->attributes["from"] = teFrom->text();
                    aBlock->attributes["to"] = teTo->text();
                    aBlock->flowChart()->update();
                    aBlock->flowChart()->makeChanged();
                }
            }
        }
        else if(aBlock->type() == "io" || aBlock->type() == "ou")
        {
            if (aBlock->type() == "io")
            {
                    dlg.setWindowTitle(tr("Input"));
                }
            else if (aBlock->type() == "ou")
            {
                    dlg.setWindowTitle(tr("Output"));
                }
            QVBoxLayout *vl = new QVBoxLayout();
            QTextEdit *te = new QTextEdit();
            QLabel *lab = new QLabel(tr("Specify the names of the variables (each per line):"));
            vl->addWidget(lab);
            vl->addWidget(te);
            mainLayout->addLayout(vl);
            mainLayout->addLayout(buttonLayout);
            te->setText(aBlock->attributes.value("vars").split(",").join("\n"));


            if (dlg.exec() == QDialog::Accepted)
            {


                if(aBlock->flowChart())
                {
                    aBlock->flowChart()->makeUndo();
                    aBlock->attributes["vars"] = te->toPlainText().split("\n", QString::SkipEmptyParts).join(",");
                    aBlock->flowChart()->update();
                    aBlock->flowChart()->makeChanged();
                }
            }
        }

        else if(aBlock->type() == "assign")
        {
            dlg.setWindowTitle(tr("Assign"));
            QGridLayout *gl = new QGridLayout();
            QLineEdit *leSrc = new QLineEdit();
            QLineEdit *leDest = new QLineEdit();
            QLabel *labSrc = new QLabel(tr("&Source:"));
            QLabel *labDest = new QLabel(tr("&Destination:"));
            gl->addWidget(labDest, 0,0);
            gl->addWidget(leDest, 0,1);
            gl->addWidget(labSrc, 1,0);
            gl->addWidget(leSrc, 1,1);
            labDest->setBuddy(leDest);
            labSrc->setBuddy(leSrc);

            mainLayout->addLayout(gl);
            mainLayout->addLayout(buttonLayout);
            leSrc->setText(aBlock->attributes.value("src", ""));
            leDest->setText(aBlock->attributes.value("dest", ""));

            if (dlg.exec() == QDialog::Accepted)
            {

                if(aBlock->flowChart())
                {
                    aBlock->flowChart()->makeUndo();
                    aBlock->attributes["dest"] = leDest->text();
                    aBlock->attributes["src"] = leSrc->text();
                    aBlock->flowChart()->update();
                    aBlock->flowChart()->makeChanged();
                }
            }
        }

    }
}

void MainWindow::slotToolAssign()
{
    if(document())
    {
        document()->setBuffer("<algorithm><branch><assign dest=\"x\" src=\"0\"/></branch></algorithm>");
        if(!document()->buffer().isEmpty())
        {
            document()->setStatus(QFlowChart::Insertion);
            document()->setMultiInsert(false);
        }
    }
}

void MainWindow::slotToolArrow()
{
    if(document())
    {
        document()->setStatus(QFlowChart::Selectable);
        document()->update();
    }
}
void MainWindow::slotToolProcess()
{
    if(document())
    {
        document()->setBuffer("<algorithm><branch><process text=\"func()\"/></branch></algorithm>");
        if(!document()->buffer().isEmpty())
        {
            document()->setStatus(QFlowChart::Insertion);
            document()->setMultiInsert(false);
        }
    }
}

void MainWindow::slotToolIf()
{
    if(document())
    {
        document()->setBuffer("<algorithm><branch><if cond=\"x &gt; 0\"><branch /><branch /></if></branch></algorithm>");
        if(!document()->buffer().isEmpty())
        {
            document()->setStatus(QFlowChart::Insertion);
            document()->setMultiInsert(false);
        }
    }
}

void MainWindow::slotToolFor()
{
    if(document())
    {
        document()->setBuffer("<algorithm><branch><for var=\"i\" from=\"0\" to=\"n - 1\"><branch /></for></branch></algorithm>");
        if(!document()->buffer().isEmpty())
        {
            document()->setStatus(QFlowChart::Insertion);
            document()->setMultiInsert(false);
        }
    }
}

void MainWindow::slotToolWhilePre()
{
    if(document())
    {
        document()->setBuffer("<algorithm><branch><pre cond=\"x &lt; n\"><branch /></pre></branch></algorithm>");
        if(!document()->buffer().isEmpty())
        {
            document()->setStatus(QFlowChart::Insertion);
            document()->setMultiInsert(false);
        }
    }
}

void MainWindow::slotToolWhilePost()
{
    if(document())
    {
        document()->setBuffer("<algorithm><branch><post cond=\"x &lt; n\"><branch /></post></branch></algorithm>");
        if(!document()->buffer().isEmpty())
        {
            document()->setStatus(QFlowChart::Insertion);
            document()->setMultiInsert(false);
        }
    }

}

void MainWindow::slotToolIo()
{
    if(document())
    {
        document()->setBuffer("<algorithm><branch><io vars=\"x,y\"/></branch></algorithm>");
        if(!document()->buffer().isEmpty())
        {
            document()->setStatus(QFlowChart::Insertion);
            document()->setMultiInsert(false);
        }
    }

}




void MainWindow::slotToolOu()
{
    if(document())
    {
        document()->setBuffer("<algorithm><branch><ou vars=\"x,y\"/></branch></algorithm>");
        if(!document()->buffer().isEmpty())
        {
            document()->setStatus(QFlowChart::Insertion);
            document()->setMultiInsert(false);
        }
    }

}

void MainWindow::slotToolCase()
{
    //  if(document())
    //  {
    //    document()->setBuffer("<algorithm><branch><case><branch /><branch /><branch /></case></branch></algorithm>");
    //    if(!document()->buffer().isEmpty())
    //    {
    //      document()->setStatus(QFlowChart::Insertion);
    //      document()->setMultiInsert(false);
    //    }
    //  }

}

void MainWindow::slotToolForCStyle()
{
    if(document())
    {
        document()->setBuffer("<algorithm> <branch> <assign dest=\"i\" src=\"0\"  /> <pre cond=\"i &lt; n\"> <branch> <assign dest=\"i\" src=\"i + 1\" /> </branch> </pre> </branch> </algorithm>");
        if(!document()->buffer().isEmpty())
        {
            document()->setStatus(QFlowChart::Insertion);
            document()->setMultiInsert(false);
        }
    }

}

QString MainWindow::getFilterFor(const QString & fileExt)
{
    return tr("%1 image (*.%2)").arg(fileExt.toUpper(), fileExt);
}

QString MainWindow::getWriteFormatFilter()
{
    QString result;
    QList<QByteArray> formats = QImageWriter::supportedImageFormats();
    for(int i = 0; i < formats.size(); ++i)
    {
        if(!result.isEmpty()) result.append(";;");
        result.append(getFilterFor(formats.at(i)));
    }
    return result;
}

QHash<QString, QString> MainWindow::enumLanguages()
{
    QHash<QString, QString> result;
#if defined(Q_WS_X11) or defined(Q_OS_LINUX)
    QDir dir(QString(PROGRAM_DATA_DIR) + "locale");
#else
    QDir dir(qApp->applicationDirPath() + "/locale");
#endif

    QStringList qms = dir.entryList(QStringList() << "afce_*.qm", QDir::Files, QDir::Name);

    QRegExp rx("^afce_([-_a-zA-Z]+)\\.qm$");
    for (int i = 0; i < qms.size(); ++i) {
        if(rx.indexIn(qms[i]) >= 0) {
            QString localeName = rx.cap(1);
            QLocale loc(localeName);
            result.insert(localeName, loc.nativeLanguageName());
        }
    }
    return result;
}

void MainWindow::setDocument(QFlowChart * aDocument)
{
    fDocument = aDocument;
    saScheme->setWidget(fDocument);
    saScheme->setAutoFillBackground(true);
    fDocument->show();
    fDocument->move(0,0);
}
void MainWindow::setZoom(int quarts)
{
    if(quarts > 20) quarts = 20;
    if(quarts < 1) quarts = 1;
    zoomLabel->setText(tr("Zoom: %1 %").arg(quarts * 25));
    if (document())
    {
        document()->setZoom(quarts * 25 / 100.0);
    }
}

void MainWindow::shiftZoom(int step)
{
    int z = zoomSlider->value();
    zoomSlider->setValue(z + step);
}


void setApplicationLocale(const QString &localeName)
{
    QLocale locale(localeName);
    QLocale::setDefault(locale);
    static QTranslator *qtTranslator = NULL;
    static QTranslator *myappTranslator = NULL;

    if(!qtTranslator) {
        qApp->removeTranslator(qtTranslator);
        delete qtTranslator;
        qtTranslator = new QTranslator;
    }
    if(!myappTranslator) {
        qApp->removeTranslator(myappTranslator);
        delete myappTranslator;
        myappTranslator = new QTranslator;
    }

#if defined(Q_WS_X11) or defined(Q_OS_LINUX)
    qtTranslator->load(locale, "qt", "_", QString(PROGRAM_DATA_DIR) + "locale", ".qm");
#else
    qtTranslator->load(locale, "qt", "_", qApp->applicationDirPath() + "/locale",  ".qm");
#endif
    qApp->installTranslator(qtTranslator);

#if defined(Q_WS_X11) or defined(Q_OS_LINUX)
    myappTranslator->load("afce_" + locale.name() + ".qm", QString(PROGRAM_DATA_DIR) + "locale");
#else
    myappTranslator->load(locale, "afce", "_", qApp->applicationDirPath() + "/locale", ".qm");
#endif
    qApp->installTranslator(myappTranslator);

    qDebug() << "Detected system locale: " << QLocale::system().name();
    qDebug() << "Set locale: " << locale.name();
    qDebug() << qApp->applicationDirPath();
    QWidgetList wl = qApp->allWidgets();
    for(int i = 0; i < wl.size(); ++i) {
        wl[i]->setLocale(locale);
    }
}
