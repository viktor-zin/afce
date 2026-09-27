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

#include "tst_codeimport_fuzz.h"

#include "codeimporter.h"
#include "importtestutil.h"
#include "importtokenizer.h"

#include <QDir>
#include <QElapsedTimer>
#include <QtTest>

#if defined(__SANITIZE_ADDRESS__)
#define AFCE_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(undefined_behavior_sanitizer)
#define AFCE_SANITIZED 1
#endif
#endif

namespace {

#ifdef AFCE_SANITIZED
const int kSlowdown = 6;
#else
const int kSlowdown = 1;
#endif

const qint64 kCaseBudgetMs = 1000 * kSlowdown;

// Small deterministic generator (identical sequences on every platform).
class Rng
{
public:
    explicit Rng(quint64 seed) : m_state(seed * 0x9E3779B97F4A7C15ull + 1) {}
    quint32 next()
    {
        m_state ^= m_state >> 12;
        m_state ^= m_state << 25;
        m_state ^= m_state >> 27;
        return quint32((m_state * 0x2545F4914F6CDD1Dull) >> 32);
    }
    int bounded(int n) { return n <= 0 ? 0 : int(next() % quint32(n)); }

private:
    quint64 m_state;
};

QString dataDir()
{
    return QStringLiteral(IMPORT_TEST_DATA);
}

QStringList corpusFiles()
{
    return QDir(dataDir()).entryList({QStringLiteral("*.c"), QStringLiteral("*.cpp")}, QDir::Files, QDir::Name);
}

afce::ImportOptions randomOptions(Rng &rng)
{
    afce::ImportOptions o;
    const quint32 bits = rng.next();
    o.keepDeclarations = bits & 1;
    o.detectIO = bits & 2;
    o.callsAsSubroutine = bits & 4;
    o.expandCompoundAssign = bits & 8;
    o.exactOutput = bits & 16;
    o.omitMainReturn = bits & 32;
    o.forStyle = static_cast<afce::ImportOptions::ForStyle>((bits >> 6) % 3);
    return o;
}

const QStringList &snippets()
{
    static const QStringList s = {
        "{", "}", "(", ")", "[", "]", ";", ",", "if", "else", "for", "while", "do", "switch", "case 1:", "default:",
        "return", "break;", "continue;", "goto x;", "x:", "\n#if 1\n", "\n#endif\n", "\"", "'", "/*", "*/", "//",
        "R\"(", "\\\n", "<", ">", ">>", "::", "template <", "class X", "struct", "operator", "=", "+=", "++", "?", ":",
        "try", "catch (...)", "{{{{", "}}}}", "((((", "))))", "cout <<", "cin >>", "printf(\"%d\\n\", ", "scanf(\"%d\", &",
        "[[fallthrough]]", "__attribute__((", "asm(", "~", "...", "#", "##", "Ж", "0x", "1'", "u8\"", "L'",
        "namespace", "extern \"C\"", "requires", "->", "auto [", "for (;", "switch (x) {", "case", "else if (y)",
        "std::", "typedef", "using", "enum class", "static_assert(", "throw", "new", "delete", "&&", "*", "&",
    };
    return s;
}

// Source split into segments: every token with the whitespace and comments
// before it, so that mutations keep the rest of the text intact.
QStringList segments(const QString &source, afce::cimport::Lang lang)
{
    QStringList result;
    const QVector<afce::cimport::Token> tokens = afce::cimport::tokenize(source, lang);
    int prev = 0;
    for (const afce::cimport::Token &t : tokens) {
        result << source.mid(prev, t.end() - prev);
        prev = t.end();
    }
    result << source.mid(prev);
    return result;
}

QString mutate(QStringList segs, Rng &rng, int operations)
{
    for (int k = 0; k < operations && !segs.isEmpty(); ++k) {
        const int i = rng.bounded(segs.size());
        switch (rng.bounded(5)) {
        case 0: // delete
            segs.removeAt(i);
            break;
        case 1: // duplicate
            segs.insert(i, segs.at(i));
            break;
        case 2: // swap with the next one
            if (i + 1 < segs.size())
                segs.swapItemsAt(i, i + 1);
            break;
        case 3: // insert a snippet
            segs.insert(i, QLatin1Char(' ') + snippets().at(rng.bounded(snippets().size())));
            break;
        default: { // delete a run
            const int n = qMin(1 + rng.bounded(20), int(segs.size()) - i);
            for (int j = 0; j < n; ++j)
                segs.removeAt(i);
            break;
        }
        }
    }
    return segs.join(QString());
}

qint64 g_slowest = 0;

// Imports every function (up to a limit) and checks the result. Returns an
// error description or an empty string.
QString check(const QString &source, const QString &fileName, const afce::ImportOptions &options, int maxFunctions = 400)
{
    QElapsedTimer timer;
    timer.start();
    afce::CodeImporter importer(options);
    importer.parse(source, fileName);
    const QList<afce::FunctionInfo> fns = importer.functions();
    const int def = importer.defaultFunctionIndex();
    if ((fns.isEmpty() && def != -1) || (!fns.isEmpty() && (def < 0 || def >= fns.size())))
        return QStringLiteral("bad default function index");
    for (const afce::FunctionInfo &f : fns) {
        if (f.startOffset < 0 || f.endOffset > source.size() || f.startOffset >= f.endOffset || f.line < 1)
            return QStringLiteral("bad function info for '%1': [%2, %3) line %4")
                .arg(f.name)
                .arg(f.startOffset)
                .arg(f.endOffset)
                .arg(f.line);
    }
    const int n = qMin(int(fns.size()), maxFunctions);
    for (int i = -1; i <= n; ++i) {
        const QDomDocument doc = importer.flowchart(i);
        const QStringList problems = importtest::validate(doc);
        if (!problems.isEmpty())
            return QStringLiteral("function %1: %2").arg(i).arg(problems.mid(0, 3).join(QLatin1String("; ")));
        for (const afce::Diagnostic &d : importer.diagnostics()) {
            if (d.message.isEmpty() || d.line < 0 || d.column < 0)
                return QStringLiteral("bad diagnostic");
        }
    }
    const qint64 ms = timer.elapsed();
    g_slowest = qMax(g_slowest, ms);
    if (ms > kCaseBudgetMs)
        return QStringLiteral("too slow: %1 ms").arg(ms);
    return QString();
}

QString failureContext(const QString &error, const QString &source)
{
    return error + QLatin1String("\n--- source (first 2000 chars) ---\n") + source.left(2000);
}

} // namespace

