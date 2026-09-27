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

// Statement mapping tests: every rule and every import option. Charts are
// compared in the compact form produced by importtest::serialize(); every
// produced document is also checked by the structural validator.

#include "tst_codeimport.h"

#include "codeimporter.h"
#include "importbuilder.h"
#include "importtestutil.h"

#include <QtTest>

using importtest::Result;

namespace {

// Raw-string charts start with a newline for readability.
QString chart(const char *s)
{
    QString r = QString::fromUtf8(s);
    if (r.startsWith(QLatin1Char('\n')))
        r.remove(0, 1);
    while (r.endsWith(QLatin1Char('\n')))
        r.chop(1);
    return r;
}

void row(const char *name, const char *options, const char *code, const char *expected)
{
    QTest::newRow(name) << QString::fromLatin1(options) << QString::fromUtf8(code) << chart(expected);
}

} // namespace

void TestCodeImport::cleanup()
{
    // every document produced through importtest::import() must be valid
    const QStringList problems = importtest::takeRecordedProblems();
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

void TestCodeImport::mapping_data()
{
    QTest::addColumn<QString>("options");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("expected");

    // ---- blocks and sequences
    row("compound flattened", "", "{ a = 1; { b = 2; } ; ; }", R"(
assign a := 1
assign b := 2)");
    row("comments and whitespace", "", "x   =  /* c */ y +\n   // line\n  z;", "assign x := y + z");
    row("empty body", "", "", "");

    // ---- if
    row("if without else", "", "if (x > 0) y = 1;", R"(
if (x > 0)
  assign y := 1)");
    row("if else", "", "if (x) a(); else b();", R"(
if (x)
  call a()
else
  call b())");
    row("else if chain", "", "if (a) x = 1; else if (b) x = 2; else { x = 3; }", R"(
if (a)
  assign x := 1
else
  if (b)
    assign x := 2
  else
    assign x := 3)");
    row("dangling else", "", "if (a) if (b) x(); else y();", R"(
if (a)
  if (b)
    call x()
  else
    call y())");
    row("if constexpr", "", "if constexpr (N > 0) f();", R"(
if (N > 0)
  call f())");
    row("if init statement", "", "if (int r = f(); r > 0) g(r);", R"(
assign r := f()
if (r > 0)
  call g(r))");
    row("declaration in condition", "", "if (auto p = find(k)) use(p);", R"(
if (auto p = find(k))
  call use(p))");
    row("empty then", "", "if (x) ; else y = 1;", R"(
if (x)
else
  assign y := 1)");

    // ---- while / do
    row("while", "", "while (i < n) i++;", R"(
pre (i < n)
  assign i := i + 1)");
    row("while declaration condition", "", "while (int c = next()) use(c);", R"(
pre (int c = next())
  call use(c))");
    row("do while", "", "do { cin >> x; } while (x < 0);", R"(
post (x < 0)
  io x)");
    row("nested loops with break continue", "",
        "while (1) { for (;;) { if (a) break; continue; } if (b) break; }", R"(
pre (1)
  forc (; ; )
    if (a)
      break
    continue
  if (b)
    break)");

    // ---- for: C style
    row("for cstyle", "", "for (int i = 0; i < n; i++) sum += i;", R"(
forc (int i = 0; i < n; i++)
  assign sum := sum + i)");
    row("for cstyle empty parts", "", "for (;;) { if (done()) break; }", R"(
forc (; ; )
  if (done())
    break)");
    row("for cstyle commas", "", "for (i = 0, j = n - 1; i < j; i++, j--) swap(a[i], a[j]);", R"(
forc (i = 0, j = n - 1; i < j; i++, j--)
  call swap(a[i], a[j]))");

    // ---- for: while style
    row("for while", "for=while", "for (int i = 0; i < n; i++) sum += i;", R"(
assign i := 0
pre (i < n)
  assign sum := sum + i
  assign i := i + 1)");
    row("for while empty condition cpp", "for=while", "for (;;) x();", R"(
pre (true)
  call x())");
    row("for while empty condition c", "for=while,c", "for (;;) x();", R"(
pre (1)
  call x())");
    row("for while commas", "for=while", "for (i = 0, j = n; i < j; i++, j--) f(i, j);", R"(
assign i := 0
assign j := n
pre (i < j)
  call f(i, j)
  assign i := i + 1
  assign j := j - 1)");
    row("for while continue fallback", "for=while", "for (int i = 0; i < n; i++) { if (i % 2) continue; f(i); }", R"(
forc (int i = 0; i < n; i++)
  if (i % 2)
    continue
  call f(i))");
    row("for while continue of inner loop", "for=while",
        "for (int i = 0; i < n; i++) { while (x) { continue; } do continue; while (y); }", R"(
assign i := 0
pre (i < n)
  pre (x)
    continue
  post (y)
    continue
  assign i := i + 1)");
    row("for while continue through switch", "for=while",
        "for (int i = 0; i < n; i++) switch (i) { case 1: continue; default: f(); }", R"(
forc (int i = 0; i < n; i++)
  case (i)
    = 1
      continue
    default
      call f())");
    row("for while body ends with break", "for=while", "for (int i = 0; i < n; i++) { f(i); break; }", R"(
assign i := 0
pre (i < n)
  call f(i)
  break)");
    row("for while keep declarations", "for=while,keepdecl", "for (int i = 0; i < n; ++i) f(i);", R"(
process int i = 0
pre (i < n)
  call f(i)
  assign i := i + 1)");
    row("for while no init no step", "for=while", "for (; x;) f();", R"(
pre (x)
  call f())");
    row("for while sequential loops", "for=while",
        "for (int i = 0; i < n; i++) f(i); for (int i = 0; i < m; i++) g(i);", R"(
assign i := 0
pre (i < n)
  call f(i)
  assign i := i + 1
assign i := 0
pre (i < m)
  call g(i)
  assign i := i + 1)");
    row("for while sequential loops keep declarations", "for=while,keepdecl",
        "for (int i = 0; i < n; i++) f(i); for (int i = 0; i < m; i++) g(i); for (int j = 0; j < 2; j++) h(j);", R"(
forc (int i = 0; i < n; i++)
  call f(i)
forc (int i = 0; i < m; i++)
  call g(i)
process int j = 0
pre (j < 2)
  call h(j)
  assign j := j + 1)");
    row("for while shadowing", "for=while", "int i = 10; for (int i = 0; i < 3; i++) f(i); g(i);", R"(
assign i := 10
forc (int i = 0; i < 3; i++)
  call f(i)
call g(i))");
    row("for while outer variable used after the loop", "for=while", "for (int i = 0; i < 3; i++) f(i); g(i);", R"(
forc (int i = 0; i < 3; i++)
  call f(i)
call g(i))");
    row("for while nested loop reuse", "for=while",
        "for (int i = 0; i < n; i++) for (int j = 0; j < i; j++) f(i, j);", R"(
assign i := 0
pre (i < n)
  assign j := 0
  pre (j < i)
    call f(i, j)
    assign j := j + 1
  assign i := i + 1)");

    // ---- for: arithmetic
    row("arith less", "for=arithmetic", "for (int i = 0; i < n; i++) f(i);", R"(
for i = 0 .. n - 1
  call f(i))");
    row("arith less equal", "for=arithmetic", "for (int i = 1; i <= 10; ++i) f(i);", R"(
for i = 1 .. 10
  call f(i))");
    row("arith literal bound", "for=arithmetic", "for (int i = 0; i < 10; i += 1) f(i);", R"(
for i = 0 .. 9
  call f(i))");
    row("arith reversed", "for=arithmetic", "for (int i = 0; n > i; i = i + 1) f(i);", R"(
for i = 0 .. n - 1
  call f(i))");
    row("arith reversed inclusive", "for=arithmetic", "for (long k = a; 10 >= k; k++) f(k);", R"(
for k = a .. 10
  call f(k))");
    row("arith call bound", "for=arithmetic", "for (size_t i = 0; i < v.size(); i++) f(v[i]);", R"(
for i = 0 .. v.size() - 1
  call f(v[i]))");
    row("arith plus one bound", "for=arithmetic", "for (int i = 0; i < n + 1; i++) f(i);", R"(
for i = 0 .. n
  call f(i))");
    row("arith expression bound", "for=arithmetic", "for (int i = 0; i < n * 2; i++) f(i);", R"(
for i = 0 .. (n * 2) - 1
  call f(i))");
    row("arith negative bound", "for=arithmetic", "for (int i = -9; i < -3; i++) f(i);", R"(
for i = -9 .. -4
  call f(i))");
    row("arith hex bound", "for=arithmetic", "for (int i = 0; i < 0x10; i++) f(i);", R"(
for i = 0 .. 15
  call f(i))");
    row("arith separators", "for=arithmetic", "for (int i = 0; i < 1'000; i++) f(i);", R"(
for i = 0 .. 999
  call f(i))");
    row("arith template bound", "for=arithmetic",
        "for (int i = 0; i < std::numeric_limits<short>::max(); i++) f(i);", R"(
for i = 0 .. std::numeric_limits<short>::max() - 1
  call f(i))");
    row("arith member named like var", "for=arithmetic", "for (int i = 0; i < n; i++) p.i = 5;", R"(
for i = 0 .. n - 1
  assign p.i := 5)");
    row("arith not declared", "for=arithmetic", "for (i = 0; i < n; i++) f(i);", R"(
forc (i = 0; i < n; i++)
  call f(i))");
    row("arith step two", "for=arithmetic", "for (int i = 0; i < n; i += 2) f(i);", R"(
forc (int i = 0; i < n; i += 2)
  call f(i))");
    row("arith downwards", "for=arithmetic", "for (int i = n; i > 0; i--) f(i);", R"(
forc (int i = n; i > 0; i--)
  call f(i))");
    row("arith body assigns", "for=arithmetic", "for (int i = 0; i < n; i++) { if (a[i] == 0) i = n; }", R"(
forc (int i = 0; i < n; i++)
  if (a[i] == 0)
    assign i := n)");
    row("arith body increments", "for=arithmetic", "for (int i = 0; i < n; i++) ++i;", R"(
forc (int i = 0; i < n; i++)
  assign i := i + 1)");
    row("arith body address", "for=arithmetic", "for (int i = 0; i < n; i++) scanf(\"%d\", &i);", R"(
forc (int i = 0; i < n; i++)
  io i)");
    row("arith body reads", "for=arithmetic", "for (int i = 0; i < n; i++) cin >> i;", R"(
forc (int i = 0; i < n; i++)
  io i)");
    row("arith two variables", "for=arithmetic", "for (int i = 0, j = 0; i < n; i++) f(i);", R"(
forc (int i = 0, j = 0; i < n; i++)
  call f(i))");
    row("arith floating", "for=arithmetic", "for (double x = 0; x < 1; x++) f(x);", R"(
forc (double x = 0; x < 1; x++)
  call f(x))");
    row("arith compound condition", "for=arithmetic", "for (int i = 0; i < n && ok; i++) f(i);", R"(
forc (int i = 0; i < n && ok; i++)
  call f(i))");
    row("arith condition on other var", "for=arithmetic", "for (int i = 0; j < n; i++) f(i);", R"(
forc (int i = 0; j < n; i++)
  call f(i))");
    row("arith floating bound", "for=arithmetic", "for (int i = 0; i < 10.5; i++) f(i);", R"(
forc (int i = 0; i < 10.5; i++)
  call f(i))");
    row("arith bound uses the variable", "for=arithmetic", "for (int i = 1; i < n / i; i++) f(i);", R"(
forc (int i = 1; i < n / i; i++)
  call f(i))");

    // ---- range for
    row("range for", "", "for (const auto &p : m) cout << p.first << endl;", R"(
foreach (const auto &p : m)
  ou p.first)");
    row("range for structured binding", "", "for (auto [k, v] : m) f(k, v);", R"(
foreach (auto [k, v] : m)
  call f(k, v))");
    row("range for init", "", "for (auto v = get(); auto x : v) use(x);", R"(
assign v := get()
foreach (auto x : v)
  call use(x))");
    row("range for ternary range", "", "for (int x : c ? a : b) f(x);", R"(
foreach (int x : c ? a : b)
  call f(x))");
    row("range for braced list", "", "for (int x : {1, 2, 3}) s += x;", R"(
foreach (int x : {1, 2, 3})
  assign s := s + x)");
    row("qt foreach and forever", "", "foreach (const QString &s, list) f(s); Q_FOREACH (int x, v) { g(x); } "
                                      "forever { if (done()) break; } int forever = 1;", R"(
foreach (const QString &s : list)
  call f(s)
foreach (int x : v)
  call g(x)
pre (true)
  if (done())
    break
assign forever := 1)");
    row("unknown macro before a block", "", "BOOST_FOREACH(int x, v) { f(x); } g();", R"(
call BOOST_FOREACH(int x, v)
call f(x)
call g())");

    // ---- switch
    row("switch basic", "", R"(
switch (op) {
case '+': r = a + b; break;
case '-': case '_':
    r = a - b;
    break;
default:
    r = 0;
})", R"(
case (op)
  = '+'
    assign r := a + b
  = '-','_'
    assign r := a - b
  default
    assign r := 0)");
    row("switch default moved last", "", R"(
switch (x) {
default: f(); break;
case 1: g(); break;
case 2: h(); break;
})", R"(
case (x)
  = 1
    call g()
  = 2
    call h()
  default
    call f())");
    row("switch without default", "", "switch (x) { case 1: f(); break; }", R"(
case (x)
  = 1
    call f()
  default)");
    row("switch fall through", "", R"(
switch (x) {
case 1: a();
case 2: b(); break;
case 3: c();
case 4: d();
case 5: e(); break;
case 6: f();
})", R"(
case (x)
  = 1
    call a()
    call b()
  = 2
    call b()
  = 3
    call c()
    call d()
    call e()
  = 4
    call d()
    call e()
  = 5
    call e()
  = 6
    call f()
  default)");
    row("switch fall through into default", "", R"(
switch (x) {
case 1: a();
default: b();
case 2: c(); break;
})", R"(
case (x)
  = 1
    call a()
    call b()
    call c()
  = 2
    call c()
  default
    call b()
    call c())");
    row("switch fallthrough attribute", "", R"(
switch (x) {
case 1: a(); [[fallthrough]];
case 2: b(); break;
})", R"(
case (x)
  = 1
    call a()
    call b()
  = 2
    call b()
  default)");
    row("switch terminating groups", "", R"(
switch (x) {
case 1: return 10;
case 2: if (y) { a(); break; } else { return 3; }
case 3: throw 1;
case 4: continue;
case 5: if (z) break; f();
case 6: g(); break;
})", R"(
case (x)
  = 1
    return 10
  = 2
    if (y)
      call a()
      break
    else
      return 3
  = 3
    process throw 1
  = 4
    continue
  = 5
    if (z)
      break
    call f()
    call g()
  = 6
    call g()
  default)");
    row("switch statements before first label", "", "switch (x) { f(); case 1: g(); }", R"(
case (x)
  = 1
    call g()
  default)");
    row("switch duffs device", "", R"(
switch (count % 4) {
case 0: do { *to = *from++;
case 3:      *to = *from++;
case 2:      *to = *from++;
case 1:      *to = *from++;
        } while (--n > 0);
})",
        "process switch (count % 4) { case 0: do { *to = *from++; case 3: *to = *from++; case 2: *to = *from++; "
        "case 1: *to = *from++; } while (--n > 0); }");
    row("switch only default", "", "switch (x) { default: f(); g(); break; }", R"(
call f()
call g())");
    row("switch only default nested break", "", "switch (x) { default: if (y) break; f(); }",
        "process switch (x) { default: if (y) break; f(); }");
    row("switch empty", "", "switch (x) { }", "");
    row("switch init statement", "", "switch (int c = get(); c) { case 1: f(); break; default: g(); }", R"(
assign c := get()
case (c)
  = 1
    call f()
  default
    call g())");
    row("switch block in case", "", "switch (x) { case 1: { int y = 2; f(y); break; } case 2: { g(); } break; }", R"(
case (x)
  = 1
    assign y := 2
    call f(y)
  = 2
    call g()
  default)");
    row("switch qualified values", "", R"(
switch (c) { case Color::Red: case Color::Green: f(); break; case Color::Blue: break; })", R"(
case (c)
  = Color::Red,Color::Green
    call f()
  = Color::Blue
  default)");
    row("switch nested break stays", "", "switch (x) { case 1: while (y) { if (z) break; } f(); break; }", R"(
case (x)
  = 1
    pre (y)
      if (z)
        break
    call f()
  default)");
    row("switch nested switch", "", "switch (a) { case 1: switch (b) { case 2: f(); break; } break; default: g(); }", R"(
case (a)
  = 1
    case (b)
      = 2
        call f()
      default
  default
    call g())");
    row("switch last case empty", "", "switch (x) { case 1: f(); break; case 2: }", R"(
case (x)
  = 1
    call f()
  = 2
  default)");
    row("switch case merged with default", "", "switch (x) { case 1: f(); break; case 3: default: g(); }", R"(
case (x)
  = 1
    call f()
  default
    call g())");
    row("switch without braces", "", "switch (x) case 1: f();", R"(
case (x)
  = 1
    call f()
  default)");

    // ---- jumps
    row("return forms", "", "if (a) return; if (b) return x + 1; return {1, 2};", R"(
if (a)
  return
if (b)
  return x + 1
return {1, 2})");
    row("goto and labels", "", "start: x++; if (x < 10) goto start; end: ;", R"(
assign x := x + 1
if (x < 10)
  process goto start)");
    row("try catch", "", "try { f(); g(); } catch (const std::exception &e) { h(); } catch (...) { }", R"(
call f()
call g())");
    row("throw", "", "throw std::runtime_error(\"bad\");", R"(process throw std::runtime_error("bad"))");
    row("asm", "", "asm volatile (\"nop\");", R"(process asm volatile ("nop"))");
    row("skipped declarations", "",
        "using namespace std; typedef int T; using V = std::vector<int>; static_assert(sizeof(T) == 4); f();",
        "call f()");
    row("local type", "", "struct P { int x; }; enum E { A, B }; struct Q { int y; } q = {1}; f();", R"(
process struct Q { int y; } q = {1}
call f())");
    row("preprocessor in body", "", "#ifdef DEBUG\n  printf(\"x\");\n#else\n  y = 1;\n#endif\n", R"(
ou "x"
assign y := 1)");
    row("co_return and co_await", "", "co_await x; co_return 5;", R"(
process co_await x
process co_return 5)");

    // ---- declarations
    row("decl no init", "", "int a; char name[50]; std::string s; std::map<std::string, int> m;", "");
    row("decl init", "", "int a = 5;", "assign a := 5");
    row("decl multiple", "", "int a = 1, b, c = 3;", R"(
assign a := 1
assign c := 3)");
    row("decl pointer array", "", "int *p = &x; int arr[5] = {1, 2}; char s[] = \"hi\";", R"(
assign p := &x
assign arr := {1, 2}
assign s := "hi")");
    row("decl direct init", "", "std::vector<int> v(n); Point p{1, 2}; std::string t(5, 'a');", R"(
process std::vector<int> v(n)
process Point p{1, 2}
process std::string t(5, 'a'))");
    row("decl nested templates", "", "vector<vector<int>> g(n, vector<int>(m, 0));",
        "process vector<vector<int>> g(n, vector<int>(m, 0))");
    row("decl structured binding", "", "auto [a, b] = pr; const auto &[x, y] = pt;", R"(
assign [a, b] := pr
assign [x, y] := pt)");
    row("decl specifiers", "",
        "static int count = 0; const std::string &name = person.name; constexpr double pi = 3.14;", R"(
assign count := 0
assign name := person.name
assign pi := 3.14)");
    row("decl function pointer", "", "int (*fp)(int) = square;", "assign fp := square");
    row("decl chained", "", "int x = y = 0;", "process int x = y = 0");
    row("decl local prototype", "", "int f(int); double g(double x, int n); f(1);", "call f(1)");
    row("decl vexing parse", "", "Foo f();", "process Foo f()");
    row("decl struct", "c", "struct Point p = {1, 2}; struct Point q;", "assign p := {1, 2}");
    row("decl shift", "", "unsigned long long big = 1ULL << 40;", "assign big := 1ULL << 40");
    row("decl lambda", "", "auto twice = [](int x) { return x * 2; };",
        "assign twice := [](int x) { return x * 2; }");
    row("decl getchar", "", "char c = getchar();", "io c");
    row("decl keep", "keepdecl", "int a = 5, b; std::vector<int> v(n);", R"(
process int a = 5, b
process std::vector<int> v(n))");
    row("decl keep without init", "keepdecl", "int n; cin >> n;", R"(
process int n
io n)");
    row("decl pointer declaration heuristics", "", "Node *cur = head; a * b; T &r = x;", R"(
assign cur := head
assign r := x)");

    // ---- expressions
    row("comma expression", "", "a = 1, b = 2;", R"(
assign a := 1
assign b := 2)");
    row("assignment", "", "a[i] = b[j] + 1; p->next = q; *p = 5;", R"(
assign a[i] := b[j] + 1
assign p->next := q
assign *p := 5)");
    row("chained assignment", "", "a = b = 0;", "process a = b = 0");
    row("compound assignment", "",
        "x += 1; x -= y - z; x *= a + b; x /= f(y); s += \"abc\"; x <<= 2; x %= n; x += -1; x += a[i]; "
        "p->count += 1; x |= (a & b); x ^= ~m;", R"(
assign x := x + 1
assign x := x - (y - z)
assign x := x * (a + b)
assign x := x / f(y)
assign s := s + "abc"
assign x := x << 2
assign x := x % n
assign x := x + (-1)
assign x := x + a[i]
assign p->count := p->count + 1
assign x := x | (a & b)
assign x := x ^ (~m))");
    row("compound with side effect", "", "a[i++] += 2;", "process a[i++] += 2");
    row("increments", "", "i++; --j; a[i]++; ++p->n; *p++;", R"(
assign i := i + 1
assign j := j - 1
assign a[i] := a[i] + 1
assign p->n := p->n + 1
process *p++)");
    row("noexpand", "noexpand", "x += 1; i++; x = 2;", R"(
process x += 1
process i++
assign x := 2)");
    row("calls", "",
        "f(x); std::sort(v.begin(), v.end()); obj.method(1); p->run(); (void)f(); max<int>(a, b); ::g(); "
        "f(x)(y); system(\"pause\");", R"(
call f(x)
call std::sort(v.begin(), v.end())
process obj.method(1)
process p->run()
process (void)f()
call max<int>(a, b)
call ::g()
process f(x)(y)
call system("pause"))");
    row("nocalls", "nocalls", "f(x); printf(\"x\\n\");", R"(
process f(x)
ou "x")");
    row("other expressions", "", "x == 5; delete p; delete[] q; new Foo; sizeof(x); x;", R"(
process x == 5
process delete p
process delete[] q
process new Foo
process sizeof(x)
process x)");
    row("ternary is not an assignment", "", "c ? a = 1 : b;", "process c ? a = 1 : b");

    // ---- C input
    row("scanf", "",
        "scanf(\"%d\", &n); scanf(\"%d %d\", &a, &b); scanf(\"%s\", name); scanf(\"%d\", &a[i]); "
        "scanf(\"%lf\", &p->x);", R"(
io n
io a,b
io name
io a[i]
io p->x)");
    row("scanf_s and fscanf", "",
        "scanf_s(\"%s %d\", buf, 10, &n); fscanf(stdin, \"%d\", &x); fscanf(f, \"%d\", &x); scanf(\"%*d\");", R"(
io buf,n
io x
call fscanf(f, "%d", &x)
call scanf("%*d"))");
    row("gets fgets", "", "gets(s); fgets(line, 100, stdin); fgets(line, 100, f); gets_s(s, 10);", R"(
io s
io line
call fgets(line, 100, f)
io s)");
    row("getchar", "", "c = getchar(); ch = getc(stdin); x = fgetc(stdin); getchar(); y = getc(f);", R"(
io c
io ch
io x
call getchar()
assign y := getc(f))");

    // ---- C output
    row("printf", "", R"(
printf("Hello\n");
printf("Sum = %d\n", s);
printf("%d %d\n", a, b);
printf("%5.2f", x);
printf("100%%\n");
printf("a" "b %d\n", x);
printf("\n");
printf("%d\n\n", x);
printf("x=%d, y=%d\n", x, y);
printf("Name: \"%s\"\n", n);
printf("%c", c);
printf("%-10s|%+05ld|%lu\n", s, l, u);
)", R"(
ou "Hello"
ou "Sum = ",s
ou a," ",b
ou x
ou "100%"
ou "ab ",x
ou
ou x,"\n"
ou "x=",x,", y=",y
ou "Name: \"",n,"\""
ou c
ou s,"|",l,"|",u)");
    row("printf fallbacks", "", R"(
printf("%*d\n", w, x);
printf(msg);
printf(fmt, a, b);
printf("%d %d\n", a);
printf("%" PRId64 "\n", v);
printf("");
printf("%n", &k);
)", R"(
ou "%*d\n",w,x
ou msg
ou fmt,a,b
ou "%d %d\n",a
ou "%" PRId64 "\n",v
process printf("")
ou "%n",&k)");
    row("other c output", "", R"(
fprintf(stderr, "error: %s\n", msg);
fprintf(stdout, "ok\n");
fprintf(f, "x");
puts("hi");
putchar('x');
putchar(c);
putchar('\n');
fputs(s, stdout);
fputs("line\n", stdout);
fputs(s, f);
)", R"(
ou "error: ",msg
ou "ok"
call fprintf(f, "x")
ou "hi"
ou 'x'
ou c
ou
ou s
ou "line"
call fputs(s, f))");
    row("printf exact", "exact", R"(
printf("Sum = %d\n", s);
printf("%c", c);
printf("%5.2f\n", x);
printf("%ld items\n", n);
printf("%lld %llu %u %i %s\n", a, b, c, d, e);
printf("%%d\n");
printf(msg);
printf("\n");
puts("hi");
putchar(c);
putchar('\n');
fputs(s, stdout);
fputs("line\n", stdout);
printf("%d %d\n", a);
)", R"(
ou "Sum = ",s,"\n"
ou (char)(c)
process printf("%5.2f\n", x)
ou n," items\n"
ou a," ",b," ",c," ",d," ",e,"\n"
ou "%d\n"
process printf(msg)
ou "\n"
ou "hi","\n"
ou (char)(c)
ou (char)('\n')
ou s
ou "line\n"
process printf("%d %d\n", a))");
    row("input exact", "exact", R"(
scanf("%d", &n);
scanf("%d%d", &a, &b);
scanf("%lf", &x);
scanf("%s", s);
c = getchar();
gets(s);
cin >> ws >> x;
)", R"(
io n
io a,b
process scanf("%lf", &x)
process scanf("%s", s)
assign c := getchar()
call gets(s)
io ws,x)");
    row("noio", "noio", R"(
printf("x\n");
scanf("%d", &n);
c = getchar();
cout << x << endl;
cin >> x;
getline(cin, s);
)", R"(
call printf("x\n")
call scanf("%d", &n)
assign c := getchar()
process cout << x << endl
process cin >> x
call getline(cin, s))");

    // ---- C++ input
    row("cin", "", "cin >> n; std::cin >> a >> b; cin >> v[i] >> s.name; ::std::cin >> z; cin >> ws >> x;", R"(
io n
io a,b
io v[i],s.name
io z
io x)");
    row("getline and get", "",
        "getline(cin, s); std::getline(std::cin, line); getline(cin, s, ','); cin.get(c); cin.ignore(); "
        "cin.get(); cin.getline(buf, 10);", R"(
io s
io line
call getline(cin, s, ',')
io c
process cin.ignore()
process cin.get()
process cin.getline(buf, 10))");

    // ---- C++ output
    row("cout", "", R"(
cout << "Hello" << endl;
std::cout << "Sum = " << s << std::endl;
cout << x << '\n';
cout << "Done\n";
cout << "a" << endl << "b" << endl;
cout << a + b << " " << (a << 1) << "\n";
cerr << "err" << endl;
std::clog << "log";
cout << endl;
cout << "Привет, мир!" << endl;
)", R"(
ou "Hello"
ou "Sum = ",s
ou x
ou "Done"
ou "a","\n","b"
ou a + b," ",(a << 1)
ou "err"
ou "log"
ou
ou "Привет, мир!")");
    row("cout manipulators", "", R"(
cout << setw(5) << x << endl;
cout << fixed << setprecision(2) << y << "\n";
std::cout << std::boolalpha << flag << std::flush;
cout << flush;
cout << setw(3);
cout.precision(3);
)", R"(
ou x
ou y
ou flag
process cout << flush
process cout << setw(3)
process cout.precision(3))");
    row("cout variable named like manipulator", "", "int left = 1; cout << left << right << endl;", R"(
assign left := 1
ou left)");
    row("cout exact", "exact", R"(
cout << "Sum = " << s << endl;
cout << setw(5) << x;
cout << x << '\n';
std::cout << "a\n" << std::endl;
)", R"(
ou "Sum = ",s,"\n"
ou setw(5),x
ou x,'\n'
ou "a\n","\n")");
}

