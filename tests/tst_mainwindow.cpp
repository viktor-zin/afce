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

// Main window tests: languages, tabs, opening / saving files, close prompts,
// recent files, zoom, tool panel, generators, wheel handling (offscreen).

#include "afceutil.h"
#include "blockcatalog.h"
#include "mainwindow.h"
#include "thelpwindow.h"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QWheelEvent>
#include <QtTest>

#include <memory>

namespace {

QString sample(const QString &name)
{
    return QStringLiteral(AFCE_TEST_DATA_DIR "/samples/") + name;
}

QString robustness(const QString &name)
{
    return QStringLiteral(AFCE_TEST_DATA_DIR "/robustness/") + name;
}

QString dockTitle(MainWindow &w, const char *objectName)
{
    QDockWidget *dock = w.findChild<QDockWidget *>(QLatin1String(objectName));
    return dock ? dock->windowTitle() : QString();
}

QAction *languageAction(MainWindow &w, const QString &locale)
{
    const QList<QAction *> actions = w.findChildren<QAction *>();
    for (QAction *action : actions) {
        if (action->isCheckable() && action->data().toString() == locale)
            return action;
    }
    return nullptr;
}

// Closes the next modal message box and remembers its text.
class MessageBoxCloser : public QObject
{
public:
    explicit MessageBoxCloser(QObject *parent = nullptr) : QObject(parent)
    {
        m_timer.setInterval(10);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this]() {
            if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                text = box->text();
                ++count;
                box->done(QMessageBox::Ok);
            }
        });
        m_timer.start();
    }
    QString text;
    int count = 0;

private:
    QTimer m_timer;
};

// Fills the line edits of the next modal dialog (in order) and presses OK.
class DialogFiller : public QObject
{
public:
    explicit DialogFiller(const QStringList &values, QObject *parent = nullptr) : QObject(parent), m_values(values)
    {
        m_timer.setInterval(10);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this]() {
            QDialog *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            title = dialog->windowTitle();
            const QList<QLineEdit *> edits = dialog->findChildren<QLineEdit *>();
            for (int i = 0; i < edits.size() && i < m_values.size(); ++i)
                edits.at(i)->setText(m_values.at(i));
            ++count;
            QDialogButtonBox *buttons = dialog->findChild<QDialogButtonBox *>();
            QPushButton *ok = buttons ? buttons->button(QDialogButtonBox::Ok) : nullptr;
            if (ok && ok->isEnabled())
                ok->click();
            else
                dialog->reject();
        });
        m_timer.start();
    }
    QString title;
    int count = 0;

private:
    QStringList m_values;
    QTimer m_timer;
};

void wheel(QWidget *target, int delta, Qt::KeyboardModifiers modifiers)
{
    const QPointF pos(target->width() / 2.0, target->height() / 2.0);
    QWheelEvent event(pos, target->mapToGlobal(pos), QPoint(), QPoint(0, delta), Qt::NoButton, modifiers,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(target, &event);
}

} // namespace

class Test_mainwindow : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();
    void embeddedLanguages();
    void resolveLocale();
    void switchLanguageAtRunTime();
    void qtTranslations();
    void generators();
    void helpSearchPaths();
    void openDocument();
    void openInvalidDocument();
    void importWithoutFunctions();
    void wheelScrollsAndZooms();
    void destroyWithDocument();
    void newTabs();
    void openInTabs();
    void modifiedMarker();
    void closeTabPrompts();
    void closeWindowPrompts();
    void saveUntitled();
    void recentFiles();
    void importIntoTab();
    void zoom();
    void toolPanel();
    void codeFollowsTab();
    void codeToFlowchart();
    void dropFiles();
    void editByDoubleClick();
};

void Test_mainwindow::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    afce::setupSearchPaths();
    setApplicationLocale(QStringLiteral("en_US"));
}

void Test_mainwindow::cleanup()
{
    setApplicationLocale(QStringLiteral("en_US"));
}

void Test_mainwindow::embeddedLanguages()
{
    const QMap<QString, QString> languages = MainWindow::enumLanguages();
    QVERIFY(languages.contains(QStringLiteral("en_US")));
    QVERIFY(languages.contains(QStringLiteral("ru_RU")));
    QVERIFY(languages.contains(QStringLiteral("uk_UA")));
    QCOMPARE(languages.value(QStringLiteral("ru_RU")), QStringLiteral("Русский"));
}

