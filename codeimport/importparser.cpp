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

#include "importparser.h"

namespace afce {
namespace cimport {

StatementParser::StatementParser(const TokenStream &ts, std::deque<Node> &arena)
    : ts(ts)
    , arena(arena)
{
}

Node *StatementParser::make(Node::Kind kind, int begin, int end)
{
    arena.emplace_back();
    Node *n = &arena.back();
    n->kind = kind;
    n->range = Range(begin, end);
    return n;
}

QVector<Node *> StatementParser::parse(int begin, int end)
{
    QVector<Node *> out;
    switches.clear();
    parseList(begin, qMin(end, ts.size()), out, 0);
    return out;
}

void StatementParser::parseList(int begin, int end, QVector<Node *> &out, int depth)
{
    // Nested compound statements are flattened iteratively, so any number of
    // nested braces costs no recursion.
    struct Frame
    {
        int pos;
        int end;
    };
    QVector<Frame> stack;
    stack.append(Frame{begin, end});
    while (!stack.isEmpty()) {
        Frame &f = stack.last();
        if (f.pos >= f.end) {
            stack.removeLast();
            continue;
        }
        if (ts.isPunct(f.pos, "{")) {
            const Frame inner{f.pos + 1, qMin(ts.contentEnd(f.pos), f.end)};
            f.pos = qMin(ts.after(f.pos), f.end);
            stack.append(inner);
            continue;
        }
        const int next = parseStatement(f.pos, f.end, out, depth);
        f.pos = qMax(next, f.pos + 1);
    }
}

int StatementParser::parseStatement(int pos, int end, QVector<Node *> &out, int depth)
{
    const int next = parseStatementImpl(pos, end, out, depth);
    return qMax(next, pos + 1);
}

int StatementParser::parseSub(int pos, int end, QVector<Node *> &out, int depth)
{
    if (pos >= end)
        return pos;
    int before = out.size();
    int p = parseStatement(pos, end, out, depth);
    // A label is not a statement by itself: the labelled statement follows.
    while (p < end && out.size() > before) {
        const Node::Kind k = out.last()->kind;
        if (k != Node::CaseLabel && k != Node::DefaultLabel && k != Node::GotoLabel)
            break;
        before = out.size();
        p = parseStatement(p, end, out, depth);
    }
    return p;
}

int StatementParser::parseStatementImpl(int pos, int end, QVector<Node *> &out, int depth)
{
    if (depth > MaxDepth) {
        const int e = skipStatement(pos, end);
        out.append(make(Node::TooDeep, pos, stripSemicolon(pos, e)));
        return e;
    }
    // Attributes before a statement: [[fallthrough]]; [[likely]] ... ; __attribute__((...))
    for (;;) {
        if (ts.isPunct(pos, "[") && ts.isPunct(pos + 1, "[")) {
            pos = qMin(ts.after(pos), end);
        } else if (ts.at(pos).isIdentifier() && ts.at(pos).is("__attribute__") && ts.isPunct(pos + 1, "(")) {
            pos = qMin(ts.after(pos + 1), end);
        } else {
            break;
        }
        if (pos >= end)
            return end;
    }
    const Token &t = ts.at(pos);
    switch (t.kind) {
    case Token::Preprocessor:
        out.append(make(Node::Preprocessor, pos, pos + 1));
        return pos + 1;
    case Token::Punct:
        if (t.is(";"))
            return pos + 1;
        if (t.is("{")) {
            parseList(pos + 1, qMin(ts.contentEnd(pos), end), out, depth);
            return qMin(ts.after(pos), end);
        }
        if (ts.isCloser(pos)) {
            out.append(make(Node::Stray, pos, pos + 1));
            return pos + 1;
        }
        break;
    case Token::Keyword:
        if (t.is("if"))
            return parseIf(pos, end, out, depth);
        if (t.is("while"))
            return parseWhile(pos, end, out, depth);
        if (t.is("do"))
            return parseDo(pos, end, out, depth);
        if (t.is("for"))
            return parseFor(pos, end, out, depth);
        if (t.is("switch"))
            return parseSwitch(pos, end, out, depth);
        if (t.is("case"))
            return parseCase(pos, end, out);
        if (t.is("default")) {
            if (ts.isPunct(pos + 1, ":")) {
                Node *n = make(Node::DefaultLabel, pos, pos + 1);
                noteLabel(n, out);
                out.append(n);
                return pos + 2;
            }
            out.append(make(Node::Stray, pos, pos + 1));
            return pos + 1;
        }
        if (t.is("return"))
            return parseKeywordStatement(Node::Return, pos, end, out);
        if (t.is("break"))
            return parseKeywordStatement(Node::Break, pos, end, out);
        if (t.is("continue"))
            return parseKeywordStatement(Node::Continue, pos, end, out);
        if (t.is("goto"))
            return parseKeywordStatement(Node::Goto, pos, end, out);
        if (t.is("try"))
            return parseTry(pos, end, out, depth);
        if (t.is("asm"))
            return parseAsm(pos, end, out);
        if (t.is("using") || t.is("typedef") || t.is("static_assert") || t.is("_Static_assert"))
            return parseKeywordStatement(Node::Skip, pos, end, out);
        if (t.is("struct") || t.is("class") || t.is("union") || t.is("enum"))
            return parseLocalType(pos, end, out);
        if (t.is("else")) {
            out.append(make(Node::Stray, pos, pos + 1));
            return pos + 1;
        }
        if (t.is("catch")) {
            int p = pos + 1;
            if (ts.isPunct(p, "("))
                p = ts.after(p);
            if (ts.isPunct(p, "{"))
                p = ts.after(p);
            out.append(make(Node::Stray, pos, pos + 1));
            return qMin(p, end);
        }
        break;
    case Token::Identifier:
        if (ts.isPunct(pos + 1, ":") && pos + 1 < end) {
            out.append(make(Node::GotoLabel, pos, pos + 1));
            return pos + 2;
        }
        if (t.is("__asm__") || t.is("__asm") || (ts.language() == Lang::C && t.is("asm")))
            return parseAsm(pos, end, out);
        if (t.is("static_assert") || t.is("_Static_assert"))
            return parseKeywordStatement(Node::Skip, pos, end, out);
        if ((t.is("foreach") || t.is("Q_FOREACH")) && ts.isPunct(pos + 1, "(")) {
            const int p = parseQtForeach(pos, end, out, depth);
            if (p > pos)
                return p;
        }
        if ((t.is("forever") || t.is("Q_FOREVER")) && ts.isPunct(pos + 1, "{") && pos + 1 < end) {
            Node *n = make(Node::Forever, pos, pos + 1);
            n->body.begin = pos + 1;
            const int p = parseSub(pos + 1, end, n->children, depth + 1);
            n->body.end = p;
            n->range.end = stripSemicolon(pos, p);
            out.append(n);
            return p;
        }
        break;
    default:
        break;
    }
    return parseSimple(pos, end, out);
}

int StatementParser::parseQtForeach(int pos, int end, QVector<Node *> &out, int depth)
{
    // Qt: foreach (const QString &s, list) statement
    const int lp = pos + 1;
    if (!ts.matched(lp))
        return -1;
    const QVector<Range> parts = splitTopLevel(ts, Range(lp + 1, ts.contentEnd(lp)), ",");
    const int body = ts.after(lp);
    if (parts.size() != 2 || parts.at(0).isEmpty() || parts.at(1).isEmpty() || body >= end || ts.isPunct(body, ";"))
        return -1;
    Node *n = make(Node::RangeFor, pos, pos + 1);
    n->init = parts.at(0);
    n->step = parts.at(1);
    n->body.begin = body;
    const int p = parseSub(body, end, n->children, depth + 1);
    n->body.end = p;
    n->range.end = stripSemicolon(pos, p);
    out.append(n);
    return p;
}

int StatementParser::findStatementEnd(int start, int end, bool *missing) const
{
    int k = start;
    while (k < end) {
        const Token &t = ts.at(k);
        if (t.isPunct(";")) {
            *missing = false;
            return k;
        }
        if (ts.isOpener(k)) {
            k = qMin(ts.after(k), end);
            continue;
        }
        if (t.isPunct("}")) {
            *missing = true;
            return k;
        }
        if (k > start && isStatementKeyword(t)) {
            *missing = true;
            return k;
        }
        ++k;
    }
    *missing = true;
    return end;
}

int StatementParser::recover(int start, int end) const
{
    int k = start;
    while (k < end) {
        if (ts.isPunct(k, ";"))
            return k + 1;
        if (ts.isPunct(k, "}"))
            return k;
        if (ts.isOpener(k)) {
            k = qMin(ts.after(k), end);
            continue;
        }
        ++k;
    }
    return end;
}

int StatementParser::stripSemicolon(int begin, int end) const
{
    return (end > begin && ts.isPunct(end - 1, ";")) ? end - 1 : end;
}

int StatementParser::error(int pos, int end, QVector<Node *> &out)
{
    const int e = qMax(recover(pos + 1, end), pos + 1);
    out.append(make(Node::Error, pos, stripSemicolon(pos, e)));
    return e;
}

int StatementParser::parseSimple(int pos, int end, QVector<Node *> &out)
{
    // MACRO(args) { ... }: the macro invocation and the block are separate
    // statements (otherwise the block and the next statement would be swallowed).
    if (ts.at(pos).isIdentifier() && ts.isPunct(pos + 1, "(") && ts.matched(pos + 1)) {
        const int a = ts.after(pos + 1);
        if (a < end && ts.isPunct(a, "{")) {
            Node *n = make(Node::Expression, pos, a);
            n->expr = Range(pos, a);
            n->macroBlock = true;
            out.append(n);
            return a;
        }
    }
    bool missing = false;
    const int e = findStatementEnd(pos, end, &missing);
    if (e <= pos) {
        out.append(make(Node::Stray, pos, pos + 1));
        return pos + 1;
    }
    const Range r(pos, e);
    Node *n = make(looksLikeDeclaration(ts, r) ? Node::Declaration : Node::Expression, pos, e);
    n->expr = r;
    n->missingSemicolon = missing;
    out.append(n);
    return (!missing && e < end) ? e + 1 : e;
}

int StatementParser::parseKeywordStatement(Node::Kind kind, int pos, int end, QVector<Node *> &out)
{
    bool missing = false;
    const int e = findStatementEnd(pos + 1, end, &missing);
    Node *n = make(kind, pos, e);
    n->expr = Range(pos + 1, e);
    n->missingSemicolon = missing;
    out.append(n);
    return (!missing && e < end) ? e + 1 : e;
}

int StatementParser::parseCondition(Node *n, int p, int end, int depth, bool allowInit)
{
    if (p >= end || !ts.isPunct(p, "("))
        return -1;
    const int ce = qMin(ts.contentEnd(p), end);
    const int semi = allowInit ? findTopLevel(ts, Range(p + 1, ce), ";") : -1;
    if (semi >= 0) {
        parseList(p + 1, semi + 1, n->initStmts, depth + 1);
        n->expr = Range(semi + 1, ce);
    } else {
        n->expr = Range(p + 1, ce);
    }
    if (n->expr.isEmpty())
        return -1;
    return qMin(ts.after(p), end);
}

int StatementParser::parseIf(int pos, int end, QVector<Node *> &out, int depth)
{
    Node *n = make(Node::If, pos, pos + 1);
    int p = pos + 1;
    if (ts.isKeyword(p, "constexpr"))
        ++p;
    if (ts.isPunct(p, "!") && ts.isKeyword(p + 1, "consteval")) {
        n->expr = Range(p, p + 2);
        p += 2;
    } else if (ts.isKeyword(p, "consteval")) {
        n->expr = Range(p, p + 1);
        ++p;
    } else {
        p = parseCondition(n, p, end, depth, true);
        if (p < 0)
            return error(pos, end, out);
    }
    p = parseSub(p, end, n->children, depth + 1);
    if (p < end && ts.isKeyword(p, "else")) {
        n->hasElse = true;
        p = parseSub(p + 1, end, n->elseChildren, depth + 1);
    }
    n->range.end = stripSemicolon(pos, p);
    out.append(n);
    return p;
}

int StatementParser::parseWhile(int pos, int end, QVector<Node *> &out, int depth)
{
    Node *n = make(Node::While, pos, pos + 1);
    int p = parseCondition(n, pos + 1, end, depth, false);
    if (p < 0)
        return error(pos, end, out);
    n->body.begin = p;
    p = parseSub(p, end, n->children, depth + 1);
    n->body.end = p;
    n->range.end = stripSemicolon(pos, p);
    out.append(n);
    return p;
}

int StatementParser::parseDo(int pos, int end, QVector<Node *> &out, int depth)
{
    Node *n = make(Node::DoWhile, pos, pos + 1);
    int p = parseSub(pos + 1, end, n->children, depth + 1);
    n->body = Range(pos + 1, p);
    if (!ts.isKeyword(p, "while") || p >= end) {
        out.append(make(Node::Error, pos, stripSemicolon(pos, p)));
        return p;
    }
    const int q = parseCondition(n, p + 1, end, depth, false);
    if (q < 0)
        return error(pos, end, out);
    p = q;
    if (p < end && ts.isPunct(p, ";")) {
        n->range.end = p;
        ++p;
    } else {
        n->range.end = p;
        n->missingSemicolon = true;
    }
    out.append(n);
    return p;
}

int StatementParser::parseFor(int pos, int end, QVector<Node *> &out, int depth)
{
    int lp = pos + 1;
    if (ts.isKeyword(lp, "co_await"))
        ++lp;
    if (!ts.isPunct(lp, "(") || lp >= end)
        return error(pos, end, out);
    const int b = lp + 1;
    const int ce = qMin(ts.contentEnd(lp), end);
    QVector<int> semis;
    int colon = -1;
    int questions = 0;
    int k = b;
    while (k < ce) {
        const Token &t = ts.at(k);
        if (ts.isOpener(k)) {
            k = ts.after(k);
            continue;
        }
        if (t.isPunct(";")) {
            semis.append(k);
            questions = 0;
        } else if (t.isPunct("?")) {
            ++questions;
        } else if (t.isPunct(":")) {
            if (questions > 0)
                --questions;
            else if (colon < 0 && semis.size() <= 1)
                colon = k;
        }
        ++k;
    }

    Node *n = nullptr;
    if (semis.size() == 2) {
        n = make(Node::For, pos, pos + 1);
        n->init = Range(b, semis.at(0));
        n->expr = Range(semis.at(0) + 1, semis.at(1));
        n->step = Range(semis.at(1) + 1, ce);
        if (!n->init.isEmpty())
            parseList(b, semis.at(0) + 1, n->initStmts, depth + 1);
    } else if (colon >= 0 && semis.size() <= 1 && (semis.isEmpty() || colon > semis.at(0))) {
        n = make(Node::RangeFor, pos, pos + 1);
        const int varBegin = semis.isEmpty() ? b : semis.at(0) + 1;
        if (!semis.isEmpty())
            parseList(b, semis.at(0) + 1, n->initStmts, depth + 1);
        n->init = Range(varBegin, colon);
        n->step = Range(colon + 1, ce);
        if (n->init.isEmpty() || n->step.isEmpty())
            return error(pos, end, out);
    } else {
        return error(pos, end, out);
    }
    int p = qMin(ts.after(lp), end);
    n->body.begin = p;
    p = parseSub(p, end, n->children, depth + 1);
    n->body.end = p;
    n->range.end = stripSemicolon(pos, p);
    out.append(n);
    return p;
}

int StatementParser::parseSwitch(int pos, int end, QVector<Node *> &out, int depth)
{
    Node *n = make(Node::Switch, pos, pos + 1);
    int p = parseCondition(n, pos + 1, end, depth, true);
    if (p < 0)
        return error(pos, end, out);
    n->body.begin = p;
    switches.append(SwitchContext{n, &n->children});
    p = parseSub(p, end, n->children, depth + 1);
    switches.removeLast();
    n->body.end = p;
    n->range.end = stripSemicolon(pos, p);
    out.append(n);
    return p;
}

int StatementParser::findLabelColon(int start, int end) const
{
    int questions = 0;
    int k = start;
    while (k < end) {
        const Token &t = ts.at(k);
        if (t.isPunct(":")) {
            if (questions == 0)
                return k;
            --questions;
        } else if (t.isPunct("?")) {
            ++questions;
        } else if (t.isPunct(";") || t.isPunct("{") || t.isPunct("}")) {
            return -1;
        } else if (ts.isOpener(k)) {
            k = ts.after(k);
            continue;
        }
        ++k;
    }
    return -1;
}

void StatementParser::noteLabel(Node *label, const QVector<Node *> &out)
{
    if (switches.isEmpty())
        label->orphan = true;
    else if (&out != switches.last().list)
        switches.last().node->nestedLabels = true;
}

int StatementParser::parseCase(int pos, int end, QVector<Node *> &out)
{
    const int colon = findLabelColon(pos + 1, end);
    if (colon < 0)
        return error(pos, end, out);
    Node *n = make(Node::CaseLabel, pos, colon);
    n->expr = Range(pos + 1, colon);
    noteLabel(n, out);
    out.append(n);
    return colon + 1;
}

int StatementParser::parseTry(int pos, int end, QVector<Node *> &out, int depth)
{
    int p = pos + 1;
    if (!ts.isPunct(p, "{") || p >= end)
        return error(pos, end, out);
    Node *n = make(Node::Try, pos, pos + 1);
    parseList(p + 1, qMin(ts.contentEnd(p), end), n->children, depth + 1);
    p = qMin(ts.after(p), end);
    while (p < end && ts.isKeyword(p, "catch")) {
        ++p;
        if (ts.isPunct(p, "("))
            p = qMin(ts.after(p), end);
        if (!ts.isPunct(p, "{") || p >= end)
            break;
        p = qMin(ts.after(p), end);
    }
    n->range.end = p;
    out.append(n);
    return p;
}

int StatementParser::parseAsm(int pos, int end, QVector<Node *> &out)
{
    int p = pos + 1;
    while (p < end && ts.at(p).isWord())
        ++p; // volatile, goto, inline, __volatile__
    if (ts.isPunct(p, "{") && p < end) {
        const int e = qMin(ts.after(p), end);
        out.append(make(Node::Asm, pos, e));
        return (e < end && ts.isPunct(e, ";")) ? e + 1 : e;
    }
    return parseKeywordStatement(Node::Asm, pos, end, out);
}

int StatementParser::parseLocalType(int pos, int end, QVector<Node *> &out)
{
    int h = pos + 1;
    if (ts.isKeyword(pos, "enum") && (ts.isKeyword(h, "class") || ts.isKeyword(h, "struct")))
        ++h;
    while (ts.isPunct(h, "[") && ts.isPunct(h + 1, "["))
        h = ts.after(h);
    if (ts.at(h).isIdentifier())
        ++h;
    if (ts.at(h).isIdentifier() && ts.at(h).is("final"))
        ++h;
    if (ts.isPunct(h, ":")) {
        while (h < end && !ts.isPunct(h, "{") && !ts.isPunct(h, ";") && !ts.isPunct(h, "}")) {
            if (ts.isOpener(h))
                h = ts.after(h);
            else
                ++h;
        }
    }
    if (h >= end || !ts.isPunct(h, "{"))
        return parseSimple(pos, end, out);
    const int b = qMin(ts.after(h), end);
    bool missing = false;
    const int e = findStatementEnd(b, end, &missing);
    Node *n = make(Node::LocalType, pos, e);
    n->expr = Range(b, e);
    out.append(n);
    return (!missing && e < end) ? e + 1 : e;
}

int StatementParser::skipStatement(int pos, int end) const
{
    QVector<char> pending; // 'i': if waiting for else, 'd': do waiting for while (...);
    int p = pos;
    for (;;) {
        if (p >= end)
            return end;
        const Token &t = ts.at(p);
        int stmtEnd = -1;
        if (t.isPunct("{")) {
            stmtEnd = qMin(ts.after(p), end);
        } else if (t.isKeyword("if")) {
            ++p;
            if (ts.isKeyword(p, "constexpr"))
                ++p;
            if (ts.isPunct(p, "!"))
                ++p;
            if (ts.isKeyword(p, "consteval"))
                ++p;
            else if (ts.isPunct(p, "("))
                p = qMin(ts.after(p), end);
            pending.append('i');
            continue;
        } else if (t.isKeyword("while") || t.isKeyword("for") || t.isKeyword("switch")) {
            ++p;
            if (ts.isPunct(p, "("))
                p = qMin(ts.after(p), end);
            continue;
        } else if (t.isKeyword("do")) {
            ++p;
            pending.append('d');
            continue;
        } else if (t.isKeyword("else")) {
            ++p;
            continue;
        } else if (t.isKeyword("case")) {
            const int c = findLabelColon(p + 1, end);
            if (c >= 0) {
                p = c + 1;
                continue;
            }
            stmtEnd = recover(p + 1, end);
        } else if (t.isKeyword("default") && ts.isPunct(p + 1, ":")) {
            p += 2;
            continue;
        } else if (t.isIdentifier() && ts.isPunct(p + 1, ":")) {
            p += 2;
            continue;
        } else if (t.isIdentifier() && (t.is("foreach") || t.is("Q_FOREACH")) && ts.isPunct(p + 1, "(")) {
            p = qMin(ts.after(p + 1), end); // loop head, the body follows
            continue;
        } else if (t.isIdentifier() && (t.is("forever") || t.is("Q_FOREVER")) && ts.isPunct(p + 1, "{")) {
            ++p;
            continue;
        } else if (t.isIdentifier() && ts.isPunct(p + 1, "(") && ts.isPunct(ts.after(p + 1), "{")) {
            stmtEnd = qMin(ts.after(p + 1), end); // MACRO(args) before a block
        } else if (t.isKeyword("try")) {
            ++p;
            if (ts.isPunct(p, "{"))
                p = qMin(ts.after(p), end);
            while (p < end && ts.isKeyword(p, "catch")) {
                ++p;
                if (ts.isPunct(p, "("))
                    p = qMin(ts.after(p), end);
                if (!ts.isPunct(p, "{"))
                    break;
                p = qMin(ts.after(p), end);
            }
            stmtEnd = p;
        } else if (t.kind == Token::Preprocessor) {
            stmtEnd = p + 1;
        } else {
            bool missing = false;
            const int e = findStatementEnd(p, end, &missing);
            stmtEnd = (!missing && e < end) ? e + 1 : e;
        }
        p = qMax(stmtEnd, p + 1);

        bool resumed = false;
        while (!pending.isEmpty()) {
            const char c = pending.takeLast();
            if (c == 'i') {
                if (p < end && ts.isKeyword(p, "else")) {
                    ++p;
                    resumed = true;
                    break;
                }
            } else if (p < end && ts.isKeyword(p, "while")) {
                ++p;
                if (ts.isPunct(p, "("))
                    p = qMin(ts.after(p), end);
                if (p < end && ts.isPunct(p, ";"))
                    ++p;
            }
        }
        if (!resumed)
            return qMin(p, end);
    }
}

} // namespace cimport
} // namespace afce