void TestFuzz::initTestCase()
{
    QVERIFY(corpusFiles().size() >= 25);
}

void TestFuzz::truncations_data()
{
    QTest::addColumn<QString>("file");
    for (const QString &f : corpusFiles())
        QTest::newRow(qPrintable(f)) << f;
}

void TestFuzz::truncations()
{
    QFETCH(QString, file);
    const QString source = importtest::readFile(dataDir() + QLatin1Char('/') + file);
    Rng rng(qHash(file));
    QList<int> cuts;
    const int steps = 60;
    for (int k = 0; k <= steps; ++k)
        cuts << int(qint64(source.size()) * k / steps);
    for (int k = 0; k < 20; ++k)
        cuts << rng.bounded(int(source.size()) + 1);
    for (int cut : std::as_const(cuts)) {
        const QString truncated = source.left(cut);
        const QString error = check(truncated, file, randomOptions(rng));
        ++m_cases;
        QVERIFY2(error.isEmpty(), qPrintable(failureContext(error, truncated)));
        // the tail alone (a file that starts in the middle of a function)
        const QString tail = source.mid(cut);
        const QString error2 = check(tail, file, afce::ImportOptions());
        ++m_cases;
        QVERIFY2(error2.isEmpty(), qPrintable(failureContext(error2, tail)));
    }
}

void TestFuzz::tokenMutations_data()
{
    QTest::addColumn<QString>("file");
    for (const QString &f : corpusFiles())
        QTest::newRow(qPrintable(f)) << f;
}