void TestCodeImport::mapping()
{
    QFETCH(QString, options);
    QFETCH(QString, code);
    QFETCH(QString, expected);
    const Result r = importtest::body(code, options);
    QVERIFY2(r.problems.isEmpty(), qPrintable(r.problems.join(QLatin1Char('\n'))));
    if (r.chart != expected)
        qWarning().noquote() << "actual chart:\n" + r.chart << "\ndiagnostics:\n" + r.diagnosticsText();
    QCOMPARE(r.chart, expected);
    QCOMPARE(r.count(afce::Diagnostic::Error), 0);
}

void TestCodeImport::diagnostics_data()
{
    QTest::addColumn<QString>("options");
    QTest::addColumn<QString>("code");
    QTest::addColumn<int>("errors");
    QTest::addColumn<int>("warnings");
    QTest::addColumn<int>("infos");
    QTest::addColumn<QString>("chart");

    QTest::newRow("goto warns once") << "" << "a: x(); goto a; goto b; b: ;" << 0 << 1 << 0
                                     << "call x()\nprocess goto a\nprocess goto b";
    QTest::newRow("try warns") << "" << "try { f(); } catch (...) {}" << 0 << 1 << 0 << "call f()";
    QTest::newRow("preprocessor warns once") << "" << "#if A\nf();\n#else\ng();\n#endif" << 0 << 1 << 0
                                             << "call f()\ncall g()";
    QTest::newRow("fall through info") << "" << "switch (x) { case 1: f(); case 2: g(); }" << 0 << 0 << 1
                                       << "case (x)\n  = 1\n    call f()\n    call g()\n  = 2\n    call g()\n  default";
    QTest::newRow("dropped before label") << "" << "switch (x) { f(); case 1: g(); }" << 0 << 1 << 0
                                          << "case (x)\n  = 1\n    call g()\n  default";
    QTest::newRow("duff") << "" << "switch (n) { case 0: do { f(); case 1: g(); } while (--n); }" << 0 << 1 << 0
                          << "process switch (n) { case 0: do { f(); case 1: g(); } while (--n); }";
    QTest::newRow("skip info") << "" << "using namespace std; typedef int T;" << 0 << 0 << 2 << "";
    QTest::newRow("local prototype info") << "" << "int f(int);" << 0 << 0 << 1 << "";
    QTest::newRow("continue fallback info") << "for=while" << "for (int i = 0; i < n; i++) continue;" << 0 << 0 << 1
                                            << "forc (int i = 0; i < n; i++)\n  continue";
    QTest::newRow("orphan case label") << "" << "case 1: f();" << 0 << 1 << 0 << "call f()";
    QTest::newRow("scope fallback info") << "for=while,keepdecl"
                                         << "for (int i = 0; i < 2; i++) f(i); for (int i = 0; i < 2; i++) g(i);"
                                         << 0 << 0 << 2
                                         << "forc (int i = 0; i < 2; i++)\n  call f(i)\nforc (int i = 0; i < 2; i++)\n  call g(i)";
    QTest::newRow("macro block warning") << "" << "EACH(x) { f(x); }" << 0 << 1 << 0 << "call EACH(x)\ncall f(x)";
    QTest::newRow("syntax error if") << "" << "if x > 0) y = 1; z = 2;" << 1 << 0 << 0
                                     << "process if x > 0) y = 1\nassign z := 2";
    QTest::newRow("missing semicolon") << "" << "x = 1\nif (x) y = 2;" << 1 << 0 << 0
                                       << "assign x := 1\nif (x)\n  assign y := 2";
    QTest::newRow("stray else") << "" << "else x = 1;" << 1 << 0 << 0 << "assign x := 1";
    QTest::newRow("stray closer") << "" << "x = 1; ) y = 2;" << 1 << 0 << 0 << "assign x := 1\nassign y := 2";
    QTest::newRow("do without while") << "" << "do { x(); } y = 1;" << 1 << 0 << 0
                                      << "process do { x(); }\nassign y := 1";
    QTest::newRow("for without parts") << "" << "for (x) y(); z();" << 1 << 0 << 0 << "process for (x) y()\ncall z()";
    QTest::newRow("empty condition") << "" << "while () f(); g();" << 1 << 0 << 0 << "process while () f()\ncall g()";
    QTest::newRow("clean code") << "" << "int a = 1; while (a < 10) a *= 2; cout << a << endl;" << 0 << 0 << 0
                                << "assign a := 1\npre (a < 10)\n  assign a := a * 2\nou a";
}

