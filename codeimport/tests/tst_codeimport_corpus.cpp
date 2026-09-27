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

#include "tst_codeimport_corpus.h"

#include "codeimporter.h"
#include "importtestutil.h"

#include <QDir>
#include <QHash>
#include <QtTest>

namespace {

// Key structural properties per file: the function list and, per function,
// lines that must appear in its chart (default options, compared trimmed).
struct Expectation
{
    QStringList functions;
    QHash<QString, QStringList> lines;
};

QHash<QString, Expectation> expectations()
{
    QHash<QString, Expectation> e;
    e[QStringLiteral("c01_sum_average.c")] = {
        {"main"},
        {{"main", {"io n", "if (n <= 0)", "forc (i = 0; i < n; i++)", "ou \"x[\",i + 1,\"] = \"",
                   "assign sum := sum + x", "ou \"Average = \",sum / n", "return 1"}}}};
    e[QStringLiteral("c02_primes.c")] = {
        {"is_prime", "main"},
        {{"is_prime", {"forc (d = 3; d <= (int)sqrt(n); d += 2)", "return n == 2", "return 1"}},
         {"main", {"continue", "break", "ou \"\\nTotal: \",count", "assign count := count + 1"}}}};
    e[QStringLiteral("c03_gcd_lcm.c")] = {
        {"gcd", "gcd_rec", "lcm", "main"},
        {{"gcd", {"pre (b != 0)", "assign t := a % b", "return a"}},
         {"gcd_rec", {"return gcd_rec(b, a % b)"}},
         {"main", {"io a,b", "ou \"lcm = \",lcm(a, b)"}}}};
    e[QStringLiteral("c04_factorial_fibonacci.c")] = {
        {"factorial", "factorial_iter", "fib", "fib_rec", "main"},
        {{"factorial", {"return n * factorial(n - 1)"}},
         {"factorial_iter", {"assign f := f * i"}},
         {"fib", {"pre (n-- > 0)"}},
         {"main", {"if (scanf(\"%d\", &n) != 1 || n < 0)", "ou \"invalid input\"",
                   "ou \"fib(\",i,\") = \",fib(i),\", \",fib_rec(i)"}}}};
    e[QStringLiteral("c05_bubble_sort.c")] = {
        {"print_array", "bubble_sort", "main"},
        {{"bubble_sort", {"post (swapped)", "assign a[j + 1] := tmp", "assign swapped := true"}},
         {"main", {"assign a := {5, 3, 9, 1, 7, 2, 8, 6, 4, 0}", "call bubble_sort(a, SIZE)"}}}};
    e[QStringLiteral("c06_selection_insertion.c")] = {
        {"swap", "selection_sort", "insertion_sort", "main"},
        {{"swap", {"assign *x := *y"}},
         {"selection_sort", {"call swap(&a[i], &a[min])"}},
         {"insertion_sort", {"pre (j >= 0 && a[j] > key)", "assign j := j - 1"}}}};
    e[QStringLiteral("c07_binary_search.c")] = {
        {"binary_search", "search_rec", "main"},
        {{"binary_search", {"pre (lo <= hi)", "assign mid := lo + (hi - lo) / 2", "return -1"}},
         {"main", {"ou \"Found at \",pos", "ou \"Not found\""}}}};
    e[QStringLiteral("c08_matrix.c")] = {
        {"read_matrix", "multiply", "transpose", "trace", "main"},
        {{"read_matrix", {"io m[i][j]"}},
         {"multiply", {"assign c[i][j] := c[i][j] + (a[i][k] * b[k][j])"}},
         {"main", {"ou c[i][j]", "ou"}}}};
    e[QStringLiteral("c09_strings.c")] = {
        {"my_strlen", "reverse", "is_palindrome", "count_words", "main"},
        {{"is_palindrome", {"continue", "return 0"}},
         {"count_words", {"forc (; *s; s++)"}},
         {"main", {"ou \"Reversed: \\\"\",line,\"\\\"\"", "assign line[strcspn(line, \"\\n\")] := '\\0'"}}}};
    e[QStringLiteral("c10_students_struct.c")] = {
        {"average", "best_student", "main"},
        {{"main", {"io group.count", "io s->name", "io s->grades[j]", "assign s := &group.list[i]"}}}};
    e[QStringLiteral("c11_menu_switch.c")] = {
        {"main"},
        {{"main", {"post (choice != 0)", "case (choice)", "= 1", "= 2", "= 3", "= 0", "default",
                   "ou \"Unknown command \",choice", "assign balance := balance - amount"}}}};
    e[QStringLiteral("c12_input_validation.c")] = {
        {"flush_line", "read_int_in_range", "main"},
        {{"flush_line", {"pre ((c = getchar()) != '\\n' && c != EOF)"}},
         {"read_int_in_range", {"post (ok != 1 || value < lo || value > hi)", "assign ok := scanf(\"%d\", &value)",
                                "continue"}},
         {"main", {"io answer"}}}};
    e[QStringLiteral("c13_digits.c")] = {
        {"digit_sum", "reverse_number", "count_digits", "is_armstrong", "main"},
        {{"count_digits", {"post (n != 0)"}},
         {"is_armstrong", {"assign k := count_digits(n)", "assign t := n", "assign sum := 0"}},
         {"main", {"io n"}}}};
    e[QStringLiteral("c14_preprocessor.c")] = {
        {"platform", "platform", "find_max", "main"},
        {{"find_max", {"assign m := MAX(m, a[i])", "ou \"max of \",n,\" values: \",m"}},
         {"main", {"call LOG(\"started\")", "ou \"Max: \",find_max(values, ARRAY_LEN(values))"}}}};
    e[QStringLiteral("c15_linked_list.c")] = {
        {"push_front", "print_list", "reverse_list", "free_list", "main"},
        {{"push_front", {"assign n := malloc(sizeof *n)", "call exit(1)", "assign n->next := head"}},
         {"print_list", {"forc (p = head; p != NULL; p = p->next)", "ou \"NULL\""}},
         {"main", {"pre (scanf(\"%d\", &x) == 1 && x != 0)"}}}};
    e[QStringLiteral("c16_patterns.c")] = {
        {"triangle", "multiplication_table", "find_pair", "main"},
        {{"triangle", {"ou ' '", "ou '*'", "ou"}},
         {"find_pair", {"process goto found", "ou i,\" * \",j,\" = \",target"}},
         {"main", {"continue", "break"}}}};
    e[QStringLiteral("c17_quadratic.c")] = {
        {"solve", "main"},
        {{"solve", {"process *x1 = *x2 = -b / (2 * a)", "return 2"}},
         {"main", {"io a,b,c", "case (roots)", "= -1", "ou \"x1 = \",x1,\", x2 = \",x2"}}}};
    e[QStringLiteral("c18_bits.c")] = {
        {"count_bits", "is_power_of_two", "print_binary", "main"},
        {{"count_bits", {"assign count := count + (x & 1u)", "assign x := x >> 1"}},
         {"main", {"assign x := x | mask", "assign x := x & (~0x10u)"}}}};

    e[QStringLiteral("cpp01_hello_io.cpp")] = {
        {"main"},
        {{"main", {"io name", "io age", "ou \"Hello, \",name,\"!\"", "ou \"In ten years you will be \",age + 10",
                   "ou \"Age cannot be negative\""}}}};
    e[QStringLiteral("cpp02_vector_stats.cpp")] = {
        {"mean", "median", "main"},
        {{"median", {"call std::sort(v.begin(), v.end())"}},
         {"main", {"assign [minIt, maxIt] := std::minmax_element(values.begin(), values.end())",
                   "foreach (const auto &x : values)", "ou x", "io x"}}}};
    e[QStringLiteral("cpp03_word_count.cpp")] = {
        {"normalize", "countWords", "main"},
        {{"normalize", {"foreach (char c : word)"}},
         {"countWords", {"pre (std::getline(in, line))", "pre (words >> w)", "assign counts[w] := counts[w] + 1"}},
         {"main", {"foreach (const auto &[word, n] : counts)", "assign it := counts.find(\"the\")"}}}};
    e[QStringLiteral("cpp04_rectangle_class.cpp")] = {
        {"Rectangle::Rectangle", "Rectangle::Rectangle", "Rectangle::~Rectangle", "Rectangle::width",
         "Rectangle::height", "Rectangle::setWidth", "Rectangle::area", "Rectangle::isSquare", "Rectangle::count",
         "operator<<", "Rectangle::perimeter", "Rectangle::scale", "main"},
        {{"Rectangle::scale", {"assign width_ := width_ * k"}},
         {"main", {"process Rectangle a(3, 4)", "ou \"objects: \",Rectangle::count()"}}}};
    e[QStringLiteral("cpp05_templates.cpp")] = {
        {"maxOf", "maxOf<std::string>", "Stack::push", "Stack::empty", "Stack::size", "Stack<T>::pop", "length",
         "main"},
        {{"Stack<T>::pop", {"process throw std::out_of_range(\"pop from an empty stack\")", "return top"}},
         {"main", {"pre (!s.empty())", "ou s.pop(),' '"}}}};
    e[QStringLiteral("cpp06_bank_accounts.cpp")] = {
        {"Account::Account", "Account::withdraw", "Account::deposit", "Account::balance", "Account::owner",
         "Account::kind", "CreditAccount::CreditAccount", "CreditAccount::withdraw", "CreditAccount::kind", "main"},
        {{"Account::withdraw", {"assign balance_ := balance_ - amount"}},
         {"main", {"foreach (auto &acc : accounts)", "process acc->withdraw(120)"}}}};
    e[QStringLiteral("cpp07_cpp17_features.cpp")] = {
        {"parseNumber", "describe", "divmod", "main"},
        {{"parseNumber", {"return std::nullopt", "return {}"}},
         {"describe", {"if (std::is_integral_v<T>)"}},
         {"main", {"assign it := stock.find(\"apple\")", "assign n := parseNumber(\"42\")", "case (n.value_or(-1))",
                   "assign [q, r, d] := divmod(17, 5)", "foreach (auto &[name, count] : stock)"}}}};
    e[QStringLiteral("cpp08_calculator_menu.cpp")] = {
        {"readNumber", "main"},
        {{"readNumber", {"pre (true)", "process cin.clear()"}},
         {"main", {"post (op != 'q')", "case (op)", "= '*','x'", "assign result := pow(a, b)",
                   "ou \"Unknown operation '\",op,\"'\""}}}};
    e[QStringLiteral("cpp09_string_processing.cpp")] = {
        {"isPalindrome", "countVowels", "capitalizeWords", "caesar", "main"},
        {{"countVowels", {"forc (string::size_type i = 0; i < s.size(); ++i)"}},
         {"caesar", {"foreach (char &c : out)"}},
         {"main", {"pre (getline(cin, line))", "call transform(upper.begin(), upper.end(), upper.begin(), ::toupper)"}}}};
    e[QStringLiteral("cpp10_matrix_class.cpp")] = {
        {"Matrix::Matrix", "Matrix::rows", "Matrix::cols", "Matrix::operator()", "Matrix::operator()",
         "Matrix::operator+", "Matrix::operator*", "Matrix::identity", "operator<<", "main"},
        {{"Matrix::operator*", {"assign sum := sum + (data_[i][k] * o.data_[k][j])", "assign result(i, j) := sum"}},
         {"main", {"ou a + b,a * b"}}}};
    e[QStringLiteral("cpp11_recursion.cpp")] = {
        {"hanoi", "power", "permutations", "binarySearch", "ackermann", "main"},
        {{"hanoi", {"call hanoi(n - 1, from, via, to)", "ou \"Move disk \",n,\" from \",from,\" to \",to"}},
         {"permutations", {"call std::swap(s[k], s[i])"}},
         {"main", {"call hanoi(3, 'A', 'C', 'B')"}}}};
    e[QStringLiteral("cpp12_sieve.cpp")] = {
        {"sieve", "isPerfect", "main"},
        {{"sieve", {"process std::vector<bool> composite(n + 1, false)", "continue"}},
         {"isPerfect", {"assign sum := sum + (n / d)", "return false"}},
         {"main", {"foreach (int p : primes)", "if (++column == 10)", "ou \"\\nFound \",primes.size(),\" primes\""}}}};
    e[QStringLiteral("cpp13_cpp20_features.cpp")] = {
        {"sum", "gcd", "square", "average", "main"},
        {{"sum", {"process T total{}"}},
         {"main", {"process Version a{.major = 1, .minor = 2}", "assign i := 0", "foreach (auto x : v)"}}}};
    e[QStringLiteral("cpp14_game_of_life.cpp")] = {
        {"neighbours", "step", "print", "main"},
        {{"neighbours", {"forc (int dr = -1; dr <= 1; ++dr)", "continue"}},
         {"print", {"foreach (const auto &row : g)", "foreach (bool cell : row)"}},
         {"main", {"call print(g)", "assign g := step(g)"}}}};
    e[QStringLiteral("cpp15_queue_simulation.cpp")] = {
        {"nextEvent", "toString", "main"},
        {{"toString", {"case (e)", "= Event::Arrival", "return \"arrival\"", "return \"?\""}},
         {"main", {"case (e)", "process q.push({nextId++, tick})", "if (q.empty())", "break"}}}};
    return e;
}

QString dataDir()
{
    return QStringLiteral(IMPORT_TEST_DATA);
}

QStringList corpusFiles()
{
    return QDir(dataDir()).entryList({QStringLiteral("*.c"), QStringLiteral("*.cpp")}, QDir::Files, QDir::Name);
}

int countBlocks(const QDomElement &e)
{
    int n = 0;
    for (QDomElement c = e.firstChildElement(); !c.isNull(); c = c.nextSiblingElement()) {
        if (c.tagName() != QLatin1String("branch"))
            ++n;
        n += countBlocks(c);
    }
    return n;
}

QStringList optionSets()
{
    return {QString(), QStringLiteral("for=while"), QStringLiteral("for=arithmetic"),
            QStringLiteral("keepdecl,exact,keepreturn"), QStringLiteral("keepdecl,exact,keepreturn,for=while"),
            QStringLiteral("noio,nocalls,noexpand"), QStringLiteral("keepdecl,noio,for=arithmetic")};
}

} // namespace