void Test_mainwindow::resolveLocale()
{
    QCOMPARE(resolveApplicationLocale(QStringLiteral("ru_RU")), QStringLiteral("ru_RU"));
    QCOMPARE(resolveApplicationLocale(QStringLiteral("uk_UA")), QStringLiteral("uk_UA"));
    QCOMPARE(resolveApplicationLocale(QStringLiteral("ru")), QStringLiteral("ru_RU"));
    QCOMPARE(resolveApplicationLocale(QStringLiteral("en_GB")), QStringLiteral("en_US"));
    QVERIFY(MainWindow::enumLanguages().contains(resolveApplicationLocale(QString())));
}

void Test_mainwindow::switchLanguageAtRunTime()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QCOMPARE(dockTitle(w, "dock_tools"), QStringLiteral("Tools"));
    QCOMPARE(w.menuBar()->actions().constFirst()->text(), QStringLiteral("&File"));

    // the language menu switches the whole UI, several times in a row
    const struct { const char *locale; const char *tools; const char *file; } steps[] = {
        {"ru_RU", "Инструменты", "&Файл"},
        {"uk_UA", "Знаряддя", "&Файл"},
        {"en_US", "Tools", "&File"},
        {"ru_RU", "Инструменты", "&Файл"},
    };
    for (const auto &step : steps) {
        QAction *action = languageAction(w, QString::fromLatin1(step.locale));
        QVERIFY2(action, step.locale);
        action->trigger();
        QCoreApplication::processEvents();
        QCOMPARE(QLocale().name(), QString::fromLatin1(step.locale));
        QCOMPARE(dockTitle(w, "dock_tools"), QString::fromUtf8(step.tools));
        QCOMPARE(w.menuBar()->actions().constFirst()->text(), QString::fromUtf8(step.file));
        QVERIFY(action->isChecked());
        // strings of the flowchart are translated too
        QCOMPARE(QCoreApplication::translate("QBlock", "BEGIN") == QLatin1String("BEGIN"),
                 QByteArray(step.locale) == "en_US");
    }
}

void Test_mainwindow::qtTranslations()
{
    // Qt's own strings (standard buttons of the close prompt, standard dialogs,
    // context menus) follow the UI language. Their translations come from the Qt
    // installation or are embedded as :/i18n/qtbase_<language>.qm at build time.
    const QString installed = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    const bool embedded = QFile::exists(QStringLiteral(":/i18n/qtbase_ru.qm"))
                          && QFile::exists(QStringLiteral(":/i18n/qtbase_uk.qm"));
    if (!embedded && !(QFile::exists(installed + QStringLiteral("/qtbase_ru.qm"))
                       && QFile::exists(installed + QStringLiteral("/qtbase_uk.qm"))))
        QSKIP("Qt's translations (qtbase_ru.qm, qtbase_uk.qm) were not found at build time (AFCE_QT_TRANSLATIONS_DIR)");

    const struct { const char *locale; const char *cancel; const char *save; } steps[] = {
        {"ru_RU", "Отмена", "Сохранить"},
        {"uk_UA", "Скасувати", "Зберегти"},
        {"en_US", "Cancel", "Save"},
    };
    for (const auto &step : steps) {
        setApplicationLocale(QString::fromLatin1(step.locale));
        QCOMPARE(QCoreApplication::translate("QPlatformTheme", "Cancel"), QString::fromUtf8(step.cancel));
        QMessageBox box;
        box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        QVERIFY(box.button(QMessageBox::Save));
        QCOMPARE(box.button(QMessageBox::Save)->text().remove(QLatin1Char('&')), QString::fromUtf8(step.save));
    }
}

void Test_mainwindow::generators()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QComboBox *combo = w.findChild<QComboBox *>(QStringLiteral("code_language"));
    QVERIFY(combo);
    QVERIFY(combo->count() >= 13);
    QVERIFY(combo->findData(QStringLiteral("c")) >= 0);
    QVERIFY(combo->findData(QStringLiteral("py")) >= 0);
    QCOMPARE(combo->currentData().toString(), QStringLiteral("cpp"));
    QCOMPARE(combo->currentText(), QStringLiteral("C++"));
    QVERIFY(w.openDocument(sample(QStringLiteral("gcd.afc"))));
    QPlainTextEdit *code = w.findChild<QPlainTextEdit *>(QStringLiteral("code_text"));
    QVERIFY(code);
    QVERIFY(!code->isReadOnly()); // C++ can be edited and turned back into the flowchart
    QVERIFY(!code->toPlainText().trimmed().isEmpty());
    combo->setCurrentIndex(combo->findData(QStringLiteral("py")));
    emit combo->activated(combo->currentIndex());
    QVERIFY(code->isReadOnly());
}

