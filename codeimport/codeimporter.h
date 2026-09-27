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

#ifndef CODEIMPORTER_H
#define CODEIMPORTER_H

#include <QDomDocument>
#include <QList>
#include <QString>

#include <memory>

namespace afce {

struct ImportOptions
{
    enum class Language { Auto, C, Cpp };
    enum class ForStyle { CStyle, While, Arithmetic };

    Language language = Language::Auto;   // Auto: by file suffix if known, else C++ (superset)
    ForStyle forStyle = ForStyle::CStyle; // CStyle: forc; While: init + pre + step; Arithmetic: `for` when canonical, else forc
    bool keepDeclarations = false;        // true: every declaration statement -> process block verbatim
    bool detectIO = true;                 // scanf/printf/puts/gets/getchar/cin/cout/getline -> io/ou
    bool callsAsSubroutine = true;        // statement that is a single free-function call -> call block
    bool expandCompoundAssign = true;     // x += e -> x := x + e ; i++ -> i := i + 1
    bool exactOutput = false;             // keep "\n", endl, manipulators in output lists (round-trip mode)
    bool omitMainReturn = true;           // drop a trailing `return 0;` at the end of main
};

struct FunctionInfo
{
    QString name;        // "main", "Foo::bar", "operator+"
    QString returnType;  // "int" (empty for constructors/destructors)
    QString parameters;  // "int a, int b" (whitespace-normalized)
    int line = 0;        // 1-based line of the definition start
    int startOffset = 0; // QString index of the first char of the definition (incl. return type / template prefix)
    int endOffset = 0;   // QString index one past the closing '}' of the body
};

struct Diagnostic
{
    enum Severity { Info, Warning, Error };

    Severity severity = Warning;
    int line = 0;        // 1-based, 0 if unknown
    int column = 0;      // 1-based, 0 if unknown
    QString message;     // translated via QCoreApplication::translate("afce::CodeImporter", ...)
};

class CodeImporterPrivate;

// Builds AFC flowcharts from C / C++ source code.
class CodeImporter
{
public:
    explicit CodeImporter(const ImportOptions &options = ImportOptions());
    ~CodeImporter();
    CodeImporter(const CodeImporter &) = delete;
    CodeImporter &operator=(const CodeImporter &) = delete;

    void setOptions(const ImportOptions &options);
    ImportOptions options() const;

    // Parses a translation unit. Returns false only if no function definition was found.
    // (When there is no definition but the text contains statements, e.g. a pasted
    // code fragment, the whole text is offered as one function with an empty name.)
    // The language (ImportOptions::language, Auto: by the suffix of fileName) is
    // applied here; the other options only affect flowchart().
    bool parse(const QString &source, const QString &fileName = QString());

    QList<FunctionInfo> functions() const;
    // Index of `main` if present, otherwise 0; -1 if there are no functions.
    int defaultFunctionIndex() const;

    // Full AFC document: <algorithm version="1.3" name=".." params=".." returns=".."><branch>...</branch></algorithm>
    QDomDocument flowchart(int functionIndex) const;

    // Parse diagnostics followed by the diagnostics of the last flowchart() call.
    QList<Diagnostic> diagnostics() const;

private:
    std::unique_ptr<CodeImporterPrivate> d;
};

} // namespace afce

#endif // CODEIMPORTER_H