void TestFuzz::tokenMutations()
{
    QFETCH(QString, file);
    const QString source = importtest::readFile(dataDir() + QLatin1Char('/') + file);
    const auto lang = file.endsWith(QLatin1String(".c")) ? afce::cimport::Lang::C : afce::cimport::Lang::Cpp;
    const QStringList segs = segments(source, lang);
    Rng rng(qHash(file) ^ 0x5eed);
    for (int variant = 0; variant < 50; ++variant) {
        const int operations = 1 + rng.bounded(variant < 25 ? 4 : 40);
        const QString mutated = mutate(segs, rng, operations);
        const QString error = check(mutated, file, randomOptions(rng));
        ++m_cases;
        QVERIFY2(error.isEmpty(), qPrintable(failureContext(error, mutated)));
    }
}

void TestFuzz::tokenSoup()
{
    static const QStringList vocabulary = {
        "int", "void", "char", "auto", "const", "static", "struct", "class", "enum", "union", "namespace", "template",
        "typename", "public", "private", "virtual", "operator", "if", "else", "for", "while", "do", "switch", "case",
        "default", "break", "continue", "return", "goto", "try", "catch", "throw", "new", "delete", "sizeof", "using",
        "typedef", "extern", "\"C\"", "x", "y", "i", "n", "main", "f", "std", "cout", "cin", "endl", "printf", "scanf",
        "getchar", "0", "1", "42", "3.14", "'a'", "\"s\"", "\"%d\\n\"", "{", "}", "(", ")", "[", "]", ";", ",", ":",
        "::", "<", ">", "<<", ">>", "=", "==", "+=", "-=", "++", "--", "+", "-", "*", "/", "%", "&", "&&", "|", "||",
        "!", "~", "?", ".", "->", "...", "#", "\n#define X\n", "\n#if 0\n", "\n#endif\n", "[[x]]", "R\"(r)\"",
    };
    Rng rng(20260922);
    for (int c = 0; c < 300; ++c) {
        const int length = 20 + rng.bounded(c < 250 ? 400 : 4000);
        QString source;
        // many soups contain a function frame so that statements get parsed
        if (c % 2 == 0)
            source = QStringLiteral("int main() {\n");
        for (int k = 0; k < length; ++k) {
            source += vocabulary.at(rng.bounded(vocabulary.size()));
            source += rng.bounded(8) == 0 ? QLatin1Char('\n') : QLatin1Char(' ');
        }
        if (c % 4 == 0)
            source += QLatin1Char('}');
        const QString error = check(source, c % 3 == 0 ? QStringLiteral("soup.c") : QStringLiteral("soup.cpp"),
                                    randomOptions(rng));
        ++m_cases;
        QVERIFY2(error.isEmpty(), qPrintable(failureContext(error, source)));
    }
}

void TestFuzz::oddCharacters()
{
    QStringList sources;
    sources << QStringLiteral("int main() { x = 1; }").replace(QLatin1Char('x'), QChar(0)); // NUL
    sources << QStringLiteral("int main() { printf(\"a\x01\x02\x1b[0m\\n\"); s = \"\x7f\"; }");
    QString surrogates = QStringLiteral("int main() { s = \"?\"; cout << \"?\" << endl; y? = 2; }");
    surrogates[surrogates.indexOf(QLatin1Char('?'))] = QChar(0xD800);
    surrogates[surrogates.indexOf(QLatin1Char('?'))] = QChar(0xDFFF);
    surrogates[surrogates.indexOf(QLatin1Char('?'))] = QChar(0xFFFF);
    sources << surrogates;
    sources << QStringLiteral("int main()\r{\r  x = 1;\r  if (x)\r    y = 2;\r}\r"); // CR only
    sources << QStringLiteral("int main()\f{\v x = 1; }");
    sources << QStringLiteral("int main() { x = 1; } \\");
    sources << QStringLiteral("int main() { s = R\"delim(unterminated");
    sources << QStringLiteral("int main() { c = '");
    sources << QStringLiteral("int main() { /* never closed");
    sources << QStringLiteral("#define A \\\n\\\n\\\n") + QString(5000, QLatin1Char('\\')) + QStringLiteral("\nint f(){}");
    sources << QStringLiteral("int main() { ") + QString(100000, QLatin1Char('a')) + QStringLiteral(" = 1; }");
    sources << QStringLiteral("int main() { s = \"") + QString(100000, QLatin1Char('b')) + QStringLiteral("\"; }");
    sources << QStringLiteral("int main() { ") + QString(20000, QLatin1Char(';')) + QStringLiteral(" }");
    sources << QStringLiteral("int main() { ") + QString(20000, QLatin1Char('#')) + QStringLiteral(" }");
    sources << QString(QChar(0xFEFF)) + QStringLiteral("int main() {") + QChar(0xFEFF) + QStringLiteral("x = 1; }");
    sources << QStringLiteral("int main() { x = 1; }\n\x1a garbage");
    sources << QStringLiteral("int main() { \U0001F600 = 1; cout << \"\U0001F600\" << endl; }");
    sources << QStringLiteral("int m\\\nain() { re\\\nturn 0; }");
    sources << QStringLiteral("}}}}}}}} int main() { ))))]]]] x = 1; }");
    sources << QStringLiteral("int main() { case 1: default: break; continue; else; catch (...) {} }");
    for (const QString &source : std::as_const(sources)) {
        for (int lang = 0; lang < 2; ++lang) {
            const QString error = check(source, lang ? QStringLiteral("odd.cpp") : QStringLiteral("odd.c"),
                                        afce::ImportOptions());
            ++m_cases;
            QVERIFY2(error.isEmpty(), qPrintable(failureContext(error, source.left(200))));
        }
    }
}