void TestCodeImport::diagnostics()
{
    QFETCH(QString, options);
    QFETCH(QString, code);
    QFETCH(int, errors);
    QFETCH(int, warnings);
    QFETCH(int, infos);
    QFETCH(QString, chart);
    const Result r = importtest::body(code, options);
    QVERIFY2(r.problems.isEmpty(), qPrintable(r.problems.join(QLatin1Char('\n'))));
    if (r.count(afce::Diagnostic::Error) != errors || r.count(afce::Diagnostic::Warning) != warnings
        || r.count(afce::Diagnostic::Info) != infos) {
        qWarning().noquote() << r.diagnosticsText();
    }
    QCOMPARE(r.count(afce::Diagnostic::Error), errors);
    QCOMPARE(r.count(afce::Diagnostic::Warning), warnings);
    QCOMPARE(r.count(afce::Diagnostic::Info), infos);
    QCOMPARE(r.chart, chart);
    for (const afce::Diagnostic &d : r.diagnostics) {
        QVERIFY(!d.message.isEmpty());
        QVERIFY(d.line >= 1);
        QVERIFY(d.column >= 1);
    }
}

void TestCodeImport::mainReturn()
{
    const QString src = QStringLiteral(
        "int main() { f(); return 0; }\n"
        "int main2() { return 0; }\n");
    Result r = importtest::import(src);
    QCOMPARE(r.chart, QStringLiteral("call f()"));
    r = importtest::import(src, importtest::options(QStringLiteral("keepreturn")));
    QCOMPARE(r.chart, QStringLiteral("call f()\nreturn 0"));
    r = importtest::import(src, afce::ImportOptions(), QStringLiteral("main2"));
    QCOMPARE(r.chart, QStringLiteral("return 0"));

    QCOMPARE(importtest::import(QStringLiteral("int main() { return EXIT_SUCCESS; }")).chart, QString());
    QCOMPARE(importtest::import(QStringLiteral("int main() { return (0); }")).chart, QString());
    QCOMPARE(importtest::import(QStringLiteral("int main() { return 1; }")).chart, QStringLiteral("return 1"));
    QCOMPARE(importtest::import(QStringLiteral("int main() { if (x) return 0; f(); }")).chart,
             QStringLiteral("if (x)\n  return 0\ncall f()"));
    QCOMPARE(importtest::import(QStringLiteral("int main() { return 0; f(); }")).chart,
             QStringLiteral("return 0\ncall f()"));
    QCOMPARE(importtest::import(QStringLiteral("int main() { f(); return 0;\n#endif\n}")).chart, QStringLiteral("call f()"));
    // only the top level of main
    QCOMPARE(importtest::import(QStringLiteral("int main() { { return 0; } }")).chart, QString());
    QCOMPARE(importtest::import(QStringLiteral("int main() { while (x) { return 0; } }")).chart,
             QStringLiteral("pre (x)\n  return 0"));
}

