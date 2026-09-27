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

// "Import from source code" dialog and the code editor (offscreen).
// With AFCE_IMPORTDIALOG_SCREENSHOTS=<dir> the test screenshots() also writes
// light and dark screenshots of the dialog to <dir>.

#include "codeeditor.h"
#include "importdialog.h"
#include "mainwindow.h"
#include "zvflowchart.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QTextBlock>
#include <QTextLayout>
#include <QtTest>

namespace {

const char *kProgram = R"(#include <stdio.h>

int sum(int a, int b)
{
    return a + b;
}

int max(int a, int b)
{
    if (a > b)
        return a;
    return b;
}

int main(void)
{
    int x, y;
    scanf("%d %d", &x, &y);
    for (int i = 0; i < 3; i++)
        printf("%d\n", sum(x, i));
    printf("max = %d\n", max(x, y));
    return 0;
}
)";

const char *kGotoProgram = R"(int main()
{
    int i = 0;
again:
    i++;
    if (i < 10)
        goto again;
    return 0;
}
)";

const char *kShowcase = R"(#include <iostream>
#include <vector>

// Greatest common divisor (Euclid)
int gcd(int a, int b)
{
    while (b != 0) {
        int r = a % b;
        a = b;
        b = r;
    }
    return a;
}

bool isPrime(int n)
{
    if (n < 2)
        return false;
    for (int d = 2; d * d <= n; d++)
        if (n % d == 0)
            return false;
    return true;
}

int main()
{
    int n;
    std::cout << "How many numbers? ";
    std::cin >> n;
    std::vector<int> numbers(n);
    for (int i = 0; i < n; i++)
        std::cin >> numbers[i];
    int primes = 0;
    for (int x : numbers) {
        if (isPrime(x))
            primes++;
    }
    switch (primes) {
    case 0:
        std::cout << "No primes" << std::endl;
        break;
    case 1:
        std::cout << "One prime" << std::endl;
        break;
    default:
        std::cout << "Primes: " << primes << std::endl;
    }
    std::cout << "gcd = " << gcd(numbers[0], numbers[1]) << std::endl;
    return 0;
}
)";

template <typename T>
T *child(ImportDialog &dialog, const char *name)
{
    T *w = dialog.findChild<T *>(QLatin1String(name));
    if (!w)
        qWarning("child %s not found", name);
    return w;
}

QStringList functionNames(const QList<QDomDocument> &docs)
{
    QStringList names;
    for (const QDomDocument &doc : docs)
        names << doc.documentElement().attribute(QStringLiteral("name"));
    return names;
}

bool containsElement(const QDomDocument &doc, const QString &tag)
{
    return !doc.elementsByTagName(tag).isEmpty();
}

QPalette darkPalette()
{
    QPalette p;
    const QColor window(0x2b, 0x2d, 0x30), base(0x1e, 0x1f, 0x22), text(0xdf, 0xe1, 0xe5);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(0x3a, 0x3d, 0x41));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::Highlight, QColor(0x35, 0x74, 0xf0));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, window);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(0x8c, 0x8f, 0x94));
    p.setColor(QPalette::Light, QColor(0x4a, 0x4d, 0x52));
    p.setColor(QPalette::Midlight, QColor(0x3f, 0x42, 0x46));
    p.setColor(QPalette::Mid, QColor(0x33, 0x35, 0x39));
    p.setColor(QPalette::Dark, QColor(0x19, 0x1a, 0x1c));
    p.setColor(QPalette::Shadow, Qt::black);
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6f, 0x73, 0x7a));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6f, 0x73, 0x7a));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6f, 0x73, 0x7a));
    return p;
}

} // namespace

class TestImportDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void listsFunctions();
    void importSelected();
    void importAll();
    void selectFunction();
    void fragment();
    void emptySource();
    void diagnostics();
    void debounce();
    void optionsRebuild();
    void loadFile();
    void loadMissingFile();
    void dropFile();
    void decodeSource();
    void highlighter();
    void editorHighlightLine();
    void screenshots();
};

void TestImportDialog::initTestCase()
{
    ImportDialog::setQuietMode(true);
}

