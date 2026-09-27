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

// Function discovery tests: names, return types, parameters, offsets.

#include "tst_codeimport_functions.h"

#include "codeimporter.h"
#include "importtestutil.h"

#include <QtTest>

using afce::CodeImporter;
using afce::FunctionInfo;

namespace {

const QChar kSep(0x1f);

// name | returnType | parameters | exact definition text
QString E(const QString &name, const QString &ret, const QString &params, const QString &definition)
{
    return QStringList({name, ret, params, definition}).join(kSep);
}

QString describe(const QString &source, const FunctionInfo &f)
{
    return E(f.name, f.returnType, f.parameters, source.mid(f.startOffset, f.endOffset - f.startOffset));
}

} // namespace

void TestFunctions::cleanup()
{
    const QStringList problems = importtest::takeRecordedProblems();
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

void TestFunctions::discovery_data()
{
    QTest::addColumn<QString>("options");
    QTest::addColumn<QString>("source");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("c basics") << "c" <<
        "#include <stdio.h>\n"
        "#define SQR(x) ((x) * (x))\n"
        "typedef struct { int x, y; } Point;\n"
        "enum Color { RED, GREEN };\n"
        "int add(int a, int b);\n"
        "static int counter = 0;\n"
        "struct S { int a; } s = { 1 };\n"
        "struct S t = { 2 };\n"
        "int table[] = { 1, 2, 3 };\n"
        "int add(int a, int b)\n{\n    return a + b;\n}\n"
        "static inline unsigned long long fact(int n) { return n < 2 ? 1 : n * fact(n - 1); }\n"
        "char *dup(const char *s) { return 0; }\n"
        "struct Point make(int x, int y) { struct Point p = { x, y }; return p; }\n"
        "enum Color pick(void) { return RED; }\n"
        "int main(void)\n{\n    return add(1, 2);\n}\n"
        << QStringList({
               E("add", "int", "int a, int b", "int add(int a, int b)\n{\n    return a + b;\n}"),
               E("fact", "unsigned long long", "int n",
                 "static inline unsigned long long fact(int n) { return n < 2 ? 1 : n * fact(n - 1); }"),
               E("dup", "char *", "const char *s", "char *dup(const char *s) { return 0; }"),
               E("make", "struct Point", "int x, int y",
                 "struct Point make(int x, int y) { struct Point p = { x, y }; return p; }"),
               E("pick", "enum Color", "void", "enum Color pick(void) { return RED; }"),
               E("main", "int", "void", "int main(void)\n{\n    return add(1, 2);\n}"),
           });

    QTest::newRow("c keywords of c++ are identifiers") << "c" <<
        "int class(int new) { return new; }\n"
        "void template(void) { }\n"
        << QStringList({E("class", "int", "int new", "int class(int new) { return new; }"),
                        E("template", "void", "void", "void template(void) { }")});

    QTest::newRow("k&r style main") << "c" << "main()\n{\n  puts(\"hi\");\n}\n"
                                    << QStringList({E("main", "", "", "main()\n{\n  puts(\"hi\");\n}")});

    QTest::newRow("namespaces") << "cpp" <<
        "namespace util {\n"
        "int a(int x) { return x; }\n"
        "namespace detail { int b() { return 1; } }\n"
        "}\n"
        "namespace x::y { void c() {} }\n"
        "namespace { void d() {} }\n"
        "inline namespace v1 { void e() {} }\n"
        "namespace fs = std::filesystem;\n"
        << QStringList({E("a", "int", "int x", "int a(int x) { return x; }"),
                        E("b", "int", "", "int b() { return 1; }"), E("c", "void", "", "void c() {}"),
                        E("d", "void", "", "void d() {}"), E("e", "void", "", "void e() {}")});

    QTest::newRow("extern c") << "cpp" <<
        "extern \"C\" {\n"
        "int f1(int x) { return x; }\n"
        "}\n"
        "extern \"C\" int f2(void) { return 2; }\n"
        "extern int g;\n"
        << QStringList({E("f1", "int", "int x", "int f1(int x) { return x; }"),
                        E("f2", "int", "void", "extern \"C\" int f2(void) { return 2; }")});

    QTest::newRow("extern c in a c file") << "c" <<
        "#ifdef __cplusplus\nextern \"C\" {\n#endif\n"
        "int f(int x) { return x; }\n"
        "#ifdef __cplusplus\n}\n#endif\n"
        << QStringList({E("f", "int", "int x", "int f(int x) { return x; }")});

    QTest::newRow("class members") << "cpp" <<
        "class Shape : public Base, private Other<int> {\n"
        "    Q_OBJECT\n"
        "public:\n"
        "    Shape() = default;\n"
        "    explicit Shape(int w) : w_(w), h_{w} { init(); }\n"
        "    Shape(const Shape &) = delete;\n"
        "    virtual ~Shape() { }\n"
        "    virtual double area() const = 0;\n"
        "    int width() const noexcept { return w_; }\n"
        "    void resize(int w) override final { w_ = w; }\n"
        "    static Shape *create() { return nullptr; }\n"
        "    bool operator==(const Shape &o) const { return w_ == o.w_; }\n"
        "    int operator()(int x) const { return x * w_; }\n"
        "    Shape &operator=(Shape &&o) noexcept(true) { return *this; }\n"
        "    explicit operator bool() const { return w_ != 0; }\n"
        "    friend std::ostream &operator<<(std::ostream &os, const Shape &s) { return os << s.w_; }\n"
        "protected slots:\n"
        "    void onChanged() {}\n"
        "signals:\n"
        "    void changed();\n"
        "private:\n"
        "    int w_ = 0, h_{0};\n"
        "    struct Inner { int get() const { return 1; } };\n"
        "};\n"
        << QStringList({
               E("Shape::Shape", "", "int w", "explicit Shape(int w) : w_(w), h_{w} { init(); }"),
               E("Shape::~Shape", "", "", "virtual ~Shape() { }"),
               E("Shape::width", "int", "", "int width() const noexcept { return w_; }"),
               E("Shape::resize", "void", "int w", "void resize(int w) override final { w_ = w; }"),
               E("Shape::create", "Shape *", "", "static Shape *create() { return nullptr; }"),
               E("Shape::operator==", "bool", "const Shape &o",
                 "bool operator==(const Shape &o) const { return w_ == o.w_; }"),
               E("Shape::operator()", "int", "int x", "int operator()(int x) const { return x * w_; }"),
               E("Shape::operator=", "Shape &", "Shape &&o",
                 "Shape &operator=(Shape &&o) noexcept(true) { return *this; }"),
               E("Shape::operator bool", "", "", "explicit operator bool() const { return w_ != 0; }"),
               E("operator<<", "std::ostream &", "std::ostream &os, const Shape &s",
                 "friend std::ostream &operator<<(std::ostream &os, const Shape &s) { return os << s.w_; }"),
               E("Shape::onChanged", "void", "", "void onChanged() {}"),
               E("Shape::Inner::get", "int", "", "int get() const { return 1; }"),
           });

    QTest::newRow("out of line members") << "cpp" <<
        "Account::Account(const std::string &owner, double balance)\n    : owner_(owner), balance_{balance}\n{\n}\n"
        "Account::~Account() {}\n"
        "void Account::deposit(double amount) { balance_ += amount; }\n"
        "Account::operator bool() const { return balance_ > 0; }\n"
        "Account &Account::operator+=(double x) { return *this; }\n"
        "std::ostream &operator<<(std::ostream &os, const Account &a) { return os; }\n"
        "bool operator<(const Account &a, const Account &b) { return a.balance() < b.balance(); }\n"
        "auto Account::balance() const -> double { return balance_; }\n"
        "template <typename T> Stack<T>::Stack() : data_() {}\n"
        "template <typename T>\nvoid Stack<T>::push(const T &v) { data_.push_back(v); }\n"
        "long double operator\"\"_km(long double v) { return v * 1000; }\n"
        "void *Pool::operator new[](std::size_t n) { return nullptr; }\n"
        << QStringList({
               E("Account::Account", "", "const std::string &owner, double balance",
                 "Account::Account(const std::string &owner, double balance)\n    : owner_(owner), balance_{balance}\n{\n}"),
               E("Account::~Account", "", "", "Account::~Account() {}"),
               E("Account::deposit", "void", "double amount",
                 "void Account::deposit(double amount) { balance_ += amount; }"),
               E("Account::operator bool", "", "", "Account::operator bool() const { return balance_ > 0; }"),
               E("Account::operator+=", "Account &", "double x",
                 "Account &Account::operator+=(double x) { return *this; }"),
               E("operator<<", "std::ostream &", "std::ostream &os, const Account &a",
                 "std::ostream &operator<<(std::ostream &os, const Account &a) { return os; }"),
               E("operator<", "bool", "const Account &a, const Account &b",
                 "bool operator<(const Account &a, const Account &b) { return a.balance() < b.balance(); }"),
               E("Account::balance", "double", "", "auto Account::balance() const -> double { return balance_; }"),
               E("Stack<T>::Stack", "", "", "template <typename T> Stack<T>::Stack() : data_() {}"),
               E("Stack<T>::push", "void", "const T &v",
                 "template <typename T>\nvoid Stack<T>::push(const T &v) { data_.push_back(v); }"),
               E("operator\"\"_km", "long double", "long double v",
                 "long double operator\"\"_km(long double v) { return v * 1000; }"),
               E("Pool::operator new[]", "void *", "std::size_t n",
                 "void *Pool::operator new[](std::size_t n) { return nullptr; }"),
           });

    QTest::newRow("templates") << "cpp" <<
        "template <typename T>\nT maxOf(T a, T b) { return a > b ? a : b; }\n"
        "template <typename T, int N = 3>\nstruct Arr {\n    T get(int i) const { return data[i]; }\n    T data[N];\n};\n"
        "template <>\nvoid show<int>(int x) { }\n"
        "template class Arr<int>;\n"
        "template <typename T> requires std::integral<T> && (sizeof(T) > 1)\nT twice(T x) { return 2 * x; }\n"
        "template <typename T>\nT half(T x) requires std::integral<T> { return x / 2; }\n"
        "template <typename T> concept Small = sizeof(T) < 4;\n"
        "template <typename... Ts> void all(Ts... xs) { (use(xs), ...); }\n"
        "std::map<std::string, std::vector<int>> index(const std::vector<std::string> &w) { return {}; }\n"
        << QStringList({
               E("maxOf", "T", "T a, T b", "template <typename T>\nT maxOf(T a, T b) { return a > b ? a : b; }"),
               E("Arr::get", "T", "int i", "T get(int i) const { return data[i]; }"),
               E("show<int>", "void", "int x", "template <>\nvoid show<int>(int x) { }"),
               E("twice", "T", "T x",
                 "template <typename T> requires std::integral<T> && (sizeof(T) > 1)\nT twice(T x) { return 2 * x; }"),
               E("half", "T", "T x",
                 "template <typename T>\nT half(T x) requires std::integral<T> { return x / 2; }"),
               E("all", "void", "Ts... xs", "template <typename... Ts> void all(Ts... xs) { (use(xs), ...); }"),
               E("index", "std::map<std::string, std::vector<int>>", "const std::vector<std::string> &w",
                 "std::map<std::string, std::vector<int>> index(const std::vector<std::string> &w) { return {}; }"),
           });

    QTest::newRow("specifiers and attributes") << "cpp" <<
        "[[nodiscard]] static inline int a() { return 1; }\n"
        "__attribute__((unused)) static int b() { return 2; }\n"
        "__declspec(dllexport) void c() {}\n"
        "constexpr int sq(int x) { return x * x; }\n"
        "consteval int cube(int x) { return x * x * x; }\n"
        "[[noreturn]] void die() { throw 1; }\n"
        "auto lambda = [](int x) { return x + 1; };\n"
        "std::function<int(int)> fn = [](int x) { return x; };\n"
        "int (*fp)(int) = nullptr;\n"
        "static_assert(sizeof(int) == 4, \"int\");\n"
        "using Vec = std::vector<int>;\n"
        "enum class Mode : int { A, B };\n"
        "Q_DECLARE_METATYPE(Mode)\n"
        "int after(int v = 5, int w = (1 + 2)) { return v; }\n"
        "auto trailing(int x) -> decltype(x + 1) { return x + 1; }\n"
        "decltype(auto) forward(int &x) { return x; }\n"
        "int main() try { return 0; } catch (...) { return 1; }\n"
        << QStringList({
               E("a", "int", "", "[[nodiscard]] static inline int a() { return 1; }"),
               E("b", "int", "", "__attribute__((unused)) static int b() { return 2; }"),
               E("c", "void", "", "__declspec(dllexport) void c() {}"),
               E("sq", "int", "int x", "constexpr int sq(int x) { return x * x; }"),
               E("cube", "int", "int x", "consteval int cube(int x) { return x * x * x; }"),
               E("die", "void", "", "[[noreturn]] void die() { throw 1; }"),
               E("after", "int", "int v = 5, int w = (1 + 2)", "int after(int v = 5, int w = (1 + 2)) { return v; }"),
               E("trailing", "decltype(x + 1)", "int x", "auto trailing(int x) -> decltype(x + 1) { return x + 1; }"),
               E("forward", "decltype(auto)", "int &x", "decltype(auto) forward(int &x) { return x; }"),
               E("main", "int", "", "int main() try { return 0; } catch (...) { return 1; }"),
           });

    QTest::newRow("macros and comments") << "cpp" <<
        "/* int commented(void) { } */\n"
        "// void alsoCommented() { }\n"
        "#if 0\n#endif\n"
        "BEGIN_DECLS\n"
        "int f(void) /* trailing */ { return 0; }\n"
        "DECLARE_THING(x, y)\n"
        "void g() Q_DECL_NOTHROW { }\n"
        "const char *s = \"int fake() { }\";\n"
        << QStringList({E("f", "int", "void", "int f(void) /* trailing */ { return 0; }"),
                        E("g", "void", "", "void g() Q_DECL_NOTHROW { }")});

    QTest::newRow("struct methods and unions") << "cpp" <<
        "struct Point {\n    int x = 0, y = 0;\n    Point(int x_, int y_) : x(x_), y(y_) {}\n"
        "    int dist2() const { return x * x + y * y; }\n};\n"
        "union U { int i; float f; int asInt() const { return i; } };\n"
        "struct { int q; } anonymous;\n"
        "class Fwd;\n"
        "struct Point origin() { return Point(0, 0); }\n"
        << QStringList({E("Point::Point", "", "int x_, int y_", "Point(int x_, int y_) : x(x_), y(y_) {}"),
                        E("Point::dist2", "int", "", "int dist2() const { return x * x + y * y; }"),
                        E("U::asInt", "int", "", "int asInt() const { return i; }"),
                        E("origin", "struct Point", "", "struct Point origin() { return Point(0, 0); }")});

    QTest::newRow("template class body and nested") << "cpp" <<
        "namespace ds {\ntemplate <typename T>\nclass Stack {\npublic:\n"
        "    void push(const T &v) { items.push_back(v); }\n"
        "    template <typename F> void each(F f) const { for (const T &x : items) f(x); }\n"
        "    class Iterator { public: bool next() { return false; } };\n"
        "private:\n    std::vector<T> items;\n};\n}\n"
        << QStringList({E("Stack::push", "void", "const T &v", "void push(const T &v) { items.push_back(v); }"),
                        E("Stack::each", "void", "F f",
                          "template <typename F> void each(F f) const { for (const T &x : items) f(x); }"),
                        E("Stack::Iterator::next", "bool", "", "bool next() { return false; }")});

    QTest::newRow("constructor forms") << "cpp" <<
        "template <typename T> Box<T>::Box(T v) : Base<T>{v}, value(std::move(v)) {}\n"
        "Widget::Widget(QWidget *parent) : QWidget(parent), ui(new Ui::Widget) { ui->setupUi(this); }\n"
        "Derived::Derived(int n) try : Base(n) { } catch (const std::exception &) { }\n"
        "Pack::Pack(Args... args) : Base(args)... { }\n"
        << QStringList({E("Box<T>::Box", "", "T v", "template <typename T> Box<T>::Box(T v) : Base<T>{v}, value(std::move(v)) {}"),
                        E("Widget::Widget", "", "QWidget *parent",
                          "Widget::Widget(QWidget *parent) : QWidget(parent), ui(new Ui::Widget) { ui->setupUi(this); }"),
                        E("Derived::Derived", "", "int n",
                          "Derived::Derived(int n) try : Base(n) { } catch (const std::exception &) { }"),
                        E("Pack::Pack", "", "Args... args", "Pack::Pack(Args... args) : Base(args)... { }")});
}

void TestFunctions::discovery()
{
    QFETCH(QString, options);
    QFETCH(QString, source);
    QFETCH(QStringList, expected);
    CodeImporter importer(importtest::options(options));
    QCOMPARE(importer.parse(source), !expected.isEmpty());
    QStringList actual;
    const QList<FunctionInfo> fns = importer.functions();
    for (const FunctionInfo &f : fns)
        actual << describe(source, f);
    if (actual != expected) {
        for (const QString &s : actual)
            qWarning().noquote() << "actual:  " << QString(s).replace(kSep, QLatin1String(" | "));
        for (const QString &s : expected)
            qWarning().noquote() << "expected:" << QString(s).replace(kSep, QLatin1String(" | "));
    }
    QCOMPARE(actual, expected);
    for (int i = 0; i < fns.size(); ++i) {
        const QString def = expected.at(i).section(kSep, 3);
        QCOMPARE(fns.at(i).startOffset, source.indexOf(def));
        QCOMPARE(fns.at(i).line, source.left(fns.at(i).startOffset).count(QLatin1Char('\n')) + 1);
        // every function imports into a valid document
        const QDomDocument doc = importer.flowchart(i);
        const QStringList problems = importtest::validate(doc);
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
    }
}

void TestFunctions::lineNumbers()
{
    const QString src = QStringLiteral("// header\n\nint a()\n{\n  return 1;\n}\n\n\ntemplate <class T>\nT b(T x)\n{ return x; }\n");
    CodeImporter importer;
    QVERIFY(importer.parse(src));
    QCOMPARE(importer.functions().size(), 2);
    QCOMPARE(importer.functions().at(0).line, 3);
    QCOMPARE(importer.functions().at(1).line, 9);
    QCOMPARE(importer.functions().at(0).endOffset, src.indexOf(QLatin1String("}\n")) + 1);
}

void TestFunctions::defaultFunction()
{
    CodeImporter importer;
    QVERIFY(importer.parse(QStringLiteral("int a() { return 1; }\nint main() { return a(); }\nint z() { return 0; }")));
    QCOMPARE(importer.defaultFunctionIndex(), 1);
    QVERIFY(importer.parse(QStringLiteral("int a() { return 1; }\nint b() { return 2; }")));
    QCOMPARE(importer.defaultFunctionIndex(), 0);
    // A member called main is not the program entry point.
    QVERIFY(importer.parse(QStringLiteral("struct App { int main() { return 1; } };\nint f() { return 0; }")));
    QCOMPARE(importer.functions().at(0).name, QStringLiteral("App::main"));
    QCOMPARE(importer.defaultFunctionIndex(), 0);
}

void TestFunctions::languageFromFileName()
{
    const QString src = QStringLiteral("int class(int new) { return new; }\n");
    CodeImporter importer;
    QVERIFY(importer.parse(src, QStringLiteral("prog.c")));
    QCOMPARE(importer.functions().size(), 1);
    QCOMPARE(importer.functions().at(0).name, QStringLiteral("class"));
    for (const char *name : {"prog.cpp", "prog.cc", "prog.cxx", "prog.c++", "prog.hpp", "prog.h", "prog.ino",
                             "prog.C", "prog.txt", ""}) {
        importer.parse(src, QString::fromLatin1(name));
        // C++: no definition named "class" (the text is only imported as a fragment)
        QCOMPARE(importer.functions().size(), 1);
        QVERIFY2(importer.functions().first().name.isEmpty(), name);
    }
    afce::ImportOptions c;
    c.language = afce::ImportOptions::Language::C;
    importer.setOptions(c);
    QCOMPARE(importer.options().language, afce::ImportOptions::Language::C);
    QVERIFY(importer.parse(src, QStringLiteral("prog.cpp")));
    QCOMPARE(importer.functions().size(), 1);
}

void TestFunctions::noFunctions()
{
    CodeImporter importer;
    QVERIFY(!importer.parse(QStringLiteral("#pragma once\nint f(int);\nstruct S { int a; };\nextern int g;\n")));
    QVERIFY(importer.functions().isEmpty());
    QCOMPARE(importer.defaultFunctionIndex(), -1);
    QVERIFY(!importer.diagnostics().isEmpty());
    QVERIFY(!importer.parse(QString()));
    // flowchart() with an invalid index still yields a valid (empty) document
    const QDomDocument doc = importer.flowchart(0);
    QVERIFY(importtest::validate(doc).isEmpty());
    QCOMPARE(importer.diagnostics().last().severity, afce::Diagnostic::Error);
    QVERIFY(importtest::validate(importer.flowchart(-1)).isEmpty());
}

void TestFunctions::fragment()
{
    const QString src = QStringLiteral("int n;\nscanf(\"%d\", &n);\nfor (int i = 0; i < n; i++)\n    printf(\"%d\\n\", i);\n");
    CodeImporter importer;
    QVERIFY(importer.parse(src, QStringLiteral("snippet.c")));
    QCOMPARE(importer.functions().size(), 1);
    const FunctionInfo f = importer.functions().first();
    QVERIFY(f.name.isEmpty());
    QCOMPARE(f.startOffset, 0);
    QCOMPARE(f.endOffset, src.size() - 1);
    const QDomDocument doc = importer.flowchart(0);
    QVERIFY(importtest::validate(doc).isEmpty());
    QVERIFY(!doc.documentElement().hasAttribute(QStringLiteral("name")));
    QCOMPARE(importtest::serialize(doc), QStringLiteral("io n\nforc (int i = 0; i < n; i++)\n  ou i"));
    QCOMPARE(importer.diagnostics().first().severity, afce::Diagnostic::Info);
}

void TestFunctions::unclosedBody()
{
    const QString src = QStringLiteral("int f(int x) {\n  if (x) {\n    return 1;\n");
    CodeImporter importer;
    QVERIFY(importer.parse(src));
    QCOMPARE(importer.functions().size(), 1);
    QCOMPARE(importer.functions().first().endOffset, src.lastIndexOf(QLatin1Char(';')) + 1);
    const QDomDocument doc = importer.flowchart(0);
    QVERIFY(importtest::validate(doc).isEmpty());
    QCOMPARE(importtest::serialize(doc), QStringLiteral("if (x)\n  return 1"));
    bool warned = false;
    for (const afce::Diagnostic &d : importer.diagnostics())
        warned = warned || d.severity == afce::Diagnostic::Warning;
    QVERIFY(warned);
}

void TestFunctions::deepTopLevelNesting()
{
    const int depth = 5000;
    QString src;
    for (int i = 0; i < depth; ++i)
        src += QStringLiteral("namespace n%1 { struct S%1 { ").arg(i);
    src += QStringLiteral("int deep() { return 42; }");
    for (int i = 0; i < depth; ++i)
        src += QStringLiteral(" }; }");
    CodeImporter importer;
    QVERIFY(importer.parse(src));
    QCOMPARE(importer.functions().size(), 1);
    QVERIFY(importer.functions().first().name.endsWith(QLatin1String("::S4999::deep")));
    QVERIFY(importer.functions().first().name.startsWith(QLatin1String("S0::S1::")));

    // Unbalanced openers only
    CodeImporter other;
    other.parse(QString(depth, QLatin1Char('{')) + QString(depth, QLatin1Char('(')));
    QVERIFY(other.functions().isEmpty());
}

void TestFunctions::algorithmAttributes()
{
    const QString src = QStringLiteral(
        "int add(int a, int b) { return a + b; }\n"
        "int main() { return 0; }\n"
        "int main2(void) { return 0; }\n"
        "struct T { T() {} };\n");
    CodeImporter importer;
    QVERIFY(importer.parse(src));
    QDomElement alg = importer.flowchart(0).documentElement();
    QCOMPARE(alg.attribute(QStringLiteral("version")), QStringLiteral("1.3"));
    QCOMPARE(alg.attribute(QStringLiteral("name")), QStringLiteral("add"));
    QCOMPARE(alg.attribute(QStringLiteral("params")), QStringLiteral("int a, int b"));
    QCOMPARE(alg.attribute(QStringLiteral("returns")), QStringLiteral("int"));
    alg = importer.flowchart(1).documentElement();
    QVERIFY(!alg.hasAttribute(QStringLiteral("name")));
    QVERIFY(!alg.hasAttribute(QStringLiteral("params")));
    QVERIFY(!alg.hasAttribute(QStringLiteral("returns")));
    alg = importer.flowchart(2).documentElement();
    QCOMPARE(alg.attribute(QStringLiteral("name")), QStringLiteral("main2"));
    QCOMPARE(alg.attribute(QStringLiteral("params")), QStringLiteral("void"));
    alg = importer.flowchart(3).documentElement();
    QCOMPARE(alg.attribute(QStringLiteral("name")), QStringLiteral("T::T"));
    QVERIFY(!alg.hasAttribute(QStringLiteral("params")));
    QVERIFY(!alg.hasAttribute(QStringLiteral("returns")));

    // main with parameters keeps its attributes
    QVERIFY(importer.parse(QStringLiteral("int main(int argc, char **argv) { return argc; }")));
    alg = importer.flowchart(0).documentElement();
    QCOMPARE(alg.attribute(QStringLiteral("name")), QStringLiteral("main"));
    QCOMPARE(alg.attribute(QStringLiteral("params")), QStringLiteral("int argc, char **argv"));
    QVERIFY(importer.parse(QStringLiteral("int main(void) { return 0; }")));
    QVERIFY(!importer.flowchart(0).documentElement().hasAttribute(QStringLiteral("name")));
}

QTEST_GUILESS_MAIN(TestFunctions)