void TestFuzz::deepNestingEverywhere_data()
{
    QTest::addColumn<QString>("source");
    const int d = 5000;
    auto rep = [](const QString &s, int n) {
        QString r;
        r.reserve(s.size() * n);
        for (int i = 0; i < n; ++i)
            r += s;
        return r;
    };
    QTest::newRow("namespaces") << rep(QStringLiteral("namespace a { "), d) + QStringLiteral("int f() { return 1; }")
            + rep(QStringLiteral("} "), d);
    QTest::newRow("classes") << rep(QStringLiteral("struct S { "), d) + QStringLiteral("int f() { return 1; }")
            + rep(QStringLiteral("}; "), d);
    QTest::newRow("global initializer parens") << QStringLiteral("int x = ") + rep(QStringLiteral("("), d)
            + QStringLiteral("1") + rep(QStringLiteral(")"), d) + QStringLiteral("; int f() { return x; }");
    QTest::newRow("template arguments") << QStringLiteral("void f() { ") + rep(QStringLiteral("A<"), d)
            + QStringLiteral("int") + rep(QStringLiteral(">"), d) + QStringLiteral(" x = 1; }");
    QTest::newRow("unclosed parens in body") << QStringLiteral("void f() { g") + rep(QStringLiteral("("), d)
            + QStringLiteral(" }");
    QTest::newRow("ifs without braces") << QStringLiteral("void f() { ") + rep(QStringLiteral("if (x) "), d)
            + QStringLiteral("y(); }");
    QTest::newRow("else chain") << QStringLiteral("void f() { if (a) b(); ") + rep(QStringLiteral("else if (a) b(); "), d)
            + QStringLiteral("}");
    QTest::newRow("switches") << QStringLiteral("void f() { ") + rep(QStringLiteral("switch (x) case 1: "), d)
            + QStringLiteral("y(); }");
    QTest::newRow("case labels") << QStringLiteral("void f() { switch (x) { ") + rep(QStringLiteral("case 1: "), d)
            + QStringLiteral("y(); } }");
    QTest::newRow("dos") << QStringLiteral("void f() { ") + rep(QStringLiteral("do "), d) + QStringLiteral("y(); }");
    QTest::newRow("tries") << QStringLiteral("void f() { ") + rep(QStringLiteral("try { "), d) + QStringLiteral("y();")
            + rep(QStringLiteral(" } catch (...) {}"), d) + QStringLiteral(" }");
    QTest::newRow("local structs") << QStringLiteral("void f() { ") + rep(QStringLiteral("struct S { "), d)
            + rep(QStringLiteral("}; "), d) + QStringLiteral("}");
    QTest::newRow("attributes") << QStringLiteral("void f() { ") + rep(QStringLiteral("[[x]] "), d)
            + QStringLiteral("y(); }");
    QTest::newRow("labels") << QStringLiteral("void f() { ") + rep(QStringLiteral("l: "), d) + QStringLiteral("y(); }");
    QTest::newRow("for headers") << QStringLiteral("void f() { ") + rep(QStringLiteral("for (;;) "), d)
            + QStringLiteral("y(); }");
    QTest::newRow("range fors") << QStringLiteral("void f() { ") + rep(QStringLiteral("for (auto x : v) "), d)
            + QStringLiteral("y(); }");
    QTest::newRow("braces mixed") << QStringLiteral("void f() { ") + rep(QStringLiteral("{ if (x) { while (y) { "), d)
            + QStringLiteral("z();") + rep(QStringLiteral(" } } }"), d) + QStringLiteral(" }");
    QTest::newRow("unbalanced mix") << QStringLiteral("void f() { ") + rep(QStringLiteral("if ( { [ ) ] } while ("), d);
}