void TestCodeImport::printfFormatParser()
{
    using afce::cimport::FlowchartBuilder;
    QVector<FlowchartBuilder::FormatPiece> p;
    QVERIFY(FlowchartBuilder::parsePrintfFormat(QStringLiteral("a %-08.3lf b %% %hhd%zu\\n"), &p));
    QCOMPARE(p.size(), 6);
    QVERIFY(p.at(0).literal);
    QCOMPARE(p.at(0).text, QStringLiteral("a "));
    QVERIFY(!p.at(1).literal);
    QCOMPARE(p.at(1).conversion, QLatin1Char('f'));
    QCOMPARE(p.at(1).length, QStringLiteral("l"));
    QVERIFY(p.at(1).flags && p.at(1).width && p.at(1).precision && !p.at(1).star);
    QCOMPARE(p.at(2).text, QStringLiteral(" b % "));
    QCOMPARE(p.at(3).length, QStringLiteral("hh"));
    QCOMPARE(p.at(4).length, QStringLiteral("z"));
    QCOMPARE(p.at(4).conversion, QLatin1Char('u'));
    QCOMPARE(p.at(5).text, QStringLiteral("\\n"));
    QVERIFY(FlowchartBuilder::parsePrintfFormat(QStringLiteral("%*.*f"), &p));
    QVERIFY(p.first().star);
    QVERIFY(!FlowchartBuilder::parsePrintfFormat(QStringLiteral("%"), &p));
    QVERIFY(!FlowchartBuilder::parsePrintfFormat(QStringLiteral("%y"), &p));
    // an escape pair is copied verbatim: "\%" does not start a conversion
    QVERIFY(FlowchartBuilder::parsePrintfFormat(QStringLiteral("\\%d"), &p));
    QCOMPARE(p.size(), 1);
    QVERIFY(p.first().literal);
}

