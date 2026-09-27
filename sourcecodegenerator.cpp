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

#include "sourcecodegenerator.h"

#include "afceutil.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <functional>
#include <optional>

namespace {

const QStringList &loopTypes()
{
    static const QStringList types = {QStringLiteral("pre"), QStringLiteral("post"), QStringLiteral("for"),
                                      QStringLiteral("forc"), QStringLiteral("foreach")};
    return types;
}

bool isIdentStart(QChar c)
{
    return c.isLetter() || c == QLatin1Char('_');
}

bool isIdentChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

bool isPlainIdentifier(const QString &s)
{
    if (s.isEmpty() || !isIdentStart(s.at(0)))
        return false;
    for (const QChar c : s) {
        if (!isIdentChar(c))
            return false;
    }
    return true;
}

// s[i] is a quote; returns the index one past the closing quote (or s.size()).
int skipLiteral(const QString &s, int i)
{
    const QChar quote = s.at(i);
    ++i;
    while (i < s.size()) {
        if (s.at(i) == QLatin1Char('\\')) {
            i += 2;
            continue;
        }
        if (s.at(i) == quote)
            return i + 1;
        ++i;
    }
    return s.size();
}

// Is the literal starting at s[i] closed by its quote?
bool literalClosed(const QString &s, int i)
{
    const QChar quote = s.at(i);
    for (++i; i < s.size(); ++i) {
        if (s.at(i) == QLatin1Char('\\'))
            ++i;
        else if (s.at(i) == quote)
            return true;
    }
    return false;
}

enum class ItemKind { Expression, String, Char };

// Position of the opening quote of a literal item (after an optional L/u/U/u8 prefix), or -1.
int literalStart(const QString &item)
{
    int i = 0;
    if (item.startsWith(QLatin1String("u8")))
        i = 2;
    else if (item.size() > 1 && (item.at(0) == QLatin1Char('L') || item.at(0) == QLatin1Char('u')
                                 || item.at(0) == QLatin1Char('U')))
        i = 1;
    if (i < item.size() && (item.at(i) == QLatin1Char('"') || item.at(i) == QLatin1Char('\'')))
        return i;
    if (item.size() > 0 && (item.at(0) == QLatin1Char('"') || item.at(0) == QLatin1Char('\'')))
        return 0;
    return -1;
}

ItemKind itemKind(const QString &item)
{
    const int start = literalStart(item);
    if (start < 0 || skipLiteral(item, start) != item.size() || item.size() - start < 2 || !literalClosed(item, start))
        return ItemKind::Expression;
    return item.at(start) == QLatin1Char('"') ? ItemKind::String : ItemKind::Char;
}

QString literalContents(const QString &item)
{
    const int start = literalStart(item);
    if (start < 0)
        return item;
    return item.mid(start + 1, item.size() - start - 2);
}

QString applyEscapes(const QString &text, const QJsonObject &escapes)
{
    if (escapes.isEmpty())
        return text;
    QStringList keys = escapes.keys();
    keys.removeAll(QString());
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) { return a.size() > b.size(); });
    QString result;
    int i = 0;
    while (i < text.size()) {
        bool matched = false;
        for (const QString &key : std::as_const(keys)) {
            if (QStringView(text).mid(i).startsWith(key)) {
                result += escapes.value(key).toString();
                i += key.size();
                matched = true;
                break;
            }
        }
        if (!matched)
            result += text.at(i++);
    }
    return result;
}

// Calls f(index, depth) for every character outside string literals.
template<typename F>
void scanTopLevel(const QString &s, F f)
{
    int depth = 0;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            i = skipLiteral(s, i) - 1;
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{')) {
            f(i, depth);
            ++depth;
            continue;
        }
        if ((c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) && depth > 0) {
            --depth;
            f(i, depth);
            continue;
        }
        f(i, depth);
    }
}

bool isComparisonChar(QChar c)
{
    return c == QLatin1Char('=') || c == QLatin1Char('<') || c == QLatin1Char('>') || c == QLatin1Char('!')
           || c == QLatin1Char(':') || c == QLatin1Char('+') || c == QLatin1Char('-') || c == QLatin1Char('*')
           || c == QLatin1Char('/') || c == QLatin1Char('%') || c == QLatin1Char('&') || c == QLatin1Char('|')
           || c == QLatin1Char('^');
}

// Index of the first top-level assignment '=' (not ==, <=, >=, !=, =>, :=), -1 if none.
int topLevelAssignment(const QString &s)
{
    int result = -1;
    scanTopLevel(s, [&](int i, int depth) {
        if (result >= 0 || depth != 0 || s.at(i) != QLatin1Char('='))
            return;
        const QChar prev = i > 0 ? s.at(i - 1) : QChar();
        const QChar next = i + 1 < s.size() ? s.at(i + 1) : QChar();
        if (next == QLatin1Char('=') || next == QLatin1Char('>') || isComparisonChar(prev))
            return;
        result = i;
    });
    return result;
}

// Index of a top-level ':' that is not part of "::", -1 if none.
int topLevelColon(const QString &s)
{
    int result = -1;
    scanTopLevel(s, [&](int i, int depth) {
        if (result >= 0 || depth != 0 || s.at(i) != QLatin1Char(':'))
            return;
        if ((i > 0 && s.at(i - 1) == QLatin1Char(':')) || (i + 1 < s.size() && s.at(i + 1) == QLatin1Char(':')))
            return;
        result = i;
    });
    return result;
}

// Splits a declaration ("int a[10]", "const char *s", "a: Integer", "x = 5") into name and type.
void splitDeclarator(const QString &text, QString *name, QString *type)
{
    QString s = text.trimmed();
    const int eq = topLevelAssignment(s);
    if (eq >= 0)
        s = s.left(eq).trimmed();
    const int colon = topLevelColon(s);
    QString typePart;
    if (colon >= 0) {
        typePart = s.mid(colon + 1).trimmed();
        s = s.left(colon).trimmed();
    }
    // strip trailing array brackets
    while (s.endsWith(QLatin1Char(']'))) {
        int depth = 0;
        int open = -1;
        for (int i = s.size() - 1; i >= 0; --i) {
            if (s.at(i) == QLatin1Char(']'))
                ++depth;
            else if (s.at(i) == QLatin1Char('[') && --depth == 0) {
                open = i;
                break;
            }
        }
        if (open < 0)
            break;
        s = s.left(open).trimmed();
    }
    int end = s.size();
    while (end > 0 && !isIdentChar(s.at(end - 1)))
        --end;
    int start = end;
    while (start > 0 && isIdentChar(s.at(start - 1)))
        --start;
    if (start == end || !isIdentStart(s.at(start))) {
        if (name)
            *name = s;
        if (type)
            *type = typePart;
        return;
    }
    if (name)
        *name = s.mid(start, end - start);
    if (type)
        *type = colon >= 0 ? typePart : s.left(start).trimmed();
}

bool isNumber(const QString &s)
{
    if (s.isEmpty())
        return false;
    int i = 0;
    if (s.at(0) == QLatin1Char('-') || s.at(0) == QLatin1Char('+'))
        i = 1;
    if (i >= s.size() || !(s.at(i).isDigit() || s.at(i) == QLatin1Char('.')))
        return false;
    bool digit = false;
    for (; i < s.size(); ++i) {
        if (s.at(i).isDigit())
            digit = true;
        else if (!(s.at(i).isLetter() || s.at(i) == QLatin1Char('.') || s.at(i) == QLatin1Char('_')))
            return false;
    }
    return digit;
}

// Identifier/number/literal, optionally followed by member access, calls and subscripts; or a
// fully parenthesized expression.
bool isSimpleExpression(const QString &text)
{
    const QString s = text.trimmed();
    if (s.isEmpty())
        return true;
    if (isNumber(s) && !s.startsWith(QLatin1Char('-')) && !s.startsWith(QLatin1Char('+')))
        return true;
    if (itemKind(s) != ItemKind::Expression)
        return true;
    if (s.startsWith(QLatin1Char('('))) {
        // fully parenthesized?
        int closing = -1;
        scanTopLevel(s, [&](int i, int depth) {
            if (closing < 0 && depth == 0 && s.at(i) == QLatin1Char(')'))
                closing = i;
        });
        return closing == s.size() - 1;
    }
    int i = 0;
    const int n = s.size();
    auto skipGroup = [&](int pos) {
        int depth = 0;
        for (int k = pos; k < n; ++k) {
            const QChar c = s.at(k);
            if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                k = skipLiteral(s, k) - 1;
                continue;
            }
            if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{'))
                ++depth;
            else if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) {
                if (--depth == 0)
                    return k + 1;
            }
        }
        return -1;
    };
    while (i < n && (s.at(i) == QLatin1Char('$') || s.at(i) == QLatin1Char('@')))
        ++i;
    if (i >= n || !isIdentStart(s.at(i)))
        return false;
    while (i < n) {
        const QChar c = s.at(i);
        if (isIdentChar(c)) {
            ++i;
        } else if (c == QLatin1Char('.')) {
            ++i;
        } else if (c == QLatin1Char(':') && i + 1 < n && s.at(i + 1) == QLatin1Char(':')) {
            i += 2;
        } else if (c == QLatin1Char('-') && i + 1 < n && s.at(i + 1) == QLatin1Char('>')) {
            i += 2;
        } else if (c == QLatin1Char('(') || c == QLatin1Char('[')) {
            i = skipGroup(i);
            if (i < 0)
                return false;
        } else {
            return false;
        }
    }
    return true;
}

// Only + - * / % operators (and operands) at top level?
bool isArithmetic(const QString &s)
{
    bool ok = true;
    scanTopLevel(s, [&](int i, int depth) {
        if (depth != 0)
            return;
        const QChar c = s.at(i);
        if (c == QLatin1Char('<') || c == QLatin1Char('>') || c == QLatin1Char('=') || c == QLatin1Char('!')
            || c == QLatin1Char('&') || c == QLatin1Char('|') || c == QLatin1Char('^') || c == QLatin1Char('?')
            || c == QLatin1Char(':') || c == QLatin1Char('~') || c == QLatin1Char(','))
            ok = false;
    });
    if (!ok)
        return false;
    static const QStringList words = {QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("not"),
                                      QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("xor"),
                                      QStringLiteral("div"), QStringLiteral("mod")};
    // word operators outside literals
    QString plain;
    for (int i = 0; i < s.size(); ++i) {
        if (s.at(i) == QLatin1Char('"') || s.at(i) == QLatin1Char('\'')) {
            i = skipLiteral(s, i) - 1;
            plain += QLatin1Char(' ');
            continue;
        }
        plain += s.at(i);
    }
    int i = 0;
    while (i < plain.size()) {
        if (isIdentStart(plain.at(i))) {
            int j = i;
            while (j < plain.size() && isIdentChar(plain.at(j)))
                ++j;
            if (words.contains(plain.mid(i, j - i), Qt::CaseInsensitive))
                return false;
            i = j;
        } else {
            ++i;
        }
    }
    return true;
}

// Returns the left operand when s ends with a top-level binary "<op> 1" (op is '+' or '-').
std::optional<QString> stripTrailingOne(const QString &s, QChar op)
{
    const QString t = s.trimmed();
    if (!t.endsWith(QLatin1Char('1')))
        return std::nullopt;
    int k = t.size() - 2;
    if (k >= 0 && (t.at(k).isLetterOrNumber() || t.at(k) == QLatin1Char('.') || t.at(k) == QLatin1Char('_')))
        return std::nullopt;
    while (k >= 0 && t.at(k).isSpace())
        --k;
    if (k < 1 || t.at(k) != op)
        return std::nullopt;
    // the operator must be binary and at top level, and the left side must be additive-safe
    const QString left = t.left(k).trimmed();
    if (left.isEmpty())
        return std::nullopt;
    const QChar last = left.at(left.size() - 1);
    if (isComparisonChar(last) || last == QLatin1Char('(') || last == QLatin1Char(','))
        return std::nullopt;
    // balanced brackets?
    bool balanced = true;
    int count = 0;
    for (int i = 0; i < left.size(); ++i) {
        const QChar c = left.at(i);
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            i = skipLiteral(left, i) - 1;
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{'))
            ++count;
        else if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}'))
            --count;
        if (count < 0)
            balanced = false;
    }
    if (!balanced || count != 0)
        return std::nullopt;
    // (a * b - 1) + 1 == a * b and (a - b + 1) - 1 == a - b: only arithmetic operators on the left.
    if (!isArithmetic(left))
        return std::nullopt;
    return left;
}

QString addOne(const QString &value, bool increment)
{
    const QString v = value.trimmed();
    bool ok = false;
    const qlonglong number = v.toLongLong(&ok);
    if (ok)
        return QString::number(increment ? number + 1 : number - 1);
    if (const auto left = stripTrailingOne(v, increment ? QLatin1Char('-') : QLatin1Char('+')))
        return *left;
    const QString op = increment ? QStringLiteral(" + 1") : QStringLiteral(" - 1");
    if (isArithmetic(v))
        return v + op;
    return QLatin1Char('(') + v + QLatin1Char(')') + op;
}

// The "semicolon" filter: appends the terminator to every line of a (multi-line) statement text
// that ends a statement. Lines that already end with it, blank and comment/preprocessor lines, lines
// ending with an opening/closing brace, a comma, an operator or a word such as "then"/"do"/"begin",
// control headers ("if (x)", "} else", "for (...)") and lines continued by the next line (which
// starts with a binary operator, ".", "?", ":" or a closing bracket) are left alone.
QString terminateStatements(const QString &text, const QString &terminator)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    auto rstrip = [](QString s) {
        int end = s.size();
        while (end > 0 && s.at(end - 1).isSpace())
            --end;
        s.truncate(end);
        return s;
    };
    auto continues = [](const QString &next) {
        const QString s = next.trimmed();
        if (s.isEmpty())
            return false;
        static const QStringList starts = {QStringLiteral("."), QStringLiteral("?"), QStringLiteral(":"),
                                           QStringLiteral(")"), QStringLiteral("]"), QStringLiteral(","),
                                           QStringLiteral("&&"), QStringLiteral("||"), QStringLiteral("<<"),
                                           QStringLiteral(">>")};
        for (const QString &start : starts) {
            if (s.startsWith(start) && !s.startsWith(QLatin1String("..")))
                return true;
        }
        static const QRegularExpression binary(QStringLiteral("^(\\+|-|\\*|/|%|==|!=|<=|>=|<|>|=|&|\\||\\^)\\s"));
        return binary.match(s).hasMatch();
    };
    static const QStringList openWords = {QStringLiteral("then"), QStringLiteral("do"), QStringLiteral("begin"),
                                          QStringLiteral("else"), QStringLiteral("of"), QStringLiteral("repeat"),
                                          QStringLiteral("var"), QStringLiteral("try"), QStringLiteral("const"),
                                          QStringLiteral("type"), QStringLiteral("record")};
    static const QStringList controlWords = {QStringLiteral("if"), QStringLiteral("while"), QStringLiteral("for"),
                                             QStringLiteral("switch"), QStringLiteral("foreach"), QStringLiteral("elsif"),
                                             QStringLiteral("elseif"), QStringLiteral("unless"), QStringLiteral("until")};
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = rstrip(lines.at(i));
        const QString code = line.trimmed();
        lines[i] = line;
        if (code.isEmpty() || code.endsWith(terminator))
            continue;
        if (code.startsWith(QLatin1Char('#')) || code.startsWith(QLatin1String("//")) || code.startsWith(QLatin1String("/*"))
            || code.startsWith(QLatin1Char('*')) || code.endsWith(QLatin1String("*/")))
            continue;
        const QChar last = code.at(code.size() - 1);
        if (last == QLatin1Char('}')) {
            // "int a[] = {1, 2}", "std::vector<int> v{1, 2}", "struct S {...}" end a statement;
            // "if (x) { ... }", "else { ... }", "{ Pascal comment }" do not
            int depth = 0;
            int open = -1;
            for (int k = code.size() - 1; k >= 0; --k) {
                if (code.at(k) == QLatin1Char('}'))
                    ++depth;
                else if (code.at(k) == QLatin1Char('{') && --depth == 0) {
                    open = k;
                    break;
                }
            }
            const QString before = open > 0 ? code.left(open).trimmed() : QString();
            int w = before.size();
            while (w > 0 && isIdentChar(before.at(w - 1)))
                --w;
            static const QStringList blockWords = {QStringLiteral("else"), QStringLiteral("do"), QStringLiteral("try"),
                                                   QStringLiteral("finally"), QStringLiteral("begin")};
            const bool initializer = !before.isEmpty()
                                     && (before.endsWith(QLatin1Char('=')) || before.endsWith(QLatin1Char(']'))
                                         || before.endsWith(QLatin1Char('>'))
                                         || (isIdentChar(before.at(before.size() - 1)) && !blockWords.contains(before.mid(w))));
            if (!initializer)
                continue;
        } else {
            static const QString noTerminator = QStringLiteral("{([,\\+-*/%=&|^<>!?.:");
            if (noTerminator.contains(last))
                continue;
        }
        int wordStart = code.size();
        while (wordStart > 0 && isIdentChar(code.at(wordStart - 1)))
            --wordStart;
        if (openWords.contains(code.mid(wordStart).toLower()) && (wordStart == 0 || !isIdentChar(code.at(wordStart - 1))))
            continue;
        if (last == QLatin1Char(')')) {
            QString head = code;
            while (head.startsWith(QLatin1Char('}')))
                head = head.mid(1).trimmed();
            if (head.startsWith(QLatin1String("else ")))
                head = head.mid(5).trimmed();
            int e = 0;
            while (e < head.size() && isIdentChar(head.at(e)))
                ++e;
            if (controlWords.contains(head.left(e).toLower()))
                continue;
        }
        int next = i + 1;
        while (next < lines.size() && lines.at(next).trimmed().isEmpty())
            ++next;
        if (next < lines.size() && continues(lines.at(next)))
            continue;
        lines[i] = line + terminator;
    }
    return lines.join(QLatin1Char('\n'));
}

