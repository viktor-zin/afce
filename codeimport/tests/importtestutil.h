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

// Shared helpers of the importer tests: a compact, readable serializer of
// AFC documents and a structural validator (SPEC section 2).

#ifndef IMPORTTESTUTIL_H
#define IMPORTTESTUTIL_H

#include "codeimporter.h"

#include <QDomDocument>
#include <QString>
#include <QStringList>

namespace importtest {

// One line per block, two spaces of indentation per nesting level:
//   process x | assign x := e | io a,b | ou "s",x | call f(x) | return v | break | continue
//   if (c) / else | pre (c) | post (c) | for v = a .. b | forc (i; c; s) | foreach (v : r)
//   case (e) with "= values" / "default" branch labels
QString serialize(const QDomDocument &doc);

// Structural problems of a document (empty list = valid).
QStringList validate(const QDomDocument &doc);

// Options from a compact spec: comma separated words
//   c, cpp, for=cstyle|while|arithmetic, keepdecl, noio, nocalls, noexpand, exact, keepreturn
afce::ImportOptions options(const QString &spec);

struct Result
{
    bool parsed = false;
    QString chart;                 // serialized body
    QDomDocument doc;
    QStringList problems;          // validator output
    QList<afce::Diagnostic> diagnostics;
    QString diagnosticsText() const;
    int count(afce::Diagnostic::Severity severity) const;
};

// Imports function `function` (default: the default function) of `source`.
Result import(const QString &source, const afce::ImportOptions &options = afce::ImportOptions(),
              const QString &function = QString(), const QString &fileName = QString());

// Wraps statements into `void f() { ... }` and imports it.
Result body(const QString &statements, const QString &optionSpec = QString());

// Validator problems of every document produced by import() / body() since
// the last call (test classes check this in their cleanup() slot, so the
// validator is applied to every output).
QStringList takeRecordedProblems();

QString readFile(const QString &path);

} // namespace importtest

#endif // IMPORTTESTUTIL_H