void Test_mainwindow::helpSearchPaths()
{
    const QStringList ru = THelpWindow::helpSearchPaths(QStringLiteral("ru_RU"));
    QVERIFY(ru.contains(QStringLiteral(":/help/ru_RU")));
    QVERIFY(ru.contains(QStringLiteral(":/help/en_US")));
    QVERIFY(ru.indexOf(QStringLiteral(":/help/ru_RU")) < ru.indexOf(QStringLiteral(":/help/en_US")));
    const QStringList de = THelpWindow::helpSearchPaths(QStringLiteral("de_DE"));
    QVERIFY(!de.isEmpty());
    QVERIFY(de.contains(QStringLiteral(":/help/en_US")));

    THelpWindow help;
    QVERIFY(help.textBrowser->toPlainText().size() > 50);
    setApplicationLocale(QStringLiteral("ru_RU"));
    QCoreApplication::processEvents();
    help.retranslateUi();
    QVERIFY(help.textBrowser->toPlainText().contains(QStringLiteral("Присваивание")));
    // links and pictures are found through the search paths
    help.textBrowser->setSource(QUrl(QStringLiteral("if.html")));
    QVERIFY(help.textBrowser->toPlainText().size() > 50);
    const QVariant picture = help.textBrowser->loadResource(QTextDocument::ImageResource, QUrl(QStringLiteral("algif.png")));
    QVERIFY(picture.isValid());
    QVERIFY(picture.toByteArray().startsWith("\x89PNG"));
    setApplicationLocale(QStringLiteral("en_US"));
    help.retranslateUi();
    QCOMPARE(help.textBrowser->source(), QUrl(QStringLiteral("if.html")));
    help.home();
    QVERIFY(help.textBrowser->toPlainText().contains(QStringLiteral("Assign")));
}

void Test_mainwindow::openDocument()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QVERIFY(w.openDocument(sample(QStringLiteral("all_blocks.afc"))));
    QVERIFY(w.windowTitle().contains(QStringLiteral("all_blocks.afc")));
    QVERIFY(w.document()->root()->item(0)->items.size() > 5);
    QVERIFY(!w.document()->canUndo());
    // the untouched empty tab was replaced
    QCOMPARE(w.tabCount(), 1);
    QVERIFY(!w.currentTab()->isModified());
    QCOMPARE(w.tabWidget()->tabText(0), QStringLiteral("all_blocks.afc"));
}

void Test_mainwindow::openInvalidDocument()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QVERIFY(w.openDocument(sample(QStringLiteral("max_of_two.afc"))));
    const QString title = w.windowTitle();
    const QString xml = w.document()->toString();

    for (const QString &file : {robustness(QStringLiteral("not_afc.xml")), robustness(QStringLiteral("broken.afc")),
                                robustness(QStringLiteral("missing.afc"))}) {
        MessageBoxCloser closer;
        QVERIFY(!w.openDocument(file));
        QCOMPARE(closer.count, 1);
        QVERIFY(!closer.text.isEmpty());
        // nothing changed
        QCOMPARE(w.windowTitle(), title);
        QCOMPARE(w.document()->toString(), xml);
        QCOMPARE(w.tabCount(), 1);
    }
}

void Test_mainwindow::importWithoutFunctions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("empty.c"));
    QFile file(source);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("/* no functions here */\nint x;\n");
    file.close();

    QDomDocument doc;
    QString error;
    QVERIFY(!MainWindow::importFromSource(source, QString(), &doc, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!MainWindow::importFromSource(dir.filePath(QStringLiteral("missing.c")), QString(), &doc, &error));
    QVERIFY(error.contains(QStringLiteral("missing.c")));

    MainWindow w;
    w.setSettingsPersistent(false);
    MessageBoxCloser closer;
    QVERIFY(!w.importSource(source));
    QCOMPARE(closer.count, 1);
}

void Test_mainwindow::wheelScrollsAndZooms()
{
    AfcScrollArea area;
    QWidget *content = new QWidget;
    content->resize(400, 4000);
    area.setWidget(content);
    area.resize(300, 300);
    area.show();
    QVERIFY(QTest::qWaitForWindowExposed(&area));
    QSignalSpy zoom(&area, &AfcScrollArea::zoomStepped);
    QScrollBar *vsb = area.verticalScrollBar();
    QCOMPARE(vsb->value(), 0);

    // a plain wheel scrolls
    wheel(area.viewport(), -120, Qt::NoModifier);
    QVERIFY(vsb->value() > 0);
    QCOMPARE(zoom.count(), 0);

    // Ctrl+wheel zooms and does not scroll
    const int value = vsb->value();
    wheel(area.viewport(), 120, Qt::ControlModifier);
    QCOMPARE(zoom.count(), 1);
    QCOMPARE(zoom.at(0).at(0).toInt(), 1);
    QCOMPARE(vsb->value(), value);

    // Shift+wheel scrolls horizontally
    content->resize(4000, 4000);
    QScrollBar *hsb = area.horizontalScrollBar();
    QCOMPARE(hsb->value(), 0);
    wheel(area.viewport(), -120, Qt::ShiftModifier);
    QVERIFY(hsb->value() > 0);
    QCOMPARE(vsb->value(), value);

    // small trackpad steps are accumulated
    for (int i = 0; i < 4; ++i)
        wheel(area.viewport(), -30, Qt::ControlModifier);
    QCOMPARE(zoom.count(), 2);
    QCOMPARE(zoom.at(1).at(0).toInt(), -1);
}