struct Token {
    bool placeholder = false;
    QString text;          // literal text
    QString name;          // placeholder name
    QStringList filters;   // placeholder filters
};

QList<Token> tokenize(const QString &s)
{
    QList<Token> tokens;
    QString literal;
    auto flush = [&]() {
        if (!literal.isEmpty()) {
            Token t;
            t.text = literal;
            tokens << t;
            literal.clear();
        }
    };
    const int n = s.size();
    int i = 0;
    while (i < n) {
        const QChar c = s.at(i);
        if (c != QLatin1Char('%')) {
            literal += c;
            ++i;
            continue;
        }
        if (i + 1 < n && s.at(i + 1) == QLatin1Char('%')) {
            literal += QLatin1Char('%');
            i += 2;
            continue;
        }
        int j = i + 1;
        if (j < n && (isIdentStart(s.at(j)) || s.at(j) == QLatin1Char('$'))) {
            int k = j;
            while (k < n && (isIdentChar(s.at(k)) || s.at(k) == QLatin1Char('.') || s.at(k) == QLatin1Char('$')))
                ++k;
            Token t;
            t.placeholder = true;
            t.name = s.mid(j, k - j);
            while (k < n && s.at(k) == QLatin1Char('|')) {
                int m = k + 1;
                while (m < n && (isIdentChar(s.at(m)) || s.at(m) == QLatin1Char(':')))
                    ++m;
                t.filters << s.mid(k + 1, m - k - 1);
                k = m;
            }
            if (k < n && s.at(k) == QLatin1Char('%') && !t.name.endsWith(QLatin1Char('.'))) {
                flush();
                tokens << t;
                i = k + 1;
                continue;
            }
        }
        literal += c;
        ++i;
    }
    flush();
    return tokens;
}

bool isBlockPlaceholder(const QString &name)
{
    if (name == QLatin1String("branches") || name == QLatin1String("body"))
        return true;
    if (!name.startsWith(QLatin1String("branch")) || name.size() <= 6)
        return false;
    for (int i = 6; i < name.size(); ++i) {
        if (!name.at(i).isDigit())
            return false;
    }
    return true;
}

QString templateText(const QJsonValue &value)
{
    if (value.isArray()) {
        QStringList lines;
        const QJsonArray array = value.toArray();
        for (const QJsonValue &line : array)
            lines << line.toString();
        return lines.join(QLatin1Char('\n'));
    }
    return value.toString();
}

QList<QDomElement> childElements(const QDomElement &e, const QString &tag = QString())
{
    QList<QDomElement> result;
    for (QDomElement c = e.firstChildElement(); !c.isNull(); c = c.nextSiblingElement()) {
        if (tag.isEmpty() || c.tagName() == tag)
            result << c;
    }
    return result;
}

// Nesting limits: deeper (hand-made) documents are rendered with a comment instead of recursing
// until the stack overflows; deeper expressions are left unchanged.
constexpr int maxBlockNesting = 500;
constexpr int maxExpressionNesting = 64;

bool branchTerminates(const QDomElement &branch, bool breakTerminates = true, int depth = 0);

// Does control never continue after this block? A "break" inside a nested case only leaves that
// case, so it does not count there.
bool blockTerminates(const QDomElement &block, bool breakTerminates = true, int depth = 0)
{
    const QString type = block.tagName();
    if (type == QLatin1String("return") || type == QLatin1String("continue"))
        return true;
    if (type == QLatin1String("break"))
        return breakTerminates;
    if ((type == QLatin1String("if") || type == QLatin1String("case")) && depth < maxBlockNesting) {
        const QList<QDomElement> branches = childElements(block, QStringLiteral("branch"));
        if (branches.size() < 2)
            return false;
        const bool inner = breakTerminates && type == QLatin1String("if");
        for (const QDomElement &b : branches) {
            if (!branchTerminates(b, inner, depth + 1))
                return false;
        }
        return true;
    }
    return false;
}

bool branchTerminates(const QDomElement &branch, bool breakTerminates, int depth)
{
    const QList<QDomElement> blocks = childElements(branch);
    return !blocks.isEmpty() && blockTerminates(blocks.last(), breakTerminates, depth);
}

enum class VarKind { Int, Real, Bool, String, Char };

VarKind mergeKinds(VarKind a, VarKind b)
{
    if (a == b)
        return a;
    if (a == VarKind::String || b == VarKind::String)
        return VarKind::String;
    if ((a == VarKind::Char && b == VarKind::Bool) || (a == VarKind::Bool && b == VarKind::Char))
        return VarKind::String;
    if (a == VarKind::Real || b == VarKind::Real)
        return VarKind::Real;
    return VarKind::Int;
}

bool isIntLike(const std::optional<VarKind> &kind)
{
    return !kind || *kind == VarKind::Int || *kind == VarKind::Char || *kind == VarKind::Bool;
}

// Kind of an arithmetic combination ("a * b", "s + t").
std::optional<VarKind> arithmeticKind(const std::optional<VarKind> &a, const std::optional<VarKind> &b)
{
    if ((a && *a == VarKind::String) || (b && *b == VarKind::String))
        return VarKind::String;
    if ((a && *a == VarKind::Real) || (b && *b == VarKind::Real))
        return VarKind::Real;
    return VarKind::Int;
}

// ---------------------------------------------------------------------------
// C declarations and types

const QSet<QString> &scalarTypeWords()
{
    static const QSet<QString> words = {
        QStringLiteral("unsigned"), QStringLiteral("signed"), QStringLiteral("short"), QStringLiteral("long"),
        QStringLiteral("int"), QStringLiteral("char"), QStringLiteral("float"), QStringLiteral("double"),
        QStringLiteral("bool"), QStringLiteral("_Bool"), QStringLiteral("wchar_t"), QStringLiteral("char8_t"),
        QStringLiteral("char16_t"), QStringLiteral("char32_t"), QStringLiteral("size_t"), QStringLiteral("ssize_t"),
        QStringLiteral("ptrdiff_t"), QStringLiteral("intptr_t"), QStringLiteral("uintptr_t"), QStringLiteral("int8_t"),
        QStringLiteral("int16_t"), QStringLiteral("int32_t"), QStringLiteral("int64_t"), QStringLiteral("uint8_t"),
        QStringLiteral("uint16_t"), QStringLiteral("uint32_t"), QStringLiteral("uint64_t"), QStringLiteral("std::size_t"),
        QStringLiteral("void"), QStringLiteral("const"), QStringLiteral("volatile")};
    return words;
}

const QSet<QString> &qualifierWords()
{
    static const QSet<QString> words = {
        QStringLiteral("const"), QStringLiteral("volatile"), QStringLiteral("static"), QStringLiteral("extern"),
        QStringLiteral("register"), QStringLiteral("inline"), QStringLiteral("constexpr"), QStringLiteral("mutable"),
        QStringLiteral("thread_local"), QStringLiteral("struct"), QStringLiteral("enum"), QStringLiteral("union"),
        QStringLiteral("typename")};
    return words;
}

// Statements that look like "word word" but are no declarations (compared in lower case).
const QSet<QString> &statementWords()
{
    static const QSet<QString> words = {
        QStringLiteral("return"), QStringLiteral("delete"), QStringLiteral("throw"), QStringLiteral("goto"),
        QStringLiteral("case"), QStringLiteral("default"), QStringLiteral("else"), QStringLiteral("do"),
        QStringLiteral("new"), QStringLiteral("sizeof"), QStringLiteral("typedef"), QStringLiteral("using"),
        QStringLiteral("namespace"), QStringLiteral("co_return"), QStringLiteral("co_await"), QStringLiteral("co_yield"),
        QStringLiteral("static_assert"), QStringLiteral("operator"), QStringLiteral("template"), QStringLiteral("if"),
        QStringLiteral("while"), QStringLiteral("for"), QStringLiteral("switch"), QStringLiteral("break"),
        QStringLiteral("continue"), QStringLiteral("not"), QStringLiteral("and"), QStringLiteral("or"),
        QStringLiteral("xor"), QStringLiteral("in"), QStringLiteral("is"), QStringLiteral("print"), QStringLiteral("puts"),
        QStringLiteral("echo"), QStringLiteral("let"), QStringLiteral("var"), QStringLiteral("my"), QStringLiteral("our"),
        QStringLiteral("local"), QStringLiteral("dim"), QStringLiteral("call"), QStringLiteral("input"),
        QStringLiteral("global"), QStringLiteral("def"), QStringLiteral("lambda"), QStringLiteral("yield"),
        QStringLiteral("del"), QStringLiteral("import"), QStringLiteral("from"), QStringLiteral("pass"),
        QStringLiteral("raise"), QStringLiteral("assert"), QStringLiteral("exit"), QStringLiteral("then"),
        QStringLiteral("end"), QStringLiteral("begin"), QStringLiteral("until"), QStringLiteral("unless"),
        QStringLiteral("elif"), QStringLiteral("elsif"), QStringLiteral("when"), QStringLiteral("next"),
        QStringLiteral("last"), QStringLiteral("die"), QStringLiteral("say"), QStringLiteral("public"),
        QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("friend")};
    return words;
}

// Heuristic kind of a declared type ("int", "const char" + pointer, "double" + array, "std::string");
// nullopt when unknown (structs, containers, other pointers, auto).
std::optional<VarKind> kindOfType(const QString &type, bool pointer, bool array)
{
    QString t = afce::simplifyCode(type);
    if (t.contains(QLatin1Char('*')))
        pointer = true;
    t.remove(QLatin1Char('*'));
    t.remove(QLatin1Char('&'));
    QStringList words;
    const QStringList parts = t.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &w : parts) {
        if (!qualifierWords().contains(w))
            words << w;
    }
    if (words.isEmpty())
        return std::nullopt;
    QString joined = words.join(QLatin1Char(' '));
    const QString lower = joined.toLower();
    static const QStringList strings = {QStringLiteral("std::string"), QStringLiteral("string"),
                                        QStringLiteral("std::wstring"), QStringLiteral("wstring"),
                                        QStringLiteral("std::string_view"), QStringLiteral("string_view"),
                                        QStringLiteral("qstring")};
    if (strings.contains(lower))
        return pointer || array ? std::nullopt : std::optional<VarKind>(VarKind::String);
    static const QStringList chars = {QStringLiteral("char"), QStringLiteral("wchar_t"), QStringLiteral("char8_t"),
                                      QStringLiteral("char16_t"), QStringLiteral("char32_t")};
    if (chars.contains(joined))
        return pointer || array ? VarKind::String : VarKind::Char;
    if (pointer)
        return std::nullopt;
    if (joined == QLatin1String("float") || joined == QLatin1String("double") || joined == QLatin1String("long double")
        || lower == QLatin1String("real") || lower == QLatin1String("single") || lower == QLatin1String("extended"))
        return VarKind::Real;
    if (joined == QLatin1String("bool") || joined == QLatin1String("_Bool") || lower == QLatin1String("boolean"))
        return VarKind::Bool;
    static const QSet<QString> intWords = {QStringLiteral("unsigned"), QStringLiteral("signed"), QStringLiteral("short"),
                                           QStringLiteral("long"), QStringLiteral("int"), QStringLiteral("char")};
    bool allInt = true;
    for (const QString &w : std::as_const(words)) {
        if (!intWords.contains(w))
            allInt = false;
    }
    if (allInt)
        return VarKind::Int;
    if (scalarTypeWords().contains(joined) && joined != QLatin1String("void"))
        return VarKind::Int; // size_t, int32_t, ...
    static const QStringList pascalInts = {QStringLiteral("integer"), QStringLiteral("longint"), QStringLiteral("int64"),
                                           QStringLiteral("cardinal"), QStringLiteral("smallint"), QStringLiteral("byte"),
                                           QStringLiteral("word")};
    if (pascalInts.contains(lower))
        return VarKind::Int;
    return std::nullopt;
}

// Category of the type of a C cast "(T)": "int", "real", "char", "bool", "string" (char pointer),
// "pointer", "void"; empty when T is not made of scalar type words.
QString castCategory(const QString &inner)
{
    QString t = afce::simplifyCode(inner);
    bool pointer = false;
    while (t.endsWith(QLatin1Char('*'))) {
        pointer = true;
        t.chop(1);
        t = t.trimmed();
    }
    if (t.isEmpty() || t.contains(QLatin1Char('*')))
        return QString();
    const QStringList words = t.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &w : words) {
        if (!scalarTypeWords().contains(w))
            return QString();
    }
    if (t == QLatin1String("void") || t == QLatin1String("const void"))
        return pointer ? QStringLiteral("pointer") : QStringLiteral("void");
    const std::optional<VarKind> kind = kindOfType(t, pointer, false);
    if (!kind)
        return pointer ? QStringLiteral("pointer") : QString();
    switch (*kind) {
    case VarKind::Int:
        return QStringLiteral("int");
    case VarKind::Real:
        return QStringLiteral("real");
    case VarKind::Char:
        return QStringLiteral("char");
    case VarKind::Bool:
        return QStringLiteral("bool");
    case VarKind::String:
        return QStringLiteral("string");
    }
    return QString();
}

std::optional<VarKind> kindOfCastCategory(const QString &category)
{
    if (category == QLatin1String("int"))
        return VarKind::Int;
    if (category == QLatin1String("real"))
        return VarKind::Real;
    if (category == QLatin1String("char"))
        return VarKind::Char;
    if (category == QLatin1String("bool"))
        return VarKind::Bool;
    if (category == QLatin1String("string"))
        return VarKind::String;
    return std::nullopt;
}

// Index one past the bracket group opening at s[open] ("(", "[", "{"), -1 when unbalanced.
int groupEnd(const QString &s, int open)
{
    int depth = 0;
    for (int i = open; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            i = skipLiteral(s, i) - 1;
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{'))
            ++depth;
        else if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) {
            if (--depth == 0)
                return i + 1;
        }
    }
    return -1;
}

// Identifier qualified with "::" starting at i; returns its end (i when there is none).
int qualifiedNameEnd(const QString &s, int i)
{
    int j = i;
    if (QStringView(s).mid(j).startsWith(QLatin1String("::")))
        j += 2;
    if (j >= s.size() || !isIdentStart(s.at(j)))
        return i;
    for (;;) {
        while (j < s.size() && isIdentChar(s.at(j)))
            ++j;
        if (QStringView(s).mid(j).startsWith(QLatin1String("::")) && j + 2 < s.size() && isIdentStart(s.at(j + 2))) {
            j += 2;
            continue;
        }
        return j;
    }
}

