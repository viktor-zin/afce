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

#include "importtokenizer.h"

#include <QCoreApplication>
#include <QSet>

#include <algorithm>

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

const QSet<QString> &cKeywords()
{
    static const QSet<QString> set = makeSet({
        "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum",
        "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return",
        "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void",
        "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
        "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local",
        // C23
        "bool", "true", "false", "nullptr", "static_assert", "thread_local", "alignas", "alignof",
        "constexpr", "typeof", "typeof_unqual",
    });
    return set;
}

const QSet<QString> &cppKeywords()
{
    static const QSet<QString> set = makeSet({
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break", "case",
        "catch", "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept", "const",
        "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await", "co_return",
        "co_yield", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
        "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int",
        "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or",
        "or_eq", "private", "protected", "public", "register", "reinterpret_cast", "requires", "return",
        "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch",
        "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
        "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
    });
    return set;
}

inline bool isIdentStart(QChar c)
{
    const ushort u = c.unicode();
    return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || u == '_' || u == '$'
        || (u >= 0x80 && u != 0xFEFF && !c.isSpace());
}

inline bool isIdentChar(QChar c)
{
    const ushort u = c.unicode();
    return (u >= '0' && u <= '9') || isIdentStart(c);
}

inline bool isDigit(QChar c)
{
    return c.unicode() >= '0' && c.unicode() <= '9';
}

// Whitespace, a byte order mark or a control character (compilers ignore NUL
// characters in the source; other control characters are treated alike).
inline bool isBlank(QChar c)
{
    const ushort u = c.unicode();
    return u < 0x20 || u == 0x7F || u == 0xFEFF || c.isSpace();
}

bool isSinglePunct(QChar c)
{
    static const QString chars = QStringLiteral("{}[]()<>;:,.?+-*/%^&|~!=#");
    return chars.contains(c);
}

const char *const kPunct3[] = { "<=>", "->*", "<<=", ">>=", "..." };
const char *const kPunct2[] = { "::", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
                                "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", ".*", "##" };

bool matchesAt(const QChar *s, int n, int i, const char *p)
{
    for (int k = 0; p[k]; ++k) {
        if (i + k >= n || s[i + k].unicode() != static_cast<ushort>(static_cast<unsigned char>(p[k])))
            return false;
    }
    return true;
}

} // namespace

bool isKeyword(const QString &word, Lang lang)
{
    return lang == Lang::C ? cKeywords().contains(word) : cppKeywords().contains(word);
}

