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

#include "importscanner.h"

#include <QCoreApplication>

namespace afce {
namespace cimport {

bool isMacroLikeName(const QString &name)
{
    if (name.size() < 2)
        return false;
    bool letter = false;
    for (const QChar c : name) {
        const ushort u = c.unicode();
        if (u >= 'A' && u <= 'Z')
            letter = true;
        else if (!(u == '_' || (u >= '0' && u <= '9')))
            return false;
    }
    return letter;
}

namespace {

bool oneOf(const Token &t, std::initializer_list<const char *> words)
{
    for (const char *w : words) {
        if (t.is(w))
            return true;
    }
    return false;
}

class Scanner
{
public:
    Scanner(const TokenStream &ts, QList<Diagnostic> &diags)
        : ts(ts)
        , diags(diags)
        , cpp(ts.language() == Lang::Cpp)
    {
    }

    QVector<FunctionDef> run();

private:
    struct Scope
    {
        int end;        // exclusive end of the scope content
        int after;      // first token after the scope
        QString prefix; // "Class::" for class scopes (accumulated)
        bool isClass;
    };

    int scopeEnd() const { return scopes.last().end; }
    void pushScope(int open, const QString &prefix, bool isClass);

    int declaration(int pos);
    int classOrDecl(int declStart, int k);
    int generic(int declStart, int specStart);
    int function(int declStart, int specStart, int nameBegin, int nameEnd, int lp);
    int ctorInitBody(int q) const;
    int skipToSemicolon(int k) const;
    int skipAttributes(int k) const;
    int skipRequiresClause(int k) const;
    int qualifiedIdEnd(int k) const;
    int operatorNameEnd(int m) const;
    QString nameText(int b, int e) const;
    QString returnTypeText(int b, int e) const;
    bool isAttributeWord(int k) const;