// Template argument list starting at s[i] == '<'; index one past the matching '>' or -1.
int templateArgumentsEnd(const QString &s, int i)
{
    const QStringView rest = QStringView(s).mid(i);
    if (rest.startsWith(QLatin1String("<<")) || rest.startsWith(QLatin1String("<=")))
        return -1;
    int depth = 0;
    for (int j = i; j < s.size(); ++j) {
        const QChar c = s.at(j);
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            j = skipLiteral(s, j) - 1;
            continue;
        }
        const QStringView two = QStringView(s).mid(j, 2);
        if (two == QLatin1String("&&") || two == QLatin1String("||") || two == QLatin1String("==")
            || two == QLatin1String("!=") || c == QLatin1Char(';') || c == QLatin1Char('{') || c == QLatin1Char('}')
            || c == QLatin1Char('?'))
            return -1;
        if (c == QLatin1Char('<'))
            ++depth;
        else if (c == QLatin1Char('>') && --depth == 0)
            return j + 1;
    }
    return -1;
}

struct Declarator {
    QString name;
    QString type;       // as written, without the declarator's '*' / '&' ("const char", "std::vector<int>")
    bool pointer = false;
    bool array = false;
    QString init;       // initializer expression ("= e" or the contents of "(e)" / "{e}")
};

// Parses a C/C++ declaration statement ("int a, b[5] = {0}, *p", "const char *s = \"x\"",
// "std::vector<int> v(10)", "unsigned long long n"). False for anything else.
bool parseDeclaration(const QString &statement, QList<Declarator> *out)
{
    QString s = afce::simplifyCode(statement);
    while (s.endsWith(QLatin1Char(';'))) {
        s.chop(1);
        s = s.trimmed();
    }
    const int n = s.size();
    QStringList words;
    int i = 0;
    for (;;) {
        while (i < n && s.at(i).isSpace())
            ++i;
        const int end = qualifiedNameEnd(s, i);
        if (end == i)
            break;
        int j = end;
        int k = j;
        while (k < n && s.at(k).isSpace())
            ++k;
        if (k < n && s.at(k) == QLatin1Char('<')) {
            const int close = templateArgumentsEnd(s, k);
            if (close < 0)
                return false;
            j = close;
        }
        words << s.mid(i, j - i);
        i = j;
    }
    if (words.isEmpty() || statementWords().contains(words.first().toLower()))
        return false;
    QString declarators = s.mid(i).trimmed();
    if (!declarators.startsWith(QLatin1Char('*')) && !declarators.startsWith(QLatin1Char('&'))) {
        if (words.size() < 2)
            return false;
        declarators.prepend(words.takeLast());
    }
    QStringList typeWords;
    for (const QString &w : std::as_const(words)) {
        if (statementWords().contains(w.toLower()))
            return false;
        if (!qualifierWords().contains(w))
            typeWords << w;
    }
    if (typeWords.isEmpty())
        return false;
    const QString type = words.join(QLatin1Char(' '));

    // split the declarators at top-level commas
    QStringList parts;
    int start = 0;
    scanTopLevel(declarators, [&](int k, int depth) {
        if (depth == 0 && declarators.at(k) == QLatin1Char(',')) {
            parts << declarators.mid(start, k - start);
            start = k + 1;
        }
    });
    parts << declarators.mid(start);

    QList<Declarator> result;
    for (const QString &part : std::as_const(parts)) {
        const QString p = part.trimmed();
        Declarator d;
        d.type = type;
        int j = 0;
        for (;;) {
            while (j < p.size() && (p.at(j).isSpace() || p.at(j) == QLatin1Char('&')))
                ++j;
            if (j < p.size() && p.at(j) == QLatin1Char('*')) {
                d.pointer = true;
                ++j;
                continue;
            }
            const int e = qualifiedNameEnd(p, j);
            const QString word = p.mid(j, e - j);
            if (word == QLatin1String("const") || word == QLatin1String("volatile")) {
                j = e;
                continue;
            }
            break;
        }
        int e = j;
        while (e < p.size() && isIdentChar(p.at(e)))
            ++e;
        d.name = p.mid(j, e - j);
        if (!isPlainIdentifier(d.name) || scalarTypeWords().contains(d.name) || qualifierWords().contains(d.name)
            || statementWords().contains(d.name.toLower()))
            return false;
        j = e;
        for (;;) {
            while (j < p.size() && p.at(j).isSpace())
                ++j;
            if (j < p.size() && p.at(j) == QLatin1Char('[')) {
                const int close = groupEnd(p, j);
                if (close < 0)
                    return false;
                d.array = true;
                j = close;
                continue;
            }
            break;
        }
        if (j < p.size()) {
            const QChar c = p.at(j);
            if (c == QLatin1Char('=') && (j + 1 >= p.size() || p.at(j + 1) != QLatin1Char('='))) {
                d.init = p.mid(j + 1).trimmed();
            } else if (c == QLatin1Char('(') || c == QLatin1Char('{')) {
                const int close = groupEnd(p, j);
                if (close != p.size())
                    return false;
                d.init = p.mid(j + 1, close - j - 2).trimmed();
                if (d.init.contains(QLatin1Char(',')))
                    d.init.clear();
            } else {
                return false;
            }
        }
        result << d;
    }
    if (out)
        *out = result;
    return true;
}

// ---------------------------------------------------------------------------
// C expressions: casts, binary operator templates, heuristic kinds

using KindLookup = std::function<std::optional<VarKind>(const QString &)>;

struct BinaryTemplate {
    QString tpl;          // "%1" = left operand, "%2" = right operand
    bool intOnly = false; // only when both operands are integers
};

struct ExprInfo {
    QString text;                 // converted text
    std::optional<VarKind> kind;  // nullopt: unknown
};

// "%1" / "%2" in one pass ("%%" = "%").
QString substituteOperands(const QString &tpl, const QString &a, const QString &b = QString())
{
    QString out;
    for (int i = 0; i < tpl.size(); ++i) {
        if (tpl.at(i) == QLatin1Char('%') && i + 1 < tpl.size()) {
            const QChar next = tpl.at(i + 1);
            if (next == QLatin1Char('1') || next == QLatin1Char('2')) {
                out += next == QLatin1Char('1') ? a : b;
                ++i;
                continue;
            }
            if (next == QLatin1Char('%')) {
                out += QLatin1Char('%');
                ++i;
                continue;
            }
        }
        out += tpl.at(i);
    }
    return out;
}

// Operand for "%1" / "%2" of a template: a fully parenthesized operand loses its parentheses when
// the placeholder is a function argument ("Mod(%1, %2)": "(a + b) % c" -> "Mod(a + b, c)").
QString templateOperand(const QString &tpl, const QString &placeholder, const QString &operand)
{
    const QString o = operand.trimmed();
    if (!o.startsWith(QLatin1Char('(')) || groupEnd(o, 0) != o.size())
        return o;
    const int at = tpl.indexOf(placeholder);
    if (at < 0)
        return o;
    const QString before = tpl.left(at).trimmed();
    const QString after = tpl.mid(at + placeholder.size()).trimmed();
    if ((before.endsWith(QLatin1Char('(')) || before.endsWith(QLatin1Char(',')))
        && (after.startsWith(QLatin1Char(')')) || after.startsWith(QLatin1Char(','))))
        return o.mid(1, o.size() - 2).trimmed();
    return o;
}

bool startsOperand(const QString &s, int i, bool unary)
{
    if (i >= s.size())
        return false;
    const QChar c = s.at(i);
    if (isIdentStart(c) || c.isDigit() || c == QLatin1Char('"') || c == QLatin1Char('\'') || c == QLatin1Char('(')
        || c == QLatin1Char('[') || c == QLatin1Char('{'))
        return true;
    if ((c == QLatin1Char('$') || c == QLatin1Char('@')) && i + 1 < s.size()
        && (isIdentStart(s.at(i + 1)) || s.at(i + 1) == QLatin1Char('{') || s.at(i + 1) == QLatin1Char('$')))
        return true;
    if (c == QLatin1Char('.') && i + 1 < s.size() && s.at(i + 1).isDigit())
        return true;
    return unary && (c == QLatin1Char('-') || c == QLatin1Char('+') || c == QLatin1Char('!') || c == QLatin1Char('~')
                     || c == QLatin1Char('*') || c == QLatin1Char('&'));
}

// A small parser for C expressions (and statement lists written in C notation). It keeps the text
// as written except for casts ("casts": category -> template, "*" = any other) and multiplicative
// operators with a template ("%" / "/", e.g. "Mod(%1, %2)"), and infers a heuristic kind. Text it
// does not understand (other languages, "**", "//", unbalanced brackets) makes it give up (nullopt).
class ExprAnalyzer
{
public:
    ExprAnalyzer(KindLookup lookup, const QHash<QString, BinaryTemplate> *binary = nullptr,
                 const QHash<QString, QString> *casts = nullptr)
        : m_lookup(std::move(lookup)), m_binary(binary), m_casts(casts)
    {
    }

    std::optional<ExprInfo> run(const QString &s, int depth = 0) const;

private:
    struct Unit {
        bool isOperator = false;
        QString op;                   // operator ("" = two operands side by side)
        QString text;                 // operators: with the surrounding whitespace
        std::optional<VarKind> kind;
    };
    KindLookup m_lookup;
    const QHash<QString, BinaryTemplate> *m_binary;
    const QHash<QString, QString> *m_casts;

    std::optional<Unit> operand(const QString &s, int &i, int depth) const;
    std::optional<QString> readOperator(const QString &s, int &i) const;
    static std::optional<VarKind> sequenceKind(const QList<Unit> &units, int from, int to);
};

std::optional<QString> ExprAnalyzer::readOperator(const QString &s, int &i) const
{
    static const QStringList three = {QStringLiteral("<<="), QStringLiteral(">>="), QStringLiteral("...")};
    static const QStringList two = {QStringLiteral("&&"), QStringLiteral("||"), QStringLiteral("=="), QStringLiteral("!="),
                                    QStringLiteral("<="), QStringLiteral(">="), QStringLiteral("<<"), QStringLiteral(">>"),
                                    QStringLiteral("+="), QStringLiteral("-="), QStringLiteral("*="), QStringLiteral("/="),
                                    QStringLiteral("%="), QStringLiteral("&="), QStringLiteral("|="), QStringLiteral("^="),
                                    QStringLiteral("<>"), QStringLiteral(":="), QStringLiteral("=>"), QStringLiteral("..")};
    const QStringView rest = QStringView(s).mid(i);
    if (rest.startsWith(QLatin1String("**")) || rest.startsWith(QLatin1String("//")) || rest.startsWith(QLatin1String("/*")))
        return std::nullopt;
    for (const QString &op : three) {
        if (rest.startsWith(op)) {
            i += op.size();
            return op;
        }
    }
    for (const QString &op : two) {
        if (rest.startsWith(op)) {
            i += op.size();
            return op;
        }
    }
    static const QString single = QStringLiteral("*/%+-<>&|^=?:,;.");
    if (single.contains(s.at(i))) {
        ++i;
        return QString(s.at(i - 1));
    }
    if (isIdentStart(s.at(i))) {
        int j = i;
        while (j < s.size() && isIdentChar(s.at(j)))
            ++j;
        const QString word = s.mid(i, j - i).toLower();
        static const QStringList logical = {QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("xor"),
                                            QStringLiteral("и"), QStringLiteral("или")};
        static const QStringList comparisons = {QStringLiteral("eq"), QStringLiteral("ne"), QStringLiteral("lt"),
                                                QStringLiteral("gt"), QStringLiteral("le"), QStringLiteral("ge")};
        static const QStringList arithmetic = {QStringLiteral("mod"), QStringLiteral("div")};
        if (logical.contains(word)) {
            i = j;
            return QStringLiteral("&&");
        }
        if (comparisons.contains(word)) {
            i = j;
            return QStringLiteral("==");
        }
        if (arithmetic.contains(word)) {
            i = j;
            return word;
        }
    }
    if (startsOperand(s, i, false))
        return QString(); // side by side ("return x", "if x", "print a")
    return std::nullopt;
}