QVector<Token> tokenize(const QString &source, Lang lang, QList<Diagnostic> *diagnostics)
{
    // Translation phase 2: remove line splices, remembering original offsets.
    QString clean;
    QVector<int> map;
    const bool hasSplices = source.contains(QLatin1String("\\\n")) || source.contains(QLatin1String("\\\r\n"));
    if (hasSplices) {
        clean.reserve(source.size());
        map.reserve(source.size() + 1);
        for (int i = 0; i < source.size(); ++i) {
            const QChar c = source.at(i);
            if (c == QLatin1Char('\\')) {
                if (i + 1 < source.size() && source.at(i + 1) == QLatin1Char('\n')) {
                    ++i;
                    continue;
                }
                if (i + 2 < source.size() && source.at(i + 1) == QLatin1Char('\r')
                    && source.at(i + 2) == QLatin1Char('\n')) {
                    i += 2;
                    continue;
                }
            }
            clean += c;
            map.append(i);
        }
        map.append(source.size());
    } else {
        clean = source;
    }
    auto orig = [&](int k) { return hasSplices ? map.at(k) : k; };

    QVector<int> lineStarts;
    lineStarts.append(0);
    for (int i = 0; i < source.size(); ++i) {
        if (source.at(i) == QLatin1Char('\n'))
            lineStarts.append(i + 1);
    }
    auto locate = [&](int offset, int &line, int &column) {
        const auto it = std::upper_bound(lineStarts.cbegin(), lineStarts.cend(), offset);
        const int idx = int(it - lineStarts.cbegin()) - 1;
        line = idx + 1;
        column = offset - lineStarts.at(idx) + 1;
    };
    auto problem = [&](int cleanOffset, const QString &message) {
        if (!diagnostics)
            return;
        Diagnostic d;
        d.severity = Diagnostic::Warning;
        locate(orig(cleanOffset), d.line, d.column);
        d.message = message;
        diagnostics->append(d);
    };

    const QChar *s = clean.constData();
    const int n = clean.size();
    QVector<Token> tokens;
    tokens.reserve(n / 4 + 16);
    int lineIdx = 0;

    auto push = [&](Token::Kind kind, int start, int endPos) {
        Token t;
        t.kind = kind;
        t.offset = orig(start);
        t.length = orig(endPos - 1) + 1 - t.offset;
        t.text = clean.mid(start, endPos - start);
        while (lineIdx + 1 < lineStarts.size() && lineStarts.at(lineIdx + 1) <= t.offset)
            ++lineIdx;
        t.line = lineIdx + 1;
        t.column = t.offset - lineStarts.at(lineIdx) + 1;
        tokens.append(t);
    };

    // Scans a quoted literal starting at the quote i. Returns the index after
    // the closing quote; an unterminated literal ends before the newline.
    auto scanQuoted = [&](int i, QChar quote, bool *terminated) -> int {
        ++i;
        while (i < n) {
            const QChar c = s[i];
            if (c == QLatin1Char('\\')) {
                i = qMin(i + 2, n);
                continue;
            }
            if (c == quote) {
                *terminated = true;
                return i + 1;
            }
            if (c == QLatin1Char('\n'))
                break;
            ++i;
        }
        *terminated = false;
        return i;
    };

    // Raw string: i is the index of the quote after the R prefix. Returns -1 when
    // the delimiter is not valid (then it is scanned as an ordinary string).
    auto scanRaw = [&](int i, bool *terminated) -> int {
        int j = i + 1;
        while (j < n && j - (i + 1) <= 16) {
            const QChar c = s[j];
            if (c == QLatin1Char('('))
                break;
            if (isBlank(c) || c == QLatin1Char(')') || c == QLatin1Char('\\') || c == QLatin1Char('"'))
                return -1;
            ++j;
        }
        if (j >= n || s[j] != QLatin1Char('('))
            return -1;
        const QString terminator = QLatin1Char(')') + clean.mid(i + 1, j - i - 1) + QLatin1Char('"');
        const int pos = clean.indexOf(terminator, j + 1);
        if (pos < 0) {
            *terminated = false;
            return n;
        }
        *terminated = true;
        return pos + terminator.size();
    };

    // User-defined literal suffix after a string literal (C++ only).
    auto scanUdSuffix = [&](int i) -> int {
        if (lang != Lang::Cpp || i >= n || !isIdentStart(s[i]))
            return i;
        int j = i;
        while (j < n && isIdentChar(s[j]))
            ++j;
        const QString suffix = clean.mid(i, j - i);
        if (suffix.startsWith(QLatin1Char('_')) || suffix == QLatin1String("s") || suffix == QLatin1String("sv"))
            return j;
        return i;
    };

    auto scanString = [&](int start, int quotePos, bool raw) -> int {
        bool terminated = true;
        int end = -1;
        if (raw)
            end = scanRaw(quotePos, &terminated);
        if (end < 0)
            end = scanQuoted(quotePos, QLatin1Char('"'), &terminated);
        if (!terminated)
            problem(start, QCoreApplication::translate("afce::CodeImporter", "Unterminated string literal"));
        return terminated ? scanUdSuffix(end) : end;
    };

    bool lineStart = true;
    int i = 0;
    while (i < n) {
        const QChar c = s[i];
        if (c == QLatin1Char('\n')) {
            lineStart = true;
            ++i;
            continue;
        }
        if (isBlank(c)) {
            ++i;
            continue;
        }
        if (c == QLatin1Char('/') && i + 1 < n && s[i + 1] == QLatin1Char('/')) {
            const int nl = clean.indexOf(QLatin1Char('\n'), i);
            i = nl < 0 ? n : nl;
            continue;
        }
        if (c == QLatin1Char('/') && i + 1 < n && s[i + 1] == QLatin1Char('*')) {
            const int close = clean.indexOf(QLatin1String("*/"), i + 2);
            if (close < 0) {
                problem(i, QCoreApplication::translate("afce::CodeImporter", "Unterminated comment"));
                i = n;
            } else {
                i = close + 2;
            }
            continue;
        }

        const int start = i;
        if (c == QLatin1Char('#') && lineStart) {
            int j = i + 1;
            int textEnd = -1;
            while (j < n) {
                const QChar d = s[j];
                if (d == QLatin1Char('\n'))
                    break;
                if (d == QLatin1Char('/') && j + 1 < n && s[j + 1] == QLatin1Char('/')) {
                    textEnd = j;
                    const int nl = clean.indexOf(QLatin1Char('\n'), j);
                    j = nl < 0 ? n : nl;
                    break;
                }
                if (d == QLatin1Char('/') && j + 1 < n && s[j + 1] == QLatin1Char('*')) {
                    const int close = clean.indexOf(QLatin1String("*/"), j + 2);
                    if (close < 0) {
                        problem(j, QCoreApplication::translate("afce::CodeImporter", "Unterminated comment"));
                        j = n;
                        break;
                    }
                    j = close + 2;
                    continue;
                }
                if (d == QLatin1Char('"') || d == QLatin1Char('\'')) {
                    bool terminated = false;
                    j = scanQuoted(j, d, &terminated);
                    continue;
                }
                ++j;
            }
            if (textEnd < 0)
                textEnd = j;
            while (textEnd > start + 1 && isBlank(s[textEnd - 1]))
                --textEnd;
            push(Token::Preprocessor, start, textEnd);
            i = j;
            continue;
        }
        lineStart = false;

        if (isIdentStart(c)) {
            int j = i + 1;
            while (j < n && isIdentChar(s[j]))
                ++j;
            const int wordLen = j - i;
            if (j < n && wordLen <= 3 && (s[j] == QLatin1Char('"') || s[j] == QLatin1Char('\''))) {
                const QString word = clean.mid(i, wordLen);
                const bool charPrefix = word == QLatin1String("u8") || word == QLatin1String("u")
                    || word == QLatin1String("U") || word == QLatin1String("L");
                const bool rawPrefix = lang == Lang::Cpp
                    && (word == QLatin1String("R") || word == QLatin1String("u8R") || word == QLatin1String("uR")
                        || word == QLatin1String("UR") || word == QLatin1String("LR"));
                if (s[j] == QLatin1Char('"') && (charPrefix || rawPrefix)) {
                    i = scanString(start, j, rawPrefix);
                    push(Token::String, start, i);
                    continue;
                }
                if (s[j] == QLatin1Char('\'') && charPrefix) {
                    bool terminated = false;
                    i = scanQuoted(j, QLatin1Char('\''), &terminated);
                    if (!terminated)
                        problem(start, QCoreApplication::translate("afce::CodeImporter", "Unterminated character literal"));
                    push(Token::Char, start, i);
                    continue;
                }
            }
            i = j;
            const QString word = clean.mid(start, wordLen);
            push(isKeyword(word, lang) ? Token::Keyword : Token::Identifier, start, i);
            continue;
        }

        if (isDigit(c) || (c == QLatin1Char('.') && i + 1 < n && isDigit(s[i + 1]))) {
            int j = i + 1;
            while (j < n) {
                const QChar d = s[j];
                const ushort u = d.unicode();
                if ((u == 'e' || u == 'E' || u == 'p' || u == 'P') && j + 1 < n
                    && (s[j + 1] == QLatin1Char('+') || s[j + 1] == QLatin1Char('-'))) {
                    j += 2;
                    continue;
                }
                if (isIdentChar(d) || d == QLatin1Char('.')) {
                    ++j;
                    continue;
                }
                if (d == QLatin1Char('\'') && j + 1 < n && isIdentChar(s[j + 1])) {
                    j += 2;
                    continue;
                }
                break;
            }
            i = j;
            push(Token::Number, start, i);
            continue;
        }

        if (c == QLatin1Char('"')) {
            i = scanString(start, i, false);
            push(Token::String, start, i);
            continue;
        }
        if (c == QLatin1Char('\'')) {
            bool terminated = false;
            i = scanQuoted(i, QLatin1Char('\''), &terminated);
            if (!terminated)
                problem(start, QCoreApplication::translate("afce::CodeImporter", "Unterminated character literal"));
            push(Token::Char, start, i);
            continue;
        }

        int len = 0;
        for (const char *p : kPunct3) {
            if (matchesAt(s, n, i, p)) {
                len = 3;
                break;
            }
        }
        if (!len) {
            for (const char *p : kPunct2) {
                if (matchesAt(s, n, i, p)) {
                    len = 2;
                    break;
                }
            }
        }
        if (len) {
            i += len;
            push(Token::Punct, start, i);
            continue;
        }
        ++i;
        push(isSinglePunct(c) ? Token::Punct : Token::Unknown, start, i);
    }
    return tokens;
}