void Test_mainwindow::destroyWithDocument()
{
    auto w = std::make_unique<MainWindow>();
    w->setSettingsPersistent(false);
    w->show();
    QVERIFY(QTest::qWaitForWindowExposed(w.get()));
    QVERIFY(w->openDocument(sample(QStringLiteral("nested.afc"))));
    w->document()->setActiveBlock(w->document()->root()->item(0)->item(0));
    w->document()->setStatus(QFlowChart::Insertion);
    w.reset(); // must not crash or call back into the destroyed window
}

namespace {

QAction *action(MainWindow &w, const char *name)
{
    return w.findChild<QAction *>(QLatin1String(name));
}

// A change of the flowchart as the editor makes it (one undo step).
void modify(QFlowChart *chart)
{
    chart->makeUndo();
    chart->root()->item(0)->append(new QBlock(QStringLiteral("process")));
    chart->root()->item(0)->items.last()->attributes.insert(QStringLiteral("text"), QStringLiteral("x()"));
    chart->realignObjects();
}

} // namespace

void Test_mainwindow::newTabs()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QCOMPARE(w.tabCount(), 1);
    QFlowChart *first = w.document();
    QVERIFY(first);
    QVERIFY(w.currentTab()->isPristine());
    action(w, "action_new")->trigger();
    QCOMPARE(w.tabCount(), 2);
    QVERIFY(w.document() != first);
    QCOMPARE(w.tabWidget()->currentIndex(), 1);
    QVERIFY(w.tabWidget()->tabText(0) != w.tabWidget()->tabText(1));
    // the edit actions work on the current tab
    modify(w.document());
    QVERIFY(action(w, "action_undo")->isEnabled());
    w.tabWidget()->setCurrentIndex(0);
    QVERIFY(!action(w, "action_undo")->isEnabled());
    // closing the last tab leaves an empty one
    QVERIFY(w.closeTab(0));
    QCOMPARE(w.tabCount(), 1);
    w.setSavePromptHook([](const QString &) { return QMessageBox::Discard; });
    QVERIFY(w.closeTab(0));
    QCOMPARE(w.tabCount(), 1);
    QVERIFY(w.currentTab()->isPristine());
}

void Test_mainwindow::openInTabs()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QVERIFY(w.openDocument(sample(QStringLiteral("gcd.afc"))));
    QVERIFY(w.openDocument(sample(QStringLiteral("factorial.afc"))));
    QCOMPARE(w.tabCount(), 2);
    QVERIFY(w.windowTitle().contains(QStringLiteral("factorial.afc")));
    // opening an open file activates its tab
    QVERIFY(w.openDocument(sample(QStringLiteral("gcd.afc"))));
    QCOMPARE(w.tabCount(), 2);
    QCOMPARE(w.tabWidget()->currentIndex(), 0);
    QVERIFY(w.windowTitle().contains(QStringLiteral("gcd.afc")));
    // a new tab that was edited is not replaced
    w.newDocument();
    modify(w.document());
    QVERIFY(w.openDocument(sample(QStringLiteral("nested.afc"))));
    QCOMPARE(w.tabCount(), 4);
}

void Test_mainwindow::modifiedMarker()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QVERIFY(w.openDocument(sample(QStringLiteral("gcd.afc"))));
    QVERIFY(!w.isWindowModified());
    QVERIFY(w.windowTitle().contains(QStringLiteral("[*]")));
    modify(w.document());
    QVERIFY(w.currentTab()->isModified());
    QVERIFY(w.isWindowModified());
    QCOMPARE(w.tabWidget()->tabText(0), QStringLiteral("gcd.afc*"));
}