std::optional<ExprAnalyzer::Unit> ExprAnalyzer::operand(const QString &s, int &i, int depth) const
{
    const int n = s.size();
    struct Prefix {
        QString text;      // as written, with the following whitespace
        QString op;        // "!", "-", "*", ... or "cast"
        QString category;  // casts
    };
    QList<Prefix> prefixes;
    for (;;) {
        const int ws = i;
        while (i < n && s.at(i).isSpace())
            ++i;
        if (!prefixes.isEmpty())
            prefixes.last().text += s.mid(ws, i - ws);
        if (i >= n)
            return std::nullopt;
        const QStringView rest = QStringView(s).mid(i);
        const QChar c = s.at(i);
        if (rest.startsWith(QLatin1String("++")) || rest.startsWith(QLatin1String("--"))) {
            prefixes << Prefix{rest.left(2).toString(), rest.left(2).toString(), QString()};
            i += 2;
            continue;
        }
        if (c == QLatin1Char('-') || c == QLatin1Char('+') || c == QLatin1Char('~') || c == QLatin1Char('*')
            || (c == QLatin1Char('!') && !rest.startsWith(QLatin1String("!=")))
            || (c == QLatin1Char('&') && !rest.startsWith(QLatin1String("&&")))) {
            prefixes << Prefix{QString(c), QString(c), QString()};
            ++i;
            continue;
        }
        if (isIdentStart(c)) {
            int j = i;
            while (j < n && isIdentChar(s.at(j)))
                ++j;
            const QString word = s.mid(i, j - i).toLower();
            if ((word == QLatin1String("not") || word == QString::fromUtf8("не")) && j < n
                && (s.at(j).isSpace() || s.at(j) == QLatin1Char('('))) {
                prefixes << Prefix{s.mid(i, j - i), QStringLiteral("!"), QString()};
                i = j;
                continue;
            }
        }
        if (c == QLatin1Char('(')) {
            const int close = groupEnd(s, i);
            if (close < 0)
                return std::nullopt;
            const QString category = castCategory(s.mid(i + 1, close - i - 2));
            int k = close;
            while (k < n && s.at(k).isSpace())
                ++k;
            if (!category.isEmpty() && startsOperand(s, k, true)) {
                prefixes << Prefix{s.mid(i, k - i), QStringLiteral("cast"), category};
                i = k;
                continue;
            }
        }
        break;
    }

    // primary
    QString core;
    std::optional<VarKind> kind;
    QString word; // identifier of the primary (function calls)
    const QChar c = s.at(i);
    if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
        const int j = skipLiteral(s, i);
        core = s.mid(i, j - i);
        if (c == QLatin1Char('"')) {
            kind = VarKind::String;
        } else {
            const QString text = literalContents(core);
            kind = text.size() == 1 || (text.size() == 2 && text.at(0) == QLatin1Char('\\')) ? VarKind::Char
                                                                                             : VarKind::String;
        }
        i = j;
    } else if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && s.at(i + 1).isDigit())) {
        int j = i;
        bool real = false;
        if (QStringView(s).mid(i).startsWith(QLatin1String("0x")) || QStringView(s).mid(i).startsWith(QLatin1String("0X"))) {
            j += 2;
            while (j < n && (s.at(j).isLetterOrNumber() || s.at(j) == QLatin1Char('\'')))
                ++j;
        } else {
            while (j < n) {
                const QChar d = s.at(j);
                if (d.isDigit() || d == QLatin1Char('\'') || d == QLatin1Char('_')) {
                    ++j;
                } else if (d == QLatin1Char('.') && !(j + 1 < n && s.at(j + 1) == QLatin1Char('.'))) {
                    real = true;
                    ++j;
                } else if ((d == QLatin1Char('e') || d == QLatin1Char('E')) && j + 1 < n
                           && (s.at(j + 1).isDigit() || ((s.at(j + 1) == QLatin1Char('-') || s.at(j + 1) == QLatin1Char('+'))
                                                         && j + 2 < n && s.at(j + 2).isDigit()))) {
                    real = true;
                    j += 2;
                } else if (d == QLatin1Char('f') || d == QLatin1Char('F')) {
                    real = true;
                    ++j;
                } else if (d.isLetter()) {
                    ++j; // suffixes u, l
                } else {
                    break;
                }
            }
        }
        core = s.mid(i, j - i);
        kind = real ? VarKind::Real : VarKind::Int;
        i = j;
    } else if (isIdentStart(c) || c == QLatin1Char('$') || c == QLatin1Char('@') || QStringView(s).mid(i).startsWith(QLatin1String("::"))) {
        int j = i;
        while (j < n && (s.at(j) == QLatin1Char('$') || s.at(j) == QLatin1Char('@')))
            ++j;
        const int end = qualifiedNameEnd(s, j);
        if (end == j)
            return std::nullopt;
        word = s.mid(j, end - j);
        core = s.mid(i, end - i);
        i = end;
        static const QStringList booleans = {QStringLiteral("true"), QStringLiteral("false"), QString::fromUtf8("да"),
                                             QString::fromUtf8("нет")};
        static const QStringList reals = {QStringLiteral("M_PI"), QStringLiteral("M_E"), QStringLiteral("M_SQRT2"),
                                          QStringLiteral("INFINITY"), QStringLiteral("NAN"), QStringLiteral("HUGE_VAL"),
                                          QStringLiteral("DBL_MAX"), QStringLiteral("DBL_MIN"), QStringLiteral("DBL_EPSILON"),
                                          QStringLiteral("FLT_MAX"), QStringLiteral("FLT_MIN"), QStringLiteral("FLT_EPSILON")};
        if (booleans.contains(word, Qt::CaseInsensitive))
            kind = VarKind::Bool;
        else if (reals.contains(word))
            kind = VarKind::Real;
        else if (m_lookup)
            kind = m_lookup(word);
    } else if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{')) {
        const int close = groupEnd(s, i);
        if (close < 0)
            return std::nullopt;
        const std::optional<ExprInfo> inner = run(s.mid(i + 1, close - i - 2), depth + 1);
        if (!inner)
            return std::nullopt;
        core = QString(c) + inner->text + s.at(close - 1);
        kind = c == QLatin1Char('(') ? inner->kind : std::nullopt;
        i = close;
    } else {
        return std::nullopt;
    }

    // postfix: calls, subscripts, members, ++/--
    for (;;) {
        int k = i;
        while (k < n && s.at(k).isSpace())
            ++k;
        if (k < n && (s.at(k) == QLatin1Char('(') || s.at(k) == QLatin1Char('['))) {
            // "if (x)", "return (a + b) / 2", "(a) (b)": not calls
            if (s.at(k) == QLatin1Char('(')
                && (word.isEmpty() ? k > i
                                   : statementWords().contains(word.toLower()) && word != QLatin1String("sizeof")))
                break;
            const int close = groupEnd(s, k);
            if (close < 0)
                return std::nullopt;
            const std::optional<ExprInfo> inner = run(s.mid(k + 1, close - k - 2), depth + 1);
            if (!inner)
                return std::nullopt;
            core += s.mid(i, k - i) + s.at(k) + inner->text + s.at(close - 1);
            if (s.at(k) == QLatin1Char('(')) {
                static const QStringList realFunctions = {
                    QStringLiteral("sqrt"), QStringLiteral("sin"), QStringLiteral("cos"), QStringLiteral("tan"),
                    QStringLiteral("atan"), QStringLiteral("atan2"), QStringLiteral("asin"), QStringLiteral("acos"),
                    QStringLiteral("exp"), QStringLiteral("log"), QStringLiteral("log10"), QStringLiteral("log2"),
                    QStringLiteral("ln"), QStringLiteral("pow"), QStringLiteral("fabs"), QStringLiteral("sqr"),
                    QStringLiteral("floor"), QStringLiteral("ceil"), QStringLiteral("round"), QStringLiteral("trunc"),
                    QStringLiteral("fmod"), QStringLiteral("hypot"), QStringLiteral("atof"), QStringLiteral("strtod"),
                    QStringLiteral("std::sqrt"), QStringLiteral("std::pow"), QStringLiteral("std::fabs"),
                    QStringLiteral("std::abs"), QStringLiteral("std::floor"), QStringLiteral("std::ceil"),
                    QStringLiteral("std::round"), QStringLiteral("std::stod")};
                static const QStringList intFunctions = {
                    QStringLiteral("abs"), QStringLiteral("strlen"), QStringLiteral("atoi"), QStringLiteral("atol"),
                    QStringLiteral("rand"), QStringLiteral("getchar"), QStringLiteral("std::stoi"), QStringLiteral("toupper"),
                    QStringLiteral("tolower"), QStringLiteral("strcmp"), QStringLiteral("sizeof")};
                if (realFunctions.contains(word.toLower()))
                    kind = VarKind::Real;
                else if (intFunctions.contains(word))
                    kind = VarKind::Int;
                else if (word.endsWith(QLatin1String("::size")) || word.endsWith(QLatin1String("::length")))
                    kind = VarKind::Int;
                else
                    kind = std::nullopt;
            } else if (kind && *kind == VarKind::String) {
                kind = VarKind::Char;
            }
            word.clear();
            i = close;
            continue;
        }
        const QStringView rest = QStringView(s).mid(i);
        if ((rest.startsWith(QLatin1Char('.')) || rest.startsWith(QLatin1String("->"))) && !rest.startsWith(QLatin1String(".."))) {
            const int skip = rest.startsWith(QLatin1Char('.')) ? 1 : 2;
            const int end = qualifiedNameEnd(s, i + skip);
            if (end == i + skip)
                break;
            const QString member = s.mid(i + skip, end - i - skip);
            core += s.mid(i, end - i);
            i = end;
            kind = std::nullopt;
            word = QStringLiteral("::") + member; // "s.size()", "v.length()"
            continue;
        }
        if (rest.startsWith(QLatin1String("++")) || rest.startsWith(QLatin1String("--"))) {
            core += rest.left(2);
            i += 2;
            continue;
        }
        break;
    }

    // prefixes, innermost first
    QString text = core;
    for (int p = prefixes.size() - 1; p >= 0; --p) {
        const Prefix &prefix = prefixes.at(p);
        if (prefix.op == QLatin1String("cast")) {
            QString tpl;
            bool found = false;
            if (m_casts) {
                if (m_casts->contains(prefix.category)) {
                    tpl = m_casts->value(prefix.category);
                    found = true;
                } else if (m_casts->contains(QStringLiteral("*"))) {
                    tpl = m_casts->value(QStringLiteral("*"));
                    found = true;
                }
            }
            if (found) {
                // "(int) (a * b)" -> "Int(a * b)" rather than "Int((a * b))"
                text = substituteOperands(tpl, templateOperand(tpl, QStringLiteral("%1"), text));
            } else {
                text = prefix.text + text;
            }
            kind = kindOfCastCategory(prefix.category);
            continue;
        }
        text = prefix.text + text;
        if (prefix.op == QLatin1String("!"))
            kind = VarKind::Bool;
        else if (prefix.op == QLatin1String("*"))
            kind = kind && *kind == VarKind::String ? std::optional<VarKind>(VarKind::Char) : std::nullopt;
        else if (prefix.op == QLatin1String("&"))
            kind = std::nullopt;
        else if (kind && (*kind == VarKind::Char || *kind == VarKind::Bool))
            kind = VarKind::Int;
    }
    Unit unit;
    unit.text = text;
    unit.kind = kind;
    return unit;
}

std::optional<VarKind> ExprAnalyzer::sequenceKind(const QList<Unit> &units, int from, int to)
{
    // after the last assignment / sequence operator
    for (int k = to - 1; k >= from; --k) {
        const QString &op = units.at(k).op;
        if (units.at(k).isOperator
            && (op == QLatin1String(",") || op == QLatin1String(";") || op == QLatin1String(":=")
                || (op.endsWith(QLatin1Char('=')) && op != QLatin1String("==") && op != QLatin1String("!=")
                    && op != QLatin1String("<=") && op != QLatin1String(">=")))) {
            from = k + 1;
            break;
        }
    }
    // a ? b : c -> b
    for (int k = from; k < to; ++k) {
        if (units.at(k).isOperator && units.at(k).op == QLatin1String("?")) {
            int end = k + 1;
            while (end < to && !(units.at(end).isOperator && units.at(end).op == QLatin1String(":")))
                ++end;
            return sequenceKind(units, k + 1, end);
        }
    }
    static const QStringList logical = {QStringLiteral("&&"), QStringLiteral("||"), QStringLiteral("=="),
                                        QStringLiteral("!="), QStringLiteral("<"), QStringLiteral(">"),
                                        QStringLiteral("<="), QStringLiteral(">="), QStringLiteral("<>")};
    int operands = 0;
    std::optional<VarKind> single;
    std::optional<VarKind> combined;
    for (int k = from; k < to; ++k) {
        const Unit &u = units.at(k);
        if (u.isOperator) {
            if (u.op.isEmpty())
                return std::nullopt;
            if (logical.contains(u.op))
                return VarKind::Bool;
            continue;
        }
        single = u.kind;
        combined = operands == 0 ? (u.kind ? u.kind : std::optional<VarKind>(VarKind::Int)) : arithmeticKind(combined, u.kind);
        ++operands;
    }
    if (operands == 1)
        return single;
    if (operands == 0)
        return std::nullopt;
    if (combined && (*combined == VarKind::Char || *combined == VarKind::Bool))
        return VarKind::Int;
    return combined;
}

std::optional<ExprInfo> ExprAnalyzer::run(const QString &s, int depth) const
{
    if (depth > maxExpressionNesting)
        return std::nullopt;
    const int n = s.size();
    int i = 0;
    while (i < n && s.at(i).isSpace())
        ++i;
    if (i >= n)
        return ExprInfo{s, std::nullopt};
    const QString lead = s.left(i);
    QString trail;
    QList<Unit> units;
    for (;;) {
        const std::optional<Unit> u = operand(s, i, depth);
        if (!u)
            return std::nullopt;
        units << *u;
        const int ws = i;
        while (i < n && s.at(i).isSpace())
            ++i;
        if (i >= n) {
            trail = s.mid(ws);
            break;
        }
        const std::optional<QString> op = readOperator(s, i);
        if (!op)
            return std::nullopt;
        while (i < n && s.at(i).isSpace())
            ++i;
        Unit opUnit;
        opUnit.isOperator = true;
        opUnit.op = *op;
        opUnit.text = s.mid(ws, i - ws);
        units << opUnit;
        if (i >= n)
            break; // trailing operator ("x = 1;", "if x:")
    }

    auto multiplicative = [](const Unit &u) {
        return u.isOperator && (u.op == QLatin1String("*") || u.op == QLatin1String("/") || u.op == QLatin1String("%"));
    };
    QString out = lead;
    int k = 0;
    while (k < units.size()) {
        if (units.at(k).isOperator) {
            out += units.at(k).text;
            ++k;
            continue;
        }
        QString acc = units.at(k).text;
        std::optional<VarKind> accKind = units.at(k).kind;
        int m = k + 1;
        while (m + 1 < units.size() && multiplicative(units.at(m)) && !units.at(m + 1).isOperator) {
            const Unit &op = units.at(m);
            const Unit &right = units.at(m + 1);
            const bool more = m + 2 < units.size() && multiplicative(units.at(m + 2));
            const auto binary = m_binary ? m_binary->constFind(op.op) : QHash<QString, BinaryTemplate>::const_iterator();
            if (m_binary && binary != m_binary->constEnd()
                && (!binary->intOnly || (isIntLike(accKind) && isIntLike(right.kind)))) {
                QString result = substituteOperands(binary->tpl, templateOperand(binary->tpl, QStringLiteral("%1"), acc),
                                                    templateOperand(binary->tpl, QStringLiteral("%2"), right.text));
                if (more && !isSimpleExpression(result))
                    result = QLatin1Char('(') + result + QLatin1Char(')');
                acc = result;
            } else {
                acc += op.text + right.text;
            }
            accKind = arithmeticKind(accKind, right.kind);
            m += 2;
        }
        out += acc;
        k = m;
    }
    out += trail;
    return ExprInfo{out, sequenceKind(units, 0, units.size())};
}

// Heuristic kind of an assigned expression (Int when unknown).
VarKind expressionKind(const QString &expression, const KindLookup &lookup)
{
    const QString e = expression.trimmed();
    if (e.isEmpty())
        return VarKind::Int;
    const std::optional<ExprInfo> info = ExprAnalyzer(lookup).run(e);
    if (info && info->kind)
        return *info->kind;
    return VarKind::Int;
}

// Converts one C statement of a for header into a plain assignment ("int i = 0" -> "i = 0",
// "i++" -> "i = i + 1", "j -= 2" -> "j = j - 2"); nullopt for a declaration without initializer.
std::optional<QString> convertCStatement(const QString &statement, bool declarationsOnly)
{
    const QString p = statement.trimmed();
    // ++x, x++, --x, x--
    for (const QString &op : {QStringLiteral("++"), QStringLiteral("--")}) {
        if (declarationsOnly)
            break;
        QString target;
        if (p.startsWith(op))
            target = p.mid(2).trimmed();
        else if (p.endsWith(op))
            target = p.left(p.size() - 2).trimmed();
        if (!target.isEmpty() && isSimpleExpression(target) && !target.startsWith(QLatin1Char('(')))
            return target + QStringLiteral(" = ") + target
                   + (op == QLatin1String("++") ? QStringLiteral(" + 1") : QStringLiteral(" - 1"));
    }
    // compound assignment "x op= e"
    int compound = -1;
    int opStart = -1;
    scanTopLevel(p, [&](int i, int depth) {
        if (compound >= 0 || depth != 0 || p.at(i) != QLatin1Char('=') || i == 0)
            return;
        if (i + 1 < p.size() && p.at(i + 1) == QLatin1Char('='))
            return;
        static const QString ops = QStringLiteral("+-*/%&|^");
        if (i >= 2 && (p.mid(i - 2, 2) == QLatin1String("<<") || p.mid(i - 2, 2) == QLatin1String(">>"))) {
            compound = i;
            opStart = i - 2;
        } else if (ops.contains(p.at(i - 1))) {
            compound = i;
            opStart = i - 1;
        }
    });
    if (compound > 0 && opStart > 0 && !declarationsOnly) {
        const QString target = p.left(opStart).trimmed();
        const QString op = p.mid(opStart, compound - opStart);
        const QString operand = p.mid(compound + 1).trimmed();
        const bool bare = isSimpleExpression(operand) || (op == QLatin1String("+") && isArithmetic(operand));
        return target + QStringLiteral(" = ") + target + QLatin1Char(' ') + op + QLatin1Char(' ')
               + (bare ? operand : QLatin1Char('(') + operand + QLatin1Char(')'));
    }
    QString name;
    QString type;
    const int eq = topLevelAssignment(p);
    if (eq >= 0) {
        QString target = p.left(eq).trimmed();
        splitDeclarator(target, &name, &type);
        if (!type.isEmpty() && isPlainIdentifier(name) && !target.endsWith(QLatin1Char(']'))
            && !target.contains(QLatin1Char('(')))
            target = name; // a declaration with initializer
        return target + QStringLiteral(" = ") + p.mid(eq + 1).trimmed();
    }
    splitDeclarator(p, &name, &type);
    static const QRegularExpression typePattern(QStringLiteral("^[A-Za-z_][\\w\\s*&:<>,]*$"));
    if (!type.isEmpty() && isPlainIdentifier(name) && !p.contains(QLatin1Char('(')) && typePattern.match(type).hasMatch())
        return std::nullopt; // "int i"
    return p;
}

// All statements of a for header part (separated by ',' or ';'). declarationsOnly: only
// "int i = 0" -> "i = 0" (for languages with C-like for headers).
QStringList convertCStatements(const QString &value, bool declarationsOnly = false)
{
    QStringList out;
    const QStringList statements = afce::splitList(value, QLatin1Char(';'));
    for (const QString &statement : statements) {
        const QStringList parts = afce::splitList(statement, QLatin1Char(','));
        for (const QString &part : parts) {
            if (const std::optional<QString> converted = convertCStatement(part, declarationsOnly))
                out << *converted;
        }
    }
    return out;
}