void TestCodeImport::deepNesting_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("expectedStart");
    const int depth = 5000;
    QString ifs;
    for (int i = 0; i < depth; ++i)
        ifs += QStringLiteral("if (x) { ");
    ifs += QStringLiteral("y = 1;");
    for (int i = 0; i < depth; ++i)
        ifs += QStringLiteral(" }");
    QTest::newRow("ifs") << ifs << "if (x)\n  if (x)";

    QTest::newRow("braces") << QString(depth, QLatin1Char('{')) + QStringLiteral(" y = 1; ") + QString(depth, QLatin1Char('}'))
                            << "assign y := 1";
    QTest::newRow("parentheses") << QStringLiteral("y = ") + QString(depth, QLatin1Char('(')) + QLatin1Char('1')
            + QString(depth, QLatin1Char(')')) + QLatin1Char(';')
                                 << "assign y := ((((";
    QString whiles;
    for (int i = 0; i < depth; ++i)
        whiles += QStringLiteral("while (a) ");
    whiles += QStringLiteral("f(); g();");
    QTest::newRow("whiles") << whiles << "pre (a)\n  pre (a)";
    QString chain = QStringLiteral("if (v == 0) f(0);");
    for (int i = 1; i < depth; ++i)
        chain += QStringLiteral(" else if (v == %1) f(%1);").arg(i);
    chain += QStringLiteral(" else g();");
    QTest::newRow("else if chain") << chain << "if (v == 0)\n  call f(0)\nelse\n  if (v == 1)";
    QString dos;
    for (int i = 0; i < depth; ++i)
        dos += QStringLiteral("do ");
    dos += QStringLiteral("f();");
    for (int i = 0; i < depth; ++i)
        dos += QStringLiteral(" while (a);");
    QTest::newRow("do whiles") << dos << "post (a)\n  post (a)";
    QString mixed;
    for (int i = 0; i < depth; ++i)
        mixed += QStringLiteral("for (;;) switch (x) { case 1: try { if (y) { ");
    mixed += QStringLiteral("z();");
    for (int i = 0; i < depth; ++i)
        mixed += QStringLiteral(" } } catch (...) {} }");
    QTest::newRow("mixed") << mixed << "forc (; ; )\n  case (x)";
    QString lambdas = QStringLiteral("f(");
    for (int i = 0; i < depth; ++i)
        lambdas += QStringLiteral("[&]{ return g(");
    for (int i = 0; i < depth; ++i)
        lambdas += QStringLiteral("); }");
    lambdas += QStringLiteral(");");
    QTest::newRow("lambdas") << lambdas << "call f([&]{ return g([&]{";
}