void Test_mainwindow::closeTabPrompts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString copy = dir.filePath(QStringLiteral("gcd.afc"));
    QVERIFY(QFile::copy(sample(QStringLiteral("gcd.afc")), copy));
    QFile::setPermissions(copy, QFile::ReadOwner | QFile::WriteOwner);

    MainWindow w;
    w.setSettingsPersistent(false);
    QStringList asked;
    QMessageBox::StandardButton answer = QMessageBox::Cancel;
    w.setSavePromptHook([&](const QString &name) { asked << name; return answer; });

    // an unmodified tab closes without a question
    QVERIFY(w.openDocument(sample(QStringLiteral("factorial.afc"))));
    QVERIFY(w.openDocument(copy));
    QCOMPARE(w.tabCount(), 2);
    QVERIFY(w.closeTab(0));
    QVERIFY(asked.isEmpty());
    QCOMPARE(w.tabCount(), 1);

    // Cancel keeps the tab
    modify(w.document());
    const QString changed = w.document()->toString();
    QVERIFY(!w.closeTab(0));
    QCOMPARE(asked, QStringList{QStringLiteral("gcd.afc")});
    QCOMPARE(w.tabCount(), 1);
    QVERIFY(w.currentTab()->isModified());

    // Save writes the file, then closes the tab
    answer = QMessageBox::Save;
    QVERIFY(w.closeTab(0));
    QCOMPARE(asked.size(), 2);
    QCOMPARE(w.tabCount(), 1);
    QVERIFY(w.currentTab()->isPristine());
    QFlowChart saved;
    QVERIFY(saved.loadFile(copy));
    QCOMPARE(saved.toString(), changed);

    // Discard closes without saving
    QVERIFY(w.openDocument(copy));
    modify(w.document());
    answer = QMessageBox::Discard;
    QVERIFY(w.closeTab(0));
    QVERIFY(saved.loadFile(copy));
    QCOMPARE(saved.toString(), changed);
}

void Test_mainwindow::closeWindowPrompts()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    QVERIFY(w.openDocument(sample(QStringLiteral("gcd.afc"))));
    QVERIFY(w.openDocument(sample(QStringLiteral("factorial.afc"))));
    QVERIFY(w.openDocument(sample(QStringLiteral("nested.afc"))));
    modify(w.tab(0)->flowChart());
    modify(w.tab(2)->flowChart());
    QStringList asked;
    QMessageBox::StandardButton answer = QMessageBox::Cancel;
    w.setSavePromptHook([&](const QString &name) { asked << name; return answer; });
    // Cancel: the window stays open, the current (modified) document is asked first
    QVERIFY(!w.close());
    QVERIFY(w.isVisible());
    QCOMPARE(asked, QStringList{QStringLiteral("nested.afc")});
    // Discard: asked once per modified document
    asked.clear();
    answer = QMessageBox::Discard;
    QVERIFY(w.close());
    QCOMPARE(asked.size(), 2);
    QVERIFY(asked.contains(QStringLiteral("gcd.afc")));
}

void Test_mainwindow::saveUntitled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    MainWindow w;
    w.setSettingsPersistent(false);
    QString suggested;
    const QString target = dir.filePath(QStringLiteral("mine"));
    w.setSaveFileNameHook([&](const QString &name) { suggested = name; return target; });
    modify(w.document());
    const QString title = w.currentTab()->displayName();
    action(w, "action_save")->trigger();
    QVERIFY(suggested.endsWith(title + QStringLiteral(".afc")));
    QVERIFY(QFile::exists(target + QStringLiteral(".afc")));
    QVERIFY(!w.currentTab()->isModified());
    QCOMPARE(w.currentTab()->displayName(), QStringLiteral("mine.afc"));
    QCOMPARE(w.recentFiles().value(0), QDir::cleanPath(target + QStringLiteral(".afc")));
    // cancelled "Save as" changes nothing
    w.setSaveFileNameHook([](const QString &) { return QString(); });
    action(w, "action_save_as")->trigger();
    QCOMPARE(w.currentTab()->displayName(), QStringLiteral("mine.afc"));
}