// ---------------------------------------------------------------------------

TokenStream::TokenStream(const QString &source, Lang lang, QList<Diagnostic> *diagnostics)
    : m_source(source)
    , m_lang(lang)
    , m_tokens(tokenize(source, lang, diagnostics))
{
    m_end.kind = Token::End;
    m_end.offset = source.size();
    computeMatches();
    computeTemplateMatches();
}

bool TokenStream::isOpener(int i) const
{
    const Token &t = at(i);
    return t.kind == Token::Punct && t.text.size() == 1
        && (t.text.at(0) == QLatin1Char('(') || t.text.at(0) == QLatin1Char('[') || t.text.at(0) == QLatin1Char('{'));
}

bool TokenStream::isCloser(int i) const
{
    const Token &t = at(i);
    return t.kind == Token::Punct && t.text.size() == 1
        && (t.text.at(0) == QLatin1Char(')') || t.text.at(0) == QLatin1Char(']') || t.text.at(0) == QLatin1Char('}'));
}

void TokenStream::computeMatches()
{
    const int n = m_tokens.size();
    m_match.fill(-1, n);
    m_matched.fill(false, n);
    QVector<int> stack;
    for (int i = 0; i < n; ++i) {
        if (isOpener(i)) {
            stack.append(i);
            continue;
        }
        if (!isCloser(i))
            continue;
        const QChar c = m_tokens.at(i).text.at(0);
        const QChar want = c == QLatin1Char(')') ? QLatin1Char('(') : c == QLatin1Char(']') ? QLatin1Char('[') : QLatin1Char('{');
        int found = -1;
        for (int k = stack.size() - 1; k >= 0; --k) {
            const QChar o = m_tokens.at(stack.at(k)).text.at(0);
            if (o == want) {
                found = k;
                break;
            }
            if (o == QLatin1Char('{'))
                break; // ')' and ']' never close across a brace
        }
        if (found < 0)
            continue; // stray closer
        while (stack.size() - 1 > found) {
            const int u = stack.takeLast();
            m_match[u] = i - 1; // group ends right before the implicit closer
        }
        const int o = stack.takeLast();
        m_match[o] = i;
        m_matched[o] = true;
        m_match[i] = o;
    }
    while (!stack.isEmpty()) {
        const int u = stack.takeLast();
        m_match[u] = n - 1;
    }
}