void TestImportDialog::listsFunctions()
{
    ImportDialog dialog;
    dialog.setSource(QString::fromUtf8(kProgram), QStringLiteral("prog.c"));
    QComboBox *functions = child<QComboBox>(dialog, "functionCombo");
    QVERIFY(functions);
    QCOMPARE(functions->count(), 3);
    QCOMPARE(dialog.currentFunction(), QStringLiteral("main"));
    QVERIFY(functions->currentText().startsWith(QLatin1String("main(")));
    QVERIFY(child<QLabel>(dialog, "infoLabel")->text().contains(QLatin1String("3")));

    QFlowChart *chart = child<QFlowChart>(dialog, "previewChart");
    QVERIFY(chart);
    QVERIFY(chart->root());
    QVERIFY(!chart->root()->items.isEmpty());
    QVERIFY(!chart->root()->items.first()->items.isEmpty()); // the body branch has blocks
    QCOMPARE(chart->document().documentElement().tagName(), QStringLiteral("algorithm"));
    QCOMPARE(child<QStackedWidget>(dialog, "previewStack")->currentIndex(), 1);
    QVERIFY(child<QPushButton>(dialog, "importButton")->isEnabled());
    QVERIFY(child<QPushButton>(dialog, "importAllButton")->isEnabled());
}

void TestImportDialog::importSelected()
{
    ImportDialog dialog;
    dialog.setSource(QString::fromUtf8(kProgram));
    QPushButton *import = child<QPushButton>(dialog, "importButton");
    QVERIFY(import);
    import->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    const QList<QDomDocument> docs = dialog.importedDocuments();
    QCOMPARE(docs.size(), 1);
    QCOMPARE(dialog.importedTitles(), QStringList{QStringLiteral("main")});
    const QDomElement root = docs.first().documentElement();
    QCOMPARE(root.tagName(), QStringLiteral("algorithm"));
    QCOMPARE(root.firstChildElement().tagName(), QStringLiteral("branch"));
    QVERIFY(QFlowChart::isValidDocument(docs.first()));
    QVERIFY(containsElement(docs.first(), QStringLiteral("forc")));
}

void TestImportDialog::importAll()
{
    ImportDialog dialog;
    dialog.setSource(QString::fromUtf8(kProgram));
    child<QPushButton>(dialog, "importAllButton")->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    const QList<QDomDocument> docs = dialog.importedDocuments();
    QCOMPARE(docs.size(), 3);
    QCOMPARE(dialog.importedTitles(), (QStringList{QStringLiteral("sum"), QStringLiteral("max"), QStringLiteral("main")}));
    // (the importer leaves the name of main empty: the chart starts with "BEGIN")
    QCOMPARE(functionNames(docs).mid(0, 2), dialog.importedTitles().mid(0, 2));
    for (const QDomDocument &doc : docs)
        QVERIFY(QFlowChart::isValidDocument(doc));

    // cancelling discards the result
    ImportDialog cancelled;
    cancelled.setSource(QString::fromUtf8(kProgram));
    child<QPushButton>(cancelled, "cancelButton")->click();
    QCOMPARE(cancelled.result(), int(QDialog::Rejected));
    QVERIFY(cancelled.importedDocuments().isEmpty());
}

void TestImportDialog::selectFunction()
{
    ImportDialog dialog;
    dialog.selectFunction(QStringLiteral("max")); // before the source: remembered
    dialog.setSource(QString::fromUtf8(kProgram));
    QCOMPARE(dialog.currentFunction(), QStringLiteral("max"));
    dialog.selectFunction(QStringLiteral("sum"));
    QCOMPARE(dialog.currentFunction(), QStringLiteral("sum"));
    QFlowChart *chart = child<QFlowChart>(dialog, "previewChart");
    QCOMPARE(chart->document().documentElement().attribute(QStringLiteral("name")), QStringLiteral("sum"));
    child<QPushButton>(dialog, "importButton")->click();
    QCOMPARE(dialog.importedTitles(), QStringList{QStringLiteral("sum")});
    QCOMPARE(dialog.importedDocuments().first().documentElement().attribute(QStringLiteral("params")),
             QStringLiteral("int a, int b"));
    // unknown names are ignored
    ImportDialog other;
    other.setSource(QString::fromUtf8(kProgram));
    other.selectFunction(QStringLiteral("nosuch"));
    QCOMPARE(other.currentFunction(), QStringLiteral("main"));
}