    const TokenStream &ts;
    QList<Diagnostic> &diags;
    const bool cpp;
    QVector<Scope> scopes;
    QVector<FunctionDef> result;
};

void Scanner::pushScope(int open, const QString &prefix, bool isClass)
{
    Scope s;
    s.end = ts.contentEnd(open);
    s.after = ts.after(open);
    s.prefix = prefix;
    s.isClass = isClass;
    scopes.append(s);
}

QVector<FunctionDef> Scanner::run()
{
    Scope global;
    global.end = ts.size();
    global.after = ts.size();
    global.isClass = false;
    scopes.append(global);

    int pos = 0;
    for (;;) {
        if (pos >= scopes.last().end) {
            if (scopes.size() == 1)
                break;
            pos = qMax(pos, scopes.last().after);
            scopes.removeLast();
            continue;
        }
        const int next = declaration(pos);
        pos = qMax(next, pos + 1);
    }

    if (result.isEmpty()) {
        // No definitions: a code fragment (statements) can still be imported.
        int first = -1;
        int last = -1;
        bool statements = false;
        for (int i = 0; i < ts.size(); ++i) {
            const Token &t = ts.at(i);
            if (t.kind == Token::Preprocessor)
                continue;
            if (first < 0)
                first = i;
            last = i;
            if ((t.kind == Token::Keyword && oneOf(t, {"if", "for", "while", "do", "switch", "return"}))
                || (t.kind == Token::Identifier && oneOf(t, {"cout", "cin", "printf", "scanf", "puts"}))) {
                statements = true;
            }
        }
        if (statements && first >= 0) {
            FunctionDef def;
            def.fragment = true;
            def.declBegin = first;
            def.bodyBegin = first;
            def.bodyEnd = last + 1;
            def.info.line = ts.at(first).line;
            def.info.startOffset = ts.at(first).offset;
            def.info.endOffset = ts.at(last).end();
            result.append(def);
            Diagnostic d;
            d.severity = Diagnostic::Info;
            d.line = ts.at(first).line;
            d.column = ts.at(first).column;
            d.message = QCoreApplication::translate("afce::CodeImporter",
                "No function definitions found; the code is imported as a fragment");
            diags.append(d);
        }
    }
    return result;
}

int Scanner::declaration(int pos)
{
    const Token &t = ts.at(pos);
    if (t.kind == Token::Preprocessor || t.kind == Token::Unknown)
        return pos + 1;
    if (t.kind == Token::Punct) {
        if (t.is(";") || ts.isCloser(pos))
            return pos + 1;
        if (t.is("{"))
            return ts.after(pos); // stray block
    }
    if (cpp) {
        if (t.isKeyword("inline") && ts.isKeyword(pos + 1, "namespace"))
            return pos + 1;
        if (t.isKeyword("export"))
            return pos + 1;
        if (t.isKeyword("namespace")) {
            int k = pos + 1;
            while (k < scopeEnd() && (ts.at(k).isIdentifier() || ts.isPunct(k, "::") || ts.isKeyword(k, "inline")))
                ++k;
            k = skipAttributes(k);
            if (ts.isPunct(k, "{")) {
                pushScope(k, scopes.last().prefix, false);
                return k + 1;
            }
            return skipToSemicolon(k);
        }
        if (t.isKeyword("template")) {
            int k = pos;
            while (ts.isKeyword(k, "template")) {
                if (!ts.isPunct(k + 1, "<"))
                    return skipToSemicolon(k + 1); // explicit instantiation
                const int e = ts.skipTemplateArgs(k + 1, scopeEnd());
                if (e < 0)
                    return skipToSemicolon(k + 1);
                k = e;
            }
            if (ts.isKeyword(k, "requires"))
                k = skipRequiresClause(k);
            return classOrDecl(pos, k);
        }
        if (t.isKeyword("using"))
            return skipToSemicolon(pos + 1);
        if (t.isKeyword("public") || t.isKeyword("protected") || t.isKeyword("private")) {
            if (ts.isPunct(pos + 1, ":"))
                return pos + 2;
            if (ts.at(pos + 1).isIdentifier() && ts.isPunct(pos + 2, ":"))
                return pos + 3; // public slots:
        }
        if (t.isIdentifier() && oneOf(t, {"signals", "slots", "Q_SIGNALS", "Q_SLOTS"}) && ts.isPunct(pos + 1, ":"))
            return pos + 2;
    }
    if (t.isKeyword("extern") && ts.at(pos + 1).kind == Token::String && ts.isPunct(pos + 2, "{")) {
        pushScope(pos + 2, scopes.last().prefix, false);
        return pos + 3;
    }
    if (t.isKeyword("typedef") || t.is("static_assert") || t.is("_Static_assert"))
        return skipToSemicolon(pos + 1);
    if (t.isIdentifier() && isMacroLikeName(t.text) && ts.at(pos + 1).line > t.line) {
        // A macro on a line of its own (Q_OBJECT, Q_GADGET, ...).
        const Token &nx = ts.at(pos + 1);
        if (nx.isWord() || nx.isPunct("}") || nx.isPunct("~") || nx.isPunct("[") || nx.kind == Token::Preprocessor)
            return pos + 1;
    }
    return classOrDecl(pos, pos);
}

int Scanner::classOrDecl(int declStart, int k)
{
    const int end = scopeEnd();
    const int h = skipAttributes(k);
    const Token &t = ts.at(h);
    const bool classKey = t.isKeyword("struct") || t.isKeyword("union") || (cpp && t.isKeyword("class"));
    const bool enumKey = t.isKeyword("enum");
    if (classKey || enumKey) {
        int m = h + 1;
        if (enumKey && (ts.isKeyword(m, "class") || ts.isKeyword(m, "struct")))
            ++m;
        m = skipAttributes(m);
        const int nameBegin = m;
        const int nameEnd = qualifiedIdEnd(m);
        m = nameEnd;
        if (ts.at(m).isIdentifier() && (ts.at(m).is("final") || ts.at(m).is("sealed")))
            ++m;
        if (ts.isPunct(m, ":")) {
            int q = m + 1;
            while (q < end && !ts.isPunct(q, "{") && !ts.isPunct(q, ";") && !ts.isCloser(q)) {
                if (ts.isOpener(q)) {
                    q = ts.after(q);
                    continue;
                }
                if (cpp && ts.isPunct(q, "<")) {
                    const int e = ts.skipTemplateArgs(q, end);
                    q = e > 0 ? e : q + 1;
                    continue;
                }
                ++q;
            }
            m = q;
        }
        if (ts.isPunct(m, "{")) {
            if (enumKey)
                return ts.after(m);
            QString prefix = scopes.last().prefix;
            if (nameEnd > nameBegin)
                prefix += nameText(nameBegin, nameEnd) + QLatin1String("::");
            pushScope(m, prefix, true);
            return m + 1;
        }
    }
    return generic(declStart, k);
}

bool Scanner::isAttributeWord(int k) const
{
    const Token &t = ts.at(k);
    if (!t.isWord())
        return false;
    return oneOf(t, {"__attribute__", "__declspec", "alignas", "_Alignas", "__asm__", "__asm", "asm"})
        && ts.isPunct(k + 1, "(");
}

int Scanner::skipAttributes(int k) const
{
    for (;;) {
        if (ts.isPunct(k, "[") && ts.isPunct(k + 1, "[")) {
            k = ts.after(k);
        } else if (isAttributeWord(k)) {
            k = ts.after(k + 1);
        } else {
            return k;
        }
    }
}

int Scanner::skipToSemicolon(int k) const
{
    const int end = scopeEnd();
    while (k < end) {
        if (ts.isPunct(k, ";"))
            return k + 1;
        if (ts.isCloser(k))
            return k;
        if (ts.isOpener(k)) {
            k = ts.after(k);
            continue;
        }
        ++k;
    }
    return k;
}

int Scanner::skipRequiresClause(int k) const
{
    const int end = scopeEnd();
    int q = k + 1;
    while (q < end) {
        while (ts.isPunct(q, "!"))
            ++q;
        const int start = q;
        if (ts.isPunct(q, "(")) {
            q = ts.after(q);
        } else if (ts.isKeyword(q, "requires")) {
            ++q;
            if (ts.isPunct(q, "("))
                q = ts.after(q);
            if (ts.isPunct(q, "{"))
                q = ts.after(q);
        } else if (ts.at(q).isIdentifier() || ts.isPunct(q, "::")) {
            q = qualifiedIdEnd(q);
            if (ts.isPunct(q, "("))
                q = ts.after(q);
        } else if (ts.at(q).kind == Token::Keyword || ts.at(q).isLiteral()) {
            ++q;
            if (ts.isPunct(q, "("))
                q = ts.after(q);
        }
        if (q == start)
            break;
        if (ts.isPunct(q, "&&") || ts.isPunct(q, "||")) {
            ++q;
            continue;
        }
        break;
    }
    return qMax(q, k + 1);
}

int Scanner::qualifiedIdEnd(int k) const
{
    const int end = scopeEnd();
    int m = k;
    if (ts.isPunct(m, "::"))
        ++m;
    const int first = m;
    while (m < end) {
        const Token &t = ts.at(m);
        if (cpp && t.isKeyword("operator"))
            return operatorNameEnd(m);
        if (t.isPunct("~") && ts.at(m + 1).isIdentifier())
            return m + 2;
        if (cpp && t.isKeyword("template") && m > first) {
            ++m;
            continue;
        }
        if (!t.isIdentifier())
            break;
        ++m;
        if (cpp && ts.isPunct(m, "<")) {
            const int e = ts.skipTemplateArgs(m, end);
            if (e > 0)
                m = e;
        }
        if (ts.isPunct(m, "::")) {
            ++m;
            continue;
        }
        return m;
    }
    return m == first ? k : m;
}

int Scanner::operatorNameEnd(int m) const
{
    const int end = scopeEnd();
    int k = m + 1;
    const Token &t = ts.at(k);
    if ((t.isPunct("(") && ts.isPunct(k + 1, ")")) || (t.isPunct("[") && ts.isPunct(k + 1, "]")))
        return k + 2;
    if (t.isKeyword("new") || t.isKeyword("delete")) {
        ++k;
        if (ts.isPunct(k, "[") && ts.isPunct(k + 1, "]"))
            k += 2;
        return k;
    }
    if (t.isKeyword("co_await"))
        return k + 1;
    if (t.kind == Token::String) {
        ++k;
        if (ts.at(k).isIdentifier())
            ++k;
        return k;
    }
    if (t.kind == Token::Punct && !t.is("(") && !t.is(";") && !t.is("{") && !ts.isCloser(k))
        return k + 1;
    // Conversion function: operator <type>
    while (k < end && !ts.isPunct(k, "(") && !ts.isPunct(k, ";") && !ts.isPunct(k, "{") && !ts.isCloser(k)) {
        if (ts.isPunct(k, "<")) {
            const int e = ts.skipTemplateArgs(k, end);
            k = e > 0 ? e : k + 1;
            continue;
        }
        ++k;
    }
    return k;
}

int Scanner::generic(int declStart, int specStart)
{
    const int end = scopeEnd();
    int k = specStart;
    while (k < end) {
        const Token &t = ts.at(k);
        if (t.kind == Token::Preprocessor || t.kind == Token::Unknown || t.isLiteral()) {
            ++k;
            continue;
        }
        if (t.kind == Token::Punct) {
            if (t.is(";"))
                return k + 1;
            if (ts.isCloser(k))
                return k;
            if (t.is("="))
                return skipToSemicolon(k + 1);
            if (ts.isOpener(k)) {
                k = ts.after(k);
                continue;
            }
            if (!t.is("::") && !t.is("~")) {
                ++k;
                continue;
            }
        } else if (t.kind == Token::Keyword && !t.is("operator")) {
            if (oneOf(t, {"decltype", "noexcept", "alignas", "sizeof", "typeof", "alignof", "_Alignas", "throw",
                          "__typeof__"})
                && ts.isPunct(k + 1, "(")) {
                k = ts.after(k + 1);
                continue;
            }
            ++k;
            continue;
        } else if (isAttributeWord(k)) {
            k = ts.after(k + 1);
            continue;
        }
        const int nameEnd = qualifiedIdEnd(k);
        if (nameEnd <= k) {
            ++k;
            continue;
        }
        if (ts.isPunct(nameEnd, "("))
            return function(declStart, specStart, k, nameEnd, nameEnd);
        k = nameEnd;
    }
    return k;
}

int Scanner::ctorInitBody(int q) const
{
    const int end = scopeEnd();
    while (q < end) {
        if (ts.isPunct(q, ";") || ts.isCloser(q))
            return -1;
        if (ts.isPunct(q, "{")) {
            const Token &prev = ts.at(q - 1);
            if (prev.isPunct(")") || prev.isPunct("}") || prev.isPunct("..."))
                return q;
            q = ts.after(q);
            continue;
        }
        if (ts.isOpener(q)) {
            q = ts.after(q);
            continue;
        }
        if (cpp && ts.isPunct(q, "<")) {
            const int e = ts.skipTemplateArgs(q, end);
            q = e > 0 ? e : q + 1;
            continue;
        }
        ++q;
    }
    return -1;
}

int Scanner::function(int declStart, int specStart, int nameBegin, int nameEnd, int lp)
{
    const int end = scopeEnd();
    if (!ts.matched(lp))
        return ts.after(lp);
    const int rp = ts.close(lp);
    int j = rp + 1;
    int trailBegin = -1;
    int trailEnd = -1;
    while (j < end) {
        const Token &t = ts.at(j);
        if (t.isKeyword("const") || t.isKeyword("volatile") || t.isPunct("&") || t.isPunct("&&")) {
            ++j;
            continue;
        }
        if (t.isIdentifier() && (t.is("override") || t.is("final"))) {
            ++j;
            continue;
        }
        if (t.isKeyword("noexcept") || t.isKeyword("throw") || isAttributeWord(j)) {
            ++j;
            if (ts.isPunct(j, "("))
                j = ts.after(j);
            continue;
        }
        if (t.isPunct("[") && ts.isPunct(j + 1, "[")) {
            j = ts.after(j);
            continue;
        }
        if (t.isPunct("->")) {
            int q = j + 1;
            trailBegin = q;
            while (q < end) {
                const Token &u = ts.at(q);
                if (u.isPunct("{") || u.isPunct(";") || u.isPunct("=") || u.isPunct(":") || u.isKeyword("requires")
                    || u.isKeyword("try") || ts.isCloser(q)
                    || (u.isIdentifier() && (u.is("override") || u.is("final")) && q > trailBegin)) {
                    break;
                }
                if (ts.isOpener(q)) {
                    q = ts.after(q);
                    continue;
                }
                if (u.isPunct("<")) {
                    const int e = ts.skipTemplateArgs(q, end);
                    q = e > 0 ? e : q + 1;
                    continue;
                }
                ++q;
            }
            trailEnd = q;
            j = q;
            continue;
        }
        if (t.isKeyword("requires")) {
            j = skipRequiresClause(j);
            continue;
        }
        if (t.isIdentifier() && isMacroLikeName(t.text)) {
            // Q_DECL_OVERRIDE, NOEXCEPT, ... (or the next macro invocation, handled below)
            ++j;
            if (ts.isPunct(j, "("))
                j = ts.after(j);
            continue;
        }
        break;
    }

    int bodyOpen = -1;
    int lastToken = -1;
    bool tryBlock = false;
    const Token &t = ts.at(j);
    if (t.isPunct("{")) {
        bodyOpen = j;
    } else if (t.isPunct(":")) {
        bodyOpen = ctorInitBody(j + 1);
        if (bodyOpen < 0)
            return skipToSemicolon(j + 1);
    } else if (t.isKeyword("try")) {
        int q = j + 1;
        if (ts.isPunct(q, ":"))
            q = ctorInitBody(q + 1);
        if (q < 0 || !ts.isPunct(q, "{"))
            return skipToSemicolon(j + 1);
        bodyOpen = q;
        tryBlock = true;
        lastToken = ts.close(q);
        int h = ts.after(q);
        while (h < end && ts.isKeyword(h, "catch")) {
            ++h;
            if (ts.isPunct(h, "("))
                h = ts.after(h);
            if (!ts.isPunct(h, "{"))
                break;
            lastToken = ts.close(h);
            h = ts.after(h);
        }
    } else if (t.isPunct("=") || t.isPunct(",")) {
        return skipToSemicolon(j + 1);
    } else if (t.isPunct(";")) {
        return j + 1;
    } else {
        // A macro invocation (DECLARE_SOMETHING(x) without ';') or unsupported
        // syntax: continue right after the parentheses.
        return rp + 1;
    }
    if (lastToken < 0)
        lastToken = ts.close(bodyOpen);

    FunctionDef def;
    const QString baseName = nameText(nameBegin, nameEnd);
    bool isFriend = false;
    for (int k = specStart; k < nameBegin; ++k) {
        if (ts.isKeyword(k, "friend"))
            isFriend = true;
    }
    const Scope &scope = scopes.last();
    def.info.name = (scope.isClass && !isFriend) ? scope.prefix + baseName : baseName;
    def.info.returnType = trailBegin >= 0 ? ts.text(trailBegin, trailEnd) : returnTypeText(specStart, nameBegin);
    def.info.parameters = ts.text(lp + 1, rp);
    def.info.line = ts.at(declStart).line;
    def.info.startOffset = ts.at(declStart).offset;
    def.info.endOffset = ts.at(lastToken).end();
    def.declBegin = declStart;
    def.bodyBegin = bodyOpen + 1;
    def.bodyEnd = ts.contentEnd(bodyOpen);
    def.paramBegin = lp + 1;
    def.paramEnd = rp;
    def.isMain = def.info.name == QLatin1String("main");
    def.functionTryBlock = tryBlock;
    if (!ts.matched(bodyOpen)) {
        Diagnostic d;
        d.severity = Diagnostic::Warning;
        d.line = ts.at(bodyOpen).line;
        d.column = ts.at(bodyOpen).column;
        d.message = QCoreApplication::translate("afce::CodeImporter", "The body of function '%1' is not closed")
                        .arg(def.info.name);
        diags.append(d);
    }
    result.append(def);
    return lastToken + 1;
}

QString Scanner::nameText(int b, int e) const
{
    QString r;
    bool prevWord = false;
    for (int k = b; k < e; ++k) {
        const Token &t = ts.at(k);
        if (t.kind == Token::Preprocessor)
            continue;
        const bool word = t.isWord() || t.kind == Token::Number;
        if (prevWord && word)
            r += QLatin1Char(' ');
        r += t.text;
        prevWord = word;
    }
    return r;
}

QString Scanner::returnTypeText(int b, int e) const
{
    QVector<int> keep;
    for (int k = b; k < e; ++k) {
        const Token &t = ts.at(k);
        if (t.kind == Token::Preprocessor)
            continue;
        if (t.isPunct("[") && ts.isPunct(k + 1, "[")) {
            k = ts.after(k) - 1;
            continue;
        }
        if (isAttributeWord(k)) {
            k = ts.after(k + 1) - 1;
            continue;
        }
        if (t.isKeyword("extern")) {
            if (ts.at(k + 1).kind == Token::String)
                ++k;
            continue;
        }
        if ((t.kind == Token::Keyword
             && oneOf(t, {"static", "inline", "virtual", "explicit", "constexpr", "consteval", "constinit", "friend",
                          "_Noreturn", "export"}))
            || (t.isIdentifier() && oneOf(t, {"__inline", "__inline__", "__forceinline", "__extension__"}))) {
            continue;
        }
        keep.append(k);
    }
    return ts.text(keep);
}

} // namespace

QVector<FunctionDef> scanFunctions(const TokenStream &tokens, QList<Diagnostic> &diagnostics)
{
    Scanner scanner(tokens, diagnostics);
    return scanner.run();
}

} // namespace cimport
} // namespace afce