void Test_mainwindow::recentFiles()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QVERIFY(w.recentFiles().isEmpty());
    const QStringList names = {"all_blocks.afc", "digits_sum.afc", "empty.afc", "factorial.afc", "gcd.afc",
                               "max_of_two.afc", "nested.afc", "old_format.afc"};
    for (const QString &name : names)
        QVERIFY(w.openDocument(sample(name)));
    QCOMPARE(w.recentFiles().size(), names.size());
    QVERIFY(w.recentFiles().constFirst().endsWith(QStringLiteral("old_format.afc")));
    // re-opening moves the file to the top, without duplicates
    QVERIFY(w.openDocument(sample(QStringLiteral("gcd.afc"))));
    QCOMPARE(w.recentFiles().size(), names.size());
    QVERIFY(w.recentFiles().constFirst().endsWith(QStringLiteral("gcd.afc")));

    QMenu *menu = w.findChild<QMenu *>(QStringLiteral("menu_recent"));
    QVERIFY(menu);
    int entries = 0;
    for (QAction *act : menu->actions()) {
        if (!act->data().toString().isEmpty())
            ++entries;
    }
    QCOMPARE(entries, names.size());

    // a missing file is dropped from the list
    QTemporaryDir dir;
    const QString gone = dir.filePath(QStringLiteral("gone.afc"));
    QVERIFY(QFile::copy(sample(QStringLiteral("gcd.afc")), gone));
    QVERIFY(w.openDocument(gone));
    QVERIFY(w.recentFiles().contains(QDir::cleanPath(gone)));
    QVERIFY(QFile::remove(gone));
    QString error;
    QVERIFY(!w.openDocument(gone, &error));
    QVERIFY(!w.recentFiles().contains(QDir::cleanPath(gone)));

    // at most 10 entries
    QTemporaryDir many;
    for (int i = 0; i < 12; ++i) {
        const QString fn = many.filePath(QStringLiteral("f%1.afc").arg(i));
        QVERIFY(QFile::copy(sample(QStringLiteral("gcd.afc")), fn));
        QVERIFY(w.openDocument(fn));
    }
    QCOMPARE(w.recentFiles().size(), 10);
    QVERIFY(w.recentFiles().constFirst().endsWith(QStringLiteral("f11.afc")));

    action(w, "action_clear_recent")->trigger();
    QVERIFY(w.recentFiles().isEmpty());
}

void Test_mainwindow::importIntoTab()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QVERIFY(w.openDocument(sample(QStringLiteral("gcd.afc"))));
    QString error;
    QVERIFY2(w.importSource(QStringLiteral(AFCE_TEST_DATA_DIR "/sources/max_of_two.c"), QStringLiteral("max"), &error),
             qPrintable(error));
    QCOMPARE(w.tabCount(), 2);
    QVERIFY(w.currentTab()->isModified());
    QVERIFY(w.currentTab()->fileName().isEmpty());
    QVERIFY(w.tabWidget()->tabText(1).startsWith(QStringLiteral("max")));

    // a document opened as a new tab with a title
    QDomDocument doc;
    QVERIFY(doc.setContent(QByteArrayLiteral("<algorithm version=\"1.3\" name=\"sum\"><branch><process text=\"a\"/></branch></algorithm>")));
    QVERIFY(w.newDocument(doc, QStringLiteral("sum")));
    QCOMPARE(w.tabCount(), 3);
    QCOMPARE(w.tabWidget()->tabText(2), QStringLiteral("sum*"));
    QVERIFY(MainWindow::isSourceFile(QStringLiteral("a.cpp")));
    QVERIFY(MainWindow::isSourceFile(QStringLiteral("B.HPP")));
    QVERIFY(!MainWindow::isSourceFile(QStringLiteral("a.afc")));
}

void Test_mainwindow::zoom()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    w.resize(1000, 700);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    QVERIFY(w.openDocument(sample(QStringLiteral("all_blocks.afc"))));
    QCOMPARE(w.zoomPercent(), 100);
    w.setZoomPercent(200);
    QCOMPARE(w.document()->zoom(), 2.0);
    action(w, "action_zoom_in")->trigger();
    QCOMPARE(w.zoomPercent(), 250);
    action(w, "action_zoom_out")->trigger();
    action(w, "action_zoom_out")->trigger();
    QCOMPARE(w.zoomPercent(), 175);
    action(w, "action_zoom_reset")->trigger();
    QCOMPARE(w.zoomPercent(), 100);
    w.setZoomPercent(1);
    QCOMPARE(w.zoomPercent(), 25);
    action(w, "action_zoom_fit")->trigger();
    const QSize viewport = w.currentTab()->viewport()->size();
    QVERIFY(w.document()->width() <= viewport.width());
    QVERIFY(w.document()->height() <= viewport.height());
    // each tab has its own zoom
    const int fit = w.zoomPercent();
    w.newDocument();
    QCOMPARE(w.zoomPercent(), 100);
    w.tabWidget()->setCurrentIndex(0);
    QCOMPARE(w.zoomPercent(), fit);
    // Ctrl+wheel zooms the current tab
    wheel(w.currentTab()->viewport(), 120, Qt::ControlModifier);
    QVERIFY(w.zoomPercent() > fit);
}