void TestCorpus::initTestCase()
{
    QVERIFY2(corpusFiles().size() >= 25, qPrintable(dataDir()));
}

void TestCorpus::corpus_data()
{
    QTest::addColumn<QString>("file");
    for (const QString &f : corpusFiles())
        QTest::newRow(qPrintable(f)) << f;
}

void TestCorpus::corpus()
{
    QFETCH(QString, file);
    const QHash<QString, Expectation> all = expectations();
    QVERIFY2(all.contains(file), "every corpus file needs an expectation entry");
    const Expectation &expected = all[file];
    const QString source = importtest::readFile(dataDir() + QLatin1Char('/') + file);
    QVERIFY(!source.isEmpty());

    for (const QString &spec : optionSets()) {
        afce::CodeImporter importer(importtest::options(spec));
        QVERIFY(importer.parse(source, file));
        const QList<afce::FunctionInfo> fns = importer.functions();
        QStringList names;
        for (const afce::FunctionInfo &f : fns)
            names << f.name;
        QCOMPARE(names, expected.functions);
        for (const afce::Diagnostic &d : importer.diagnostics())
            QVERIFY2(d.severity != afce::Diagnostic::Error, qPrintable(d.message));
        if (spec.isEmpty())
            m_functions += fns.size();

        for (int i = 0; i < fns.size(); ++i) {
            const afce::FunctionInfo &f = fns.at(i);
            QVERIFY(f.startOffset >= 0 && f.endOffset <= source.size() && f.startOffset < f.endOffset);
            QCOMPARE(source.at(f.endOffset - 1), QLatin1Char('}'));
            const QDomDocument doc = importer.flowchart(i);
            ++m_documents;
            const QStringList problems = importtest::validate(doc);
            QVERIFY2(problems.isEmpty(), qPrintable(f.name + QLatin1String(": ") + problems.join(QLatin1Char('\n'))));
            for (const afce::Diagnostic &d : importer.diagnostics()) {
                QVERIFY2(d.severity != afce::Diagnostic::Error,
                         qPrintable(QStringLiteral("%1 [%2] %3:%4 %5").arg(f.name, spec).arg(d.line).arg(d.column).arg(d.message)));
            }
            const QString chart = importtest::serialize(doc);
            if (spec.isEmpty()) {
                m_blocks += countBlocks(doc.documentElement());
                QStringList lines;
                for (const QString &l : chart.split(QLatin1Char('\n')))
                    lines << l.trimmed();
                for (const QString &want : expected.lines.value(f.name)) {
                    if (!lines.contains(want))
                        qWarning().noquote() << f.name << "chart:\n" + chart;
                    QVERIFY2(lines.contains(want), qPrintable(f.name + QLatin1String(": missing line: ") + want));
                }
            }
        }
    }
}