void TestImportDialog::fragment()
{
    ImportDialog dialog;
    dialog.setSource(QStringLiteral("int s = 0;\nfor (int i = 1; i <= 10; i++)\n    s += i;\nprintf(\"%d\\n\", s);\n"));
    QComboBox *functions = child<QComboBox>(dialog, "functionCombo");
    QCOMPARE(functions->count(), 1);
    QCOMPARE(functions->currentText(), QStringLiteral("(whole fragment)"));
    QVERIFY(!child<QPushButton>(dialog, "importAllButton")->isEnabled());
    child<QPushButton>(dialog, "importButton")->click();
    QCOMPARE(dialog.result(), int(QDialog::Accepted));
    QCOMPARE(dialog.importedTitles(), QStringList{QStringLiteral("Fragment")});
    QVERIFY(containsElement(dialog.importedDocuments().first(), QStringLiteral("ou")));
}

void TestImportDialog::emptySource()
{
    ImportDialog dialog;
    QCOMPARE(child<QStackedWidget>(dialog, "previewStack")->currentIndex(), 0);
    QVERIFY(!child<QPushButton>(dialog, "importButton")->isEnabled());
    QVERIFY(!child<QPushButton>(dialog, "importAllButton")->isEnabled());
    QVERIFY(child<QLabel>(dialog, "emptyTitle")->text().contains(QLatin1String("Paste")));
    dialog.setSource(QStringLiteral("   \n"));
    QVERIFY(!child<QPushButton>(dialog, "importButton")->isEnabled());
    child<QPushButton>(dialog, "importButton")->click(); // disabled: nothing happens
    QVERIFY(dialog.importedDocuments().isEmpty());
}

void TestImportDialog::diagnostics()
{
    ImportDialog dialog;
    dialog.setSource(QString::fromUtf8(kGotoProgram));
    QListWidget *list = child<QListWidget>(dialog, "diagnosticsList");
    QVERIFY(list);
    int line = 0;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->flags() != Qt::NoItemFlags && list->item(i)->data(Qt::UserRole).toInt() > 0)
            line = list->item(i)->data(Qt::UserRole).toInt();
    QVERIFY2(line > 0, "a diagnostic with a line number is listed for goto");
    CodeEditor *editor = child<CodeEditor>(dialog, "sourceEditor");
    QVERIFY(!editor->lineMarkers().isEmpty());

    // double-click (activation) jumps to the line
    QListWidgetItem *item = nullptr;
    for (int i = 0; i < list->count() && !item; ++i)
        if (list->item(i)->data(Qt::UserRole).toInt() == line)
            item = list->item(i);
    emit list->itemActivated(item);
    QCOMPARE(editor->textCursor().blockNumber(), line - 1);

    // clean code: no diagnostics (a single disabled placeholder item)
    dialog.setSource(QString::fromUtf8(kProgram));
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0)->flags(), Qt::ItemFlags(Qt::NoItemFlags));
    QVERIFY(editor->lineMarkers().isEmpty());
}

void TestImportDialog::debounce()
{
    ImportDialog dialog;
    dialog.setSource(QString::fromUtf8(kProgram));
    QSignalSpy spy(&dialog, &ImportDialog::previewUpdated);
    CodeEditor *editor = child<CodeEditor>(dialog, "sourceEditor");
    QComboBox *functions = child<QComboBox>(dialog, "functionCombo");
    QElapsedTimer timer;
    timer.start();
    editor->setPlainText(QStringLiteral("int twice(int x)\n{\n    return 2 * x;\n}\n"));
    editor->appendPlainText(QStringLiteral("int main()\n{\n    return twice(2);\n}\n"));
    // not parsed yet
    QCOMPARE(spy.count(), 0);
    QCOMPARE(functions->count(), 3);
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
    QVERIFY2(timer.elapsed() >= 250, "the preview waits for the debounce delay");
    QCOMPARE(functions->count(), 2);
    QCOMPARE(dialog.currentFunction(), QStringLiteral("main"));
    // several quick edits: one update
    spy.clear();
    for (int i = 0; i < 5; ++i)
        editor->appendPlainText(QStringLiteral("// edit %1").arg(i));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
    QTest::qWait(400);
    QCOMPARE(spy.count(), 1);
}

