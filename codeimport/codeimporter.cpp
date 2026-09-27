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

// C / C++ -> AFC flowchart importer.
//
// Pipeline (no external parser, error tolerant, never recursing on raw
// nesting depth):
//   importtokenizer  - tokens with source offsets + bracket matching table
//   importscanner    - function definitions at namespace / class level
//   importparser     - recursive-descent statement parser -> small AST
//   importexpr       - heuristics for expressions and declarations
//   importbuilder    - AST -> <algorithm version="1.3"> document

#include "codeimporter.h"

#include "importbuilder.h"
#include "importscanner.h"
#include "importtokenizer.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace afce {

class CodeImporterPrivate
{
public:
    ImportOptions options;
    std::unique_ptr<cimport::TokenStream> tokens;
    QVector<cimport::FunctionDef> definitions;
    QList<FunctionInfo> functions;
    QList<Diagnostic> parseDiagnostics;
    mutable QList<Diagnostic> flowchartDiagnostics;
};

namespace {

cimport::Lang resolveLanguage(ImportOptions::Language language, const QString &fileName)
{
    switch (language) {
    case ImportOptions::Language::C:
        return cimport::Lang::C;
    case ImportOptions::Language::Cpp:
        return cimport::Lang::Cpp;
    case ImportOptions::Language::Auto:
        break;
    }
    const QString suffix = QFileInfo(fileName).suffix();
    if (suffix == QLatin1String("c"))
        return cimport::Lang::C;
    return cimport::Lang::Cpp; // .cpp .cc .cxx .c++ .hpp .hh .hxx .h .ino, unknown: C++ (superset)
}

QDomDocument emptyDocument()
{
    QDomDocument doc(QStringLiteral("AFC"));
    QDomElement algorithm = doc.createElement(QStringLiteral("algorithm"));
    algorithm.setAttribute(QStringLiteral("version"), QStringLiteral("1.3"));
    algorithm.appendChild(doc.createElement(QStringLiteral("branch")));
    doc.appendChild(algorithm);
    return doc;
}

} // namespace

CodeImporter::CodeImporter(const ImportOptions &options)
    : d(new CodeImporterPrivate)
{
    d->options = options;
}

CodeImporter::~CodeImporter() = default;

void CodeImporter::setOptions(const ImportOptions &options)
{
    d->options = options;
}

ImportOptions CodeImporter::options() const
{
    return d->options;
}

bool CodeImporter::parse(const QString &source, const QString &fileName)
{
    d->parseDiagnostics.clear();
    d->flowchartDiagnostics.clear();
    d->functions.clear();
    d->definitions.clear();

    const cimport::Lang lang = resolveLanguage(d->options.language, fileName);
    d->tokens.reset(new cimport::TokenStream(source, lang, &d->parseDiagnostics));
    d->definitions = cimport::scanFunctions(*d->tokens, d->parseDiagnostics);
    for (const cimport::FunctionDef &def : std::as_const(d->definitions))
        d->functions.append(def.info);

    if (d->definitions.isEmpty()) {
        Diagnostic diag;
        diag.severity = Diagnostic::Warning;
        diag.message = QCoreApplication::translate("afce::CodeImporter", "No function definitions found");
        d->parseDiagnostics.append(diag);
        return false;
    }
    return true;
}

QList<FunctionInfo> CodeImporter::functions() const
{
    return d->functions;
}

int CodeImporter::defaultFunctionIndex() const
{
    if (d->definitions.isEmpty())
        return -1;
    for (int i = 0; i < d->definitions.size(); ++i) {
        if (d->definitions.at(i).isMain)
            return i;
    }
    return 0;
}

QDomDocument CodeImporter::flowchart(int functionIndex) const
{
    d->flowchartDiagnostics.clear();
    if (!d->tokens || functionIndex < 0 || functionIndex >= d->definitions.size()) {
        Diagnostic diag;
        diag.severity = Diagnostic::Error;
        diag.message = QCoreApplication::translate("afce::CodeImporter", "There is no function with index %1")
                           .arg(functionIndex);
        d->flowchartDiagnostics.append(diag);
        return emptyDocument();
    }
    cimport::FlowchartBuilder builder(*d->tokens, d->options, d->flowchartDiagnostics);
    return builder.build(d->definitions.at(functionIndex));
}

QList<Diagnostic> CodeImporter::diagnostics() const
{
    return d->parseDiagnostics + d->flowchartDiagnostics;
}

} // namespace afce
