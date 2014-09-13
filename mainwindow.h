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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "thelpwindow.h"
#include "zvflowchart.h"

#include <QtGui>
#include <QMainWindow>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QComboBox>
#include <QLabel>
#include <QApplication>
#include <QTextCodec>


class AfcScrollArea : public QScrollArea
{
  Q_OBJECT
  protected:
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
  signals:
    void mouseDown();
    void zoomStepped(int);
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
  bool isSaved;
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
  QToolButton *tbOu;
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
  QAction *actTools;
  QAction *actCode;
  QAction *acteng;
  QAction *actrus;
  QList<QAction *> actLanguages;

//  QAction *actPageSetup;

  QMenu *menuFile;
  QMenu *menuEdit;
  QMenu *menuHelp;
  QMenu *menuWindow;
  QMenu *menuLanguage;
  QString fileName;
  QToolBar *toolBar;
  QLabel *codeLabel;
  QLabel *zoomLabel;
  QLabel *labelMenu;
  QSlider *zoomSlider;
  //QLabel *labelFile;


  THelpWindow *helpWindow;

  QStack<QString> undoStack;
  QStack<QString> redoStack;

  void setupUi();
  void readSettings();
  void writeSettings();
  void createMenu();
  void createActions();
  void createToolbox();
  void createToolBar();
 //void closeEvent(QCloseEvent *event);
  bool okToContinue();
protected:

void closeEvent(QCloseEvent *event);

public:
    MainWindow(QWidget *parent = 0, Qt::WindowFlags flags = 0);
    ~MainWindow();
//    TAlgorithmBlock *root() const;
//    void setRoot(TAlgorithmBlock *aRoot);
    QFlowChart * document() const { return fDocument; }
    void setDocument(QFlowChart * aDocument);
  static QString getFilterFor(const QString & fileExt);
  static QString getWriteFormatFilter();
  static QHash<QString, QString> enumLanguages();

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
  void slotHelpAbout();
  void slotHelpAboutQt();
  void slotToolArrow();
  void slotToolProcess();
  void slotToolIf();
  void slotToolFor();
  void slotToolWhilePre();
  void slotToolWhilePost();
  void slotToolIo();
  void slotToolOu();
  void slotToolCase();
  void slotToolForCStyle();
  void slotToolAssign();
  void slotDocumentSaved();
  void slotDocumentChanged();
  void slotDocumentLoaded();
  void slotChangeLanguage();
  void slotReloadGenerators();

  void setZoom(int quarts);
  void shiftZoom(int step);
  void slotStatusChanged();
  void slotEditBlock(QBlock *aBlock);
  void updateActions();
  void generateCode();
  void codeLangChanged(int index);
  void docToolsVisibilityChanged(bool visible);
  void docCodeVisibilityChanged(bool visible);

  void slotOpenDocument(const QString &fn);
signals:
  void documentLoaded();
  void documentSaved();
  void documentChanged();
  void documentUnloaded();


};

QString afceVersion();
void setApplicationLocale(const QString &localeName);

#endif // MAINWINDOW_H