void TestImportDialog::optionsRebuild()
{
    ImportDialog dialog;
    dialog.setSource(QString::fromUtf8(kProgram));
    QFlowChart *chart = child<QFlowChart>(dialog, "previewChart");
    QVERIFY(containsElement(chart->document(), QStringLiteral("forc")));
    QSignalSpy spy(&dialog, &ImportDialog::previewUpdated);
    QComboBox *forStyle = child<QComboBox>(dialog, "forStyleCombo");
    forStyle->setCurrentIndex(forStyle->findData(int(afce::ImportOptions::ForStyle::While)));
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
    QVERIFY(!containsElement(chart->document(), QStringLiteral("forc")));
    QVERIFY(containsElement(chart->document(), QStringLiteral("pre")));
    QCOMPARE(dialog.options().forStyle, afce::ImportOptions::ForStyle::While);

    child<QCheckBox>(dialog, "detectIO")->setChecked(false);
    dialog.updatePreview();
    QVERIFY(!containsElement(chart->document(), QStringLiteral("ou")));
    QVERIFY(!dialog.options().detectIO);

    afce::ImportOptions o;
    o.keepDeclarations = true;
    o.omitMainReturn = false;
    dialog.setOptions(o);
    QCOMPARE(dialog.options().forStyle, afce::ImportOptions::ForStyle::CStyle);
    QVERIFY(child<QCheckBox>(dialog, "keepDeclarations")->isChecked());
    QVERIFY(child<QCheckBox>(dialog, "keepMainReturn")->isChecked());
    dialog.updatePreview();
    QVERIFY(containsElement(chart->document(), QStringLiteral("return")));
}

void TestImportDialog::loadFile()
{
    ImportDialog dialog;
    QVERIFY(dialog.loadFile(QStringLiteral(AFCE_TEST_DATA_DIR "/sources/max_of_two.c")));
    QVERIFY(dialog.lastError().isEmpty());
    QVERIFY(dialog.windowTitle().contains(QLatin1String("max_of_two.c")));
    QCOMPARE(dialog.currentFunction(), QStringLiteral("main"));
    QCOMPARE(child<QComboBox>(dialog, "functionCombo")->count(), 2);
    QVERIFY(dialog.source().contains(QLatin1String("scanf")));
    QVERIFY(dialog.fileName().endsWith(QLatin1String("max_of_two.c")));
}

void TestImportDialog::loadMissingFile()
{
    ImportDialog dialog;
    dialog.setSource(QString::fromUtf8(kProgram));
    QVERIFY(!dialog.loadFile(QDir::temp().filePath(QStringLiteral("afce_no_such_file_4711.c"))));
    QVERIFY(!dialog.lastError().isEmpty());
    // the editor keeps its code
    QCOMPARE(child<QComboBox>(dialog, "functionCombo")->count(), 3);

    // binary files are rejected
    QTemporaryFile binary;
    QVERIFY(binary.open());
    binary.write(QByteArray("\x7f" "ELF\0\0\0\x01\x02", 9));
    binary.close();
    QVERIFY(!dialog.loadFile(binary.fileName()));
}

void TestImportDialog::dropFile()
{
    ImportDialog dialog;
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QStringLiteral(AFCE_TEST_DATA_DIR "/sources/max_of_two.c"))});
    QDragEnterEvent enter(QPoint(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&dialog, &enter);
    QVERIFY(enter.isAccepted());
    QDropEvent drop(QPointF(10, 10), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&dialog, &drop);
    QVERIFY(dialog.fileName().endsWith(QLatin1String("max_of_two.c")));
    QCOMPARE(dialog.currentFunction(), QStringLiteral("main"));

    // plain text drops are not files
    QMimeData text;
    text.setText(QStringLiteral("int x;"));
    QDragEnterEvent enterText(QPoint(10, 10), Qt::CopyAction, &text, Qt::LeftButton, Qt::NoModifier);
    enterText.setAccepted(false);
    QApplication::sendEvent(&dialog, &enterText);
    QVERIFY(!enterText.isAccepted());
}