// Logical negation in C notation (mapped to the target language by "operators" afterwards):
// "!x" -> "x", "a < b" -> "a >= b", otherwise "!(expr)".
QString negateExpression(const QString &expression)
{
    const QString v = expression.trimmed();
    if (v.isEmpty())
        return v;
    auto stripParens = [](const QString &e) {
        QString t = e.trimmed();
        while (t.startsWith(QLatin1Char('(')) && isSimpleExpression(t) && t.endsWith(QLatin1Char(')')))
            t = t.mid(1, t.size() - 2).trimmed();
        return t;
    };
    // leading negation of a simple operand
    for (const QString &prefix : {QStringLiteral("!"), QStringLiteral("not "), QStringLiteral("не ")}) {
        if (v.startsWith(prefix, Qt::CaseInsensitive) && !v.startsWith(QLatin1String("!="))) {
            const QString rest = v.mid(prefix.size()).trimmed();
            if (!rest.isEmpty() && isSimpleExpression(rest))
                return stripParens(rest);
        }
    }
    // a single top-level comparison
    struct Op { int pos; int len; QString inverse; };
    QList<Op> comparisons;
    bool logical = false;
    int depth = 0;
    const int n = v.size();
    for (int i = 0; i < n; ++i) {
        const QChar c = v.at(i);
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            i = skipLiteral(v, i) - 1;
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{')) {
            ++depth;
            continue;
        }
        if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) {
            --depth;
            continue;
        }
        if (depth != 0)
            continue;
        const QString two = v.mid(i, 2);
        if (two == QLatin1String("&&") || two == QLatin1String("||") || c == QLatin1Char('?')) {
            logical = true;
            break;
        }
        static const QHash<QString, QString> inverse2 = {
            {QStringLiteral("=="), QStringLiteral("!=")}, {QStringLiteral("!="), QStringLiteral("==")},
            {QStringLiteral("<="), QStringLiteral(">")},  {QStringLiteral(">="), QStringLiteral("<")},
            {QStringLiteral("<>"), QStringLiteral("=")}};
        if (inverse2.contains(two)) {
            comparisons << Op{i, 2, inverse2.value(two)};
            ++i;
            continue;
        }
        if (two == QLatin1String("<<") || two == QLatin1String(">>") || two == QLatin1String("->")
            || two == QLatin1String("=>") || two == QLatin1String(":=")) {
            ++i;
            continue;
        }
        const QChar prev = i > 0 ? v.at(i - 1) : QChar();
        if (c == QLatin1Char('<'))
            comparisons << Op{i, 1, QStringLiteral(">=")};
        else if (c == QLatin1Char('>'))
            comparisons << Op{i, 1, QStringLiteral("<=")};
        else if (c == QLatin1Char('=') && !isComparisonChar(prev))
            comparisons << Op{i, 1, QStringLiteral("<>")};
        else if (c == QLatin1Char('!'))
            logical = true;
        else if (isIdentStart(c)) {
            int j = i;
            while (j < n && isIdentChar(v.at(j)))
                ++j;
            static const QStringList words = {QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("not"),
                                              QStringLiteral("xor"), QStringLiteral("и"), QStringLiteral("или"),
                                              QStringLiteral("не"), QStringLiteral("if"), QStringLiteral("else")};
            if (words.contains(v.mid(i, j - i), Qt::CaseInsensitive)) {
                logical = true;
                break;
            }
            i = j - 1;
        }
    }
    if (!logical && comparisons.size() == 1) {
        const Op &op = comparisons.first();
        return v.left(op.pos) + op.inverse + v.mid(op.pos + op.len);
    }
    if (isSimpleExpression(v))
        return QLatin1Char('!') + v;
    return QStringLiteral("!(") + v + QLatin1Char(')');
}

// Wraps the operands of top-level && / || that contain comparisons in parentheses, recursively
// ("a > 0 && b < 1" -> "(a > 0) && (b < 1)"), for languages whose and/or bind tighter than
// comparisons (Pascal).
QString parenthesizeLogical(const QString &expression, int nesting = 0)
{
    if (nesting > maxExpressionNesting)
        return expression;
    const QString s = expression;
    // 1. process the contents of every top-level bracket group
    QString rebuilt;
    int depth = 0;
    int groupStart = -1;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            const int j = skipLiteral(s, i);
            if (depth == 0)
                rebuilt += s.mid(i, j - i);
            i = j - 1;
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char('[')) {
            if (depth == 0) {
                groupStart = i;
                rebuilt += c;
            }
            ++depth;
            continue;
        }
        if ((c == QLatin1Char(')') || c == QLatin1Char(']')) && depth > 0) {
            if (--depth == 0) {
                rebuilt += parenthesizeLogical(s.mid(groupStart + 1, i - groupStart - 1), nesting + 1);
                rebuilt += c;
            }
            continue;
        }
        if (depth == 0)
            rebuilt += c;
    }
    if (depth != 0)
        return expression; // unbalanced: leave it alone

    // 2. split at top-level && / ||
    QStringList operands;
    QStringList operators;
    QString current;
    depth = 0;
    for (int i = 0; i < rebuilt.size(); ++i) {
        const QChar c = rebuilt.at(i);
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            const int j = skipLiteral(rebuilt, i);
            current += rebuilt.mid(i, j - i);
            i = j - 1;
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char('['))
            ++depth;
        else if ((c == QLatin1Char(')') || c == QLatin1Char(']')) && depth > 0)
            --depth;
        const QStringView rest = QStringView(rebuilt).mid(i);
        if (depth == 0 && (rest.startsWith(QLatin1String("&&")) || rest.startsWith(QLatin1String("||")))) {
            operands << current;
            operators << rest.left(2).toString();
            current.clear();
            ++i;
            continue;
        }
        current += c;
    }
    operands << current;
    if (operators.isEmpty())
        return rebuilt;

    auto hasComparison = [](const QString &operand) {
        bool found = false;
        scanTopLevel(operand, [&](int i, int d) {
            if (d != 0)
                return;
            const QChar c = operand.at(i);
            if (c == QLatin1Char('<') || c == QLatin1Char('>'))
                found = true;
            else if (c == QLatin1Char('=') && (i == 0 || operand.at(i - 1) != QLatin1Char(':')))
                found = true;
        });
        return found;
    };
    QString result;
    for (int k = 0; k < operands.size(); ++k) {
        const QString trimmed = operands.at(k).trimmed();
        QString operand = trimmed;
        if (hasComparison(trimmed) && !(trimmed.startsWith(QLatin1Char('(')) && isSimpleExpression(trimmed)))
            operand = QLatin1Char('(') + trimmed + QLatin1Char(')');
        if (k > 0)
            result += QLatin1Char(' ') + operators.at(k - 1) + QLatin1Char(' ');
        result += operand;
    }
    return result;
}

// Attribute value as the generator sees it:
//  * io/ou files older than AFCE 0.9.7 store their items in t1..t8 instead of "vars";
//  * every attribute except "text" (process, call) is whitespace-normalized, so a line break inside
//    a condition cannot break line-oriented languages (string literals are kept as written);
//  * "void" as the algorithm's parameter list or return type (C "f(void)", void functions) means
//    "none".
QString elementAttribute(const QDomElement &e, const QString &name)
{
    QString value = e.attribute(name);
    if (name == QLatin1String("vars") && (e.tagName() == QLatin1String("io") || e.tagName() == QLatin1String("ou"))) {
        QStringList items = afce::splitList(value);
        for (int i = 1; i <= 8; ++i) {
            const QString legacy = e.attribute(QStringLiteral("t%1").arg(i)).trimmed();
            if (!legacy.isEmpty())
                items << legacy;
        }
        if (!items.isEmpty())
            value = afce::joinList(items);
    }
    if (name != QLatin1String("text"))
        value = afce::simplifyCode(value);
    if (e.tagName() == QLatin1String("algorithm") && (name == QLatin1String("params") || name == QLatin1String("returns"))
        && value.trimmed() == QLatin1String("void"))
        value.clear();
    return value;
}

// "const std::string &" -> "std::string", "char *" -> "char*"
QString normalizedType(const QString &type)
{
    QString t = afce::simplifyCode(type);
    t.replace(QStringLiteral(" *"), QStringLiteral("*"));
    t.replace(QStringLiteral(" &"), QStringLiteral("&"));
    if (t.startsWith(QLatin1String("const ")))
        t = t.mid(6);
    while (t.endsWith(QLatin1Char('&')))
        t.chop(1);
    return t.trimmed();
}

// ---------------------------------------------------------------------------
struct Scope {
    QDomElement element;   // attributes visible as %attr%
    QString type;
    QJsonObject rule;
    QDomElement branch;    // case branch (value / body)
    QHash<QString, QString> extras;
    QDomElement loop;      // loop context
    int levels = 1;
    // list item
    bool inItem = false;
    QString item;
    int itemIndex = 0;
    ItemKind kind = ItemKind::Expression;
    QJsonObject listConfig;
    // declaration collection: raw values, no lists, no prefix
    bool rawMode = false;
};

class Renderer
{
public:
    explicit Renderer(const QJsonObject &rule);
    QString render(const QDomDocument &doc);

private:
    QJsonObject m_rule;
    QString m_unit;
    QStringList m_emptyBody;
    QString m_prefix;
    QSet<QString> m_keywords;
    bool m_caseInsensitive = false;
    QList<QPair<QString, QString>> m_operators; // longest first
    bool m_logicalParens = false;
    QStringList m_prefixSkip;
    QHash<QString, QString> m_types;
    QDomElement m_algorithm;
    QHash<QString, QString> m_variables; // "variables", "variables_int", ... -> comma separated names
    QHash<QString, VarKind> m_kinds;     // normalized variable name -> heuristic kind
    QHash<QString, BinaryTemplate> m_binary; // "operators" with an object value
    QHash<QString, QString> m_casts;
    QJsonObject m_literals;
    QString m_prefixSubscript;
    QList<QDomElement> m_stack;
    int m_depth = 0;

    QJsonObject ruleFor(const QString &type) const;
    bool isKeyword(const QString &word) const;
    QString normalizedName(const QString &name) const;
    std::optional<VarKind> lookupKind(const QString &identifier) const;
    std::optional<ExprInfo> analyze(const QString &expression, bool convert) const;
    QString applyPrefix(const QString &value) const;
    QString mapOperators(const QString &value) const;
    QString convertLiterals(const QString &value) const;
    QString transformExpression(const QString &value) const;
    QString truthy(const QString &value, int depth = 0) const;

    Scope makeScope(const QDomElement &e, const QJsonObject &rule) const;
    QString rawAttribute(const Scope &scope, const QString &name) const;
    QString attributeValue(const QDomElement &e, const QJsonObject &rule, const QString &name) const;
    QJsonObject listConfig(const QJsonObject &rule, const QString &placeholder, bool *found) const;
    QJsonValue listSetting(const Scope &scope, const QJsonObject &config, const QString &key) const;
    QString expandList(const Scope &scope, const QString &placeholder, const QJsonObject &config);
    bool conditionEmpty(const Scope &scope, const QString &condition) const;
    QString selectTemplate(const Scope &scope) const;

    QString resolveValue(const Token &token, const Scope &scope, bool *prefixed);
    QString expandInline(const QString &tpl, const Scope &scope);
    QStringList expand(const QString &tpl, const Scope &scope);
    QStringList blockLines(const QString &name, const Scope &scope);

    QStringList renderBlock(const QDomElement &e);
    QStringList renderBranch(const QDomElement &branch);
    bool isKnownBlock(const QString &type) const;
    QStringList renderComment(const QString &text);
    QStringList renderIfChain(const QDomElement &e, const QJsonObject &rule);
    QStringList renderCaseBranches(const Scope &scope);
    void collectVariables();
};

Renderer::Renderer(const QJsonObject &rule)
    : m_rule(rule)
{
    const QJsonObject settings = rule.value(QLatin1String("additional_settings")).toObject();
    m_unit = settings.contains(QLatin1String("indentation_preference"))
                 ? settings.value(QLatin1String("indentation_preference")).toString()
                 : QStringLiteral("  ");
    if (settings.contains(QLatin1String("empty_body")))
        m_emptyBody = templateText(settings.value(QLatin1String("empty_body"))).split(QLatin1Char('\n'));
    m_prefix = settings.value(QLatin1String("variable_prefix")).toString();
    m_caseInsensitive = settings.value(QLatin1String("case_insensitive")).toBool();
    const QJsonObject operators = settings.value(QLatin1String("operators")).toObject();
    for (auto it = operators.begin(); it != operators.end(); ++it) {
        if (!it.key().isEmpty() && it.value().isString())
            m_operators.append(qMakePair(it.key(), it.value().toString()));
    }
    std::sort(m_operators.begin(), m_operators.end(),
              [](const QPair<QString, QString> &a, const QPair<QString, QString> &b) { return a.first.size() > b.first.size(); });
    for (auto it = operators.begin(); it != operators.end(); ++it) {
        const QJsonObject binary = it.value().toObject();
        if (binary.contains(QLatin1String("binary")))
            m_binary.insert(it.key(), BinaryTemplate{binary.value(QLatin1String("binary")).toString(),
                                                     binary.value(QLatin1String("when")).toString() == QLatin1String("int")});
    }
    const QJsonObject casts = settings.value(QLatin1String("casts")).toObject();
    for (auto it = casts.begin(); it != casts.end(); ++it)
        m_casts.insert(it.key(), it.value().toString());
    m_literals = settings.value(QLatin1String("literals")).toObject();
    m_prefixSubscript = settings.value(QLatin1String("prefix_subscript")).toString();
    m_logicalParens = settings.value(QLatin1String("logical_parens")).toBool();
    const QJsonArray keywords = settings.value(QLatin1String("keywords")).toArray();
    for (const QJsonValue &k : keywords)
        m_keywords.insert(m_caseInsensitive ? k.toString().toLower() : k.toString());
    if (settings.contains(QLatin1String("variable_prefix_skip"))) {
        const QJsonArray skip = settings.value(QLatin1String("variable_prefix_skip")).toArray();
        for (const QJsonValue &s : skip)
            m_prefixSkip << s.toString();
    } else {
        m_prefixSkip << QStringLiteral("name") << QStringLiteral("returns");
    }
    const QJsonObject types = settings.value(QLatin1String("types")).toObject();
    for (auto it = types.begin(); it != types.end(); ++it)
        m_types.insert(normalizedType(it.key()), it.value().toString());
}

QJsonObject Renderer::ruleFor(const QString &type) const
{
    return m_rule.value(type).toObject();
}

bool Renderer::isKeyword(const QString &word) const
{
    return m_keywords.contains(m_caseInsensitive ? word.toLower() : word);
}

QString Renderer::applyPrefix(const QString &value) const
{
    if (m_prefix.isEmpty())
        return value;
    QString out;
    const int n = value.size();
    int i = 0;
    while (i < n) {
        const QChar c = value.at(i);
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            const int j = skipLiteral(value, i);
            out += value.mid(i, j - i);
            i = j;
            continue;
        }
        if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && value.at(i + 1).isDigit())) {
            int j = i;
            while (j < n && (isIdentChar(value.at(j)) || value.at(j) == QLatin1Char('.')))
                ++j;
            out += value.mid(i, j - i);
            i = j;
            continue;
        }
        if (c == QLatin1Char('(')) {
            // a C cast "(double) x" names a type, not a variable
            const int close = groupEnd(value, i);
            if (close > 0 && !castCategory(value.mid(i + 1, close - i - 2)).isEmpty()) {
                int k = close;
                while (k < n && value.at(k).isSpace())
                    ++k;
                if (startsOperand(value, k, true)) {
                    out += value.mid(i, close - i);
                    i = close;
                    continue;
                }
            }
        }
        if (isIdentStart(c)) {
            int j = i;
            while (j < n && isIdentChar(value.at(j)))
                ++j;
            const QString word = value.mid(i, j - i);
            bool skip = isKeyword(word);
            const QChar prev = i > 0 ? value.at(i - 1) : QChar();
            if (prev == QLatin1Char('$') || prev == QLatin1Char('@') || (i > 0 && isIdentChar(prev)))
                skip = true;
            if (i >= 2 && (value.mid(i - 2, 2) == QLatin1String("->") || value.mid(i - 2, 2) == QLatin1String("::")))
                skip = true;
            int k = j;
            while (k < n && value.at(k) == QLatin1Char(' '))
                ++k;
            if (k < n && value.at(k) == QLatin1Char('('))
                skip = true;
            // a word followed by another operand is a named operator or a call without
            // parentheses ("print $x", "push @a, 1", "echo \"x\""): never a variable in C notation
            if (k > j && k < n) {
                const QChar next = value.at(k);
                if (next == QLatin1Char('"') || next == QLatin1Char('\'')
                    || ((next == QLatin1Char('$') || next == QLatin1Char('@')) && k + 1 < n && isIdentStart(value.at(k + 1))))
                    skip = true;
                if (isIdentStart(next)) {
                    int e = k;
                    while (e < n && isIdentChar(value.at(e)))
                        ++e;
                    if (!isKeyword(value.mid(k, e - k)))
                        skip = true;
                }
            }
            if (value.mid(j, 2) == QLatin1String("::") || value.mid(k, 2) == QLatin1String("=>"))
                skip = true;
            if (!skip)
                out += m_prefix;
            out += word;
            if (!skip && j < n && value.at(j) == QLatin1Char('['))
                out += m_prefixSubscript; // Perl array references: $a->[i]
            i = j;
            continue;
        }
        out += c;
        ++i;
    }
    return out;
}