void TestFuzz::deepNestingEverywhere()
{
    QFETCH(QString, source);
    for (const char *name : {"deep.c", "deep.cpp"}) {
        for (int style = 0; style < 3; ++style) {
            afce::ImportOptions o;
            o.forStyle = static_cast<afce::ImportOptions::ForStyle>(style);
            const QString error = check(source, QString::fromLatin1(name), o);
            ++m_cases;
            QVERIFY2(error.isEmpty(), qPrintable(error));
        }
    }
}

void TestFuzz::largeSyntheticFile()
{
    // ~20k lines: many functions mixing all supported constructs.
    QString source = QStringLiteral("#include <stdio.h>\n#include <iostream>\nusing namespace std;\n\n");
    int functions = 0;
    while (source.count(QLatin1Char('\n')) < 20000) {
        const QString n = QString::number(functions++);
        source += QStringLiteral(
            "// function %1\n"
            "int work%1(int n, const int *a)\n"
            "{\n"
            "    int sum = 0, best = a[0];\n"
            "    for (int i = 0; i < n; i++) {\n"
            "        if (a[i] > best)\n"
            "            best = a[i];\n"
            "        else if (a[i] < 0)\n"
            "            continue;\n"
            "        sum += a[i] * 2;\n"
            "    }\n"
            "    while (sum > 100) sum /= 2;\n"
            "    do { sum--; } while (sum > 50);\n"
            "    switch (sum % 3) {\n"
            "    case 0: printf(\"zero %d\\n\", sum); break;\n"
            "    case 1: cout << \"one \" << sum << endl;\n"
            "    default: sum++;\n"
            "    }\n"
            "    for (auto x : {1, 2, 3}) best += x;\n"
            "    scanf(\"%d\", &sum);\n"
            "    return sum + best;\n"
            "}\n\n")
                      .arg(n);
    }
    source += QStringLiteral("int main() { return work0(0, 0); }\n");
    const int lines = source.count(QLatin1Char('\n'));
    QVERIFY(lines >= 20000);

    QElapsedTimer timer;
    timer.start();
    afce::CodeImporter importer;
    QVERIFY(importer.parse(source, QStringLiteral("big.cpp")));
    const qint64 parseMs = timer.elapsed();
    QCOMPARE(importer.functions().size(), functions + 1);
    QCOMPARE(importer.defaultFunctionIndex(), functions);
    for (int i = 0; i < importer.functions().size(); ++i) {
        const QDomDocument doc = importer.flowchart(i);
        if (i % 97 == 0)
            QVERIFY(importtest::validate(doc).isEmpty());
    }
    const qint64 totalMs = timer.elapsed();
    qInfo().noquote() << QStringLiteral("synthetic file: %1 lines, %2 functions: parse %3 ms, parse + all charts %4 ms")
                             .arg(lines)
                             .arg(functions + 1)
                             .arg(parseMs)
                             .arg(totalMs);
    QVERIFY2(parseMs < 2000 * kSlowdown, qPrintable(QString::number(parseMs)));
    QVERIFY2(totalMs < 4000 * kSlowdown, qPrintable(QString::number(totalMs)));
}

void TestFuzz::cleanupTestCase()
{
    qInfo().noquote() << QStringLiteral("fuzz: %1 cases, slowest case %2 ms (budget %3 ms)")
                             .arg(m_cases)
                             .arg(g_slowest)
                             .arg(kCaseBudgetMs);
}

QTEST_GUILESS_MAIN(TestFuzz)