void TestCodeImport::deepNesting()
{
    QFETCH(QString, code);
    QFETCH(QString, expectedStart);
    QElapsedTimer timer;
    timer.start();
    const Result r = importtest::body(code);
    QVERIFY2(r.problems.isEmpty(), qPrintable(r.problems.mid(0, 5).join(QLatin1Char('\n'))));
    QVERIFY2(r.chart.startsWith(expectedStart), qPrintable(r.chart.left(200)));
    QCOMPARE(r.count(afce::Diagnostic::Error), 0);
    QVERIFY2(timer.elapsed() < 10000, qPrintable(QString::number(timer.elapsed())));
}

void TestCodeImport::optionsRoundTrip()
{
    afce::ImportOptions o;
    o.forStyle = afce::ImportOptions::ForStyle::Arithmetic;
    o.exactOutput = true;
    afce::CodeImporter importer(o);
    QCOMPARE(importer.options().forStyle, afce::ImportOptions::ForStyle::Arithmetic);
    QVERIFY(importer.options().exactOutput);
    // options may change between flowchart() calls without parsing again
    QVERIFY(importer.parse(QStringLiteral("void f() { for (int i = 0; i < 3; i++) g(i); }")));
    QCOMPARE(importtest::serialize(importer.flowchart(0)), QStringLiteral("for i = 0 .. 2\n  call g(i)"));
    o.forStyle = afce::ImportOptions::ForStyle::CStyle;
    importer.setOptions(o);
    QCOMPARE(importtest::serialize(importer.flowchart(0)), QStringLiteral("forc (int i = 0; i < 3; i++)\n  call g(i)"));
}

