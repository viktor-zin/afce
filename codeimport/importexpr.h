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

// Internal header of the C / C++ importer: token-range helpers for
// expressions and declarations (heuristic, no symbol table).

#ifndef IMPORTEXPR_H
#define IMPORTEXPR_H

#include "importtokenizer.h"

#include <QString>
#include <QVector>

namespace afce {
namespace cimport {

struct Range
{
    int begin = 0;
    int end = 0;

    Range() = default;
    Range(int b, int e) : begin(b), end(e) {}
    bool isEmpty() const { return end <= begin; }
    int size() const { return end > begin ? end - begin : 0; }
};

// Builtin type keyword (int, unsigned, auto, ...).
bool isBuiltinType(const Token &t);
// Declaration specifier / cv-qualifier that may precede the type (static, const, ...).
bool isDeclSpecifier(const Token &t);
// Keyword that can only start a statement (if, for, return, ...).
bool isStatementKeyword(const Token &t);
// Assignment operator token (=, +=, ..., >>=).
bool isAssignmentOp(const Token &t);

// End of a (possibly ::-qualified) name starting at k, including template
// argument lists. With `strictTemplates` a template argument list is only
// accepted when followed by '(', '::' or '{' (expression context).
// Returns k when there is no name at k.
int qualifiedNameEnd(const TokenStream &ts, int k, int limit, bool strictTemplates);

// Splits a range at top-level occurrences of the punctuator `sep`.
QVector<Range> splitTopLevel(const TokenStream &ts, Range r, const char *sep);
// First top-level occurrence of `sep` in r, -1 if none.
int findTopLevel(const TokenStream &ts, Range r, const char *sep);
// First top-level assignment operator in r; -1 if none or when a '?' precedes it.
int findAssignment(const TokenStream &ts, Range r);
// Whether r contains the token kind/text at top level or anywhere.
bool containsPunct(const TokenStream &ts, Range r, const char *p);

// identifier, literal, call, member access, subscript, parenthesized expression.
bool isPrimaryExpression(const TokenStream &ts, Range r);
// Name + '(' args ')' covering the whole range. `name` gets the compact callee
// text (no spaces), `args` the argument ranges.
bool parseCall(const TokenStream &ts, Range r, QString *name, QVector<Range> *args);
// Compact text (tokens joined without spaces).
QString compactText(const TokenStream &ts, Range r);

// Heuristic declaration detection (see the importer documentation).
bool looksLikeDeclaration(const TokenStream &ts, Range r);

struct Declarator
{
    enum Init { None, Assign, Direct, Invalid };
    Range range;       // declarator tokens including the initializer
    QString name;      // declared name ("x", "[a, b]")
    Init init = None;
    Range initializer; // Assign: expression after '='; Direct: the (...) / {...} group
    bool prototype = false; // Direct with a parameter-like list: a local function declaration
};

struct Declaration
{
    bool ok = false;
    Range type;        // specifiers + type (from the start of the statement)
    QVector<Declarator> declarators;
};

Declaration parseDeclaration(const TokenStream &ts, Range r);

} // namespace cimport
} // namespace afce

#endif // IMPORTEXPR_H
