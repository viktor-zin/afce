/****************************************************************************
**                                                                         **
** Copyright (C) 2009 Victor Zinkevich. All rights reserved.               **
** Contact: vicking@yandex.ru                                              **
**                                                                         **
** This file is part of the Algorithm Flowchart Editor project.            **
**                                                                         **
** This file may be used under the terms of the GNU                        **
** General Public License versions 2.0 or 3.0 as published by the Free     **
** Software Foundation and appearing in the file LICENSE.TXT included in   **
** the packaging of this file.                                             **
** You can find license at http://www.gnu.org/licenses/gpl.html            **
**                                                                         **
****************************************************************************/

#include "mainwindow.h"
#include "zvflowchart.h"
#include <QtGui>
#include <QtSvg>
#include "qflowchartstyle.h"
#include "zvcodegen.h"
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>

QString afceVersion()
{
  return "1.0.0";
}


void AfcScrollArea::mousePressEvent(QMouseEvent *event)
{
  event->accept();
  emit mouseDown();
}


MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags), fDocument(0)
{
  setupUi();
  
  retranslateUi();
  resize(800, 600);

  QFlowChart *fc = new QFlowChart(this);
  setDocument(fc);
  document()->setZoom(1);
  connect(document(), SIGNAL(statusChanged()), this, SLOT(slotStatusChanged()));
  connect(document(), SIGNAL(editBlock(QBlock *)), this, SLOT(slotEditBlock(QBlock *)));
  connect(actUndo, SIGNAL(triggered()), document(), SLOT(undo()));
  connect(actRedo, SIGNAL(triggered()), document(), SLOT(redo()));
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
  //  saScheme->palette().setColor(QPalette::Window, saScheme->palette().color(QPalette::Base));

  setCentralWidget(body);
  QVBoxLayout *bodyLayout = new QVBoxLayout;
  bodyLayout->addWidget(saScheme);
  bodyLayout->addWidget(zoomPanel);  
  body->setLayout(bodyLayout);
  QSlider *zoomSlider = new QSlider(Qt::Horizontal, zoomPanel);
  zoomLabel = new QLabel;
  QHBoxLayout *zoomLayout= new QHBoxLayout;
  zoomLayout->addStretch();
  zoomLayout->addWidget(zoomLabel);
  zoomLayout->addWidget(zoomSlider);
  zoomPanel->setLayout(zoomLayout);
  zoomSlider->setRange(10, 500);
  zoomSlider->setSingleStep(10);
  zoomSlider->setPageStep(100);
  connect(zoomSlider, SIGNAL(valueChanged(int)),this, SLOT(setZoom(int)));
  zoomSlider->setValue(100);
  createToolbox();

  dockCode = new QDockWidget(this);
  dockCode->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::RightDockWidgetArea, dockCode);
  codeWidget = new QFrame;
  codeLanguage = new QComboBox;
  codeText = new QTextEdit;
  codeLabel = new QLabel;

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
  helpWindow->setAllowedAreas(Qt::AllDockWidgetAreas);
  addDockWidget(Qt::RightDockWidgetArea, helpWindow);
  helpWindow->hide();

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
  dockTools->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  dockTools->setMinimumWidth(150);
  addDockWidget(Qt::LeftDockWidgetArea, dockTools);

  tbArrow = createToolButton(":/images/arrow.png");
  tbArrow->setChecked(true);
  tbProcess = createToolButton(":/images/simple.png");
  tbIf = createToolButton(":/images/if.png");
  tbFor = createToolButton(":/images/for.png");
  tbWhilePre = createToolButton(":/images/while.png");
  tbWhilePost = createToolButton(":/images/until.png");
  tbIo = createToolButton(":/images/io.png");
//  tbCase = createToolButton(":/images/case.png");
  tbForCStyle = createToolButton(":/images/forc.png");
  tbAssign = createToolButton(":/images/assign.png");

  connect(tbArrow, SIGNAL(pressed()), this, SLOT(slotToolArrow()));
  connect(tbProcess, SIGNAL(pressed()), this, SLOT(slotToolProcess()));
  connect(tbIf, SIGNAL(pressed()), this, SLOT(slotToolIf()));
  connect(tbFor, SIGNAL(pressed()), this, SLOT(slotToolFor()));
  connect(tbWhilePre, SIGNAL(pressed()), this, SLOT(slotToolWhilePre()));
  connect(tbWhilePost, SIGNAL(pressed()), this, SLOT(slotToolWhilePost()));
  connect(tbIo, SIGNAL(pressed()), this, SLOT(slotToolIo()));