void Test_mainwindow::toolPanel()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    const QList<afce::BlockKind> kinds = afce::blockKinds();
    QVERIFY(!kinds.isEmpty());
    QAbstractButton *select = w.findChild<QAbstractButton *>(QStringLiteral("tool_select"));
    QVERIFY(select);
    QVERIFY(select->isChecked());
    for (const afce::BlockKind &kind : kinds)
        QVERIFY2(w.findChild<QAbstractButton *>(QStringLiteral("tool_") + kind.type), qPrintable(kind.type));

    QAbstractButton *tool = w.findChild<QAbstractButton *>(QStringLiteral("tool_") + kinds.constFirst().type);
    tool->click();
    QCOMPARE(w.document()->status(), int(QFlowChart::Insertion));
    QVERIFY(tool->isChecked());
    QVERIFY(!select->isChecked());
    QVERIFY(!action(w, "action_delete")->isEnabled());
    // Esc returns to the selection mode
    action(w, "action_cancel_insertion")->trigger();
    QCOMPARE(w.document()->status(), int(QFlowChart::Selectable));
    QVERIFY(select->isChecked());
    // switching tabs cancels the insertion
    tool->click();
    w.newDocument();
    QCOMPARE(w.tab(0)->flowChart()->status(), int(QFlowChart::Selectable));
    QVERIFY(select->isChecked());
}

void Test_mainwindow::codeFollowsTab()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QPlainTextEdit *code = w.findChild<QPlainTextEdit *>(QStringLiteral("code_text"));
    QVERIFY(code);
    QVERIFY(w.openDocument(sample(QStringLiteral("gcd.afc"))));
    const QString gcd = code->toPlainText();
    QVERIFY(w.openDocument(sample(QStringLiteral("factorial.afc"))));
    const QString factorial = code->toPlainText();
    QVERIFY(gcd != factorial);
    w.tabWidget()->setCurrentIndex(0);
    QCOMPARE(code->toPlainText(), gcd);
    // editing regenerates the code (debounced)
    modify(w.document());
    QTRY_VERIFY(code->toPlainText() != gcd);
}

void Test_mainwindow::codeToFlowchart()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    QVERIFY(w.openDocument(sample(QStringLiteral("gcd.afc"))));
    QComboBox *combo = w.findChild<QComboBox *>(QStringLiteral("code_language"));
    QPlainTextEdit *code = w.findChild<QPlainTextEdit *>(QStringLiteral("code_text"));
    QAbstractButton *apply = w.findChild<QAbstractButton *>(QStringLiteral("code_apply"));
    QAbstractButton *revert = w.findChild<QAbstractButton *>(QStringLiteral("code_revert"));
    QVERIFY(combo && code && apply && revert);
    QCOMPARE(combo->currentData().toString(), QStringLiteral("cpp"));
    QVERIFY(!code->isReadOnly());
    QVERIFY(!apply->isHidden());
    QVERIFY(revert->isHidden());

    const QString before = w.document()->toString();
    const QString generated = code->toPlainText();
    // the user edits the code: the generator no longer overwrites it
    code->setPlainText(QStringLiteral("int main()\n{\n    int s = 0;\n    for (int i = 1; i <= 10; i++)\n"
                                      "        s += i;\n    std::cout << \"Sum = \" << s << std::endl;\n}\n"));
    QVERIFY(!revert->isHidden());
    const QString edited = code->toPlainText();
    w.generateCode();
    QCOMPARE(code->toPlainText(), edited);

    // revert shows the generated code again
    revert->click();
    QCOMPARE(code->toPlainText(), generated);
    QVERIFY(revert->isHidden());

    // edit again and build the flowchart from the code
    code->setPlainText(edited);
    apply->click();
    const QString after = w.document()->toString();
    QVERIFY2(after.contains(QStringLiteral("<forc")), qPrintable(after));
    QVERIFY(after.contains(QStringLiteral("<ou")));
    QVERIFY(after.contains(QStringLiteral("s + i")));
    QVERIFY(revert->isHidden());
    QVERIFY(code->toPlainText().contains(QStringLiteral("for (int i = 1; i <= 10; i++)")));
    QVERIFY(w.currentTab()->isModified());
    // one undo step returns the previous flowchart
    QVERIFY(w.document()->canUndo());
    w.document()->undo();
    QCOMPARE(w.document()->toString(), before);

    // other languages are read-only and cannot be applied
    combo->setCurrentIndex(combo->findData(QStringLiteral("py")));
    emit combo->activated(combo->currentIndex());
    QVERIFY(code->isReadOnly());
    QVERIFY(apply->isHidden());
    combo->setCurrentIndex(combo->findData(QStringLiteral("c")));
    emit combo->activated(combo->currentIndex());
    QVERIFY(!code->isReadOnly());
    QVERIFY(!apply->isHidden());
}

