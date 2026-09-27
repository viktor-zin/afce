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

#include "importbuilder.h"

#include "afceutil.h"

#include <QCoreApplication>
#include <QSet>

#include <deque>

namespace afce {
namespace cimport {

namespace {

const QString kNewlineItem = QStringLiteral("\"\\n\"");

// Replaces characters that are not allowed in XML 1.0: control characters
// (they only occur inside literals) by spaces, non-characters and unpaired
// surrogates by U+FFFD.
QString sanitized(const QString &value)
{
    QString s = value;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        const ushort u = c.unicode();
        if (u < 0x20 && u != '\t' && u != '\n' && u != '\r') {
            s[i] = QLatin1Char(' ');
        } else if (u == 0xFFFE || u == 0xFFFF) {
            s[i] = QChar(QChar::ReplacementCharacter);
        } else if (c.isHighSurrogate()) {
            if (i + 1 < s.size() && s.at(i + 1).isLowSurrogate())
                ++i;
            else
                s[i] = QChar(QChar::ReplacementCharacter);
        } else if (c.isLowSurrogate()) {
            s[i] = QChar(QChar::ReplacementCharacter);
        }
    }
    return s;
}

// Whether a literal's content ends with an unescaped "\n".
bool endsWithNewlineEscape(const QString &s)
{
    if (s.size() < 2 || s.at(s.size() - 1) != QLatin1Char('n'))
        return false;
    int backslashes = 0;
    for (int i = s.size() - 2; i >= 0 && s.at(i) == QLatin1Char('\\'); --i)
        ++backslashes;
    return backslashes % 2 == 1;
}

bool parseInteger(QString s, qlonglong *value)
{
    s.remove(QLatin1Char('\''));
    while (!s.isEmpty()) {
        const QChar c = s.at(s.size() - 1).toLower();
        if (c == QLatin1Char('u') || c == QLatin1Char('l') || c == QLatin1Char('z'))
            s.chop(1);
        else
            break;
    }
    if (s.isEmpty())
        return false;
    bool ok = false;
    if (s.startsWith(QLatin1String("0x")) || s.startsWith(QLatin1String("0X")))
        *value = s.mid(2).toLongLong(&ok, 16);
    else if (s.startsWith(QLatin1String("0b")) || s.startsWith(QLatin1String("0B")))
        *value = s.mid(2).toLongLong(&ok, 2);
    else if (s.size() > 1 && s.startsWith(QLatin1Char('0')))
        *value = s.toLongLong(&ok, 8);
    else
        *value = s.toLongLong(&ok, 10);
    return ok;
}

bool isManipulatorName(const QString &name)
{
    static const QSet<QString> names = [] {
        QSet<QString> s;
        for (const char *w : {"flush", "ends", "setw", "setprecision", "setfill", "setbase", "setiosflags",
                              "resetiosflags", "fixed", "scientific", "hexfloat", "defaultfloat", "left", "right",
                              "internal", "boolalpha", "noboolalpha", "showbase", "noshowbase", "showpoint",
                              "noshowpoint", "showpos", "noshowpos", "uppercase", "nouppercase", "hex", "dec", "oct",
                              "unitbuf", "nounitbuf", "skipws", "noskipws", "ws", "put_money", "put_time", "quoted"})
            s.insert(QString::fromLatin1(w));
        return s;
    }();
    return names.contains(name);
}

} // namespace

FlowchartBuilder::FlowchartBuilder(const TokenStream &ts, const ImportOptions &options, QList<Diagnostic> &diagnostics)
    : ts(ts)
    , options(options)
    , diags(diagnostics)
{
}

// ---------------------------------------------------------------------------
// Emission helpers

QDomElement FlowchartBuilder::element(const char *tag)
{
    return doc.createElement(QString::fromLatin1(tag));
}

void FlowchartBuilder::setAttr(QDomElement &e, const char *name, const QString &value)
{
    e.setAttribute(QString::fromLatin1(name), sanitized(value));
}

void FlowchartBuilder::addProcess(QDomElement &branch, const QString &text)
{
    if (text.isEmpty())
        return;
    QDomElement e = element("process");
    setAttr(e, "text", text);
    branch.appendChild(e);
}

void FlowchartBuilder::addCall(QDomElement &branch, const QString &text)
{
    if (text.isEmpty())
        return;
    QDomElement e = element("call");
    setAttr(e, "text", text);
    branch.appendChild(e);
}

void FlowchartBuilder::addAssign(QDomElement &branch, const QString &dest, const QString &src)
{
    QDomElement e = element("assign");
    setAttr(e, "dest", dest);
    setAttr(e, "src", src);
    branch.appendChild(e);
}

void FlowchartBuilder::addList(QDomElement &branch, const char *tag, const QStringList &items)
{
    QDomElement e = element(tag);
    setAttr(e, "vars", afce::joinList(items));
    branch.appendChild(e);
}

QDomElement FlowchartBuilder::addLoop(QDomElement &branch, const char *tag)
{
    QDomElement e = element(tag);
    branch.appendChild(e);
    return e;
}

void FlowchartBuilder::diag(Diagnostic::Severity severity, int token, const QString &message)
{
    Diagnostic d;
    d.severity = severity;
    const Token &t = ts.at(token);
    if (t.kind != Token::End) {
        d.line = t.line;
        d.column = t.column;
    }
    d.message = message;
    diags.append(d);
}

// ---------------------------------------------------------------------------

QDomDocument FlowchartBuilder::build(const FunctionDef &function)
{
    def = &function;
    doc = QDomDocument(QStringLiteral("AFC"));
    warnedGoto = false;
    warnedPreprocessor = false;
    variableCache.clear();

    QDomElement algorithm = element("algorithm");
    setAttr(algorithm, "version", QStringLiteral("1.3"));
    if (!function.fragment) {
        const QString params = function.info.parameters;
        const bool plainMain = function.isMain && (params.isEmpty() || params == QLatin1String("void"));
        if (!plainMain) {
            setAttr(algorithm, "name", function.info.name);
            if (!params.isEmpty())
                setAttr(algorithm, "params", params);
            if (!function.info.returnType.isEmpty())
                setAttr(algorithm, "returns", function.info.returnType);
        }
    }
    doc.appendChild(algorithm);
    QDomElement body = element("branch");
    algorithm.appendChild(body);

    if (function.functionTryBlock) {
        diag(Diagnostic::Warning, function.bodyBegin - 1,
             QCoreApplication::translate("afce::CodeImporter",
                 "Exception handlers are not imported; only the try block is shown"));
    }

    std::deque<Node> arena;
    StatementParser parser(ts, arena);
    const QVector<Node *> statements = parser.parse(function.bodyBegin, function.bodyEnd);
    plainDeclarations.clear();
    loopScopes.clear();
    for (int k = function.paramBegin; k < function.paramEnd; ++k) {
        if (ts.at(k).isIdentifier())
            ++plainDeclarations[ts.at(k).text];
    }
    collectDeclarations(statements);
    buildList(statements, body, true);
    def = nullptr;
    return doc;
}

void FlowchartBuilder::buildList(const QVector<Node *> &list, QDomElement &branch, bool functionTop)
{
    int skip = -1;
    if (functionTop && def && def->isMain && options.omitMainReturn) {
        for (int i = list.size() - 1; i >= 0; --i) {
            const Node *n = list.at(i);
            if (n->kind == Node::Preprocessor || n->kind == Node::Skip || n->kind == Node::GotoLabel)
                continue;
            if (n->kind == Node::Return) {
                const QString value = compactText(ts, n->expr);
                if (value == QLatin1String("0") || value == QLatin1String("(0)")
                    || value == QLatin1String("EXIT_SUCCESS")) {
                    skip = i;
                }
            }
            break;
        }
    }
    for (int i = 0; i < list.size(); ++i) {
        if (i != skip)
            buildNode(list.at(i), branch);
    }
}

void FlowchartBuilder::buildNode(const Node *n, QDomElement &branch)
{
    // Nodes with children are built by separate functions with small stack
    // frames: this function is on the recursion path (nesting depth).
    switch (n->kind) {
    case Node::If:
        buildIf(n, branch);
        break;
    case Node::While:
    case Node::DoWhile:
    case Node::Forever:
        buildLoop(n, branch);
        break;
    case Node::For:
        buildFor(n, branch);
        break;
    case Node::RangeFor:
        buildRangeFor(n, branch);
        break;
    case Node::Switch:
        buildSwitch(n, branch);
        break;
    case Node::Try:
        buildTry(n, branch);
        break;
    default:
        buildLeaf(n, branch);
        break;
    }
}

void FlowchartBuilder::buildIf(const Node *n, QDomElement &branch)
{
    buildList(n->initStmts, branch);
    QDomElement e = element("if");
    setAttr(e, "cond", text(n->expr));
    QDomElement yes = element("branch");
    QDomElement no = element("branch");
    buildList(n->children, yes);
    buildList(n->elseChildren, no);
    e.appendChild(yes);
    e.appendChild(no);
    branch.appendChild(e);
}

void FlowchartBuilder::buildLoop(const Node *n, QDomElement &branch)
{
    QDomElement e = addLoop(branch, n->kind == Node::DoWhile ? "post" : "pre");
    if (n->kind == Node::Forever)
        setAttr(e, "cond", ts.language() == Lang::C ? QStringLiteral("1") : QStringLiteral("true"));
    else
        setAttr(e, "cond", text(n->expr));
    if (n->missingSemicolon)
        noteMissingSemicolon(n);
    QDomElement body = element("branch");
    buildList(n->children, body);
    e.appendChild(body);
}

void FlowchartBuilder::buildRangeFor(const Node *n, QDomElement &branch)
{
    buildList(n->initStmts, branch);
    QDomElement e = addLoop(branch, "foreach");
    setAttr(e, "var", text(n->init));
    setAttr(e, "range", text(n->step));
    QDomElement body = element("branch");
    buildList(n->children, body);
    e.appendChild(body);
}

void FlowchartBuilder::buildTry(const Node *n, QDomElement &branch)
{
    diag(Diagnostic::Warning, n->range.begin,
         QCoreApplication::translate("afce::CodeImporter",
             "Exception handlers are not imported; only the try block is shown"));
    buildList(n->children, branch);
}

void FlowchartBuilder::noteMissingSemicolon(const Node *n)
{
    diag(Diagnostic::Error, n->range.end > n->range.begin ? n->range.end - 1 : n->range.begin,
         QCoreApplication::translate("afce::CodeImporter", "Missing ';'"));
}

void FlowchartBuilder::buildLeaf(const Node *n, QDomElement &branch)
{
    if (n->missingSemicolon)
        noteMissingSemicolon(n);
    switch (n->kind) {
    case Node::Expression:
        if (n->macroBlock) {
            diag(Diagnostic::Warning, n->range.begin,
                 QCoreApplication::translate("afce::CodeImporter",
                     "Unknown macro '%1' before a block: the block is imported as ordinary statements")
                     .arg(ts.at(n->range.begin).text));
        }
        mapExpressionStatement(n->expr, branch);
        break;
    case Node::Declaration:
        mapDeclaration(n->expr, branch);
        break;
    case Node::CaseLabel:
    case Node::DefaultLabel:
        diag(Diagnostic::Warning, n->range.begin,
             QCoreApplication::translate("afce::CodeImporter", "A case label outside of a switch statement was ignored"));
        break;
    case Node::GotoLabel:
    case Node::Goto:
        if (!warnedGoto) {
            warnedGoto = true;
            diag(Diagnostic::Warning, n->range.begin,
                 QCoreApplication::translate("afce::CodeImporter",
                     "goto and labels cannot be shown in a flowchart: labels are dropped and goto is imported as text"));
        }
        if (n->kind == Node::Goto)
            addProcess(branch, text(n->range));
        break;
    case Node::Return: {
        QDomElement e = element("return");
        const QString value = text(n->expr);
        if (!value.isEmpty())
            setAttr(e, "value", value);
        branch.appendChild(e);
        break;
    }
    case Node::Break:
        branch.appendChild(element("break"));
        break;
    case Node::Continue:
        branch.appendChild(element("continue"));
        break;
    case Node::Asm:
        addProcess(branch, text(n->range));
        break;
    case Node::Skip:
        diag(Diagnostic::Info, n->range.begin,
             QCoreApplication::translate("afce::CodeImporter", "Skipped: %1").arg(text(n->range)));
        break;
    case Node::LocalType:
        if (n->expr.isEmpty()) {
            diag(Diagnostic::Info, n->range.begin,
                 QCoreApplication::translate("afce::CodeImporter", "Local type definition skipped"));
        } else {
            addProcess(branch, text(n->range));
        }
        break;
    case Node::Preprocessor:
        if (!warnedPreprocessor) {
            warnedPreprocessor = true;
            diag(Diagnostic::Warning, n->range.begin,
                 QCoreApplication::translate("afce::CodeImporter",
                     "Preprocessor directives inside a function are ignored; the code of all conditional branches is imported"));
        }
        break;
    case Node::Error:
        diag(Diagnostic::Error, n->range.begin,
             QCoreApplication::translate("afce::CodeImporter", "Could not parse the statement; it is imported as text"));
        addProcess(branch, text(n->range));
        break;
    case Node::Stray:
        diag(Diagnostic::Error, n->range.begin,
             QCoreApplication::translate("afce::CodeImporter", "Unexpected '%1'").arg(text(n->range)));
        break;
    case Node::TooDeep: {
        diag(Diagnostic::Warning, n->range.begin,
             QCoreApplication::translate("afce::CodeImporter", "The nesting is too deep; the statement is imported as text"));
        // Only pathological code gets here; keep the block displayable.
        const int maxLength = 4000;
        QString t = text(n->range);
        if (t.size() > maxLength)
            t = t.left(maxLength) + QStringLiteral(" ...");
        addProcess(branch, t);
        break;
    }
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Loops

void FlowchartBuilder::buildForC(const Node *n, QDomElement &branch)
{
    QDomElement e = addLoop(branch, "forc");
    setAttr(e, "init", text(n->init));
    setAttr(e, "cond", text(n->expr));
    setAttr(e, "step", text(n->step));
    QDomElement body = element("branch");
    buildList(n->children, body);
    e.appendChild(body);
}

void FlowchartBuilder::buildFor(const Node *n, QDomElement &branch)
{
    switch (options.forStyle) {
    case ImportOptions::ForStyle::CStyle:
        buildForC(n, branch);
        return;
    case ImportOptions::ForStyle::While:
        buildForWhile(n, branch);
        return;
    case ImportOptions::ForStyle::Arithmetic:
        buildForArithmetic(n, branch);
        return;
    }
}

bool FlowchartBuilder::whileStyleAllowed(const Node *n)
{
    if (containsJump(n->children, Node::Continue)) {
        diag(Diagnostic::Info, n->range.begin,
             QCoreApplication::translate("afce::CodeImporter",
                 "The loop body contains 'continue'; the for loop is kept as a C-style loop"));
        return false;
    }
    if (hoistingChangesScope(n)) {
        diag(Diagnostic::Info, n->range.begin,
             QCoreApplication::translate("afce::CodeImporter",
                 "The loop variable is declared or used elsewhere in the function; the for loop is kept as a C-style loop"));
        return false;
    }
    return true;
}

void FlowchartBuilder::buildForWhile(const Node *n, QDomElement &branch)
{
    if (!whileStyleAllowed(n)) {
        buildForC(n, branch);
        return;
    }
    buildList(n->initStmts, branch);
    QDomElement e = addLoop(branch, "pre");
    if (n->expr.isEmpty())
        setAttr(e, "cond", ts.language() == Lang::C ? QStringLiteral("1") : QStringLiteral("true"));
    else
        setAttr(e, "cond", text(n->expr));
    QDomElement body = element("branch");
    buildList(n->children, body);
    if (!terminates(n->children))
        mapExpressionStatement(n->step, body);
    e.appendChild(body);
}

void FlowchartBuilder::buildForArithmetic(const Node *n, QDomElement &branch)
{
    QString var;
    QString from;
    QString to;
    if (!canonicalFor(n, &var, &from, &to)) {
        buildForC(n, branch);
        return;
    }
    QDomElement e = addLoop(branch, "for");
    setAttr(e, "var", var);
    setAttr(e, "from", from);
    setAttr(e, "to", to);
    QDomElement body = element("branch");
    buildList(n->children, body);
    e.appendChild(body);
}

bool FlowchartBuilder::canonicalFor(const Node *n, QString *var, QString *from, QString *to) const
{
    // init: T v = A (one declarator, declared here)
    if (n->init.isEmpty() || !looksLikeDeclaration(ts, n->init))
        return false;
    const Declaration d = parseDeclaration(ts, n->init);
    if (!d.ok || d.declarators.size() != 1)
        return false;
    const Declarator &dc = d.declarators.first();
    if (dc.init != Declarator::Assign || !ts.at(dc.range.begin).isIdentifier() || !ts.isPunct(dc.range.begin + 1, "="))
        return false;
    for (int k = d.type.begin; k < d.type.end; ++k) {
        if (ts.at(k).is("float") || ts.at(k).is("double"))
            return false;
    }
    if (dc.initializer.size() == 1 && ts.at(dc.initializer.begin).kind == Token::Number) {
        const QString a = ts.at(dc.initializer.begin).text;
        if (!a.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)
            && (a.contains(QLatin1Char('.')) || a.contains(QLatin1Char('e'), Qt::CaseInsensitive))) {
            return false;
        }
    }
    const QString v = dc.name;

    // cond: v < B | v <= B | B > v | B >= v
    const Range c = n->expr;
    if (c.size() < 3)
        return false;
    bool inclusive = false;
    Range bound;
    const Token &first = ts.at(c.begin);
    const Token &last = ts.at(c.end - 1);
    if (first.isIdentifier() && first.text == v && (ts.isPunct(c.begin + 1, "<") || ts.isPunct(c.begin + 1, "<="))) {
        inclusive = ts.isPunct(c.begin + 1, "<=");
        bound = Range(c.begin + 2, c.end);
    } else if (last.isIdentifier() && last.text == v && (ts.isPunct(c.end - 2, ">") || ts.isPunct(c.end - 2, ">="))) {
        inclusive = ts.isPunct(c.end - 2, ">=");
        bound = Range(c.begin, c.end - 2);
    } else {
        return false;
    }
    if (bound.isEmpty())
        return false;
    // The bound must bind tighter than a relational operator.
    for (int k = bound.begin; k < bound.end;) {
        const Token &t = ts.at(k);
        if (ts.isOpener(k)) {
            k = ts.after(k);
            continue;
        }
        if (t.isIdentifier() && ts.isPunct(k + 1, "<")) {
            const int e = qualifiedNameEnd(ts, k, bound.end, true);
            if (e > k + 1) {
                k = e;
                continue;
            }
        }
        if (t.kind == Token::Punct
            && (t.is("<") || t.is(">") || t.is("<=") || t.is(">=") || t.is("==") || t.is("!=") || t.is("<=>")
                || t.is("&") || t.is("^") || t.is("|") || t.is("&&") || t.is("||") || t.is("?") || t.is(":")
                || t.is(",") || isAssignmentOp(t))) {
            return false;
        }
        if (t.kind == Token::Keyword && (t.is("and") || t.is("or") || t.is("not") || t.is("bitand") || t.is("bitor")))
            return false;
        if (t.isIdentifier() && t.text == v)
            return false;
        if (t.kind == Token::Number && !t.text.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)
            && (t.text.contains(QLatin1Char('.')) || t.text.contains(QLatin1Char('e'), Qt::CaseInsensitive))) {
            return false; // floating-point bound
        }
        ++k;
    }

    // step: v++ | ++v | v += 1 | v = v + 1
    const Range s = n->step;
    auto isVar = [&](int k) { return ts.at(k).isIdentifier() && ts.at(k).text == v; };
    auto isOne = [&](int k) { return ts.at(k).kind == Token::Number && ts.at(k).text == QLatin1String("1"); };
    bool stepOk = false;
    if (s.size() == 2)
        stepOk = (isVar(s.begin) && ts.isPunct(s.begin + 1, "++")) || (ts.isPunct(s.begin, "++") && isVar(s.begin + 1));
    else if (s.size() == 3)
        stepOk = isVar(s.begin) && ts.isPunct(s.begin + 1, "+=") && isOne(s.begin + 2);
    else if (s.size() == 5)
        stepOk = isVar(s.begin) && ts.isPunct(s.begin + 1, "=") && isVar(s.begin + 2) && ts.isPunct(s.begin + 3, "+")
            && isOne(s.begin + 4);
    if (!stepOk)
        return false;

    if (modifies(v, n->body))
        return false;

    *var = v;
    *from = text(dc.initializer);
    *to = inclusive ? text(bound) : boundMinusOne(bound);
    return true;
}

QString FlowchartBuilder::boundMinusOne(Range bound) const
{
    const bool negative = bound.size() == 2 && ts.isPunct(bound.begin, "-");
    const int k = negative ? bound.begin + 1 : bound.begin;
    if ((bound.size() == 1 || negative) && ts.at(k).kind == Token::Number) {
        qlonglong value = 0;
        if (parseInteger(ts.at(k).text, &value))
            return QString::number((negative ? -value : value) - 1);
    }
    if (isPrimaryExpression(ts, bound))
        return text(bound) + QLatin1String(" - 1");
    // X + 1 -> X
    if (bound.size() >= 3 && ts.isPunct(bound.end - 2, "+") && ts.at(bound.end - 1).kind == Token::Number
        && ts.at(bound.end - 1).text == QLatin1String("1") && isPrimaryExpression(ts, Range(bound.begin, bound.end - 2))) {
        return text(Range(bound.begin, bound.end - 2));
    }
    return QLatin1Char('(') + text(bound) + QLatin1String(") - 1");
}

QStringList FlowchartBuilder::declaredNames(const QVector<Node *> &list) const
{
    QStringList names;
    for (const Node *n : list) {
        if (n->kind != Node::Declaration)
            continue;
        const Declaration d = parseDeclaration(ts, n->expr);
        for (const Declarator &dc : d.declarators) {
            if (dc.init == Declarator::Invalid)
                continue;
            if (dc.name.startsWith(QLatin1Char('['))) {
                // structured binding: every identifier between the brackets
                for (int k = dc.range.begin; k < dc.range.end && !ts.isPunct(k, "]"); ++k) {
                    if (ts.at(k).isIdentifier())
                        names << ts.at(k).text;
                }
            } else if (!dc.name.isEmpty()) {
                names << dc.name;
            }
        }
    }
    return names;
}

void FlowchartBuilder::collectDeclarations(const QVector<Node *> &list)
{
    for (const QString &name : declaredNames(list))
        ++plainDeclarations[name];
    for (const Node *n : list) {
        if (n->kind == Node::For) {
            for (const QString &name : declaredNames(n->initStmts))
                loopScopes[name].append(Range(n->range.begin, n->range.end));
        } else if (n->kind == Node::RangeFor) {
            collectDeclarations(n->initStmts);
            if (looksLikeDeclaration(ts, n->init)) {
                const Declaration d = parseDeclaration(ts, n->init);
                for (const Declarator &dc : d.declarators) {
                    if (!dc.name.isEmpty())
                        loopScopes[dc.name].append(Range(n->range.begin, n->range.end));
                }
            }
        } else {
            collectDeclarations(n->initStmts);
        }
        collectDeclarations(n->children);
        collectDeclarations(n->elseChildren);
    }
}

bool FlowchartBuilder::hoistingChangesScope(const Node *n) const
{
    const QStringList names = declaredNames(n->initStmts);
    if (names.isEmpty() || !def)
        return false;
    for (const QString &name : names) {
        // another variable with this name would be overwritten
        if (plainDeclarations.value(name) > 0)
            return true;
        const QVector<Range> scopes = loopScopes.value(name);
        // round-trip mode: the declaration itself is kept, a second one in the
        // same scope would not compile
        if (options.keepDeclarations && scopes.size() > 1)
            return true;
        // a use after the loop that is not inside another loop declaring the
        // name refers to a different (outer) variable
        for (int k = n->range.end; k < def->bodyEnd; ++k) {
            const Token &t = ts.at(k);
            if (!t.isIdentifier() || t.text != name || ts.isPunct(k - 1, ".") || ts.isPunct(k - 1, "->")
                || ts.isPunct(k - 1, "::")) {
                continue;
            }
            bool covered = false;
            for (const Range &s : scopes) {
                if (s.begin != n->range.begin && s.begin <= k && k < s.end)
                    covered = true;
            }
            if (!covered)
                return true;
        }
    }
    return false;
}

bool FlowchartBuilder::modifies(const QString &var, Range body) const
{
    for (int k = body.begin; k < body.end; ++k) {
        const Token &t = ts.at(k);
        if (!t.isIdentifier() || t.text != var)
            continue;
        const Token &prev = ts.at(k - 1);
        if (prev.isPunct(".") || prev.isPunct("->") || prev.isPunct("::"))
            continue;
        const Token &next = ts.at(k + 1);
        if (isAssignmentOp(next) || next.isPunct("++") || next.isPunct("--"))
            return true;
        if (prev.isPunct("++") || prev.isPunct("--") || prev.isPunct("&") || prev.isPunct(">>"))
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// switch

bool FlowchartBuilder::rangeContainsKeyword(Range r, const char *keyword) const
{
    for (int k = r.begin; k < r.end; ++k) {
        if (ts.isKeyword(k, keyword))
            return true;
    }
    return false;
}

bool FlowchartBuilder::containsJump(const QVector<Node *> &list, Node::Kind jump) const
{
    // Looks for a break / continue that targets the enclosing construct.
    const char *keyword = jump == Node::Break ? "break" : "continue";
    for (const Node *n : list) {
        switch (n->kind) {
        case Node::Break:
        case Node::Continue:
            if (n->kind == jump)
                return true;
            break;
        case Node::If:
            if (containsJump(n->children, jump) || containsJump(n->elseChildren, jump))
                return true;
            break;
        case Node::Try:
            if (containsJump(n->children, jump))
                return true;
            break;
        case Node::Switch:
            // break inside a switch targets the switch, continue passes through it
            if (jump == Node::Continue) {
                if (n->nestedLabels ? rangeContainsKeyword(n->range, keyword) : containsJump(n->children, jump))
                    return true;
            }
            break;
        case Node::TooDeep:
        case Node::Error:
            if (rangeContainsKeyword(n->range, keyword))
                return true;
            break;
        default:
            break;
        }
    }
    return false;
}

bool FlowchartBuilder::terminates(const QVector<Node *> &list) const
{
    for (int i = list.size() - 1; i >= 0; --i) {
        const Node *n = list.at(i);
        switch (n->kind) {
        case Node::Preprocessor:
        case Node::Skip:
        case Node::GotoLabel:
            continue;
        case Node::Break:
        case Node::Continue:
        case Node::Return:
        case Node::Goto:
            return true;
        case Node::Expression:
            return ts.isKeyword(n->expr.begin, "throw");
        case Node::If:
            return n->hasElse && terminates(n->children) && terminates(n->elseChildren);
        case Node::Try:
            return terminates(n->children);
        default:
            return false;
        }
    }
    return false;
}

FlowchartBuilder::SwitchPlan FlowchartBuilder::planSwitch(const Node *n)
{
    SwitchPlan plan;
    if (n->nestedLabels) {
        diag(Diagnostic::Warning, n->range.begin,
             QCoreApplication::translate("afce::CodeImporter",
                 "A switch with case labels inside nested statements cannot be shown as a flowchart; it is imported as text"));
        plan.mode = SwitchPlan::AsText;
        return plan;
    }

    struct Group
    {
        QStringList values;
        bool isDefault = false;
        int token = 0;
        QVector<Node *> statements;
    };
    QVector<Group> groups;
    int dropped = -1;
    for (Node *c : n->children) {
        if (c->kind == Node::CaseLabel || c->kind == Node::DefaultLabel) {
            QString value;
            if (c->kind == Node::CaseLabel) {
                value = text(c->expr);
                if (value.isEmpty()) {
                    diag(Diagnostic::Error, c->range.begin,
                         QCoreApplication::translate("afce::CodeImporter", "Empty case label ignored"));
                    continue;
                }
            }
            if (groups.isEmpty() || !groups.last().statements.isEmpty()) {
                groups.append(Group());
                groups.last().token = c->range.begin;
            }
            if (c->kind == Node::DefaultLabel)
                groups.last().isDefault = true;
            else
                groups.last().values << value;
            continue;
        }
        if (groups.isEmpty()) {
            if (c->kind != Node::Preprocessor && dropped < 0)
                dropped = c->range.begin;
            continue;
        }
        groups.last().statements.append(c);
    }
    if (dropped >= 0) {
        diag(Diagnostic::Warning, dropped,
             QCoreApplication::translate("afce::CodeImporter",
                 "Statements before the first case label are never executed and were dropped"));
    }

    // Trailing break removal and termination
    QVector<QVector<Node *>> stripped(groups.size());
    QVector<bool> terminated(groups.size(), false);
    for (int i = 0; i < groups.size(); ++i) {
        QVector<Node *> list = groups.at(i).statements;
        int lastSignificant = list.size() - 1;
        while (lastSignificant >= 0 && list.at(lastSignificant)->kind == Node::Preprocessor)
            --lastSignificant;
        if (lastSignificant >= 0 && list.at(lastSignificant)->kind == Node::Break) {
            list.remove(lastSignificant);
            terminated[i] = true;
        } else {
            terminated[i] = terminates(list);
        }
        stripped[i] = list;
    }

    int defaultIndex = -1;
    for (int i = 0; i < groups.size(); ++i) {
        if (!groups.at(i).isDefault)
            continue;
        if (defaultIndex < 0) {
            defaultIndex = i;
        } else {
            diag(Diagnostic::Error, groups.at(i).token,
                 QCoreApplication::translate("afce::CodeImporter", "Duplicate default label ignored"));
            groups[i].isDefault = false;
        }
    }

    // Fall-through: append the statements of the following groups (in source
    // order) until one of them terminates.
    QVector<QVector<Node *>> effective(groups.size());
    for (int i = 0; i < groups.size(); ++i) {
        effective[i] = stripped.at(i);
        if (terminated.at(i) || i + 1 >= groups.size())
            continue;
        const bool emitted = groups.at(i).isDefault || !groups.at(i).values.isEmpty();
        if (emitted) {
            diag(Diagnostic::Info, groups.at(i).token,
                 QCoreApplication::translate("afce::CodeImporter",
                     "The case falls through into the next one; the statements of the following cases are repeated in its branch"));
        }
        for (int j = i + 1; j < groups.size(); ++j) {
            effective[i] += stripped.at(j);
            if (terminated.at(j))
                break;
        }
    }

    QVector<int> cases;
    for (int i = 0; i < groups.size(); ++i) {
        if (!groups.at(i).isDefault && !groups.at(i).values.isEmpty())
            cases.append(i);
    }
    if (cases.isEmpty()) {
        if (defaultIndex < 0)
            return plan; // nothing to show
        plan.otherwise = effective.at(defaultIndex);
        if (containsJump(plan.otherwise, Node::Break)) {
            diag(Diagnostic::Warning, n->range.begin,
                 QCoreApplication::translate("afce::CodeImporter",
                     "A switch with only a default label that uses 'break' is imported as text"));
            plan.mode = SwitchPlan::AsText;
            return plan;
        }
        plan.mode = SwitchPlan::Inline;
        return plan;
    }
    plan.mode = SwitchPlan::Case;
    for (int i : std::as_const(cases)) {
        plan.values.append(groups.at(i).values);
        plan.bodies.append(effective.at(i));
    }
    if (defaultIndex >= 0)
        plan.otherwise = effective.at(defaultIndex);
    return plan;
}

void FlowchartBuilder::buildSwitch(const Node *n, QDomElement &branch)
{
    buildList(n->initStmts, branch);
    const SwitchPlan plan = planSwitch(n);
    switch (plan.mode) {
    case SwitchPlan::Nothing:
        return;
    case SwitchPlan::AsText:
        addProcess(branch, text(n->range));
        return;
    case SwitchPlan::Inline:
        buildList(plan.otherwise, branch);
        return;
    case SwitchPlan::Case:
        break;
    }
    QDomElement e = element("case");
    setAttr(e, "expr", text(n->expr));
    for (int i = 0; i < plan.bodies.size(); ++i) {
        QDomElement b = element("branch");
        setAttr(b, "value", afce::joinList(plan.values.at(i)));
        buildList(plan.bodies.at(i), b);
        e.appendChild(b);
    }
    QDomElement otherwise = element("branch");
    buildList(plan.otherwise, otherwise);
    e.appendChild(otherwise);
    branch.appendChild(e);
}

// ---------------------------------------------------------------------------
// Expression statements

void FlowchartBuilder::mapExpressionStatement(Range r, QDomElement &branch)
{
    if (r.isEmpty())
        return;
    const QVector<Range> parts = splitTopLevel(ts, r, ",");
    for (const Range &p : parts) {
        if (!p.isEmpty())
            mapSingleExpression(p, branch);
    }
}

void FlowchartBuilder::mapSingleExpression(Range r, QDomElement &branch)
{
    if (options.detectIO && (tryInput(r, branch) || tryOutput(r, branch)))
        return;
    if (tryAssignment(r, branch))
        return;
    if (tryIncrement(r, branch))
        return;
    if (options.callsAsSubroutine && parseCall(ts, r, nullptr, nullptr)) {
        addCall(branch, text(r));
        return;
    }
    addProcess(branch, text(r));
}

bool FlowchartBuilder::tryAssignment(Range r, QDomElement &branch)
{
    const int k = findAssignment(ts, r);
    if (k < 0)
        return false;
    const Range dest(r.begin, k);
    const Range src(k + 1, r.end);
    if (dest.isEmpty() || src.isEmpty())
        return false;
    if (findAssignment(ts, src) >= 0) {
        addProcess(branch, text(r)); // chained assignment
        return true;
    }
    const Token &op = ts.at(k);
    if (op.is("=")) {
        addAssign(branch, text(dest), text(src));
        return true;
    }
    if (!options.expandCompoundAssign || containsPunct(ts, dest, "++") || containsPunct(ts, dest, "--"))
        return false;
    QString binop = op.text;
    binop.chop(1);
    QString e = text(src);
    if (!isPrimaryExpression(ts, src))
        e = QLatin1Char('(') + e + QLatin1Char(')');
    const QString d = text(dest);
    addAssign(branch, d, d + QLatin1Char(' ') + binop + QLatin1Char(' ') + e);
    return true;
}

bool FlowchartBuilder::tryIncrement(Range r, QDomElement &branch)
{
    if (r.size() < 2)
        return false;
    Range operand;
    bool increment = false;
    if (ts.isPunct(r.begin, "++") || ts.isPunct(r.begin, "--")) {
        operand = Range(r.begin + 1, r.end);
        increment = ts.isPunct(r.begin, "++");
    } else if (ts.isPunct(r.end - 1, "++") || ts.isPunct(r.end - 1, "--")) {
        operand = Range(r.begin, r.end - 1);
        increment = ts.isPunct(r.end - 1, "++");
    } else {
        return false;
    }
    if (!options.expandCompoundAssign || !isPrimaryExpression(ts, operand) || containsPunct(ts, operand, "++")
        || containsPunct(ts, operand, "--")) {
        return false;
    }
    const QString v = text(operand);
    addAssign(branch, v, v + (increment ? QLatin1String(" + 1") : QLatin1String(" - 1")));
    return true;
}

int FlowchartBuilder::streamEnd(Range r, std::initializer_list<const char *> names) const
{
    if (r.isEmpty() || !(ts.at(r.begin).isIdentifier() || ts.isPunct(r.begin, "::")))
        return -1;
    const int m = qualifiedNameEnd(ts, r.begin, r.end, true);
    if (m == r.begin)
        return -1;
    QString name = compactText(ts, Range(r.begin, m));
    if (name.startsWith(QLatin1String("::")))
        name = name.mid(2);
    for (const char *s : names) {
        if (name == QLatin1String(s))
            return m;
    }
    return -1;
}

bool FlowchartBuilder::literalContent(Range r, QString *content) const
{
    if (r.isEmpty())
        return false;
    QString s;
    for (int k = r.begin; k < r.end; ++k) {
        const Token &t = ts.at(k);
        if (t.kind == Token::Preprocessor)
            continue;
        if (t.kind != Token::String || t.text.size() < 2 || !t.text.startsWith(QLatin1Char('"'))
            || !t.text.endsWith(QLatin1Char('"'))) {
            return false;
        }
        s += t.text.mid(1, t.text.size() - 2);
    }
    *content = s;
    return true;
}

QString FlowchartBuilder::withoutAddressOf(Range r) const
{
    if (r.size() > 1 && ts.isPunct(r.begin, "&"))
        return text(Range(r.begin + 1, r.end));
    return text(r);
}

bool FlowchartBuilder::isGetchar(Range r) const
{
    QString name;
    QVector<Range> args;
    if (!parseCall(ts, r, &name, &args))
        return false;
    if (name == QLatin1String("getchar") || name == QLatin1String("std::getchar"))
        return args.isEmpty();
    if (name == QLatin1String("getc") || name == QLatin1String("fgetc"))
        return args.size() == 1 && compactText(ts, args.first()) == QLatin1String("stdin");
    return false;
}

bool FlowchartBuilder::tryInput(Range r, QDomElement &branch)
{
    const int m = streamEnd(r, {"cin", "std::cin"});
    if (m >= 0) {
        if (ts.isPunct(m, ">>")) {
            const QVector<Range> items = splitTopLevel(ts, Range(m + 1, r.end), ">>");
            QStringList vars;
            for (const Range &it : items) {
                if (it.isEmpty())
                    return false;
                const QString c = compactText(ts, it);
                if (!options.exactOutput && (c == QLatin1String("ws") || c == QLatin1String("std::ws")))
                    continue;
                vars << text(it);
            }
            if (vars.isEmpty())
                return false;
            addList(branch, "io", vars);
            return true;
        }
        if (ts.isPunct(m, ".") && ts.at(m + 1).is("get") && ts.isPunct(m + 2, "(") && ts.matched(m + 2)
            && ts.after(m + 2) == r.end) {
            const Range content(m + 3, ts.contentEnd(m + 2));
            if (!content.isEmpty() && findTopLevel(ts, content, ",") < 0) {
                addList(branch, "io", QStringList() << text(content));
                return true;
            }
        }
        return false;
    }

    QString name;
    QVector<Range> args;
    if (parseCall(ts, r, &name, &args)) {
        if ((name == QLatin1String("getline") || name == QLatin1String("std::getline")) && args.size() == 2) {
            const Range stream = args.at(0);
            const QString s = compactText(ts, stream);
            if ((s == QLatin1String("cin") || s == QLatin1String("std::cin")) && !args.at(1).isEmpty()) {
                addList(branch, "io", QStringList() << text(args.at(1)));
                return true;
            }
            return false;
        }
        if (name == QLatin1String("scanf") || name == QLatin1String("scanf_s"))
            return tryScanf(r, args, 0, name == QLatin1String("scanf_s"), branch);
        if (name == QLatin1String("fscanf") && args.size() >= 2 && compactText(ts, args.first()) == QLatin1String("stdin"))
            return tryScanf(r, args, 1, false, branch);
        if (!options.exactOutput && !args.isEmpty() && !args.first().isEmpty()) {
            const bool gets = (name == QLatin1String("gets") && args.size() == 1)
                || (name == QLatin1String("gets_s") && args.size() == 2)
                || (name == QLatin1String("fgets") && args.size() == 3
                    && compactText(ts, args.at(2)) == QLatin1String("stdin"));
            if (gets) {
                addList(branch, "io", QStringList() << text(args.first()));
                return true;
            }
        }
        return false;
    }

    if (!options.exactOutput) {
        const int k = findAssignment(ts, r);
        if (k > r.begin && ts.isPunct(k, "=") && isGetchar(Range(k + 1, r.end))
            && findAssignment(ts, Range(k + 1, r.end)) < 0) {
            addList(branch, "io", QStringList() << text(Range(r.begin, k)));
            return true;
        }
    }
    return false;
}

bool FlowchartBuilder::tryScanf(Range r, const QVector<Range> &args, int formatIndex, bool secure, QDomElement &branch)
{
    if (args.size() <= formatIndex + 1)
        return false;
    QVector<Range> targets = args.mid(formatIndex + 1);
    for (const Range &t : targets) {
        if (t.isEmpty())
            return false;
    }
    QString format;
    QVector<QChar> conversions; // assigned conversions only
    bool parsed = literalContent(args.at(formatIndex), &format);
    bool plainInts = parsed;
    if (parsed) {
        for (int i = 0; i < format.size(); ++i) {
            if (format.at(i) == QLatin1Char('\\')) {
                ++i;
                continue;
            }
            if (format.at(i) != QLatin1Char('%'))
                continue;
            ++i;
            if (i < format.size() && format.at(i) == QLatin1Char('%'))
                continue;
            bool suppressed = false;
            bool decorated = false;
            if (i < format.size() && format.at(i) == QLatin1Char('*')) {
                suppressed = true;
                ++i;
            }
            while (i < format.size() && (format.at(i).isDigit() || QStringLiteral("hlLzjtqI").contains(format.at(i)))) {
                decorated = true;
                ++i;
            }
            if (i >= format.size()) {
                parsed = false;
                break;
            }
            QChar conv = format.at(i);
            if (conv == QLatin1Char('[')) {
                int j = i + 1;
                if (j < format.size() && format.at(j) == QLatin1Char('^'))
                    ++j;
                if (j < format.size() && format.at(j) == QLatin1Char(']'))
                    ++j;
                while (j < format.size() && format.at(j) != QLatin1Char(']'))
                    ++j;
                i = j;
            } else if (!QStringLiteral("diouxXfFeEgGaAcspn").contains(conv)) {
                parsed = false;
                break;
            }
            if (suppressed || decorated || (conv != QLatin1Char('d') && conv != QLatin1Char('i')))
                plainInts = false;
            if (!suppressed)
                conversions.append(conv);
        }
    }
    if (options.exactOutput && (!parsed || !plainInts || conversions.size() != targets.size())) {
        // Round-trip mode: only plain int input maps to an input block.
        addProcess(branch, text(r));
        return true;
    }
    if (secure && parsed) {
        QVector<Range> filtered;
        int ti = 0;
        for (const QChar conv : std::as_const(conversions)) {
            if (ti >= targets.size())
                break;
            filtered.append(targets.at(ti++));
            if (conv == QLatin1Char('c') || conv == QLatin1Char('s') || conv == QLatin1Char('['))
                ++ti;
        }
        if (ti == targets.size())
            targets = filtered;
    }
    QStringList vars;
    for (const Range &t : std::as_const(targets))
        vars << withoutAddressOf(t);
    addList(branch, "io", vars);
    return true;
}

bool FlowchartBuilder::isVariableName(const QString &name)
{
    const auto it = variableCache.constFind(name);
    if (it != variableCache.constEnd())
        return it.value();
    bool variable = false;
    if (def) {
        for (int k = def->declBegin; k < def->bodyEnd && !variable; ++k) {
            const Token &t = ts.at(k);
            if (!t.isIdentifier() || t.text != name)
                continue;
            const Token &prev = ts.at(k - 1);
            const Token &next = ts.at(k + 1);
            if (prev.isPunct("::") || prev.isPunct(".") || prev.isPunct("->"))
                continue;
            if (isAssignmentOp(next) || next.isPunct("++") || next.isPunct("--") || prev.isPunct("++")
                || prev.isPunct("--") || prev.isPunct(">>") || prev.isPunct("&")) {
                variable = true;
            } else if ((isBuiltinType(prev) || prev.isIdentifier() || prev.isPunct("*"))
                       && (next.isPunct(";") || next.isPunct(",") || next.isPunct("=") || next.isPunct(")")
                           || next.isPunct("[") || next.isPunct("{"))) {
                variable = true; // declared here
            }
        }
    }
    variableCache.insert(name, variable);
    return variable;
}

bool FlowchartBuilder::isManipulator(Range item)
{
    QString c = compactText(ts, item);
    bool qualified = false;
    if (c.startsWith(QLatin1String("std::"))) {
        c = c.mid(5);
        qualified = true;
    }
    const int paren = c.indexOf(QLatin1Char('('));
    const QString base = paren >= 0 ? c.left(paren) : c;
    if (!isManipulatorName(base))
        return false;
    if (paren >= 0 && !c.endsWith(QLatin1Char(')')))
        return false;
    return qualified || paren >= 0 || !isVariableName(base);
}

bool FlowchartBuilder::tryOutput(Range r, QDomElement &branch)
{
    const int m = streamEnd(r, {"cout", "std::cout", "cerr", "std::cerr", "clog", "std::clog"});
    if (m >= 0) {
        if (!ts.isPunct(m, "<<"))
            return false;
        const QVector<Range> items = splitTopLevel(ts, Range(m + 1, r.end), "<<");
        for (const Range &it : items) {
            if (it.isEmpty())
                return false;
        }
        auto isEndl = [&](Range it) {
            const QString c = compactText(ts, it);
            return c == QLatin1String("endl") || c == QLatin1String("std::endl");
        };
        QStringList out;
        if (options.exactOutput) {
            for (const Range &it : items)
                out << (isEndl(it) ? kNewlineItem : text(it));
            addList(branch, "ou", out);
            return true;
        }
        QVector<Range> kept;
        for (const Range &it : items) {
            if (isEndl(it) || !isManipulator(it))
                kept.append(it);
        }
        bool newline = false;
        for (int i = 0; i < kept.size(); ++i) {
            const Range it = kept.at(i);
            if (isEndl(it)) {
                if (i == kept.size() - 1)
                    newline = true;
                else
                    out << kNewlineItem;
                continue;
            }
            if (i == kept.size() - 1 && it.size() == 1) {
                const Token &t = ts.at(it.begin);
                if (t.kind == Token::Char && t.text == QLatin1String("'\\n'")) {
                    newline = true;
                    continue;
                }
                QString content;
                if (t.kind == Token::String && literalContent(it, &content) && endsWithNewlineEscape(content)) {
                    newline = true;
                    content.chop(2);
                    if (!content.isEmpty())
                        out << QLatin1Char('"') + content + QLatin1Char('"');
                    continue;
                }
            }
            out << text(it);
        }
        if (out.isEmpty() && !newline) {
            addProcess(branch, text(r));
            return true;
        }
        addList(branch, "ou", out);
        return true;
    }

    QString name;
    QVector<Range> args;
    if (!parseCall(ts, r, &name, &args))
        return false;
    for (const Range &a : std::as_const(args)) {
        if (a.isEmpty())
            return false;
    }
    if (name == QLatin1String("printf") && !args.isEmpty())
        return tryPrintf(r, args, 0, branch);
    if (name == QLatin1String("fprintf") && args.size() >= 2) {
        const QString stream = compactText(ts, args.first());
        if (stream == QLatin1String("stdout") || stream == QLatin1String("stderr"))
            return tryPrintf(r, args, 1, branch);
        return false;
    }
    if (name == QLatin1String("puts") && args.size() == 1) {
        QStringList out;
        out << text(args.first());
        if (options.exactOutput)
            out << kNewlineItem;
        addList(branch, "ou", out);
        return true;
    }
    if (name == QLatin1String("putchar") && args.size() == 1) {
        const QString c = text(args.first());
        if (options.exactOutput)
            addList(branch, "ou", QStringList() << QLatin1String("(char)(") + c + QLatin1Char(')'));
        else if (c == QLatin1String("'\\n'"))
            addList(branch, "ou", QStringList()); // the output block prints the line break
        else
            addList(branch, "ou", QStringList() << c);
        return true;
    }
    if (name == QLatin1String("fputs") && args.size() == 2 && compactText(ts, args.at(1)) == QLatin1String("stdout")) {
        QString content;
        if (!options.exactOutput && args.first().size() == 1 && literalContent(args.first(), &content)
            && endsWithNewlineEscape(content)) {
            content.chop(2);
            addList(branch, "ou", content.isEmpty() ? QStringList() : QStringList() << QLatin1Char('"') + content + QLatin1Char('"'));
            return true;
        }
        addList(branch, "ou", QStringList() << text(args.first()));
        return true;
    }
    return false;
}

bool FlowchartBuilder::parsePrintfFormat(const QString &format, QVector<FormatPiece> *pieces)
{
    pieces->clear();
    QString literal;
    auto flush = [&]() {
        if (!literal.isEmpty()) {
            FormatPiece p;
            p.literal = true;
            p.text = literal;
            pieces->append(p);
            literal.clear();
        }
    };
    const int n = format.size();
    int i = 0;
    while (i < n) {
        const QChar c = format.at(i);
        if (c == QLatin1Char('\\')) {
            literal += c;
            if (i + 1 < n)
                literal += format.at(i + 1);
            i += 2;
            continue;
        }
        if (c != QLatin1Char('%')) {
            literal += c;
            ++i;
            continue;
        }
        if (i + 1 < n && format.at(i + 1) == QLatin1Char('%')) {
            literal += QLatin1Char('%');
            i += 2;
            continue;
        }
        FormatPiece p;
        p.literal = false;
        int j = i + 1;
        while (j < n && QStringLiteral("-+ #0'").contains(format.at(j))) {
            p.flags = true;
            ++j;
        }
        if (j < n && format.at(j) == QLatin1Char('*')) {
            p.star = true;
            p.width = true;
            ++j;
        }
        while (j < n && format.at(j).isDigit()) {
            p.width = true;
            ++j;
        }
        if (j < n && format.at(j) == QLatin1Char('.')) {
            p.precision = true;
            ++j;
            if (j < n && format.at(j) == QLatin1Char('*')) {
                p.star = true;
                ++j;
            }
            while (j < n && format.at(j).isDigit())
                ++j;
        }
        static const char *const lengths[] = {"hh", "ll", "I64", "I32", "h", "l", "L", "z", "j", "t", "q", "I"};
        for (const char *len : lengths) {
            const QLatin1String l(len);
            if (format.mid(j, l.size()) == l) {
                p.length = l;
                j += l.size();
                break;
            }
        }
        if (j >= n || !QStringLiteral("diouxXfFeEgGaAcspn").contains(format.at(j)))
            return false;
        p.conversion = format.at(j);
        p.text = format.mid(i, j + 1 - i);
        flush();
        pieces->append(p);
        i = j + 1;
    }
    flush();
    return true;
}

bool FlowchartBuilder::tryPrintf(Range r, const QVector<Range> &args, int formatIndex, QDomElement &branch)
{
    const QVector<Range> values = args.mid(formatIndex + 1);
    auto verbatim = [&]() {
        if (options.exactOutput) {
            addProcess(branch, text(r));
            return true;
        }
        QStringList out;
        for (int k = formatIndex; k < args.size(); ++k)
            out << text(args.at(k));
        addList(branch, "ou", out);
        return true;
    };
    QString format;
    if (!literalContent(args.at(formatIndex), &format))
        return verbatim();
    QVector<FormatPiece> pieces;
    if (!parsePrintfFormat(format, &pieces))
        return verbatim();
    int conversions = 0;
    for (const FormatPiece &p : std::as_const(pieces)) {
        if (p.literal)
            continue;
        ++conversions;
        if (p.star || p.conversion == QLatin1Char('n'))
            return verbatim();
        if (options.exactOutput) {
            const QChar c = p.conversion;
            const bool integer = c == QLatin1Char('d') || c == QLatin1Char('i') || c == QLatin1Char('u');
            const bool simple = !p.flags && !p.width && !p.precision
                && ((integer && (p.length.isEmpty() || p.length == QLatin1String("l") || p.length == QLatin1String("ll")))
                    || ((c == QLatin1Char('s') || c == QLatin1Char('c')) && p.length.isEmpty()));
            if (!simple)
                return verbatim();
        }
    }
    if (conversions != values.size())
        return verbatim();

    QVector<Item> items;
    int vi = 0;
    for (const FormatPiece &p : std::as_const(pieces)) {
        if (p.literal) {
            if (!items.isEmpty() && items.last().literal)
                items.last().text += p.text;
            else
                items.append(Item{p.text, true});
            continue;
        }
        QString value = text(values.at(vi++));
        if (options.exactOutput && p.conversion == QLatin1Char('c'))
            value = QLatin1String("(char)(") + value + QLatin1Char(')');
        items.append(Item{value, false});
    }
    bool newline = false;
    if (!options.exactOutput && !items.isEmpty() && items.last().literal && endsWithNewlineEscape(items.last().text)) {
        items.last().text.chop(2);
        newline = true;
    }
    QStringList out;
    for (const Item &it : std::as_const(items)) {
        if (it.literal) {
            if (!it.text.isEmpty())
                out << QLatin1Char('"') + it.text + QLatin1Char('"');
        } else {
            out << it.text;
        }
    }
    if (out.isEmpty() && !newline && !options.exactOutput) {
        addProcess(branch, text(r));
        return true;
    }
    if (out.isEmpty() && options.exactOutput) {
        addProcess(branch, text(r));
        return true;
    }
    addList(branch, "ou", out);
    return true;
}

// ---------------------------------------------------------------------------
// Declarations

void FlowchartBuilder::mapDeclaration(Range r, QDomElement &branch)
{
    if (options.keepDeclarations) {
        addProcess(branch, text(r));
        return;
    }
    const Declaration d = parseDeclaration(ts, r);
    bool valid = d.ok;
    for (const Declarator &dc : d.declarators) {
        if (dc.init == Declarator::Invalid)
            valid = false;
    }
    if (!valid) {
        addProcess(branch, text(r));
        return;
    }
    const QString typeText = text(d.type);
    for (const Declarator &dc : d.declarators) {
        switch (dc.init) {
        case Declarator::None:
        case Declarator::Invalid:
            break;
        case Declarator::Assign:
            if (findAssignment(ts, dc.initializer) >= 0) {
                addProcess(branch, typeText + QLatin1Char(' ') + text(dc.range));
            } else if (options.detectIO && !options.exactOutput && isGetchar(dc.initializer)
                       && ts.at(dc.range.begin).isIdentifier()) {
                addList(branch, "io", QStringList() << dc.name);
            } else {
                addAssign(branch, dc.name, text(dc.initializer));
            }
            break;
        case Declarator::Direct:
            if (dc.prototype) {
                diag(Diagnostic::Info, dc.range.begin,
                     QCoreApplication::translate("afce::CodeImporter", "Local function declaration skipped"));
            } else {
                addProcess(branch, typeText + QLatin1Char(' ') + text(dc.range));
            }
            break;
        }
    }
}

} // namespace cimport
} // namespace afce