QString Renderer::normalizedName(const QString &name) const
{
    return m_caseInsensitive ? name.toLower() : name;
}

std::optional<VarKind> Renderer::lookupKind(const QString &identifier) const
{
    const auto it = m_kinds.constFind(normalizedName(identifier));
    if (it == m_kinds.constEnd())
        return std::nullopt;
    return *it;
}

std::optional<ExprInfo> Renderer::analyze(const QString &expression, bool convert) const
{
    const ExprAnalyzer analyzer([this](const QString &identifier) { return lookupKind(identifier); },
                                convert ? &m_binary : nullptr, convert ? &m_casts : nullptr);
    return analyzer.run(expression);
}

// String and char literals in the target language's notation ("literals": {"string": {"item",
// "text_escape"}, "char": {...}}; a missing "char" falls back to "string").
QString Renderer::convertLiterals(const QString &value) const
{
    if (m_literals.isEmpty())
        return value;
    QString out;
    const int n = value.size();
    int i = 0;
    while (i < n) {
        const QChar c = value.at(i);
        if (c != QLatin1Char('"') && c != QLatin1Char('\'')) {
            out += c;
            ++i;
            continue;
        }
        const int j = skipLiteral(value, i);
        const QString literal = value.mid(i, j - i);
        i = j;
        const ItemKind kind = itemKind(literal);
        QJsonObject config = m_literals.value(kind == ItemKind::Char ? QLatin1String("char") : QLatin1String("string")).toObject();
        if (config.isEmpty())
            config = m_literals.value(QLatin1String("string")).toObject();
        if (kind == ItemKind::Expression || !config.contains(QLatin1String("item"))) {
            out += literal; // unterminated
            continue;
        }
        const QString text = applyEscapes(literalContents(literal), config.value(QLatin1String("text_escape")).toObject());
        const QString tpl = templateText(config.value(QLatin1String("item")));
        for (int k = 0; k < tpl.size(); ++k) {
            const QStringView rest = QStringView(tpl).mid(k);
            if (rest.startsWith(QLatin1String("%$text%"))) {
                out += text;
                k += 6;
            } else if (rest.startsWith(QLatin1String("%$%"))) {
                out += literal;
                k += 2;
            } else if (rest.startsWith(QLatin1String("%%"))) {
                out += QLatin1Char('%');
                ++k;
            } else {
                out += tpl.at(k);
            }
        }
    }
    return out;
}

// Expressions are written in C notation: casts and multiplicative operators with templates are
// converted first, then "operators" and the variable prefix are applied, literals come last.
QString Renderer::transformExpression(const QString &value) const
{
    QString v = value;
    if (!m_binary.isEmpty() || !m_casts.isEmpty()) {
        if (const std::optional<ExprInfo> info = analyze(v, true))
            v = info->text;
    }
    return convertLiterals(applyPrefix(mapOperators(v)));
}

// C truthiness made explicit for languages where 0 is true (Ruby) or not a Boolean (Pascal):
// numeric operands of a condition get "!= 0", "!n" becomes "n == 0" (C notation, mapped later).
QString Renderer::truthy(const QString &value, int depth) const
{
    const QString v = value.trimmed();
    if (v.isEmpty() || depth > maxExpressionNesting)
        return value;
    for (const QString &op : {QStringLiteral("||"), QStringLiteral("&&")}) {
        QStringList parts;
        int start = 0;
        scanTopLevel(v, [&](int i, int d) {
            if (d == 0 && QStringView(v).mid(i).startsWith(op) && i >= start) {
                parts << v.mid(start, i - start);
                start = i + 2;
            }
        });
        if (!parts.isEmpty()) {
            parts << v.mid(start);
            for (QString &p : parts)
                p = truthy(p, depth + 1).trimmed();
            return parts.join(QLatin1Char(' ') + op + QLatin1Char(' '));
        }
    }
    auto fullyParenthesized = [](const QString &e) {
        return e.startsWith(QLatin1Char('(')) && groupEnd(e, 0) == e.size();
    };
    auto numeric = [&](const QString &e) -> bool {
        const std::optional<ExprInfo> info = analyze(e, false);
        return info && info->kind && (*info->kind == VarKind::Int || *info->kind == VarKind::Real);
    };
    auto isNumberLiteral = [](const QString &e) { return isNumber(e) && !e.startsWith(QLatin1Char('-')); };
    auto isZero = [](const QString &e) {
        bool ok = false;
        const qlonglong integer = e.toLongLong(&ok, 0);
        if (ok)
            return integer == 0;
        const double real = e.toDouble(&ok);
        return ok && real == 0;
    };
    if (v.startsWith(QLatin1Char('!')) && !v.startsWith(QLatin1String("!="))) {
        const QString rest = v.mid(1).trimmed();
        if (!isSimpleExpression(rest))
            return v;
        if (fullyParenthesized(rest))
            return QStringLiteral("!(") + truthy(rest.mid(1, rest.size() - 2), depth + 1).trimmed() + QLatin1Char(')');
        if (isNumberLiteral(rest))
            return isZero(rest) ? QStringLiteral("true") : QStringLiteral("false");
        if (numeric(rest))
            return rest + QStringLiteral(" == 0");
        return v;
    }
    if (fullyParenthesized(v))
        return QLatin1Char('(') + truthy(v.mid(1, v.size() - 2), depth + 1).trimmed() + QLatin1Char(')');
    if (isNumberLiteral(v))
        return isZero(v) ? QStringLiteral("false") : QStringLiteral("true");
    if (numeric(v))
        return (isSimpleExpression(v) || isArithmetic(v) ? v : QLatin1Char('(') + v + QLatin1Char(')')) + QStringLiteral(" != 0");
    return v;
}

QString Renderer::mapOperators(const QString &value) const
{
    if (m_operators.isEmpty())
        return value;
    const QString source = m_logicalParens ? parenthesizeLogical(value) : value;
    QString out;
    const int n = source.size();
    int i = 0;
    auto appendWord = [&](const QString &replacement, int next) {
        // keep words apart from neighbouring identifiers ("!x" -> "not x", "a%b" -> "a mod b")
        const bool wordStart = !replacement.isEmpty() && isIdentChar(replacement.at(0));
        const bool wordEnd = !replacement.isEmpty() && isIdentChar(replacement.at(replacement.size() - 1));
        if (wordStart && !out.isEmpty() && (isIdentChar(out.at(out.size() - 1)) || out.endsWith(QLatin1Char(')'))))
            out += QLatin1Char(' ');
        out += replacement;
        if (wordEnd && next < n && (isIdentChar(source.at(next)) || source.at(next) == QLatin1Char('(')))
            out += QLatin1Char(' ');
    };
    while (i < n) {
        const QChar c = source.at(i);
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            const int j = skipLiteral(source, i);
            out += source.mid(i, j - i);
            i = j;
            continue;
        }
        if (isIdentStart(c) || c.isDigit()) {
            int j = i;
            while (j < n && isIdentChar(source.at(j)))
                ++j;
            const QString word = source.mid(i, j - i);
            bool replaced = false;
            if (isIdentStart(c)) {
                for (const auto &op : m_operators) {
                    if (op.first == word) {
                        appendWord(op.second, j);
                        replaced = true;
                        break;
                    }
                }
            }
            if (!replaced)
                out += word;
            i = j;
            continue;
        }
        bool replaced = false;
        for (const auto &op : m_operators) {
            if (isIdentStart(op.first.at(0)))
                continue;
            if (QStringView(source).mid(i).startsWith(op.first)) {
                appendWord(op.second, i + op.first.size());
                i += op.first.size();
                replaced = true;
                break;
            }
        }
        if (!replaced)
            out += source.at(i++);
    }
    return out;
}

Scope Renderer::makeScope(const QDomElement &e, const QJsonObject &rule) const
{
    Scope scope;
    scope.element = e;
    scope.type = e.tagName();
    scope.rule = rule;
    QStringList contextTypes;
    if (rule.contains(QLatin1String("context"))) {
        const QJsonArray array = rule.value(QLatin1String("context")).toArray();
        for (const QJsonValue &v : array)
            contextTypes << v.toString();
    } else {
        contextTypes = loopTypes();
        if (scope.type != QLatin1String("continue"))
            contextTypes << QStringLiteral("case");
    }
    int skippedCases = 0;
    for (int i = m_stack.size() - 1; i >= 0; --i) {
        const QString t = m_stack.at(i).tagName();
        if (contextTypes.contains(t)) {
            scope.loop = m_stack.at(i);
            break;
        }
        if (t == QLatin1String("case"))
            ++skippedCases;
    }
    scope.levels = skippedCases + 1;
    return scope;
}

QString Renderer::rawAttribute(const Scope &scope, const QString &name) const
{
    if (!scope.branch.isNull() && scope.branch.hasAttribute(name))
        return elementAttribute(scope.branch, name);
    if (scope.element == m_algorithm && m_variables.contains(name) && !scope.element.hasAttribute(name))
        return m_variables.value(name);
    return elementAttribute(scope.element, name);
}

QString Renderer::attributeValue(const QDomElement &e, const QJsonObject &rule, const QString &name) const
{
    QString value;
    if (e == m_algorithm && m_variables.contains(name) && !e.hasAttribute(name))
        value = m_variables.value(name);
    else
        value = elementAttribute(e, name);
    value = value.trimmed();
    if (value.isEmpty())
        value = rule.value(QLatin1String("defaults")).toObject().value(name).toString();
    value.remove(QLatin1Char('\r'));
    return value;
}

QJsonObject Renderer::listConfig(const QJsonObject &rule, const QString &placeholder, bool *found) const
{
    const QJsonValue list = rule.value(QLatin1String("list"));
    *found = false;
    if (list.isArray()) {
        if (list.toArray().contains(QJsonValue(placeholder)))
            *found = true;
        return QJsonObject();
    }
    if (list.isObject() && list.toObject().contains(placeholder)) {
        *found = true;
        return list.toObject().value(placeholder).toObject();
    }
    return QJsonObject();
}

QJsonValue Renderer::listSetting(const Scope &scope, const QJsonObject &config, const QString &key) const
{
    if (config.contains(key))
        return config.value(key);
    return scope.rule.value(key);
}

QString Renderer::expandList(const Scope &scope, const QString &placeholder, const QJsonObject &config)
{
    const QString separator = listSetting(scope, config, QStringLiteral("separator")).toString(QStringLiteral(","));
    // "source" may name several attributes ("a,b"): their lists are concatenated
    QStringList raws;
    const QStringList sources = config.value(QLatin1String("source")).toString(placeholder).split(QLatin1Char(','));
    for (const QString &s : sources) {
        const QString source = s.trimmed();
        QString value = rawAttribute(scope, source).trimmed();
        if (value.isEmpty())
            value = scope.rule.value(QLatin1String("defaults")).toObject().value(source).toString();
        if (!value.trimmed().isEmpty())
            raws << value;
    }
    QString raw = raws.join(separator.isEmpty() ? QStringLiteral(",") : separator);
    raw.remove(QLatin1Char('\r'));
    QStringList items;
    if (separator.size() == 1) {
        items = afce::splitList(raw, separator.at(0));
    } else {
        const QStringList parts = raw.split(separator.isEmpty() ? QStringLiteral(",") : separator);
        for (const QString &p : parts) {
            if (!p.trimmed().isEmpty())
                items << p.trimmed();
        }
    }

    auto has = [&](const QString &key) {
        return config.contains(key) || scope.rule.contains(key);
    };
    auto str = [&](const QString &key) { return templateText(listSetting(scope, config, key)); };
    auto build = [&](const QString &itemKey, const QString &prefixKey, const QString &suffixKey) -> std::optional<QString> {
        if (has(itemKey))
            return str(itemKey);
        if (has(prefixKey) || has(suffixKey))
            return str(prefixKey) + QStringLiteral("%$%") + str(suffixKey);
        return std::nullopt;
    };

    // expression items with a heuristic kind other than int ("item_real": "%g")
    static const QList<QPair<VarKind, QString>> typedKeys = {
        {VarKind::Real, QStringLiteral("item_real")}, {VarKind::Bool, QStringLiteral("item_bool")},
        {VarKind::String, QStringLiteral("item_string")}, {VarKind::Char, QStringLiteral("item_char")}};
    bool typed = false;
    for (const auto &key : typedKeys)
        typed = typed || has(key.second);
    const int limit = listSetting(scope, config, QStringLiteral("limit")).toInt(0);

    QStringList parts;
    for (int i = 0; i < items.size(); ++i) {
        if (limit > 0 && parts.size() >= limit)
            break;
        const ItemKind kind = itemKind(items.at(i));
        std::optional<QString> tpl;
        if (kind == ItemKind::Char)
            tpl = build(QStringLiteral("char_item"), QStringLiteral("char_prefix"), QStringLiteral("char_suffix"));
        if (!tpl && kind != ItemKind::Expression)
            tpl = build(QStringLiteral("literal_item"), QStringLiteral("literal_prefix"), QStringLiteral("literal_suffix"));
        if (!tpl && typed) {
            const std::optional<ExprInfo> info = analyze(items.at(i), false);
            for (const auto &key : typedKeys) {
                if (info && info->kind == key.first && has(key.second))
                    tpl = str(key.second);
            }
        }
        if (!tpl)
            tpl = build(QStringLiteral("item"), QStringLiteral("prefix"), QStringLiteral("suffix"));
        if (!tpl)
            tpl = QStringLiteral("%$%");
        Scope itemScope = scope;
        itemScope.inItem = true;
        itemScope.item = items.at(i);
        itemScope.itemIndex = i + 1;
        itemScope.kind = kind;
        itemScope.listConfig = config;
        const QString rendered = expandInline(*tpl, itemScope);
        if (!rendered.isEmpty())
            parts << rendered;
    }
    if (parts.isEmpty())
        return QString();
    return str(QStringLiteral("head")) + parts.join(str(QStringLiteral("glue"))) + str(QStringLiteral("tail"));
}

bool Renderer::conditionEmpty(const Scope &scope, const QString &condition) const
{
    if (isBlockPlaceholder(condition) && condition.startsWith(QLatin1String("branch"))
        && condition != QLatin1String("branches")) {
        const int index = condition.mid(6).toInt();
        const QList<QDomElement> branches = childElements(scope.element, QStringLiteral("branch"));
        if (index < 1 || index > branches.size())
            return true;
        return childElements(branches.at(index - 1)).isEmpty();
    }
    if (condition == QLatin1String("cases")) // a case without regular (non-default) branches
        return childElements(scope.element, QStringLiteral("branch")).size() <= 1;
    QString value;
    if (condition.startsWith(QLatin1String("loop."))) {
        if (scope.loop.isNull())
            return true;
        value = elementAttribute(scope.loop, condition.mid(5));
    } else if (condition.startsWith(QLatin1String("algorithm."))) {
        const QString attr = condition.mid(10);
        value = m_variables.contains(attr) && !m_algorithm.hasAttribute(attr) ? m_variables.value(attr)
                                                                                : elementAttribute(m_algorithm, attr);
    } else {
        value = rawAttribute(scope, condition);
    }
    return value.trimmed().isEmpty() || afce::splitList(value).isEmpty();
}