bool TokenStream::matched(int i) const
{
    return i >= 0 && i < m_matched.size() && m_matched.at(i);
}

int TokenStream::close(int i) const
{
    if (!isOpener(i))
        return i;
    return qMax(i, m_match.at(i));
}

int TokenStream::contentEnd(int i) const
{
    if (!isOpener(i))
        return i + 1;
    return matched(i) ? close(i) : close(i) + 1;
}

int TokenStream::after(int i) const
{
    if (!isOpener(i))
        return i + 1;
    return close(i) + 1;
}

int TokenStream::opener(int i) const
{
    if (!isCloser(i))
        return -1;
    return m_match.at(i);
}

QString TokenStream::text(int begin, int end) const
{
    QString result;
    begin = qMax(begin, 0);
    end = qMin(end, m_tokens.size());
    int prevEnd = -1;
    bool gap = false;
    for (int k = begin; k < end; ++k) {
        const Token &t = m_tokens.at(k);
        if (t.kind == Token::Preprocessor) {
            gap = true;
            continue;
        }
        if (!result.isEmpty() && (gap || t.offset > prevEnd))
            result += QLatin1Char(' ');
        result += t.text;
        prevEnd = t.end();
        gap = false;
    }
    return result;
}

QString TokenStream::text(const QVector<int> &indexes) const
{
    QString result;
    int prevEnd = -1;
    for (int k : indexes) {
        const Token &t = at(k);
        if (t.kind == Token::Preprocessor || t.kind == Token::End)
            continue;
        if (!result.isEmpty() && t.offset > prevEnd)
            result += QLatin1Char(' ');
        result += t.text;
        prevEnd = t.end();
    }
    return result;
}

void TokenStream::computeTemplateMatches()
{
    // Equivalent to scanning forward from every '<' (counting '<' / '>' /
    // '>>', skipping bracket groups, failing at ';' or a closer), done for all
    // of them in one linear pass: a bracket group opens a new level, a '<'
    // waits on the stack of its level until a '>' resolves it.
    const int n = m_tokens.size();
    m_templateEnd.fill(0, n);
    struct Level
    {
        int groupEnd;      // index of the last token of the group
        QVector<int> open; // unresolved '<'
    };
    QVector<Level> levels;
    levels.append(Level{n, {}});
    for (int k = 0; k < n; ++k) {
        const Token &t = m_tokens.at(k);
        if (t.kind == Token::Punct) {
            QVector<int> &open = levels.last().open;
            if (t.is("<")) {
                open.append(k);
            } else if (t.is(">")) {
                if (!open.isEmpty())
                    m_templateEnd[open.takeLast()] = k + 1;
            } else if (t.is(">>")) {
                for (int twice = 0; twice < 2 && !open.isEmpty(); ++twice)
                    m_templateEnd[open.takeLast()] = k + 1;
            } else if (t.is(";") || isCloser(k)) {
                open.clear(); // these never close
            } else if (isOpener(k)) {
                levels.append(Level{close(k), {}});
            }
        }
        while (levels.size() > 1 && levels.last().groupEnd <= k)
            levels.removeLast();
    }
}

int TokenStream::skipTemplateArgs(int i, int limit) const
{
    if (!isPunct(i, "<"))
        return -1;
    const int end = m_templateEnd.at(i);
    return (end > 0 && end <= qMin(limit, m_tokens.size())) ? end : -1;
}

} // namespace cimport
} // namespace afce
