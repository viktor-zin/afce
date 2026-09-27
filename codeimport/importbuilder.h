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

// Internal header of the C / C++ importer: statement AST -> AFC document.
//
// Mapping overview (details and examples: tests/tst_codeimport.cpp):
//   { ... }              flattened;  ;  ignored
//   if / else if / else  if (else-if chains nest in the "no" branch); C++17 init emitted before
//   while / do-while     pre / post
//   for                  CStyle: forc;  While: init + pre + body + step (falls back to forc when the
//                        body continues this loop or hoisting the init changes scoping);
//                        Arithmetic: for var = from .. to (inclusive) for canonical loops, else forc
//   range-for, foreach   foreach (var : range)
//   switch               case: labels grouped, trailing break removed, fall-through duplicated,
//                        default last and always present; Duff's device -> process
//   return/break/continue return / break / continue (trailing `return 0;` of main omitted)
//   goto, labels, try, asm, preprocessor  graceful fallbacks + diagnostics
//   declarations         T x = e -> assign; T x(a) / T x{a} -> process; no initializer -> nothing
//   expressions          I/O -> io / ou (printf formats decomposed), assignments (compound ones
//                        expanded), ++/--, free function call -> call, anything else -> process

#ifndef IMPORTBUILDER_H
#define IMPORTBUILDER_H

#include "codeimporter.h"
#include "importparser.h"
#include "importscanner.h"

#include <QDomDocument>
#include <QDomElement>
#include <QHash>

namespace afce {
namespace cimport {

class FlowchartBuilder
{
public:
    FlowchartBuilder(const TokenStream &ts, const ImportOptions &options, QList<Diagnostic> &diagnostics);

    QDomDocument build(const FunctionDef &def);

    // Parsed printf conversion (exposed for tests).
    struct FormatPiece
    {
        bool literal = true;
        QString text;       // literal text (escapes verbatim) / whole conversion spec
        QChar conversion;   // d, s, f, ...
        QString length;     // h, l, ll, ...
        bool flags = false;
        bool width = false;
        bool precision = false;
        bool star = false;
    };
    static bool parsePrintfFormat(const QString &format, QVector<FormatPiece> *pieces);

private:
    struct Item
    {
        QString text;
        bool literal = false; // text is the content of a string literal (without quotes)
    };

    QString text(Range r) const { return ts.text(r.begin, r.end); }
    QDomElement element(const char *tag);
    void setAttr(QDomElement &e, const char *name, const QString &value);
    void addProcess(QDomElement &branch, const QString &text);
    void addCall(QDomElement &branch, const QString &text);
    void addAssign(QDomElement &branch, const QString &dest, const QString &src);
    void addList(QDomElement &branch, const char *tag, const QStringList &items);
    QDomElement addLoop(QDomElement &branch, const char *tag);
    void diag(Diagnostic::Severity severity, int token, const QString &message);

    void buildList(const QVector<Node *> &list, QDomElement &branch, bool functionTop = false);
    void buildNode(const Node *n, QDomElement &branch);
    void buildLeaf(const Node *n, QDomElement &branch);
    void buildIf(const Node *n, QDomElement &branch);
    void buildLoop(const Node *n, QDomElement &branch);
    void buildRangeFor(const Node *n, QDomElement &branch);
    void buildTry(const Node *n, QDomElement &branch);
    void noteMissingSemicolon(const Node *n);
    void buildFor(const Node *n, QDomElement &branch);
    void buildForC(const Node *n, QDomElement &branch);
    void buildForWhile(const Node *n, QDomElement &branch);
    bool whileStyleAllowed(const Node *n);
    void buildForArithmetic(const Node *n, QDomElement &branch);

    // switch -> case: grouping is computed first (planSwitch, large frame),
    // then emitted (buildSwitch, small frame on the recursion path).
    struct SwitchPlan
    {
        enum Mode { Nothing, AsText, Inline, Case } mode = Nothing;
        QVector<QStringList> values;                // per case branch
        QVector<QVector<Node *>> bodies;            // per case branch
        QVector<Node *> otherwise;                  // default branch / inline statements
    };
    SwitchPlan planSwitch(const Node *n);
    void buildSwitch(const Node *n, QDomElement &branch);
    bool canonicalFor(const Node *n, QString *var, QString *from, QString *to) const;
    // Names declared by the declaration statements of a list (not nested).
    QStringList declaredNames(const QVector<Node *> &list) const;
    void collectDeclarations(const QVector<Node *> &list);
    // While style: moving the for-init declaration out of the loop would change
    // what a name refers to (declared elsewhere in the function / used after the loop).
    bool hoistingChangesScope(const Node *n) const;
    QString boundMinusOne(Range bound) const;
    bool modifies(const QString &var, Range body) const;

    void mapExpressionStatement(Range r, QDomElement &branch);
    void mapSingleExpression(Range r, QDomElement &branch);
    void mapDeclaration(Range r, QDomElement &branch);
    bool tryInput(Range r, QDomElement &branch);
    bool tryScanf(Range r, const QVector<Range> &args, int formatIndex, bool secure, QDomElement &branch);
    bool tryOutput(Range r, QDomElement &branch);
    bool tryPrintf(Range r, const QVector<Range> &args, int formatIndex, QDomElement &branch);
    bool tryAssignment(Range r, QDomElement &branch);
    bool tryIncrement(Range r, QDomElement &branch);

    int streamEnd(Range r, std::initializer_list<const char *> names) const;
    bool literalContent(Range r, QString *content) const;
    QString withoutAddressOf(Range r) const;
    bool isManipulator(Range item);
    bool isVariableName(const QString &name);
    bool isGetchar(Range r) const;

    bool terminates(const QVector<Node *> &list) const;
    bool containsJump(const QVector<Node *> &list, Node::Kind jump) const;
    bool rangeContainsKeyword(Range r, const char *keyword) const;

    const TokenStream &ts;
    const ImportOptions options;
    QList<Diagnostic> &diags;
    QDomDocument doc;
    const FunctionDef *def = nullptr;
    bool warnedGoto = false;
    bool warnedPreprocessor = false;
    QHash<QString, bool> variableCache;
    QHash<QString, int> plainDeclarations;      // name -> declarations (and parameters) in the function
    QHash<QString, QVector<Range>> loopScopes;  // name -> for statements that declare it in their init
};

} // namespace cimport
} // namespace afce

#endif // IMPORTBUILDER_H