void TestImportDialog::decodeSource()
{
    const QString russian = QStringLiteral("// Привет, мир! Ёё\nint main() {}\n");
    QCOMPARE(ImportDialog::decodeSource(russian.toUtf8()), russian);
    // UTF-8 with BOM
    QCOMPARE(ImportDialog::decodeSource(QByteArray("\xEF\xBB\xBF") + russian.toUtf8()), russian);
    // Windows-1251
    QByteArray cp1251("// \xCF\xF0\xE8\xE2\xE5\xF2, \xEC\xE8\xF0! \xA8\xB8\nint main() {}\n");
    QCOMPARE(ImportDialog::decodeSource(cp1251), russian);
    // Latin-1
    const QString german = QStringLiteral("// Größe und Länge\nint main() {}\n");
    QCOMPARE(ImportDialog::decodeSource(german.toLatin1()), german);
    // UTF-16 with BOM
    QByteArray utf16("\xFF\xFE");
    for (QChar c : russian)
        utf16.append(char(c.unicode() & 0xff)).append(char(c.unicode() >> 8));
    QCOMPARE(ImportDialog::decodeSource(utf16), russian);
}

void TestImportDialog::highlighter()
{
    // (a document without a layout emits no contentsChange: use the editor's document)
    CodeEditor editor;
    QTextDocument &doc = *editor.document();
    CodeHighlighter highlighter(&doc);
    highlighter.setLanguage(QStringLiteral("cpp"));
    QCOMPARE(highlighter.language(), QStringLiteral("cpp"));
    doc.setPlainText(QStringLiteral("#include <cstdio>\nint x = 42; // answer\n/* multi\nline */ return \"s\";\n"));
    auto formatAt = [&doc](int line, int column) {
        const QTextBlock block = doc.findBlockByNumber(line);
        const QList<QTextLayout::FormatRange> ranges = block.layout()->formats();
        for (const QTextLayout::FormatRange &r : ranges)
            if (column >= r.start && column < r.start + r.length)
                return r.format;
        return QTextCharFormat();
    };
    const QColor plain = QTextCharFormat().foreground().color();
    QVERIFY(formatAt(0, 0).foreground().color() != plain);            // #include
    QVERIFY(formatAt(0, 10).foreground().color() != plain);           // <cstdio>
    QVERIFY(formatAt(1, 0).foreground().color() != plain);            // int
    QVERIFY(formatAt(1, 8).foreground().color() != plain);            // 42
    QVERIFY(formatAt(1, 15).fontItalic());                            // // answer
    QVERIFY(formatAt(2, 3).fontItalic());                             // /* multi
    QVERIFY(formatAt(3, 2).fontItalic());                             // line */
    QVERIFY(!formatAt(3, 9).fontItalic());                            // return
    QCOMPARE(formatAt(3, 9).fontWeight(), int(QFont::DemiBold));      // keyword
    QVERIFY(formatAt(3, 16).foreground().color() != plain);           // "s"

    // Russian keywords of the e87 language, Pascal comments, Python triple quotes
    highlighter.setLanguage(QStringLiteral("e87"));
    doc.setPlainText(QStringLiteral("алг сумма\nнач | комментарий\nкон\n"));
    QCOMPARE(formatAt(0, 1).fontWeight(), int(QFont::DemiBold));
    QVERIFY(formatAt(1, 6).fontItalic());
    highlighter.setLanguage(QStringLiteral("pas"));
    doc.setPlainText(QStringLiteral("BEGIN { a\ncomment } WriteLn('x')\nEND.\n"));
    QCOMPARE(formatAt(0, 1).fontWeight(), int(QFont::DemiBold));
    QVERIFY(formatAt(1, 2).fontItalic());
    QVERIFY(!formatAt(1, 12).fontItalic());
    highlighter.setLanguage(QStringLiteral("py"));
    doc.setPlainText(QStringLiteral("s = \"\"\"a\n# not a comment\n\"\"\"\n# comment\n"));
    QVERIFY(!formatAt(1, 2).fontItalic());
    QVERIFY(formatAt(1, 2).foreground().color() != plain);
    QVERIFY(formatAt(3, 2).fontItalic());
    // every generator id is accepted
    for (const char *id : {"c", "cpp", "py", "pas", "js", "php", "perl", "ruby", "vbs", "autoit", "bas256", "freebasic", "e87"}) {
        highlighter.setLanguage(QString::fromLatin1(id));
        doc.setPlainText(QStringLiteral("x = \"a\" ' b # c // d ; e | f\n"));
    }
}

