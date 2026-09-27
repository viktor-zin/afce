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

// Internal header of the C / C++ importer: statement AST and the
// recursive-descent statement parser.

#ifndef IMPORTPARSER_H
#define IMPORTPARSER_H

#include "importexpr.h"
#include "importtokenizer.h"

#include <QVector>

#include <deque>

namespace afce {
namespace cimport {

struct Node
{
    enum Kind {
        Expression,   // expression statement (expr)
        Declaration,  // declaration statement (expr)
        If,           // expr = condition, children = then, elseChildren = else, initStmts = C++17 init
        While,        // expr = condition, children = body
        DoWhile,      // expr = condition, children = body
        For,          // init / expr (condition) / step, children = body, initStmts = parsed init
        RangeFor,     // init = loop variable declaration, step = range, initStmts = C++20 init
                      // (also Qt's foreach (var, container))
        Forever,      // Qt's forever { ... }: endless loop
        Switch,       // expr = condition, children = body (with label nodes), initStmts = C++17 init
        CaseLabel,    // expr = value
        DefaultLabel,
        GotoLabel,    // label: (dropped)
        Return,       // expr = value (may be empty)
        Break,
        Continue,
        Goto,         // expr = label
        Try,          // children = try block (handlers are dropped)
        Asm,
        Skip,         // using / typedef / static_assert
        LocalType,    // local struct / class / union / enum definition; expr = declarators after '}'
        Preprocessor,
        Error,        // unparsable statement (range)
        Stray,        // unexpected token (range)
        TooDeep       // nesting limit exceeded (range)
    };

    Kind kind = Expression;
    Range range;       // the whole statement (without a trailing ';')
    Range expr;
    Range init;
    Range step;
    Range body;        // loops: tokens of the body statement
    QVector<Node *> initStmts;
    QVector<Node *> children;
    QVector<Node *> elseChildren;
    bool hasElse = false;
    bool nestedLabels = false;     // switch: case labels inside nested statements
    bool missingSemicolon = false; // expression / declaration without ';'
    bool orphan = false;           // case / default label outside a switch
    bool macroBlock = false;       // expression: MACRO(args) directly followed by a { block }
};

class StatementParser
{
public:
    static const int MaxDepth = 200;

    StatementParser(const TokenStream &ts, std::deque<Node> &arena);

    // Parses the statements in tokens [begin, end) (a function body).
    QVector<Node *> parse(int begin, int end);

private:
    struct SwitchContext
    {
        Node *node;
        const QVector<Node *> *list;
    };

    Node *make(Node::Kind kind, int begin, int end);
    void parseList(int begin, int end, QVector<Node *> &out, int depth);
    int parseStatement(int pos, int end, QVector<Node *> &out, int depth);
    int parseStatementImpl(int pos, int end, QVector<Node *> &out, int depth);
    int parseSub(int pos, int end, QVector<Node *> &out, int depth);
    int parseSimple(int pos, int end, QVector<Node *> &out);
    int parseIf(int pos, int end, QVector<Node *> &out, int depth);
    int parseWhile(int pos, int end, QVector<Node *> &out, int depth);
    int parseDo(int pos, int end, QVector<Node *> &out, int depth);
    int parseFor(int pos, int end, QVector<Node *> &out, int depth);
    int parseSwitch(int pos, int end, QVector<Node *> &out, int depth);
    int parseCase(int pos, int end, QVector<Node *> &out);
    int parseTry(int pos, int end, QVector<Node *> &out, int depth);
    int parseAsm(int pos, int end, QVector<Node *> &out);
    int parseLocalType(int pos, int end, QVector<Node *> &out);
    int parseQtForeach(int pos, int end, QVector<Node *> &out, int depth);
    int parseKeywordStatement(Node::Kind kind, int pos, int end, QVector<Node *> &out);
    // Parses "( [init;] cond )" at p for if / switch; returns the position after ')' or -1.
    int parseCondition(Node *n, int p, int end, int depth, bool allowInit);
    int error(int pos, int end, QVector<Node *> &out);
    void noteLabel(Node *label, const QVector<Node *> &out);

    // Index of the statement-terminating ';' (or where the statement ends
    // without one: a '}' or a statement keyword); `missing` is set then.
    int findStatementEnd(int start, int end, bool *missing) const;
    // Recovery point after an error: after the next top-level ';' or at the next '}'.
    int recover(int start, int end) const;
    int findLabelColon(int start, int end) const;
    int stripSemicolon(int begin, int end) const;
    // Iteratively skips one statement (used when the nesting limit is exceeded).
    int skipStatement(int pos, int end) const;

    const TokenStream &ts;
    std::deque<Node> &arena;
    QVector<SwitchContext> switches;
};

} // namespace cimport
} // namespace afce

#endif // IMPORTPARSER_H
