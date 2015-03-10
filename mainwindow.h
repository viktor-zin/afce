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
#include "afcscrollarea.h"


namespace Ui {
class MainWindow;
}


class MainWindow : public QMainWindow
{
    Q_OBJECT
signals:
    void rootChanged();
private:
//  TAlgorithmBlock *fRoot;
  QFlowChart *fDocument;
  Ui::MainWindow *ui;
  bool isSaved;
  QVBoxLayout *fVLayout;
  QHBoxLayout *fHLayout;
  AfcScrollArea *saScheme;
//  QDockWidget *dock_tools;
  QDockWidget *dockCode;
//  QFrame *toolsWidget;
  QFrame *codeWidget;
//  QToolButton *tbArrow;
//  QToolButton *tbProcess;
//  QToolButton *tbIf;
//  QToolButton *tbFor;
//  QToolButton *tbWhilePre;
//  QToolButton *tbWhilePost;
//  QToolButton *tbIo;
//  QToolButton *tbOu;
//  QToolButton *tbForCStyle;
//  QToolButton *tbAssign;

  QComboBox *codeLanguage;
  QTextEdit *codeText;

//  QAction *actNew;
//  QAction *actOpen;
//  QAction *actSave;
//  QAction *actSaveAs;
//  QAction *actExit;
//  QAction *actUndo;
//  QAction *actRedo;
//  QAction *actCut;
//  QAction *actCopy;
//  QAction *actPaste;
//  QAction *actDelete;
//  QAction *actHelp;
//  QAction *actAbout;
//  QAction *actAboutQt;
//  QAction *actExport;
//  QAction *actExportSVG;
//  QAction *actPrint;
//  QAction *actTools;
//  QAction *actCode;
//  QAction *acteng;
//  QAction *actrus;
  QList<QAction *> actLanguages;

//  QMenu *menuFile;
//  QMenu *menuEdit;
//  QMenu *menuHelp;
//  QMenu *menuWindow;
  QMenu *menuLanguage;
  QString fileName;
//  QToolBar *toolBar;
  QLabel *codeLabel;
  QLabel *zoomLabel;
  QLabel *labelMenu;
  QSlider *zoomSlider;

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
    QFlowChart * document() const { return fDocument; }
    void setDocument(QFlowChart * aDocument);
    static QString getFilterFor(const QString & fileExt);
    static QString getWriteFormatFilter();
    static QHash<QString, QString> enumLanguages();

public slots:
    void retranslateUi();
    void on_actOpen_triggered();
    void on_actNew_triggered();
    void on_actSave_triggered();
    void on_actSaveAs_triggered();
    void on_actExport_triggered();
    void on_actExportSVG_triggered();
    void on_actPrint_triggered();
    void on_actCopy_triggered();
    void on_actCut_triggered();
    void on_actPaste_triggered();
    void on_actDelete_triggered();
    void on_actAbout_triggered();
    void on_actAboutQt_triggered();
    void on_tbArrow_pressed();
    void on_tbProcess_pressed();
    void on_tbIf_pressed();
    void on_tbFor_pressed();
    void on_tbWhilePre_pressed();
    void on_tbWhilePost_pressed();
    void on_tbIo_pressed();
    void on_tbOu_pressed();
    void slotToolCase();
    void on_tbForCStyle_pressed();
    void on_tbAssign_pressed();
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