void TestImportDialog::editorHighlightLine()
{
    CodeEditor editor;
    editor.setPlainText(QStringLiteral("a\n    b\nc\n"));
    editor.highlightLine(2);
    QCOMPARE(editor.textCursor().blockNumber(), 1);
    QCOMPARE(editor.textCursor().positionInBlock(), 4);
    editor.highlightLine(99); // ignored
    QCOMPARE(editor.textCursor().blockNumber(), 1);
    QVERIFY(editor.lineNumbersVisible());
    QVERIFY(editor.lineNumberAreaWidth() > 0);
    editor.setLineNumbersVisible(false);
    QCOMPARE(editor.lineNumberAreaWidth(), 0);
    QCOMPARE(editor.tabStopDistance(), 4 * QFontMetricsF(editor.font()).horizontalAdvance(QLatin1Char(' ')));
}

void TestImportDialog::screenshots()
{
    const QString dir = qEnvironmentVariable("AFCE_IMPORTDIALOG_SCREENSHOTS");
    if (dir.isEmpty())
        QSKIP("set AFCE_IMPORTDIALOG_SCREENSHOTS=<dir> to write screenshots");
    QDir().mkpath(dir);
    // AFCE_SCREENSHOT_LANGUAGE=ru_RU: screenshots of the translated dialog
    const QString language = qEnvironmentVariable("AFCE_SCREENSHOT_LANGUAGE");
    if (!language.isEmpty())
        setApplicationLocale(language);
    const QPalette original = QApplication::palette();
    struct Variant { const char *name; QPalette palette; const char *code; const char *function; };
    const QList<Variant> variants = {
        {"light", original, kShowcase, "main"},
        {"dark", darkPalette(), kShowcase, "main"},
        {"light_gcd", original, kShowcase, "gcd"},
        {"dark_goto", darkPalette(), kGotoProgram, "main"},
        {"light_empty", original, "", ""},
    };
    for (const Variant &v : variants) {
        QApplication::setPalette(v.palette);
        ImportDialog dialog;
        dialog.resize(1180, 760);
        dialog.setSource(QString::fromUtf8(v.code), QStringLiteral("primes.cpp"));
        dialog.selectFunction(QString::fromLatin1(v.function));
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QTest::qWait(400);
        dialog.updatePreview();
        QTest::qWait(100);
        const QString file = QDir(dir).filePath(QStringLiteral("importdialog_%1.png").arg(QLatin1String(v.name)));
        QVERIFY(dialog.grab().save(file));
        qInfo("wrote %s", qPrintable(file));
    }
    // AFCE_IMPORTDIALOG_FILES="a.c;b.cpp#Stats::mean": the dialog with these files
    // (loaded like File > Import does), light and dark.
    const QStringList files = qEnvironmentVariable("AFCE_IMPORTDIALOG_FILES").split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &spec : files) {
        const QString path = spec.section(QLatin1Char('#'), 0, 0);
        const QString function = spec.section(QLatin1Char('#'), 1);
        for (const bool dark : {false, true}) {
            QApplication::setPalette(dark ? darkPalette() : original);
            ImportDialog dialog;
            dialog.resize(1180, 760);
            QVERIFY2(dialog.loadFile(path), qPrintable(dialog.lastError()));
            if (!function.isEmpty())
                dialog.selectFunction(function);
            dialog.show();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            QTest::qWait(400);
            dialog.updatePreview();
            QTest::qWait(100);
            QString name = QFileInfo(path).fileName() + (function.isEmpty() ? QString() : QLatin1Char('_') + function);
            name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.]")), QStringLiteral("_"));
            const QString file = QDir(dir).filePath(
                QStringLiteral("importdialog_%1_%2.png").arg(name, dark ? QStringLiteral("dark") : QStringLiteral("light")));
            QVERIFY(dialog.grab().save(file));
            qInfo("wrote %s", qPrintable(file));
        }
    }
    QApplication::setPalette(original);
}

QTEST_MAIN(TestImportDialog)
#include "tst_importdialog.moc"
