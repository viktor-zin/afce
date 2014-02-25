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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui>
#include "thelpwindow.h"
#include "zvflowchart.h"


class AfcScrollArea : public QScrollArea
{
  Q_OBJECT
  protected:
    virtual void mousePressEvent(QMouseEvent *event);
  signals:
    void mouseDown();
  public:
     explicit AfcScrollArea(QWidget* parent=0) : QScrollArea(parent) { }
    ~AfcScrollArea() { }

};

class MainWindow : public QMainWindow
{
    Q_OBJECT
signals:
    void rootChanged();
private:
//  TAlgorithmBlock *fRoot;
  QFlowChart *fDocument;
  QVBoxLayout *fVLayout;
  QHBoxLayout *fHLayout;
  AfcScrollArea *saScheme;
  QDockWidget *dockTools;
  QDockWidget *dockCode;
  QFrame *toolsWidget, *codeWidget;
  QToolButton *tbArrow;
  QToolButton *tbProcess;
  QToolButton *tbIf;
  QToolButton *tbFor;
  QToolButton *tbWhilePre;
  QToolButton *tbWhilePost;
  QToolButton *tbIo;
//  QToolButton *tbCase;
  QToolButton *tbForCStyle;
  QToolButton *tbAssign;

  QComboBox *codeLanguage;
  QTextEdit *codeText;

  QAction *actNew;
  QAction *actOpen;
  QAction *actSave;
  QAction *actSaveAs;
  QAction *actExit;
  QAction *actUndo;
  QAction *actRedo;
  QAction *actCut;
  QAction *actCopy;
  QAction *actPaste;
  QAction *actDelete;
  QAction *actHelp;
  QAction *actAbout;
  QAction *actAboutQt;
  QAction *actExport;
  QAction *actExportSVG;
  QAction *actPrint;
//  QAction *actPageSetup;

  QMenu *menuFile;
  QMenu *menuEdit;
  QMenu *menuHelp;
  QString fileName;
  QToolBar *toolBar;
  QLabel *codeLabel;
  QLabel *zoomLabel;

  THelpWindow *helpWindow;

  QStack<QString> undoStack;
  QStack<QString> redoStack;

  void setupUi();
  void createMenu();
  void createActions();
  void createToolbox();
  void createToolBar();

public:
    MainWindow(QWidget *parent = 0, Qt::WFlags flags = 0);
    ~MainWindow();
//    TAlgorithmBlock *root() const;
//    void setRoot(TAlgorithmBlock *aRoot);
    QFlowChart * document() const { return fDocument; }
    void setDocument(QFlowChart * aDocument);
  static QString getFilterFor(const QString & fileExt);
  static QString getWriteFormatFilter();

public slots:
  void retranslateUi();
  void slotFileOpen();
  void slotFileNew();
  void slotFileSave();
  void slotFileSaveAs();
  void slotFileExport();
  void slotFileExportSVG();
  void slotFilePrint();
  void slotEditCopy();
  void slotEditCut();
  void slotEditPaste();
  void slotEditDelete();
  void slotHelpHelp();
  void slotHelpAbout();
  void slotHelpAboutQt();

  void slotToolArrow();
  void slotToolProcess();
  void slotToolIf();
  void slotToolFor();
  void slotToolWhilePre();
  void slotToolWhilePost();
  void slotToolIo();
  void slotToolCase();
  void slotToolForCStyle();
  void slotToolAssing();


  void setZoom(int Percents);
  void slotStatusChanged();
  void slotEditBlock(QBlock *aBlock);
  void updateActions();
  void generateCode();
  void codeLangChanged(int index);

signals:
  void documentLoaded();
  void documentSaved();
  void documentChanged();
  void documentUnloaded();


};

QString afceVersion();

#endif // MAINWINDOW_H