//  connect(tbCase, SIGNAL(pressed()), this, SLOT(slotToolCase()));
  connect(tbForCStyle, SIGNAL(pressed()), this, SLOT(slotToolForCStyle()));
  connect(tbAssign, SIGNAL(pressed()), this, SLOT(slotToolAssing()));

  toolsWidget = new QFrame;
  toolsWidget->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
  QVBoxLayout *tl = new QVBoxLayout;
  tl->setSpacing(2);
  tl->addWidget(tbArrow);
  tl->addWidget(tbAssign);
  tl->addWidget(tbProcess);
  tl->addWidget(tbIf);
  tl->addWidget(tbFor);
  tl->addWidget(tbWhilePre);
  tl->addWidget(tbWhilePost);
  tl->addWidget(tbIo);
//  tl->addWidget(tbCase);
  tl->addWidget(tbForCStyle);
  tl->addStretch();
  toolsWidget->setLayout(tl);
  dockTools->setWidget(toolsWidget);
}

void MainWindow::retranslateUi()
{
//  QTextCodec::setCodecForTr(QTextCodec::codecForName("Windows-1251"));
  dockTools->setWindowTitle(tr("Tools"));
  tbArrow->setText(tr("Select"));
  tbProcess->setText(tr("Process"));
  tbIf->setText(tr("If...then...else"));
  tbFor->setText(tr("FOR loop"));
  tbWhilePre->setText(tr("loop with pre-condition"));
  tbWhilePost->setText(tr("loop with post-condition"));
  tbIo->setText(tr("Input / output"));
//  tbCase->setText(tr("case statement"));
  tbForCStyle->setText(tr("FOR loop (C/C++)"));
  tbAssign->setText(tr("Assing"));

  actExit->setText(tr("E&xit"));
  actOpen->setText(tr("&Open..."));
  actSave->setText(tr("&Save"));
  actSaveAs->setText(tr("Save &as..."));
  actExport->setText(tr("&Export to raster..."));
  actExportSVG->setText(tr("&Export to SVG..."));
  actPrint->setText(tr("&Print..."));
  actNew->setText(tr("&New"));
  actUndo->setText(tr("&Undo"));
  actRedo->setText(tr("&Redo"));

  actCut->setText(tr("Cu&t"));
  actCopy->setText(tr("&Copy"));
  actPaste->setText(tr("&Paste"));
  actDelete->setText(tr("&Delete"));
  actHelp->setText(tr("&Help"));
  actAbout->setText(tr("&About"));
  actAboutQt->setText(tr("About &Qt"));
  

  actExit->setShortcut(tr("Alt+X"));
  actOpen->setShortcut(tr("Ctrl+O"));
  actSave->setShortcut(tr("Ctrl+S"));
  actNew->setShortcut(tr("Ctrl+N"));
  actUndo->setShortcut(tr("Ctrl+Z"));
  actRedo->setShortcut(tr("Ctrl+Y"));
  actCut->setShortcut(tr("Ctrl+X"));
  actCopy->setShortcut(tr("Ctrl+C"));
  actPaste->setShortcut(tr("Ctrl+V"));
  actDelete->setShortcut(tr("Del"));
  actHelp->setShortcut(tr("F1"));
  actPrint->setShortcut(tr("Ctrl+P"));

  menuFile->setTitle(tr("&File"));
  menuEdit->setTitle(tr("&Edit"));
  menuHelp->setTitle(tr("&Help"));

  toolBar->setWindowTitle(tr("Standard"));
  dockCode->setWindowTitle(tr("Source code"));
    
  
  int i = codeLanguage->currentIndex();
  codeLanguage->clear();
  codeLanguage->addItem(tr("Pascal"), "pas");
  codeLanguage->addItem(tr("C/C++"), "cpp");
  codeLanguage->addItem(tr("Ershov's algorithm language"), "e87");
  if (i!=-1)
    codeLanguage->setCurrentIndex(i);
  else
    codeLanguage->setCurrentIndex(0);


  codeLabel->setText(tr("&Select programming language:"));

  if (!fileName.isEmpty())
    setWindowTitle(tr("%1 - Algorithm Flowchart Editor").arg(fileName));
  else
    setWindowTitle(tr("Algorithm Flowchart Editor"));
  helpWindow->setWindowTitle(tr("Help window"));

}