QString Renderer::selectTemplate(const Scope &scope) const
{
    const QJsonObject &rule = scope.rule;
    if (!scope.loop.isNull()) {
        if (scope.levels > 1 && rule.contains(QLatin1String("template_through_case")))
            return templateText(rule.value(QLatin1String("template_through_case")));
        const QString key = QStringLiteral("template_in_") + scope.loop.tagName();
        // the last block of a branch that belongs directly to the context element
        const QDomNode parent = scope.element.parentNode();
        if (rule.contains(key + QStringLiteral("_last")) && scope.element.nextSiblingElement().isNull()
            && parent.nodeName() == QLatin1String("branch") && parent.parentNode() == scope.loop)
            return templateText(rule.value(key + QStringLiteral("_last")));
        if (rule.contains(key))
            return templateText(rule.value(key));
    }
    QString best;
    int bestCount = 0;
    const QStringList keys = rule.keys();
    for (const QString &key : keys) {
        QStringList conditions;
        if (key.startsWith(QLatin1String("template_empty_")))
            conditions = key.mid(15).split(QLatin1Char('+'), Qt::SkipEmptyParts);
        else
            continue;
        if (conditions.isEmpty())
            continue;
        bool all = true;
        for (const QString &c : std::as_const(conditions)) {
            if (!conditionEmpty(scope, c)) {
                all = false;
                break;
            }
        }
        if (all && conditions.size() > bestCount) {
            best = key;
            bestCount = conditions.size();
        }
    }
    if (!best.isEmpty())
        return templateText(rule.value(best));
    if (rule.contains(QLatin1String("template_shortened")) && conditionEmpty(scope, QStringLiteral("branch2")))
        return templateText(rule.value(QLatin1String("template_shortened")));
    for (const QString &key : keys) {
        if (!key.startsWith(QLatin1String("template_single_")))
            continue;
        const QString attr = key.mid(16);
        if (afce::splitList(rawAttribute(scope, attr)).size() == 1)
            return templateText(rule.value(key));
    }
    return templateText(rule.value(QLatin1String("template")));
}

QString Renderer::resolveValue(const Token &token, const Scope &scope, bool *prefixable)
{
    const QString &name = token.name;
    QString value;
    *prefixable = false;
    if (scope.inItem && name.startsWith(QLatin1Char('$'))) {
        const QString what = name.mid(1);
        if (what.isEmpty()) {
            value = scope.item;
            *prefixable = scope.kind == ItemKind::Expression;
        } else if (what == QLatin1String("name")) {
            splitDeclarator(scope.item, &value, nullptr);
            *prefixable = true;
        } else if (what == QLatin1String("type")) {
            splitDeclarator(scope.item, nullptr, &value);
        } else if (what == QLatin1String("text")) {
            value = scope.kind == ItemKind::Expression
                        ? scope.item
                        : applyEscapes(literalContents(scope.item),
                                       listSetting(scope, scope.listConfig, QStringLiteral("text_escape")).toObject());
        } else if (what == QLatin1String("index")) {
            value = QString::number(scope.itemIndex);
        }
    } else if (scope.extras.contains(name)) {
        value = scope.extras.value(name);
    } else if (name == QLatin1String("levels")) {
        value = QString::number(scope.levels);
    } else if (name.startsWith(QLatin1String("loop."))) {
        const QString attr = name.mid(5);
        if (!scope.loop.isNull())
            value = attributeValue(scope.loop, ruleFor(scope.loop.tagName()), attr);
        *prefixable = !m_prefixSkip.contains(attr);
    } else if (name.startsWith(QLatin1String("algorithm."))) {
        const QString attr = name.mid(10);
        value = attributeValue(m_algorithm, ruleFor(QStringLiteral("algorithm")), attr);
        *prefixable = !m_prefixSkip.contains(attr);
    } else {
        bool isList = false;
        const QJsonObject config = scope.rawMode ? QJsonObject() : listConfig(scope.rule, name, &isList);
        if (isList && !scope.rawMode) {
            value = expandList(scope, name, config);
        } else {
            value = rawAttribute(scope, name).trimmed();
            if (value.isEmpty())
                value = scope.rule.value(QLatin1String("defaults")).toObject().value(name).toString();
            value.remove(QLatin1Char('\r'));
            *prefixable = !m_prefixSkip.contains(name);
        }
    }

    for (const QString &filter : token.filters) {
        const int colon = filter.indexOf(QLatin1Char(':'));
        const QString f = colon >= 0 ? filter.left(colon) : filter;
        const QString arg = colon >= 0 ? filter.mid(colon + 1) : QString();
        if (f == QLatin1String("raw")) {
            *prefixable = false;
        } else if (f == QLatin1String("inc") || f == QLatin1String("dec")) {
            if (!value.trimmed().isEmpty())
                value = addOne(value, f == QLatin1String("inc"));
        } else if (f == QLatin1String("paren")) {
            if (!isSimpleExpression(value))
                value = QLatin1Char('(') + value.trimmed() + QLatin1Char(')');
        } else if (f == QLatin1String("negate")) {
            if (!value.trimmed().isEmpty())
                value = negateExpression(value);
        } else if (f == QLatin1String("operand")) {
            if (!isSimpleExpression(value) && !isArithmetic(value))
                value = QLatin1Char('(') + value.trimmed() + QLatin1Char(')');
        } else if (f == QLatin1String("nosemi")) {
            while (!value.isEmpty() && (value.endsWith(QLatin1Char(';')) || value.at(value.size() - 1).isSpace()))
                value.chop(1);
        } else if (f == QLatin1String("name")) {
            splitDeclarator(value, &value, nullptr);
        } else if (f == QLatin1String("type")) {
            splitDeclarator(value, nullptr, &value);
            *prefixable = false;
        } else if (f == QLatin1String("cstmt")) {
            // C for-header statements -> plain assignments, one per line; "colon": "x := e";
            // "pascal": "x := e" joined by ";\n"; "decl": only declarations are rewritten, ", " joined
            QStringList statements = convertCStatements(value, arg == QLatin1String("decl"));
            if (arg == QLatin1String("colon") || arg == QLatin1String("pascal")) {
                for (QString &statement : statements) {
                    const int eq = topLevelAssignment(statement);
                    if (eq >= 0)
                        statement = statement.left(eq).trimmed() + QStringLiteral(" := ") + statement.mid(eq + 1).trimmed();
                }
            }
            value = statements.join(arg == QLatin1String("pascal") ? QStringLiteral(";\n")
                                    : arg == QLatin1String("decl") ? QStringLiteral(", ")
                                                                   : QStringLiteral("\n"));
        } else if (f == QLatin1String("callparens")) {
            if (isPlainIdentifier(value.trimmed()))
                value = value.trimmed() + QStringLiteral("()");
        } else if (f == QLatin1String("colonassign")) {
            // "x = e" -> "x := e" for every statement of the value (Pascal, e87)
            QStringList statements = afce::splitList(value, QLatin1Char(';'));
            for (QString &statement : statements) {
                const int eq = topLevelAssignment(statement);
                if (eq >= 0)
                    statement = statement.left(eq).trimmed() + QStringLiteral(" := ") + statement.mid(eq + 1).trimmed();
            }
            value = statements.join(QStringLiteral("; "));
        } else if (f == QLatin1String("lhs")) {
            const int eq = topLevelAssignment(value);
            if (eq >= 0)
                value = value.left(eq).trimmed();
        } else if (f == QLatin1String("deftype")) {
            if (isPlainIdentifier(value.trimmed()) && !isKeyword(value.trimmed()))
                value = arg + QLatin1Char(' ') + value.trimmed();
        } else if (f == QLatin1String("maptype")) {
            const QString key = normalizedType(value);
            if (m_types.contains(key))
                value = m_types.value(key);
            *prefixable = false;
        } else if (f == QLatin1String("truthy")) {
            value = truthy(value);
        } else if (f == QLatin1String("lvalue")) {
            // "long k" -> "k"; "a[i]" and "k" stay
            QList<Declarator> declarators;
            if (parseDeclaration(value, &declarators) && declarators.size() == 1)
                value = declarators.first().name;
        } else if (f == QLatin1String("semicolon")) {
            value = terminateStatements(value, arg.isEmpty() ? QStringLiteral(";") : arg);
        } else if (f == QLatin1String("array")) {
            const QString v = value.trimmed();
            if (isPlainIdentifier(v))
                value = (arg == QLatin1String("ref") ? QStringLiteral("@$") : QStringLiteral("@")) + v;
            else if (v.startsWith(QLatin1Char('[')) && v.endsWith(QLatin1Char(']')) && isSimpleExpression(QLatin1Char('(') + v.mid(1, v.size() - 2) + QLatin1Char(')')))
                value = QLatin1Char('(') + v.mid(1, v.size() - 2) + QLatin1Char(')');
        }
    }
    if (scope.rawMode)
        *prefixable = false;
    return value;
}

QString Renderer::expandInline(const QString &tpl, const Scope &scope)
{
    QString result;
    const QList<Token> tokens = tokenize(tpl);
    for (const Token &t : tokens) {
        if (!t.placeholder) {
            QString text = t.text;
            result += text.replace(QLatin1Char('\t'), m_unit);
            continue;
        }
        if (isBlockPlaceholder(t.name))
            continue;
        bool prefixable = false;
        const QString value = resolveValue(t, scope, &prefixable);
        result += prefixable ? transformExpression(value) : value;
    }
    return result;
}

QStringList Renderer::blockLines(const QString &name, const Scope &scope)
{
    if (name == QLatin1String("branches"))
        return renderCaseBranches(scope);
    if (name == QLatin1String("body"))
        return renderBranch(scope.branch);
    const int index = name.mid(6).toInt();
    const QList<QDomElement> branches = childElements(scope.element, QStringLiteral("branch"));
    if (index >= 1 && index <= branches.size())
        return renderBranch(branches.at(index - 1));
    return renderBranch(QDomElement());
}

QStringList Renderer::expand(const QString &tpl, const Scope &scope)
{
    QStringList out;
    if (tpl.isEmpty())
        return out;
    auto indentLine = [&](const QString &line, int level) {
        return line.trimmed().isEmpty() ? QString() : m_unit.repeated(level) + line;
    };

    const QStringList templateLines = tpl.split(QLatin1Char('\n'));
    for (const QString &templateLine : templateLines) {
        int level = 0;
        while (level < templateLine.size() && templateLine.at(level) == QLatin1Char('\t'))
            ++level;
        const QList<Token> tokens = tokenize(templateLine.mid(level));

        // a block placeholder alone on its line?
        int blocks = 0;
        bool otherContent = false;
        for (const Token &t : tokens) {
            if (t.placeholder && isBlockPlaceholder(t.name))
                ++blocks;
            else if (t.placeholder || !t.text.trimmed().isEmpty())
                otherContent = true;
        }
        if (blocks == 1 && !otherContent) {
            for (const Token &t : tokens) {
                if (t.placeholder) {
                    const QStringList lines = blockLines(t.name, scope);
                    for (const QString &l : lines)
                        out << indentLine(l, level);
                }
            }
            continue;
        }

        // Lines produced by this template line. A line whose value placeholders all expanded to
        // nothing and whose remaining text is only blanks and ';' is dropped, as is a line that
        // contained values and produced only blank lines.
        QStringList buffer;
        QString current;
        bool hadValue = false;
        bool allValuesEmpty = true;
        bool punctuationOnly = true;
        bool afterBlock = false;
        for (const Token &t : tokens) {
            if (!t.placeholder) {
                QString text = t.text;
                current += text.replace(QLatin1Char('\t'), m_unit);
                for (const QChar c : std::as_const(t.text)) {
                    if (!c.isSpace() && c != QLatin1Char(';'))
                        punctuationOnly = false;
                }
                continue;
            }
            if (isBlockPlaceholder(t.name)) {
                if (!current.trimmed().isEmpty())
                    buffer << indentLine(current, level);
                const QStringList lines = blockLines(t.name, scope);
                for (const QString &l : lines)
                    buffer << indentLine(l, level + 1);
                current.clear();
                afterBlock = true;
                punctuationOnly = false;
                continue;
            }
            bool prefixable = false;
            QString value = resolveValue(t, scope, &prefixable);
            if (prefixable)
                value = transformExpression(value);
            hadValue = true;
            if (!value.isEmpty())
                allValuesEmpty = false;
            const QStringList valueLines = value.split(QLatin1Char('\n'));
            current += valueLines.first();
            for (int k = 1; k < valueLines.size(); ++k) {
                buffer << indentLine(current, level);
                const QString next = valueLines.at(k);
                int tabs = 0;
                while (tabs < next.size() && next.at(tabs) == QLatin1Char('\t'))
                    ++tabs;
                current = m_unit.repeated(tabs) + next.mid(tabs);
            }
        }
        if (!(afterBlock && current.trimmed().isEmpty()))
            buffer << indentLine(current, level);
        if (hadValue && allValuesEmpty && punctuationOnly)
            continue;
        if (hadValue) {
            bool blank = true;
            for (const QString &l : std::as_const(buffer)) {
                if (!l.isEmpty()) {
                    blank = false;
                    break;
                }
            }
            if (blank)
                continue;
        }
        out << buffer;
    }
    return out;
}

QStringList Renderer::renderBranch(const QDomElement &branch)
{
    QStringList lines;
    bool hasCode = false; // comments for unknown (or too deeply nested) blocks do not count as code
    if (!branch.isNull()) {
        const QList<QDomElement> blocks = childElements(branch);
        for (const QDomElement &b : blocks) {
            const bool known = isKnownBlock(b.tagName()) && m_depth < maxBlockNesting;
            const QStringList blockLines = renderBlock(b);
            if (!hasCode && known) {
                for (const QString &l : blockLines) {
                    if (!l.trimmed().isEmpty()) {
                        hasCode = true;
                        break;
                    }
                }
            }
            lines << blockLines;
        }
    }
    if (hasCode)
        return lines;
    // nothing but blank lines and comments: add the empty body ("pass"), keep the comments
    QStringList result;
    for (const QString &l : std::as_const(lines)) {
        if (!l.trimmed().isEmpty())
            result << l;
    }
    return result << m_emptyBody;
}

bool Renderer::isKnownBlock(const QString &type) const
{
    return type != QLatin1String("name") && type != QLatin1String("additional_settings")
           && type != QLatin1String("branch") && m_rule.value(type).isObject();
}

QStringList Renderer::renderComment(const QString &text)
{
    const QJsonValue comment = m_rule.value(QLatin1String("comment"));
    const QString tpl = comment.isObject() ? templateText(comment.toObject().value(QLatin1String("template")))
                                           : templateText(comment);
    if (tpl.isEmpty())
        return QStringList();
    Scope scope;
    scope.extras.insert(QStringLiteral("text"), text);
    return expand(tpl, scope);
}

QStringList Renderer::renderBlock(const QDomElement &e)
{
    const QString type = e.tagName();
    if (!isKnownBlock(type))
        return renderComment(QCoreApplication::translate("SourceCodeGenerator", "unsupported block \"%1\"").arg(type));
    if (m_depth >= maxBlockNesting)
        return renderComment(QCoreApplication::translate("SourceCodeGenerator", "nesting too deep"));
    struct DepthGuard {
        int &depth;
        explicit DepthGuard(int &d) : depth(d) { ++depth; }
        ~DepthGuard() { --depth; }
    } guard(m_depth);
    const QJsonObject rule = m_rule.value(type).toObject();

    if (type == QLatin1String("if") && rule.value(QLatin1String("chain")).isObject()) {
        const QList<QDomElement> branches = childElements(e, QStringLiteral("branch"));
        if (branches.size() >= 2) {
            const QList<QDomElement> elseBlocks = childElements(branches.at(1));
            if (elseBlocks.size() == 1 && elseBlocks.first().tagName() == QLatin1String("if"))
                return renderIfChain(e, rule);
        }
    }

    const Scope scope = makeScope(e, rule);
    const QString tpl = selectTemplate(scope);
    m_stack.append(e);
    const QStringList lines = expand(tpl, scope);
    m_stack.removeLast();
    return lines;
}

