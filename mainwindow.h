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

#include <QDomDocument>
#include <QMainWindow>
#include <QMap>
#include <QMessageBox>
#include <QScrollArea>
#include <QString>
#include <QStringList>

#include <functional>

class QAction;
class QActionGroup;
class QButtonGroup;
class QComboBox;
class QDockWidget;
class QLabel;
class QMenu;
class QSlider;
class QTabWidget;
class QTimer;
class QToolBar;
class QToolButton;
class QVBoxLayout;
class CodeEditor;
class CodeHighlighter;


// Scroll area of a flowchart: Ctrl+wheel zooms, Shift+wheel scrolls horizontally.
class AfcScrollArea : public QScrollArea
{
  Q_OBJECT
  protected:
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
  signals:
    void mouseDown();
    void zoomStepped(int);
    // The same as zoomStepped with the cursor position (viewport coordinates).
    void zoomSteppedAt(int steps, const QPoint &position);
    void scrollStepped(int);
  public:
    explicit AfcScrollArea(QWidget* parent = nullptr) : QScrollArea(parent) { }
  private:
    int fZoomDelta = 0; // accumulated Ctrl+wheel angle (trackpads send small steps)
};


// One open document: a flowchart in its scroll area (a tab of the main window).
class DocumentTab : public AfcScrollArea
{
  Q_OBJECT
  public:
    explicit DocumentTab(QWidget *parent = nullptr);
    QFlowChart *flowChart() const { return fChart; }

    // Absolute path of the file; empty for a new or imported document.
    QString fileName() const { return fFileName; }
    void setFileName(const QString &fileName);
    // Name of an unsaved document ("Untitled 2", or the imported function).
    QString title() const { return fTitle; }
    void setTitle(const QString &title);
    // The file name without the path, or the title.
    QString displayName() const;

    bool isModified() const { return fModified; }
    void setModified(bool modified);
    // A new, empty, unmodified document without a file: Open replaces it.
    bool isPristine() const;

    double zoom() const;
    // Sets the zoom factor keeping the point at anchor (viewport coordinates;
    // default: the centre of the viewport) in place.
    void setZoom(double zoom, const QPoint &anchor = QPoint(-1, -1));
    // Zoom factor that shows the whole flowchart in the viewport.
    double fitZoom() const;

  signals:
    void modificationChanged(bool modified);
    void pinchZoomed(double factor, const QPoint &position);
    void smartZoomRequested();

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool viewportEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

  private:
    bool handleGesture(QEvent *event, QWidget *receiver);

    void syncCanvas();

    QWidget *fCanvas;    // the chart with a margin around it (the widget of the scroll area)
    QFlowChart *fChart;
    QString fFileName;
    QString fTitle;
    bool fModified = false;
};