void TestCorpus::allOptionCombinations()
{
    // Every combination of the boolean options and for styles on every
    // function of the corpus: always a valid document, never an Error.
    QStringList sources;
    for (const QString &f : corpusFiles())
        sources << importtest::readFile(dataDir() + QLatin1Char('/') + f);
    const QStringList files = corpusFiles();
    int documents = 0;
    for (int mask = 0; mask < 64; ++mask) {
        for (int style = 0; style < 3; ++style) {
            afce::ImportOptions o;
            o.keepDeclarations = mask & 1;
            o.detectIO = mask & 2;
            o.callsAsSubroutine = mask & 4;
            o.expandCompoundAssign = mask & 8;
            o.exactOutput = mask & 16;
            o.omitMainReturn = mask & 32;
            o.forStyle = static_cast<afce::ImportOptions::ForStyle>(style);
            // A subset of files per combination keeps the run time low while
            // every file is covered by many combinations.
            for (int f = (mask + style) % 4; f < files.size(); f += 4) {
                afce::CodeImporter importer(o);
                importer.parse(sources.at(f), files.at(f));
                for (int i = 0; i < importer.functions().size(); ++i) {
                    const QDomDocument doc = importer.flowchart(i);
                    ++documents;
                    const QStringList problems = importtest::validate(doc);
                    QVERIFY2(problems.isEmpty(), qPrintable(files.at(f) + QLatin1String(": ") + problems.join(QLatin1Char('\n'))));
                    for (const afce::Diagnostic &d : importer.diagnostics())
                        QVERIFY2(d.severity != afce::Diagnostic::Error, qPrintable(files.at(f) + QLatin1String(": ") + d.message));
                }
            }
        }
    }
    m_documents += documents;
}

void TestCorpus::cleanupTestCase()
{
    qInfo().noquote() << QStringLiteral("corpus: %1 files, %2 functions, %3 blocks (default options), %4 documents validated")
                             .arg(corpusFiles().size())
                             .arg(m_functions)
                             .arg(m_blocks)
                             .arg(m_documents);
}

QTEST_GUILESS_MAIN(TestCorpus)