QStringList Renderer::renderIfChain(const QDomElement &e, const QJsonObject &rule)
{
    const QJsonObject chain = rule.value(QLatin1String("chain")).toObject();
    QStringList lines;
    const int depth = m_stack.size();

    Scope scope = makeScope(e, rule);
    m_stack.append(e);
    lines << expand(templateText(chain.value(QLatin1String("head"))), scope);

    QDomElement current = childElements(childElements(e, QStringLiteral("branch")).at(1)).first();
    while (!current.isNull()) {
        Scope inner = makeScope(current, rule);
        m_stack.append(current);
        lines << expand(templateText(chain.value(QLatin1String("else_if"))), inner);
        const QList<QDomElement> branches = childElements(current, QStringLiteral("branch"));
        const QList<QDomElement> elseBlocks = branches.size() >= 2 ? childElements(branches.at(1)) : QList<QDomElement>();
        if (elseBlocks.size() == 1 && elseBlocks.first().tagName() == QLatin1String("if")) {
            current = elseBlocks.first();
            continue;
        }
        if (!elseBlocks.isEmpty())
            lines << expand(templateText(chain.value(QLatin1String("else"))), inner);
        break;
    }
    while (m_stack.size() > depth + 1)
        m_stack.removeLast();
    lines << expand(templateText(chain.value(QLatin1String("end"))), scope);
    m_stack.removeLast();
    return lines;
}

QStringList Renderer::renderCaseBranches(const Scope &scope)
{
    QStringList lines;
    const QList<QDomElement> branches = childElements(scope.element, QStringLiteral("branch"));
    if (branches.isEmpty())
        return lines;
    const QJsonObject &rule = scope.rule;
    auto pick = [&](const QString &key, const QDomElement &branch) -> QString {
        const QString terminated = key + QStringLiteral("_terminated");
        if (rule.contains(terminated) && branchTerminates(branch))
            return templateText(rule.value(terminated));
        return templateText(rule.value(key));
    };
    auto renderOne = [&](const QString &tpl, const QDomElement &branch) {
        Scope branchScope = scope;
        branchScope.branch = branch;
        bool found = false;
        const QJsonObject config = listConfig(rule, QStringLiteral("value"), &found);
        branchScope.extras.insert(QStringLiteral("value"), expandList(branchScope, QStringLiteral("value"), config));
        lines << expand(tpl, branchScope);
    };

    const int regular = branches.size() - 1;
    for (int i = 0; i < regular; ++i) {
        const QString key = i == 0 && rule.contains(QLatin1String("first_case_branch"))
                                ? QStringLiteral("first_case_branch")
                                : QStringLiteral("case_branch");
        renderOne(pick(key, branches.at(i)), branches.at(i));
    }
    const QDomElement defaultBranch = branches.last();
    QString key = QStringLiteral("default_branch");
    if (regular == 0 && rule.contains(QLatin1String("default_branch_only")))
        key = QStringLiteral("default_branch_only");
    else if (childElements(defaultBranch).isEmpty() && rule.contains(QLatin1String("default_branch_empty")))
        key = QStringLiteral("default_branch_empty");
    renderOne(pick(key, defaultBranch), defaultBranch);
    return lines;
}

void Renderer::collectVariables()
{
    QStringList order;                       // collected variables in order of first appearance
    QHash<QString, QString> spelling;         // normalized -> name
    QHash<QString, QStringList> expressions;  // normalized -> assigned expressions ("" = integer)
    QHash<QString, VarKind> declaredKinds;    // from C declarations and typed parameters
    QSet<QString> excluded;                   // parameters, the algorithm name
    QSet<QString> needed;                     // assigned outside a loop header that declares them
    QSet<QString> chartDeclared;              // declared by a C declaration in a process block
    auto normalized = [&](const QString &s) { return normalizedName(s); };

    auto declare = [&](const Declarator &d) {
        const QString key = normalized(d.name);
        if (!declaredKinds.contains(key)) {
            if (const std::optional<VarKind> kind = kindOfType(d.type, d.pointer, d.array))
                declaredKinds.insert(key, *kind);
            else if (!d.init.isEmpty())
                expressions[key] << d.init; // auto x = 1.5
        }
    };
    auto declarations = [](const QString &text) {
        QList<Declarator> all;
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const QStringList statements = afce::splitList(line, QLatin1Char(';'));
            for (const QString &statement : statements) {
                QList<Declarator> ds;
                if (parseDeclaration(statement, &ds))
                    all << ds;
            }
        }
        return all;
    };

    const QStringList params = afce::splitList(elementAttribute(m_algorithm, QStringLiteral("params")));
    for (const QString &p : params) {
        QList<Declarator> ds;
        QString name;
        if (parseDeclaration(p, &ds) && ds.size() == 1) {
            declare(ds.first());
            name = ds.first().name;
        } else {
            QString type;
            splitDeclarator(p, &name, &type);
            if (type.isEmpty() && isPlainIdentifier(name))
                declaredKinds.insert(normalized(name), VarKind::Int); // untyped parameters are int (as in C)
            else if (const std::optional<VarKind> kind = kindOfType(type, false, p.contains(QLatin1Char('['))))
                declaredKinds.insert(normalized(name), *kind);
        }
        excluded.insert(normalized(name));
    }
    const QString algorithmName = elementAttribute(m_algorithm, QStringLiteral("name")).trimmed();
    if (!algorithmName.isEmpty())
        excluded.insert(normalized(algorithmName));

    // scoped: the variable is declared by the loop header itself (C-like for / foreach / forc init)
    auto add = [&](const QString &candidate, const QString &expression = QString(), bool scoped = false) {
        const QString name = candidate.trimmed();
        const QString key = normalized(name);
        if (!isPlainIdentifier(name) || isKeyword(name) || excluded.contains(key))
            return;
        if (!spelling.contains(key)) {
            spelling.insert(key, name);
            order << key;
        }
        expressions[key] << expression.trimmed();
        if (!scoped)
            needed.insert(key);
    };
    auto addAssignment = [&](const QString &statement) {
        const int eq = topLevelAssignment(statement);
        if (eq >= 0)
            add(statement.left(eq), statement.mid(eq + 1));
    };

    // iterative pre-order walk (hand-made documents may be nested very deeply)
    QList<QDomElement> pending = childElements(m_algorithm);
    std::reverse(pending.begin(), pending.end());
    while (!pending.isEmpty()) {
        const QDomElement e = pending.takeLast();
        QList<QDomElement> children = childElements(e);
        for (auto it = children.crbegin(); it != children.crend(); ++it)
            pending << *it;
        const QString type = e.tagName();
        if (type == QLatin1String("branch"))
            continue;

        // declared types (whatever the rule collects)
        if (type == QLatin1String("process")) {
            const QList<Declarator> ds = declarations(e.attribute(QStringLiteral("text")));
            for (const Declarator &d : ds) {
                declare(d);
                chartDeclared.insert(normalized(d.name));
            }
        } else if (type == QLatin1String("forc")) {
            const QList<Declarator> ds = declarations(elementAttribute(e, QStringLiteral("init")));
            for (const Declarator &d : ds)
                declare(d);
        } else if (type == QLatin1String("for") || type == QLatin1String("foreach")) {
            QList<Declarator> ds;
            if (parseDeclaration(elementAttribute(e, QStringLiteral("var")), &ds) && ds.size() == 1)
                declare(ds.first());
        }

        const QJsonObject rule = ruleFor(type);
        if (rule.contains(QLatin1String("declares"))) {
            Scope scope;
            scope.element = e;
            scope.type = type;
            scope.rule = rule;
            scope.rawMode = true;
            const QJsonArray declares = rule.value(QLatin1String("declares")).toArray();
            for (const QJsonValue &d : declares) {
                const QStringList names = afce::splitList(expandInline(d.toString(), scope));
                for (const QString &n : names)
                    add(n);
            }
        } else if (type == QLatin1String("assign")) {
            add(elementAttribute(e, QStringLiteral("dest")), elementAttribute(e, QStringLiteral("src")));
        } else if (type == QLatin1String("for")) {
            const QString var = elementAttribute(e, QStringLiteral("var"));
            QList<Declarator> ds;
            add(parseDeclaration(var, &ds) && ds.size() == 1 ? ds.first().name : var,
                elementAttribute(e, QStringLiteral("from")), true);
        } else if (type == QLatin1String("foreach")) {
            QString name;
            splitDeclarator(elementAttribute(e, QStringLiteral("var")), &name, nullptr);
            const QString range = elementAttribute(e, QStringLiteral("range")).trimmed();
            add(name, isPlainIdentifier(range) ? range + QStringLiteral("[0]") : QString(), true);
        } else if (type == QLatin1String("forc")) {
            const QStringList statements = afce::splitList(elementAttribute(e, QStringLiteral("init")), QLatin1Char(';'));
            for (const QString &statement : statements) {
                QList<Declarator> ds;
                if (parseDeclaration(statement, &ds)) {
                    for (const Declarator &d : std::as_const(ds))
                        add(d.name, d.init, true);
                    continue;
                }
                const QStringList converted = convertCStatements(statement);
                for (const QString &c : converted)
                    addAssignment(c);
            }
        } else if (type == QLatin1String("io")) {
            const QStringList items = afce::splitList(elementAttribute(e, QStringLiteral("vars")));
            for (const QString &item : items)
                add(item);
        } else if (type == QLatin1String("process")) {
            // simple assignments written as a process ("count = 0", "a = 1; b = 2")
            const QStringList lines = e.attribute(QStringLiteral("text")).split(QLatin1Char('\n'));
            for (const QString &line : lines) {
                const QStringList statements = afce::splitList(line, QLatin1Char(';'));
                for (const QString &statement : statements) {
                    if (!parseDeclaration(statement, nullptr))
                        addAssignment(statement);
                }
            }
        }
    }

    // type inference: declared types win, otherwise the assigned expressions decide (several passes
    // so that "b = a" follows a)
    QHash<QString, VarKind> kinds = declaredKinds;
    QStringList inferred;
    for (auto it = expressions.cbegin(); it != expressions.cend(); ++it) {
        if (!declaredKinds.contains(it.key()))
            inferred << it.key();
    }
    std::sort(inferred.begin(), inferred.end());
    for (int pass = 0; pass < 3; ++pass) {
        for (const QString &key : std::as_const(inferred)) {
            std::optional<VarKind> kind;
            const QStringList exprs = expressions.value(key);
            for (const QString &expr : exprs) {
                const VarKind k = expressionKind(expr, [&](const QString &identifier) -> std::optional<VarKind> {
                    const QString other = normalized(identifier);
                    if (other != key && kinds.contains(other))
                        return kinds.value(other);
                    return std::nullopt;
                });
                kind = kind ? mergeKinds(*kind, k) : k;
            }
            kinds.insert(key, kind.value_or(VarKind::Int));
        }
    }
    m_kinds = kinds;

    QStringList all;
    QHash<VarKind, QStringList> byKind;
    QStringList undeclared;
    QHash<VarKind, QStringList> undeclaredByKind;
    for (const QString &key : std::as_const(order)) {
        const VarKind kind = kinds.value(key, VarKind::Int);
        if (chartDeclared.contains(key))
            continue; // the chart declares it itself (a C declaration in a process block)
        all << spelling.value(key);
        byKind[kind] << spelling.value(key);
        if (needed.contains(key)) {
            undeclared << spelling.value(key);
            undeclaredByKind[kind] << spelling.value(key);
        }
    }
    static const QList<QPair<VarKind, QString>> suffixes = {
        {VarKind::Int, QStringLiteral("_int")}, {VarKind::Real, QStringLiteral("_real")},
        {VarKind::Bool, QStringLiteral("_bool")}, {VarKind::String, QStringLiteral("_string")},
        {VarKind::Char, QStringLiteral("_char")}};
    m_variables.clear();
    m_variables.insert(QStringLiteral("variables"), all.join(QLatin1Char(',')));
    m_variables.insert(QStringLiteral("undeclared"), undeclared.join(QLatin1Char(',')));
    for (const auto &suffix : suffixes) {
        m_variables.insert(QStringLiteral("variables") + suffix.second, byKind.value(suffix.first).join(QLatin1Char(',')));
        m_variables.insert(QStringLiteral("undeclared") + suffix.second,
                           undeclaredByKind.value(suffix.first).join(QLatin1Char(',')));
    }
    // the parameters of the program's entry point (main(argc, argv)): scripting languages call it
    const bool entry = algorithmName.isEmpty() || algorithmName == QLatin1String("main");
    m_variables.insert(QStringLiteral("main_params"), entry ? afce::joinList(params) : QString());
}

QString Renderer::render(const QDomDocument &doc)
{
    m_algorithm = doc.firstChildElement(QStringLiteral("algorithm"));
    if (m_algorithm.isNull()) {
        const QDomElement root = doc.documentElement();
        if (root.tagName() == QLatin1String("algorithm"))
            m_algorithm = root;
    }
    if (m_algorithm.isNull())
        return QString();
    collectVariables();

    QStringList lines;
    if (m_rule.value(QLatin1String("algorithm")).isObject()) {
        lines = renderBlock(m_algorithm);
    } else {
        Scope scope = makeScope(m_algorithm, QJsonObject());
        m_stack.append(m_algorithm);
        lines = expand(QStringLiteral("%branch1%"), scope);
        m_stack.removeLast();
    }

    for (QString &line : lines) {
        int end = line.size();
        while (end > 0 && line.at(end - 1).isSpace())
            --end;
        line.truncate(end);
    }
    while (!lines.isEmpty() && lines.first().isEmpty())
        lines.removeFirst();
    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();
    if (lines.isEmpty())
        return QString();
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

} // namespace

// ---------------------------------------------------------------------------

SourceCodeGenerator::SourceCodeGenerator(QObject *parent)
    : QObject(parent)
{
}

SourceCodeGenerator::~SourceCodeGenerator() = default;

void SourceCodeGenerator::loadRule(const QString &fileName)
{
    QString path = fileName;
    if (!QFile::exists(path) && !path.contains(QLatin1Char('/')) && !path.contains(QLatin1Char(':'))
        && !path.endsWith(QLatin1String(".json"))) {
        path = QStringLiteral("generators:") + path + QStringLiteral(".json");
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_rule = QJsonObject();
        m_loaded = false;
        m_error = tr("Unable to load code generation rules from \"%1\": %2").arg(fileName, f.errorString());
        return;
    }
    ruleFromJSON(f.readAll());
    if (!m_loaded)
        m_error = tr("Invalid code generation rules in \"%1\": %2").arg(fileName, m_error);
}

void SourceCodeGenerator::ruleFromJSON(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        m_rule = QJsonObject();
        m_loaded = false;
        m_error = error.error != QJsonParseError::NoError
                      ? tr("%1 at offset %2").arg(error.errorString()).arg(error.offset)
                      : tr("the top-level JSON value is not an object");
        return;
    }
    m_rule = doc.object();
    m_loaded = true;
    m_error.clear();
}

QString SourceCodeGenerator::applyRule(const QDomDocument &xml)
{
    Renderer renderer(m_rule);
    return renderer.render(xml);
}

bool SourceCodeGenerator::isLoaded() const
{
    return m_loaded;
}

QString SourceCodeGenerator::errorString() const
{
    return m_error;
}

QJsonObject SourceCodeGenerator::rule() const
{
    return m_rule;
}

QString SourceCodeGenerator::languageName(const QLocale &locale, const QString &fallback) const
{
    return languageName(m_rule, locale, fallback);
}

QString SourceCodeGenerator::languageName(const QJsonObject &rule, const QLocale &locale, const QString &fallback)
{
    const QJsonValue name = rule.value(QLatin1String("name"));
    if (name.isString())
        return name.toString();
    const QJsonObject names = name.toObject();
    const QString full = locale.name();
    if (names.contains(full))
        return names.value(full).toString();
    const QString language = full.section(QLatin1Char('_'), 0, 0);
    for (auto it = names.begin(); it != names.end(); ++it) {
        if (it.key().section(QLatin1Char('_'), 0, 0) == language)
            return it.value().toString();
    }
    if (names.contains(QLatin1String("en_US")))
        return names.value(QLatin1String("en_US")).toString();
    return fallback;
}