class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, Qt::WindowFlags flags = {});
    ~MainWindow() override;

    // The flowchart of the current tab (never null while the window exists).
    QFlowChart *document() const;
    DocumentTab *currentTab() const;
    DocumentTab *tab(int index) const;
    int tabCount() const;
    QTabWidget *tabWidget() const { return tabs; }

    // Restores the window geometry, dock layout, view options and recent files of the previous session.
    void readSettings();
    // When false the window never writes QSettings (tests, --screenshot).
    void setSettingsPersistent(bool persistent) { fPersistSettings = persistent; }

    // Opens an .afc file in a new tab (or activates the tab that shows it already,
    // or reuses an untouched empty tab); shows an error message and returns false on failure.
    bool openDocument(const QString &fn);
    // The same without a message box: the error is stored in errorMessage.
    bool openDocument(const QString &fn, QString *errorMessage);
    // Opens a file given by the OS (Finder, drag and drop): .afc or a C/C++ source.
    bool requestOpenDocument(const QString &fn);
    // Opens a new tab with an empty flowchart and returns it.
    DocumentTab *newDocument();
    // Opens doc in a new unsaved (modified) tab titled title.
    DocumentTab *newDocument(const QDomDocument &doc, const QString &title, QString *errorMessage = nullptr);
    // Closes the tab (asks about unsaved changes); false if the user cancelled.
    bool closeTab(int index);

    // Builds the flowchart of a function of a C/C++ source file (the default
    // function when functionName is empty) and shows it in a new unsaved tab.
    bool importSource(const QString &sourceFile, const QString &functionName = QString());
    // The same without a message box: the error is stored in errorMessage.
    bool importSource(const QString &sourceFile, const QString &functionName, QString *errorMessage);
    // The import itself, without any UI.
    static bool importFromSource(const QString &sourceFile, const QString &functionName,
                                 QDomDocument *doc, QString *errorMessage);
    // Shows the "Import from source code" dialog (optionally with a file loaded).
    void showImportDialog(const QString &sourceFile = QString());

    QStringList recentFiles() const { return fRecentFiles; }

    // Test hooks: replace the modal "save changes?" question and the save file dialog.
    void setSavePromptHook(const std::function<QMessageBox::StandardButton(const QString &documentName)> &hook)
    { fSavePromptHook = hook; }
    void setSaveFileNameHook(const std::function<QString(const QString &suggestedName)> &hook)
    { fSaveFileNameHook = hook; }

    static QString getFilterFor(const QString & fileExt);
    static QString getWriteFormatFilter();
    // Embedded translations: locale name (e.g. "ru_RU") -> native language name.
    static QMap<QString, QString> enumLanguages();
    // File suffixes of C/C++ sources accepted by the importer (drag and drop).
    static bool isSourceFile(const QString &fileName);

public slots:
    void retranslateUi();
    void slotFileNew();
    void slotFileOpen();
    void slotFileImport();
    void slotFileSave();
    void slotFileSaveAs();
    void slotFileExportPng();
    void slotFileExportSvg();
    void slotFileExportPdf();
    void slotFileCopyImage();
    void slotFilePrint();
    void slotFileCloseTab();
    void slotEditCopy();
    void slotEditCut();
    void slotEditPaste();
    void slotEditDelete();
    void slotEditSelectAll();
    void slotHelpAbout();
    void slotHelpAboutQt();
    void slotChangeLanguage();
    void slotReloadGenerators();
    void slotCancelInsertion();

    void zoomIn();
    void zoomOut();
    void zoomReset();
    void zoomToFit();
    // Zoom factor of the current tab in percent (25..500).
    void setZoomPercent(int percent);
    int zoomPercent() const;

    void updateActions();
    void generateCode();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void currentTabChanged(int index);
    void chartStatusChanged();
    void chartChanged();
    void chartModified();
    void chartEditBlock(QBlock *block);
    void toolClicked(int id);
    void codeLangChanged(int index);
    void codeCopy();
    void codeSaveAs();
    // Builds the flowchart of the current tab from the (edited) C/C++ code of the code panel.
    void codeToFlowchart();
    // Discards the edits in the code panel and shows the generated code again.
    void codeRevert();
    void codeTextChanged();
    void updateRecentMenu();
    void clearRecentFiles();
    void setMonochrome(bool monochrome);
    void setAssignSymbol(const QString &symbol);

