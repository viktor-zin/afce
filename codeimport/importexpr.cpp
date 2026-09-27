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

#include "importexpr.h"

#include <QSet>

namespace afce {
namespace cimport {

namespace {

QSet<QString> makeSet(std::initializer_list<const char *> words)
{
    QSet<QString> set;
    for (const char *w : words)
        set.insert(QString::fromLatin1(w));
    return set;
}

bool isAttributeStart(const TokenStream &ts, int k)
{
    if (ts.isPunct(k, "[") && ts.isPunct(k + 1, "["))
        return true;
    const Token &t = ts.at(k);
    return t.isWord() && (t.is("__attribute__") || t.is("__declspec") || t.is("alignas") || t.is("_Alignas"))
        && ts.isPunct(k + 1, "(");
}

int skipAttribute(const TokenStream &ts, int k)
{
    if (ts.isPunct(k, "["))
        return ts.after(k);
    return ts.after(k + 1);
}

} // namespace

bool isBuiltinType(const Token &t)
{
    static const QSet<QString> words = makeSet({
        "void", "bool", "char", "char8_t", "char16_t", "char32_t", "wchar_t", "short", "int", "long", "float",
        "double", "signed", "unsigned", "auto", "_Bool", "_Complex", "__int64", "__int128", "__int32",
    });
    return t.isWord() && words.contains(t.text);
}

bool isDeclSpecifier(const Token &t)
{
    static const QSet<QString> words = makeSet({
        "static", "const", "constexpr", "register", "volatile", "thread_local", "extern", "mutable", "inline",
        "_Thread_local", "constinit", "restrict", "__restrict", "__restrict__", "__extension__",
    });
    return t.isWord() && words.contains(t.text);
}

bool isStatementKeyword(const Token &t)
{
    static const QSet<QString> words = makeSet({
        "if", "for", "while", "do", "switch", "return", "break", "continue", "case", "default", "else", "goto", "try",
    });
    return t.kind == Token::Keyword && words.contains(t.text);
}

bool isAssignmentOp(const Token &t)
{
    static const QSet<QString> ops = makeSet({"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="});
    return t.kind == Token::Punct && ops.contains(t.text);
}

int qualifiedNameEnd(const TokenStream &ts, int k, int limit, bool strictTemplates)
{
    const bool cpp = ts.language() == Lang::Cpp;
    int m = k;
    if (ts.isPunct(m, "::"))
        ++m;
    const int first = m;
    while (m < limit) {
        const Token &t = ts.at(m);
        if (cpp && t.isKeyword("template") && m > first) {
            ++m;
            continue;
        }
        if (t.isPunct("~") && ts.at(m + 1).isIdentifier() && m + 1 < limit)
            return m + 2;
        if (!t.isIdentifier())
            break;
        ++m;
        if (cpp && ts.isPunct(m, "<")) {
            const int e = ts.skipTemplateArgs(m, limit);
            if (e > 0) {
                const Token &nx = ts.at(e);
                if (!strictTemplates || (e < limit && (nx.isPunct("(") || nx.isPunct("::") || nx.isPunct("{"))))
                    m = e;
            }
        }
        if (m < limit && ts.isPunct(m, "::")) {
            ++m;
            continue;
        }
        return m;
    }
    return m == first ? k : m;
}

QVector<Range> splitTopLevel(const TokenStream &ts, Range r, const char *sep)
{
    QVector<Range> parts;
    int start = r.begin;
    int k = r.begin;
    while (k < r.end) {
        if (ts.isOpener(k)) {
            k = qMin(ts.after(k), r.end);
            continue;
        }
        const Token &t = ts.at(k);
        if (t.isIdentifier() && ts.isPunct(k + 1, "<")) {
            const int e = qualifiedNameEnd(ts, k, r.end, true);
            if (e > k + 1) {
                k = e;
                continue;
            }
        }
        if (t.isPunct(sep)) {
            parts.append(Range(start, k));
            start = k + 1;
        }
        ++k;
    }
    parts.append(Range(start, r.end));
    return parts;
}

int findTopLevel(const TokenStream &ts, Range r, const char *sep)
{
    int k = r.begin;
    while (k < r.end) {
        if (ts.isPunct(k, sep))
            return k;
        if (ts.isOpener(k)) {
            k = ts.after(k);
            continue;
        }
        ++k;
    }
    return -1;
}

int findAssignment(const TokenStream &ts, Range r)
{
    int k = r.begin;
    while (k < r.end) {
        const Token &t = ts.at(k);
        if (ts.isOpener(k)) {
            k = ts.after(k);
            continue;
        }
        if (t.isPunct("?"))
            return -1;
        if (isAssignmentOp(t))
            return k;
        if (t.isIdentifier() && ts.isPunct(k + 1, "<")) {
            const int e = qualifiedNameEnd(ts, k, r.end, true);
            if (e > k + 1) {
                k = e;
                continue;
            }
        }
        ++k;
    }
    return -1;
}

bool containsPunct(const TokenStream &ts, Range r, const char *p)
{
    for (int k = r.begin; k < r.end; ++k) {
        if (ts.isPunct(k, p))
            return true;
    }
    return false;
}

bool isPrimaryExpression(const TokenStream &ts, Range r)
{
    if (r.isEmpty())
        return false;
    int k = r.begin;
    const Token &t = ts.at(k);
    if (t.isPunct("(")) {
        if (!ts.matched(k))
            return false;
        k = ts.after(k);
    } else if (t.kind == Token::String) {
        while (k < r.end && ts.at(k).kind == Token::String)
            ++k;
    } else if (t.kind == Token::Number || t.kind == Token::Char) {
        ++k;
    } else if (t.kind == Token::Keyword) {
        if (t.is("this") || t.is("true") || t.is("false") || t.is("nullptr")) {
            ++k;
        } else if (t.is("sizeof") || t.is("alignof") || t.is("static_cast") || t.is("dynamic_cast")
                   || t.is("reinterpret_cast") || t.is("const_cast") || t.is("typeid") || t.is("decltype")
                   || t.is("noexcept")) {
            ++k;
            if (ts.isPunct(k, "<")) {
                const int e = ts.skipTemplateArgs(k, r.end);
                if (e < 0)
                    return false;
                k = e;
            }
            if (!ts.isPunct(k, "("))
                return false;
            k = ts.after(k);
        } else {
            return false;
        }
    } else if (t.isIdentifier() || t.isPunct("::")) {
        const int e = qualifiedNameEnd(ts, k, r.end, true);
        if (e == k)
            return false;
        k = e;
    } else {
        return false;
    }
    while (k < r.end) {
        const Token &u = ts.at(k);
        if (u.isPunct("(") || u.isPunct("[") || u.isPunct("{")) {
            if (!ts.matched(k))
                return false;
            k = ts.after(k);
        } else if (u.isPunct(".") || u.isPunct("->")) {
            const int e = qualifiedNameEnd(ts, k + 1, r.end, true);
            if (e == k + 1)
                return false;
            k = e;
        } else {
            return false;
        }
    }
    return k == r.end;
}

QString compactText(const TokenStream &ts, Range r)
{
    QString s;
    for (int k = r.begin; k < r.end; ++k) {
        if (ts.at(k).kind != Token::Preprocessor)
            s += ts.at(k).text;
    }
    return s;
}

bool parseCall(const TokenStream &ts, Range r, QString *name, QVector<Range> *args)
{
    if (r.isEmpty() || !(ts.at(r.begin).isIdentifier() || ts.isPunct(r.begin, "::")))
        return false;
    const int m = qualifiedNameEnd(ts, r.begin, r.end, true);
    if (m == r.begin || !ts.isPunct(m, "(") || !ts.matched(m) || ts.after(m) != r.end)
        return false;
    if (name)
        *name = compactText(ts, Range(r.begin, m));
    if (args) {
        args->clear();
        const Range content(m + 1, ts.contentEnd(m));
        if (!content.isEmpty())
            *args = splitTopLevel(ts, content, ",");
    }
    return true;
}

bool looksLikeDeclaration(const TokenStream &ts, Range r)
{
    int k = r.begin;
    while (k < r.end) {
        if (isDeclSpecifier(ts.at(k))) {
            ++k;
        } else if (isAttributeStart(ts, k)) {
            k = skipAttribute(ts, k);
        } else {
            break;
        }
    }
    if (k >= r.end)
        return false;
    const Token &t = ts.at(k);
    if (isBuiltinType(t))
        return true;
    if ((t.isKeyword("struct") || t.isKeyword("union") || t.isKeyword("enum") || t.isKeyword("class"))
        && ts.at(k + 1).isIdentifier()) {
        return true;
    }
    if (t.isKeyword("typename"))
        return true;
    if ((t.isKeyword("decltype") || t.isKeyword("typeof") || t.is("__typeof__")) && ts.isPunct(k + 1, "("))
        return true;
    if (!(t.isIdentifier() || t.isPunct("::")))
        return false;
    int m = qualifiedNameEnd(ts, k, r.end, false);
    if (m == k)
        return false;
    while (m < r.end && (ts.isPunct(m, "*") || ts.isPunct(m, "&") || ts.isPunct(m, "&&") || ts.isKeyword(m, "const")
                         || ts.isKeyword(m, "volatile"))) {
        ++m;
    }
    if (m >= r.end || !ts.at(m).isIdentifier())
        return false;
    if (m + 1 >= r.end)
        return true;
    const Token &nx = ts.at(m + 1);
    return nx.isPunct("=") || nx.isPunct(",") || nx.isPunct("[") || nx.isPunct("(") || nx.isPunct("{")
        || nx.isPunct(";");
}

Declaration parseDeclaration(const TokenStream &ts, Range r)
{
    Declaration d;
    int k = r.begin;
    bool haveType = false;
    while (k < r.end) {
        const Token &t = ts.at(k);
        if (isDeclSpecifier(t)) {
            ++k;
            continue;
        }
        if (isAttributeStart(ts, k)) {
            k = skipAttribute(ts, k);
            continue;
        }
        if (isBuiltinType(t)) {
            ++k;
            haveType = true;
            continue;
        }
        if (t.isKeyword("struct") || t.isKeyword("union") || t.isKeyword("enum") || t.isKeyword("class")
            || t.isKeyword("typename")) {
            const bool isEnum = t.isKeyword("enum");
            ++k;
            if (isEnum && (ts.isKeyword(k, "class") || ts.isKeyword(k, "struct")))
                ++k;
            const int m = qualifiedNameEnd(ts, k, r.end, false);
            if (m > k)
                k = m;
            haveType = true;
            continue;
        }
        if ((t.isKeyword("decltype") || t.isKeyword("typeof") || t.is("__typeof__") || t.is("_Atomic"))
            && ts.isPunct(k + 1, "(")) {
            k = ts.after(k + 1);
            haveType = true;
            continue;
        }
        if (!haveType && (t.isIdentifier() || t.isPunct("::"))) {
            const int m = qualifiedNameEnd(ts, k, r.end, false);
            if (m == k)
                break;
            k = m;
            haveType = true;
            continue;
        }
        break;
    }
    if (!haveType || k >= r.end)
        return d;
    d.type = Range(r.begin, k);

    const QVector<Range> pieces = splitTopLevel(ts, Range(k, r.end), ",");
    for (const Range &piece : pieces) {
        Declarator dc;
        dc.range = piece;
        int m = piece.begin;
        while (m < piece.end
               && (ts.isPunct(m, "*") || ts.isPunct(m, "&") || ts.isPunct(m, "&&") || ts.isKeyword(m, "const")
                   || ts.isKeyword(m, "volatile") || ts.at(m).is("restrict") || ts.at(m).is("__restrict")
                   || ts.at(m).is("__restrict__"))) {
            ++m;
        }
        if (m >= piece.end) {
            dc.init = Declarator::Invalid;
        } else if (ts.isPunct(m, "[")) {
            // structured binding: auto [a, b] = ...
            dc.name = ts.text(m, qMin(ts.after(m), piece.end));
            m = ts.after(m);
        } else if (ts.isPunct(m, "(")) {
            // function pointer / parenthesized declarator: int (*fp)(int)
            const int ce = qMin(ts.contentEnd(m), piece.end);
            int q = m + 1;
            while (q < ce && !ts.at(q).isIdentifier())
                ++q;
            if (q < ce)
                dc.name = ts.at(q).text;
            else
                dc.init = Declarator::Invalid;
            m = ts.after(m);
            while (m < piece.end && (ts.isPunct(m, "(") || ts.isPunct(m, "[")))
                m = ts.after(m);
        } else if (ts.at(m).isIdentifier()) {
            dc.name = ts.at(m).text;
            ++m;
        } else {
            dc.init = Declarator::Invalid;
        }
        if (dc.init != Declarator::Invalid) {
            while (m < piece.end && ts.isPunct(m, "["))
                m = ts.after(m);
            if (m >= piece.end) {
                dc.init = Declarator::None;
            } else if (ts.isPunct(m, "=")) {
                dc.init = Declarator::Assign;
                dc.initializer = Range(m + 1, piece.end);
                if (dc.initializer.isEmpty())
                    dc.init = Declarator::Invalid;
            } else if ((ts.isPunct(m, "(") || ts.isPunct(m, "{")) && ts.matched(m) && ts.after(m) == piece.end) {
                dc.init = Declarator::Direct;
                dc.initializer = Range(m, piece.end);
                if (ts.isPunct(m, "(")) {
                    const Range content(m + 1, ts.contentEnd(m));
                    if (!content.isEmpty()) {
                        const Range firstParam = splitTopLevel(ts, content, ",").first();
                        dc.prototype = looksLikeDeclaration(ts, firstParam) || ts.isPunct(firstParam.begin, "...");
                    }
                }
            } else {
                dc.init = Declarator::Invalid;
            }
        }
        d.declarators.append(dc);
    }
    d.ok = !d.declarators.isEmpty();
    return d;
}

} // namespace cimport
} // namespace afce