void TestCodeImport::repeatedFlowchartCalls()
{
    afce::CodeImporter importer;
    QVERIFY(importer.parse(QStringLiteral("void f() { goto x; x: ; }\nvoid g() { h(); }")));
    const int parseDiagnostics = importer.diagnostics().size();
    importer.flowchart(0);
    QCOMPARE(importer.diagnostics().size(), parseDiagnostics + 1);
    importer.flowchart(0);
    QCOMPARE(importer.diagnostics().size(), parseDiagnostics + 1);
    importer.flowchart(1);
    QCOMPARE(importer.diagnostics().size(), parseDiagnostics);
    QCOMPARE(importtest::serialize(importer.flowchart(1)), QStringLiteral("call h()"));
}

void TestCodeImport::forWhileParameterShadowing()
{
    const afce::ImportOptions o = importtest::options(QStringLiteral("for=while"));
    Result r = importtest::import(QStringLiteral("void f(int i) { for (int i = 0; i < 3; i++) g(i); }"), o);
    QCOMPARE(r.chart, QStringLiteral("forc (int i = 0; i < 3; i++)\n  call g(i)"));
    QCOMPARE(r.count(afce::Diagnostic::Info), 1);
    r = importtest::import(QStringLiteral("void f(int n) { for (int i = 0; i < n; i++) g(i); }"), o);
    QCOMPARE(r.chart, QStringLiteral("assign i := 0\npre (i < n)\n  call g(i)\n  assign i := i + 1"));
    QCOMPARE(r.diagnostics.size(), 0);
}

QTEST_GUILESS_MAIN(TestCodeImport)