void Test_mainwindow::dropFiles()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    QMimeData data;
    data.setUrls({QUrl::fromLocalFile(sample(QStringLiteral("gcd.afc"))),
                  QUrl::fromLocalFile(sample(QStringLiteral("factorial.afc")))});
    const QPoint pos = w.rect().center();
    QDragEnterEvent enter(pos, Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&w, &enter);
    QVERIFY(enter.isAccepted());
    QDropEvent drop(pos, Qt::CopyAction, &data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&w, &drop);
    QVERIFY(drop.isAccepted());
    QTRY_COMPARE(w.tabCount(), 2);
    QCOMPARE(w.currentTab()->displayName(), QStringLiteral("factorial.afc"));

    // other files are refused
    QMimeData other;
    other.setUrls({QUrl::fromLocalFile(QStringLiteral("/tmp/picture.png"))});
    QDragEnterEvent refused(pos, Qt::CopyAction, &other, Qt::LeftButton, Qt::NoModifier);
    refused.ignore();
    QApplication::sendEvent(&w, &refused);
    QVERIFY(!refused.isAccepted());
}

// Double-click on a block opens its property dialog (BlockEditDialog); OK
// changes the model with one undo step, marks the tab modified and
// regenerates the code. The start terminator edits the algorithm.
void Test_mainwindow::editByDoubleClick()
{
    MainWindow w;
    w.setSettingsPersistent(false);
    w.resize(1200, 800);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    QVERIFY(w.openDocument(sample(QStringLiteral("new_blocks.afc"))));
    QFlowChart *chart = w.document();
    QPlainTextEdit *code = w.findChild<QPlainTextEdit *>(QStringLiteral("code_text"));
    QVERIFY(code);
    QVERIFY(!w.currentTab()->isModified());

    QBlock *assign = chart->root()->item(0)->item(1);
    QCOMPARE(assign->type(), QStringLiteral("assign"));
    {
        DialogFiller filler({QStringLiteral("total"), QStringLiteral("42")});
        QTest::mouseDClick(chart, Qt::LeftButton, Qt::NoModifier, assign->symbolRect().center().toPoint());
        QCOMPARE(filler.count, 1);
        QCOMPARE(filler.title, QStringLiteral("Assignment"));
    }
    assign = chart->root()->item(0)->item(1);
    QCOMPARE(assign->attributes.value(QStringLiteral("dest")), QStringLiteral("total"));
    QCOMPARE(assign->attributes.value(QStringLiteral("src")), QStringLiteral("42"));
    QVERIFY(w.currentTab()->isModified());
    QVERIFY(chart->canUndo());
    QVERIFY(w.findChild<QAction *>(QStringLiteral("action_undo"))->isEnabled());
    QTRY_VERIFY(code->toPlainText().contains(QLatin1String("total = 42")));

    // the start terminator: name / parameters / return type of the algorithm
    {
        DialogFiller filler({QStringLiteral("renamed"), QStringLiteral("int k"), QStringLiteral("long")});
        QTest::mouseDClick(chart, Qt::LeftButton, Qt::NoModifier, chart->root()->symbolRect().center().toPoint());
        QCOMPARE(filler.count, 1);
        QCOMPARE(filler.title, QStringLiteral("Algorithm Properties"));
    }
    QCOMPARE(chart->root()->attributes.value(QStringLiteral("name")), QStringLiteral("renamed"));
    QCOMPARE(chart->root()->attributes.value(QStringLiteral("params")), QStringLiteral("int k"));
    QTRY_VERIFY(code->toPlainText().contains(QLatin1String("renamed")));

    // two undo steps bring the original back
    chart->undo();
    chart->undo();
    QCOMPARE(chart->root()->attributes.value(QStringLiteral("name")), QStringLiteral("classify"));
    QCOMPARE(chart->root()->item(0)->item(1)->attributes.value(QStringLiteral("dest")), QStringLiteral("s"));

    // jumps have no properties: a double-click opens nothing
    QBlock *caseBlock = chart->root()->item(0)->item(3);
    QCOMPARE(caseBlock->type(), QStringLiteral("case"));
    QBlock *jump = caseBlock->item(1)->item(1);
    QCOMPARE(jump->type(), QStringLiteral("break"));
    DialogFiller none({});
    QTest::mouseDClick(chart, Qt::LeftButton, Qt::NoModifier, jump->symbolRect().center().toPoint());
    QTest::qWait(50);
    QCOMPARE(none.count, 0);
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("afce-tests"));
    Test_mainwindow test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_mainwindow.moc"
