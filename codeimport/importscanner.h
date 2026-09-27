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

// Internal header of the C / C++ importer: discovery of function definitions.

#ifndef IMPORTSCANNER_H
#define IMPORTSCANNER_H

#include "codeimporter.h"
#include "importtokenizer.h"

#include <QVector>

namespace afce {
namespace cimport {

struct FunctionDef
{
    FunctionInfo info;
    int declBegin = 0;        // first token of the definition
    int bodyBegin = 0;        // first token inside the body braces
    int bodyEnd = 0;          // exclusive end of the body content
    int paramBegin = 0;       // tokens of the parameter list (without parentheses)
    int paramEnd = 0;
    bool isMain = false;      // unqualified free function `main`
    bool fragment = false;    // no definitions found: the whole file is imported as statements
    bool functionTryBlock = false;
};

// Finds function definitions at namespace / class / extern "C" level.
QVector<FunctionDef> scanFunctions(const TokenStream &tokens, QList<Diagnostic> &diagnostics);

// Upper-case macro-like identifier (Q_OBJECT, DEBUG_PRINT, ...).
bool isMacroLikeName(const QString &name);

} // namespace cimport
} // namespace afce

#endif // IMPORTSCANNER_H