void MainWindow::createMenu()
{
  menuFile = menuBar()->addMenu("");
  menuFile->addAction(actNew);
  menuFile->addAction(actOpen);
  menuFile->addSeparator();
  menuFile->addAction(actSave);
  menuFile->addAction(actSaveAs);
  menuFile->addSeparator();
  menuFile->addAction(actExport);
  menuFile->addAction(actExportSVG);
  menuFile->addSeparator();
  menuFile->addAction(actPrint);
  menuFile->addSeparator();
  menuFile->addAction(actExit);

  menuEdit = menuBar()->addMenu("");
  menuEdit->addAction(actUndo);
  menuEdit->addAction(actRedo);
  menuEdit->addSeparator();
  menuEdit->addAction(actCut);
  menuEdit->addAction(actCopy);
  menuEdit->addAction(actPaste);
  menuEdit->addSeparator();
  menuEdit->addAction(actDelete);

  menuHelp = menuBar()->addMenu("");
  menuHelp->addAction(actHelp);
  menuHelp->addSeparator();
  menuHelp->addAction(actAbout);
  menuHelp->addAction(actAboutQt);
}

void MainWindow::createToolBar()
{
  toolBar = addToolBar("");
  toolBar->setIconSize(QSize(32,32));
  toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

  toolBar->addAction(actNew);
  toolBar->addAction(actOpen);
  toolBar->addAction(actSave);
  toolBar->addSeparator();
  toolBar->addAction(actUndo);
  toolBar->addAction(actRedo);
  toolBar->addSeparator();
  toolBar->addAction(actCut);
  toolBar->addAction(actCopy);
  toolBar->addAction(actPaste);
  toolBar->addSeparator();
  toolBar->addAction(actHelp);
}

void MainWindow::createActions()
{
  actExit = new QAction(this);
  actOpen = new QAction(QIcon(":/images/open_document_32_h.png"), "", this);
  actNew = new QAction(QIcon(":/images/new_document_32_h.png"), "", this);
  actSave = new QAction(QIcon(":/images/save_32_h.png"), "", this);
  actSaveAs = new QAction(this);
  actUndo = new QAction(QIcon(":/images/undo_32_h.png"), "", this);
  actRedo = new QAction(QIcon(":/images/redo_32_h.png"), "", this);
  actCut = new QAction(QIcon(":/images/cut_clipboard_32_h.png"), "", this);
  actCopy = new QAction(QIcon(":/images/copy_clipboard_32_h.png"), "", this);
  actPaste = new QAction(QIcon(":/images/paste_clipboard_32_h.png"), "", this);
  actDelete = new QAction(QIcon(":/images/delete_x_32_h.png"), "", this);
  actExport = new QAction(this);
  actExportSVG = new QAction(this);
  actHelp = new QAction(QIcon(":/images/help_32_h.png"), "", this);
  actAbout = new QAction(this);
  actAboutQt = new QAction(this);
  actPrint = new QAction(this);

  connect(actExit, SIGNAL(triggered()), this, SLOT(close()));
  connect(actNew, SIGNAL(triggered()), this, SLOT(slotFileNew()));
  connect(actOpen, SIGNAL(triggered()), this, SLOT(slotFileOpen()));
  connect(actSave, SIGNAL(triggered()), this, SLOT(slotFileSave()));
  connect(actSaveAs, SIGNAL(triggered()), this, SLOT(slotFileSaveAs()));
  connect(actExport, SIGNAL(triggered()), this, SLOT(slotFileExport()));
  connect(actExportSVG, SIGNAL(triggered()), this, SLOT(slotFileExportSVG()));
  connect(actPrint, SIGNAL(triggered()), this, SLOT(slotFilePrint()));

  connect(actCut, SIGNAL(triggered()), this, SLOT(slotEditCut()));
  connect(actCopy, SIGNAL(triggered()), this, SLOT(slotEditCopy()));
  connect(actPaste, SIGNAL(triggered()), this, SLOT(slotEditPaste()));
  connect(actDelete, SIGNAL(triggered()), this, SLOT(slotEditDelete()));
  
  connect(actHelp, SIGNAL(triggered()), this, SLOT(slotHelpHelp()));
  connect(actAbout, SIGNAL(triggered()), this, SLOT(slotHelpAbout()));
  connect(actAboutQt, SIGNAL(triggered()), this, SLOT(slotHelpAboutQt()));
}