private:
    // ---- widgets
    QTabWidget *tabs;
    QDockWidget *dockTools;
    QDockWidget *dockCode;
    THelpWindow *helpWindow;
    QWidget *toolsWidget;
    QVBoxLayout *toolsLayout;
    QButtonGroup *toolGroup;
    QList<QString> toolXml;      // insertion buffers by tool id (id 0 = Select)
    QList<QString> toolTitles;
    QComboBox *codeLanguage;
    CodeEditor *codeText;
    CodeHighlighter *codeHighlighter;
    QToolButton *btnCodeCopy;
    QToolButton *btnCodeSave;
    QWidget *codeApplyRow;       // "Code -> flowchart" controls (C and C++ only)
    QLabel *codeHint;
    QToolButton *btnCodeRevert;
    QToolButton *btnCodeApply;
    QToolBar *toolBar;
    QLabel *statusLabel;
    QToolButton *btnZoomOut;
    QToolButton *btnZoomIn;
    QToolButton *btnZoomValue;
    QSlider *zoomSlider;
    QTimer *codeTimer;          // debounces the code generation while editing

    // ---- actions and menus
    QAction *actNew;
    QAction *actOpen;
    QAction *actImport;
    QAction *actSave;
    QAction *actSaveAs;
    QAction *actExportPng;
    QAction *actExportSvg;
    QAction *actExportPdf;
    QAction *actCopyImage;
    QAction *actPrint;
    QAction *actCloseTab;
    QAction *actExit;
    QAction *actUndo;
    QAction *actRedo;
    QAction *actCut;
    QAction *actCopy;
    QAction *actPaste;
    QAction *actDelete;
    QAction *actSelectAll;
    QAction *actTools;
    QAction *actCode;
    QAction *actZoomIn;
    QAction *actZoomOut;
    QAction *actZoomReset;
    QAction *actZoomFit;
    QAction *actMonochrome;
    QAction *actHelp;
    QAction *actAbout;
    QAction *actAboutQt;
    QAction *actClearRecent;
    QAction *actCancelInsertion;
    QActionGroup *assignGroup;
    QList<QAction *> actAssign;
    QActionGroup *languageGroup;
    QList<QAction *> actLanguages;

    QMenu *menuFile;
    QMenu *menuRecent;
    QMenu *menuExport;
    QMenu *menuEdit;
    QMenu *menuView;
    QMenu *menuAssign;
    QMenu *menuLanguage;
    QMenu *menuHelp;

    // ---- state
    bool fPersistSettings = true;
    bool fMonochrome = false;
    QString fAssignSymbol;
    QStringList fRecentFiles;
    QString fLastDir;
    QString fLastExportDir;
    int fUntitledCounter = 0;
    bool fCodeEdited = false;   // the user changed the code in the code panel
    bool fSettingCode = false;  // the code panel is being filled by the generator
    std::function<QMessageBox::StandardButton(const QString &)> fSavePromptHook;
    std::function<QString(const QString &)> fSaveFileNameHook;

    void createActions();
    void createMenus();
    void createToolBar();
    void createToolPanel();
    void rebuildToolPanel();
    void createCodePanel();
    void createStatusBar();
    void updateIcons();
    void updateToolPanelStyle();
    void writeSettings();
    void saveSetting(const QString &key, const QVariant &value);
    void addRecentFile(const QString &fileName);
    DocumentTab *addTab(DocumentTab *tab, int index = -1);
    void installCloseButton(DocumentTab *tab);
    void connectTab(DocumentTab *tab);
    DocumentTab *tabOf(QObject *chart) const;
    int findTab(const QString &fileName) const;
    void updateTabText(DocumentTab *tab);
    void updateTitle();
    void updateZoomWidgets();
    void applyChartStyle();
    void applyChartStyle(DocumentTab *tab);
    void insertTool(const QString &xml, bool multi);
    bool maybeSave(DocumentTab *tab);
    bool saveTab(DocumentTab *tab, bool saveAs = false);
    QString exportFileName(const QString &suffix, const QString &filter);
    bool exportTo(const QString &fn, const QByteArray &format);
    void showStatusHint();
    bool codeIsImportable() const;   // the code panel shows C or C++
    void updateCodePanelState();
};

QString afceVersion();
// Installs the translators (AFCE and Qt) for localeName and makes it the
// default locale. Can be called again at run time to switch the language.
void setApplicationLocale(const QString &localeName);
// Chooses the UI language: the saved one if available, otherwise the best
// match of the system UI languages among the embedded translations (en_US by default).
QString resolveApplicationLocale(const QString &preferred);

#endif // MAINWINDOW_H
