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

// Internal header of the C / C++ importer: tokenizer and token stream.

#ifndef IMPORTTOKENIZER_H
#define IMPORTTOKENIZER_H

#include "codeimporter.h"

#include <QList>
#include <QString>
#include <QVector>

namespace afce {
namespace cimport {

enum class Lang { C, Cpp };

struct Token
{
    enum Kind : unsigned char {
        Identifier,   // also contextual words (override, final, ...) and, in C, C++-only keywords
        Keyword,      // language keyword (depends on Lang)
        Number,       // pp-number: 42, 0x1F, 1'000, 1.5e-3f, 10_km
        String,       // "..." with optional prefix (u8, u, U, L, R...) and user-defined suffix
        Char,         // '...' with optional prefix
        Punct,        // operator / punctuator (longest match)
        Preprocessor, // a whole directive, from '#' to the end of line (continuations included)
        Unknown,      // any other character
        End           // sentinel returned for out-of-range indexes
    };

    Kind kind = End;
    int offset = 0;  // index of the first character in the original source
    int length = 0;  // length in the original source (line splices included)
    int line = 0;    // 1-based
    int column = 0;  // 1-based
    QString text;    // spelling, line splices removed

    int end() const { return offset + length; }
    bool is(const char *s) const { return text == QLatin1String(s); }
    bool isPunct(const char *s) const { return kind == Punct && is(s); }
    bool isKeyword(const char *s) const { return kind == Keyword && is(s); }
    bool isIdentifier() const { return kind == Identifier; }
    bool isWord() const { return kind == Identifier || kind == Keyword; }
    bool isLiteral() const { return kind == Number || kind == String || kind == Char; }
};

bool isKeyword(const QString &word, Lang lang);

// Splits source code into tokens. Comments are skipped, every preprocessor
// directive becomes a single Preprocessor token. Problems (unterminated
// comments / literals) are appended to `diagnostics` when it is not null.
QVector<Token> tokenize(const QString &source, Lang lang, QList<Diagnostic> *diagnostics = nullptr);

// Tokens of a translation unit plus a bracket matching table.
// Bracket matching is error tolerant: an unclosed opener ends right before the
// closer that implicitly closes it (or at the end of input); a closer without
// an opener is "stray". ')' and ']' never close across an open '{'.
class TokenStream
{
public:
    TokenStream() = default;
    TokenStream(const QString &source, Lang lang, QList<Diagnostic> *diagnostics = nullptr);

    int size() const { return m_tokens.size(); }
    Lang language() const { return m_lang; }
    const QString &source() const { return m_source; }
    const QVector<Token> &tokens() const { return m_tokens; }

    const Token &at(int i) const { return (i >= 0 && i < m_tokens.size()) ? m_tokens.at(i) : m_end; }
    bool isPunct(int i, const char *s) const { return at(i).isPunct(s); }
    bool isKeyword(int i, const char *s) const { return at(i).isKeyword(s); }
    bool isWord(int i, const char *s) const { return at(i).isWord() && at(i).is(s); }

    // Brackets: ( [ {
    bool isOpener(int i) const;
    bool isCloser(int i) const;
    // For an opener: whether a closer of the right kind was found.
    bool matched(int i) const;
    // For an opener: index of its closer (matched) or of the last token of the group.
    int close(int i) const;
    // For an opener: exclusive end of the group's content. For other tokens i + 1.
    int contentEnd(int i) const;
    // Index of the first token after the group starting at i (i + 1 for non-openers).
    int after(int i) const;
    // For a closer: the index of its opener, -1 if stray.
    int opener(int i) const;

    // Source text of tokens [begin, end): tokens are joined with a single space
    // where the source had whitespace or comments between them; preprocessor
    // tokens are skipped. Literal contents are kept verbatim.
    QString text(int begin, int end) const;
    // Same for an arbitrary, ascending list of token indexes.
    QString text(const QVector<int> &indexes) const;

    // Skips a template argument list: i is the index of '<'. Returns the index
    // after the matching '>' or -1 when no plausible end is found before `limit`.
    int skipTemplateArgs(int i, int limit) const;

private:
    void computeMatches();
    void computeTemplateMatches();

    QString m_source;
    Lang m_lang = Lang::Cpp;
    QVector<Token> m_tokens;
    QVector<int> m_match; // opener -> close index; closer -> opener index or -1; others -1
    QVector<bool> m_matched;
    QVector<int> m_templateEnd; // '<' -> index after its '>' (0: none)
    Token m_end;
};

} // namespace cimport
} // namespace afce

#endif // IMPORTTOKENIZER_H