MainWindow::~MainWindow()
{
}

void MainWindow::slotFileOpen()
{
  QString fn = QFileDialog::getOpenFileName ( this,
   tr("Select a file to open"), "", tr("Algorithm flowcharts (*.afc)"));
  if(!fn.isEmpty())
  {
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
}

void MainWindow::slotFilePrint()
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

void MainWindow::slotFileNew()
{
  QProcess::startDetached(QApplication::applicationFilePath());
}

void MainWindow::slotFileSave()
{
  if (fileName.isEmpty())
  {
    slotFileSaveAs();
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

void MainWindow::slotFileSaveAs()
{
  QString fn = QFileDialog::getSaveFileName(this, "", "", tr("Algorithm flowcharts (*.afc)"));
  if (!fn.isEmpty())
  {
    fileName = fn;
    setWindowTitle(tr("%1 - Algorithm Flowchart Editor").arg(fileName));
    slotFileSave();
  }
}

void MainWindow::slotFileExport()
{
  QString filter = getWriteFormatFilter();
  QString fn = QFileDialog::getSaveFileName(this, "", "", filter);
  if(!fn.isEmpty())
  {
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

void MainWindow::slotFileExportSVG()
{
  QString filter = getWriteFormatFilter();
  QString fn = QFileDialog::getSaveFileName(this, "", "", getFilterFor("svg"));
  if(!fn.isEmpty())
  {
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
//    document()->paintTo(&canvas);

    document()->setZoom(oldZoom);
    document()->setStatus(QFlowChart::Selectable);
  }
}


void MainWindow::slotEditCut()
{
  slotEditCopy();
  slotEditDelete();
}

void MainWindow::slotEditCopy()
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

void MainWindow::slotEditPaste()
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
void MainWindow::slotEditDelete()
{
  if(document())
  {
    if(document()->status() == QFlowChart::Selectable)
    {
      document()->deleteActiveBlock();
    }
  }
}

void MainWindow::slotHelpHelp()
{
  helpWindow->show();
}

void MainWindow::slotHelpAbout()
{
  QDialog dlg;
  QPushButton *ok = new QPushButton(tr("&OK"));
  QLabel *text = new QLabel("<html><h1>AFCE</h1><p>Algorithm Flowchart Editor</p><p>Copyright 2008-2014 Zinkevich Viktor. All rights reserved.</p><p>The program is provided AS IS with NO WARRANTY OF ANY KIND,<br> INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND<br> FITNESS FOR A PARTICULAR PURPOSE.</p></html>");
  QLabel *ico = new QLabel();
  ico->setPixmap(QPixmap(":/images/icon.png"));
  QGridLayout *layout = new QGridLayout;
  QHBoxLayout *bl = new QHBoxLayout;
  bl->addStretch();
  bl->addWidget(ok);
//  bl->addStretch();
  layout->addWidget(ico, 0, 0, 2, 1, Qt::AlignTop);
  layout->addWidget(text, 0, 1);
  layout->addLayout(bl, 1, 1);
  dlg.setLayout(layout);
  connect(ok, SIGNAL(clicked()), &dlg, SLOT(accept()));
  dlg.exec();
}

void MainWindow::slotHelpAboutQt()
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
    actUndo->setEnabled(document()->canUndo() && document()->status() != QFlowChart::Insertion);
    actRedo->setEnabled(document()->canRedo() && document()->status() != QFlowChart::Insertion);
    actOpen->setEnabled(document()->status() != QFlowChart::Insertion);
    actSave->setEnabled(document()->status() != QFlowChart::Insertion);
    actSaveAs->setEnabled(document()->status() != QFlowChart::Insertion);
    actPrint->setEnabled(document()->status() != QFlowChart::Insertion);
    actExport->setEnabled(document()->status() != QFlowChart::Insertion);
    actExportSVG->setEnabled(document()->status() != QFlowChart::Insertion);
    actCopy->setEnabled(document()->status() == QFlowChart::Selectable && document()->activeBlock());
    actCut->setEnabled(document()->status() == QFlowChart::Selectable && document()->activeBlock());
    actPaste->setEnabled(document()->status() == QFlowChart::Selectable && document()->canPaste());
    actDelete->setEnabled(document()->status() == QFlowChart::Selectable && document()->activeBlock());
  }
  else
  {
    actUndo->setEnabled(false);
    actRedo->setEnabled(false);
    actOpen->setEnabled(false);
    actSave->setEnabled(false);
    actSaveAs->setEnabled(false);
    actPrint->setEnabled(false);
    actExport->setEnabled(false);
    actExportSVG->setEnabled(false);
    actCopy->setEnabled(false);
    actCut->setEnabled(false);
    actPaste->setEnabled(false);
    actDelete->setEnabled(false);
  }
}

void MainWindow::codeLangChanged(int )
{
  generateCode();
}

void MainWindow::generateCode()
{
  if (document())
  {
    switch (codeLanguage->currentIndex())
    {
      case 0:
        {
          codeText->setText(xmlToPascal(document()->document()));
          break;
        }
      case 1:
        {
          codeText->setText(xmlToC(document()->document()));
          break;
        }
      case 2:
        {
          codeText->setText(xmlToE87(document()->document()));
          break;
        }
    }
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
    if(aBlock->type() == "process" || aBlock->type() == "io"|| aBlock->type() == "if" || aBlock->type() == "pre" || aBlock->type() == "post" )
    {

      QLineEdit *text = new QLineEdit();
      QLabel *lab = new QLabel(tr("&Content:"));
      QString attr = "text";
      if (aBlock->type() == "process")
      {
        dlg.setWindowTitle(tr("Process"));
      }
      else if (aBlock->type() == "io")
      {
        dlg.setWindowTitle(tr("Input/output"));
      }
      else if (aBlock->type() == "if")
      {
        dlg.setWindowTitle(tr("Branching"));
        lab->setText("&Condition:");
        attr = "cond";
      }
      else if (aBlock->type() == "pre")
      {
        dlg.setWindowTitle(tr("WHILE loop"));
        lab->setText("&Condition:");
        attr = "cond";
      }
      else if (aBlock->type() == "post")
      {
        dlg.setWindowTitle(tr("Post-condition loop"));
        lab->setText("&Condition:");
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
        aBlock->attributes[attr] = text->text();
        if(aBlock->flowChart())
        {
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
       aBlock->attributes["var"] = teVar->text();
       aBlock->attributes["from"] = teFrom->text();
       aBlock->attributes["to"] = teTo->text();
       if(aBlock->flowChart())
       {
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
       aBlock->attributes["dest"] = leDest->text();
       aBlock->attributes["src"] = leSrc->text();

       if(aBlock->flowChart())
       {
         aBlock->flowChart()->update();
         aBlock->flowChart()->makeChanged();
       }
      }
    }

  }
}

void MainWindow::slotToolAssing()
{
  if(document())
  {
    document()->setBuffer("<algorithm><branch><assign dest=\"X\" src=\"0\"/></branch></algorithm>");
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
    document()->setBuffer("<algorithm><branch><process text=\"doSomething()\"/></branch></algorithm>");
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
    document()->setBuffer("<algorithm><branch><if cond=\"X &gt; 0\"><branch /><branch /></if></branch></algorithm>");
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
    document()->setBuffer("<algorithm><branch><pre cond=\"X &lt; n\"><branch /></pre></branch></algorithm>");
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
    document()->setBuffer("<algorithm><branch><post cond=\"X &lt; n\"><branch /></post></branch></algorithm>");
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
    document()->setBuffer("<algorithm><branch><io text=\"Readln()\"/></branch></algorithm>");
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

void MainWindow::setDocument(QFlowChart * aDocument)
{
  fDocument = aDocument;
  saScheme->setWidget(fDocument);
  saScheme->setAutoFillBackground(true);
  fDocument->show();
  fDocument->move(0,0);
}
void MainWindow::setZoom(int Percents)
{
  zoomLabel->setText(tr("Zoom: %1 %").arg(Percents));
  if (document())
  {
    document()->setZoom(Percents / 100.0);
  }
}
