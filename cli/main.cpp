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

// afce-cli: console front end of AFCE (no GUI).
//
//   afce-cli functions <source> [--json]
//   afce-cli import <source> [--function NAME | --index N] [--output file.afc] [--lang c|cpp]
//            [--for-style cstyle|while|arithmetic] [--keep-declarations] [--no-io] [--no-calls]
//            [--no-expand-assign] [--exact-output] [--keep-main-return] [--generate LANG] [--diagnostics]
//   afce-cli generate <file.afc> --lang LANG [--output file]
//   afce-cli languages
//
// A source or flowchart file name "-" means standard input.

#include "afceutil.h"
#include "codeimporter.h"
#include "sourcecodegenerator.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QStringDecoder>

#include <cstdio>

namespace {

enum ExitCode { ExitOk = 0, ExitFailure = 1, ExitUsage = 2 };

void writeTo(FILE *stream, const QString &text)
{
    const QByteArray data = text.toUtf8();
    std::fwrite(data.constData(), 1, size_t(data.size()), stream);
    std::fflush(stream);
}

void printError(const QString &message)
{
    writeTo(stderr, QStringLiteral("afce-cli: ") + message + QLatin1Char('\n'));
}

QString nativeName(const QString &fileName)
{
    return fileName == QLatin1String("-") ? QCoreApplication::translate("afce-cli", "<stdin>") : QDir::toNativeSeparators(fileName);
}

bool readInput(const QString &fileName, QByteArray *data, QString *errorMessage)
{
    QFile file;
    bool opened = false;
    if (fileName == QLatin1String("-")) {
        opened = file.open(stdin, QIODevice::ReadOnly);
    } else {
        file.setFileName(fileName);
        opened = file.open(QIODevice::ReadOnly);
    }
    if (!opened) {
        *errorMessage = QCoreApplication::translate("afce-cli", "cannot open '%1': %2")
                            .arg(nativeName(fileName), file.errorString());
        return false;
    }
    *data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        *errorMessage = QCoreApplication::translate("afce-cli", "cannot read '%1': %2")
                            .arg(nativeName(fileName), file.errorString());
        return false;
    }
    return true;
}

// Writes text (UTF-8) to fileName, or to stdout when fileName is empty or "-".
bool writeOutput(const QString &fileName, const QString &text, QString *errorMessage)
{
    if (fileName.isEmpty() || fileName == QLatin1String("-")) {
        writeTo(stdout, text);
        return true;
    }
    QSaveFile file(fileName);
    file.setDirectWriteFallback(true);
    const QByteArray data = text.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *errorMessage = QCoreApplication::translate("afce-cli", "cannot write '%1': %2")
                            .arg(nativeName(fileName), file.errorString());
        return false;
    }
    return true;
}

QString withTrailingNewline(const QString &text)
{
    return text.endsWith(QLatin1Char('\n')) ? text : text + QLatin1Char('\n');
}

QString severityName(afce::Diagnostic::Severity severity)
{
    switch (severity) {
    case afce::Diagnostic::Info:
        return QStringLiteral("note");
    case afce::Diagnostic::Warning:
        return QStringLiteral("warning");
    case afce::Diagnostic::Error:
        break;
    }
    return QStringLiteral("error");
}

// Prints diagnostics in the usual compiler format: file:line:column: severity: message.
// Errors are always printed, warnings and notes only when all is true.
void printDiagnostics(const QString &fileName, const QList<afce::Diagnostic> &diagnostics, bool all)
{
    for (const afce::Diagnostic &d : diagnostics) {
        if (!all && d.severity != afce::Diagnostic::Error)
            continue;
        QString location = nativeName(fileName);
        if (d.line > 0) {
            location += QStringLiteral(":%1").arg(d.line);
            if (d.column > 0)
                location += QStringLiteral(":%1").arg(d.column);
        }
        writeTo(stderr, QStringLiteral("%1: %2: %3\n").arg(location, severityName(d.severity), d.message));
    }
}

// ---------------------------------------------------------------------------
// generators

QString generatorId(const QString &path)
{
    return QFileInfo(path).completeBaseName();
}

QString generatorName(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return generatorId(path);
    const QJsonObject names = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("name")).toObject();
    QString name = names.value(QStringLiteral("en_US")).toString();
    return name.isEmpty() ? generatorId(path) : name;
}

// LANG = generator id (json base name) or a path to a .json rule file.
bool resolveGenerator(const QString &lang, QString *path, QString *errorMessage)
{
    const QFileInfo asFile(lang);
    if (lang.endsWith(QLatin1String(".json"), Qt::CaseInsensitive) || lang.contains(QLatin1Char('/'))
        || lang.contains(QLatin1Char('\\'))) {
        if (!asFile.isFile()) {
            *errorMessage = QCoreApplication::translate("afce-cli", "generator rule file '%1' does not exist")
                                .arg(QDir::toNativeSeparators(lang));
            return false;
        }
        *path = asFile.absoluteFilePath();
    } else {
        const QStringList files = afce::generatorFiles();
        QStringList ids;
        for (const QString &file : files) {
            if (generatorId(file).compare(lang, Qt::CaseInsensitive) == 0) {
                *path = file;
                break;
            }
            ids << generatorId(file);
        }
        if (path->isEmpty()) {
            *errorMessage = QCoreApplication::translate("afce-cli", "unknown language '%1' (available: %2)")
                                .arg(lang, ids.join(QStringLiteral(", ")));
            return false;
        }
    }
    QFile file(*path);
    if (!file.open(QIODevice::ReadOnly)) {
        *errorMessage = QCoreApplication::translate("afce-cli", "cannot open '%1': %2")
                            .arg(QDir::toNativeSeparators(*path), file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        *errorMessage = QCoreApplication::translate("afce-cli", "invalid generator rules '%1': %2")
                            .arg(QDir::toNativeSeparators(*path),
                                 parseError.error != QJsonParseError::NoError ? parseError.errorString()
                                                                              : QCoreApplication::translate("afce-cli", "not a JSON object"));
        return false;
    }
    return true;
}

bool generateCode(const QDomDocument &doc, const QString &lang, QString *code, QString *errorMessage)
{
    QString rules;
    if (!resolveGenerator(lang, &rules, errorMessage))
        return false;
    SourceCodeGenerator generator;
    generator.loadRule(rules);
    *code = generator.applyRule(doc);
    return true;
}

// ---------------------------------------------------------------------------
// flowcharts

// Converts documents of old AFCE versions (io/ou attributes t1..t8) like the editor does.
void upgradeDocument(QDomElement element)
{
    if (element.tagName() == QLatin1String("io") || element.tagName() == QLatin1String("ou")) {
        QStringList items;
        for (int i = 1; i <= 8; ++i) {
            const QString attr = QStringLiteral("t%1").arg(i);
            if (element.hasAttribute(attr)) {
                const QString value = element.attribute(attr);
                if (!value.isEmpty())
                    items << value;
                element.removeAttribute(attr);
            }
        }
        if (!items.isEmpty()) {
            QStringList vars = afce::splitList(element.attribute(QStringLiteral("vars")));
            vars << items;
            element.setAttribute(QStringLiteral("vars"), afce::joinList(vars));
        }
    }
    for (QDomElement child = element.firstChildElement(); !child.isNull(); child = child.nextSiblingElement())
        upgradeDocument(child);
}

bool loadFlowchart(const QString &fileName, QDomDocument *doc, QString *errorMessage)
{
    QByteArray data;
    if (!readInput(fileName, &data, errorMessage))
        return false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const QDomDocument::ParseResult result = doc->setContent(data);
    const bool parsed = bool(result);
    const QString xmlError = result.errorMessage;
    const qint64 errorLine = result.errorLine;
    const qint64 errorColumn = result.errorColumn;
#else
    // Qt 6.4 (Ubuntu 24.04, Debian 12) has no ParseResult yet
    QString xmlError;
    int errorLine = 0;
    int errorColumn = 0;
    const bool parsed = doc->setContent(data, &xmlError, &errorLine, &errorColumn);
#endif
    if (!parsed) {
        *errorMessage = QCoreApplication::translate("afce-cli", "%1:%2:%3: invalid XML: %4")
                            .arg(nativeName(fileName)).arg(errorLine).arg(errorColumn).arg(xmlError);
        return false;
    }
    const QDomElement root = doc->documentElement();
    if (root.tagName() != QLatin1String("algorithm") || root.firstChildElement(QStringLiteral("branch")).isNull()) {
        *errorMessage = QCoreApplication::translate("afce-cli", "'%1' is not an AFCE flowchart (expected <algorithm><branch>...)")
                            .arg(nativeName(fileName));
        return false;
    }
    upgradeDocument(root);
    return true;
}

// ---------------------------------------------------------------------------
// commands

QCommandLineOption helpOption()
{
    return QCommandLineOption(QStringList() << QStringLiteral("h") << QStringLiteral("help"),
                              QCoreApplication::translate("afce-cli", "Displays help for this command."));
}

// Help of a command: QCommandLineParser names the program after argv[0], use "afce-cli <command>".
QString commandHelp(const QCommandLineParser &parser, const QString &commandName)
{
    QString help = parser.helpText();
    const QString program = QCoreApplication::arguments().constFirst();
    const int lineEnd = help.indexOf(QLatin1Char('\n'));
    const int pos = help.indexOf(program);
    if (pos >= 0 && (lineEnd < 0 || pos < lineEnd))
        help.replace(pos, program.size(), commandName);
    return help;
}

// Parses the arguments of a command. Returns -1 to continue, otherwise the exit code.
int parseCommand(QCommandLineParser &parser, const QStringList &arguments, int minPositional, int maxPositional)
{
    const QCommandLineOption help = helpOption();
    parser.addOption(help);
    if (!parser.parse(arguments)) {
        printError(parser.errorText());
        return ExitUsage;
    }
    if (parser.isSet(help)) {
        writeTo(stdout, commandHelp(parser, arguments.constFirst()));
        return ExitOk;
    }
    const int count = int(parser.positionalArguments().size());
    if (count < minPositional || count > maxPositional) {
        printError(count < minPositional ? QCoreApplication::translate("afce-cli", "missing argument") : QCoreApplication::translate("afce-cli", "too many arguments"));
        writeTo(stderr, commandHelp(parser, arguments.constFirst()));
        return ExitUsage;
    }
    return -1;
}

// Source files are usually UTF-8; older Russian/Ukrainian sources are often
// in the Windows code page: fall back to the system codec, then windows-1251.
QString decodeSourceText(const QByteArray &data)
{
    QStringDecoder utf8(QStringDecoder::Utf8);
    const QString text = utf8.decode(data);
    if (!utf8.hasError())
        return text;
    QStringDecoder system(QStringDecoder::System);
    const QString local = system.decode(data);
    if (!system.hasError() && system.isValid() && local != text)
        return local;
    QStringDecoder cp1251("windows-1251");
    if (cp1251.isValid()) {
        const QString cyrillic = cp1251.decode(data);
        if (!cp1251.hasError())
            return cyrillic;
    }
    return text;
}

bool parseSource(afce::CodeImporter &importer, const QString &fileName, QString *errorMessage)
{
    QByteArray data;
    if (!readInput(fileName, &data, errorMessage))
        return false;
    const QString source = decodeSourceText(data);
    importer.parse(source, fileName == QLatin1String("-") ? QString() : fileName);
    return true;
}

bool parseLanguage(const QString &value, afce::ImportOptions::Language *language)
{
    const QString v = value.toLower();
    if (v == QLatin1String("c"))
        *language = afce::ImportOptions::Language::C;
    else if (v == QLatin1String("cpp") || v == QLatin1String("c++") || v == QLatin1String("cxx"))
        *language = afce::ImportOptions::Language::Cpp;
    else if (v == QLatin1String("auto"))
        *language = afce::ImportOptions::Language::Auto;
    else
        return false;
    return true;
}

int commandFunctions(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("afce-cli", "Lists the functions defined in a C/C++ source file."));
    parser.addPositionalArgument(QStringLiteral("source"), QCoreApplication::translate("afce-cli", "C or C++ source file (\"-\" = standard input)."));
    const QCommandLineOption jsonOption(QStringLiteral("json"),
        QCoreApplication::translate("afce-cli", "JSON output: name, returnType, parameters, line, startOffset, endOffset."));
    const QCommandLineOption langOption(QStringLiteral("lang"), QCoreApplication::translate("afce-cli", "Source language: c or cpp (default: by file suffix)."),
                                        QStringLiteral("c|cpp"));
    const QCommandLineOption diagnosticsOption(QStringLiteral("diagnostics"), QCoreApplication::translate("afce-cli", "Print all diagnostics to stderr."));
    parser.addOptions({jsonOption, langOption, diagnosticsOption});
    const int parsed = parseCommand(parser, arguments, 1, 1);
    if (parsed >= 0)
        return parsed;

    afce::ImportOptions options;
    if (parser.isSet(langOption) && !parseLanguage(parser.value(langOption), &options.language)) {
        printError(QCoreApplication::translate("afce-cli", "invalid --lang value '%1' (expected c or cpp)").arg(parser.value(langOption)));
        return ExitUsage;
    }
    const QString source = parser.positionalArguments().constFirst();
    afce::CodeImporter importer(options);
    QString error;
    if (!parseSource(importer, source, &error)) {
        printError(error);
        return ExitFailure;
    }
    printDiagnostics(source, importer.diagnostics(), parser.isSet(diagnosticsOption));
    const QList<afce::FunctionInfo> functions = importer.functions();

    if (parser.isSet(jsonOption)) {
        QJsonArray array;
        for (const afce::FunctionInfo &f : functions) {
            QJsonObject object;
            object.insert(QStringLiteral("name"), f.name);
            object.insert(QStringLiteral("returnType"), f.returnType);
            object.insert(QStringLiteral("parameters"), f.parameters);
            object.insert(QStringLiteral("line"), f.line);
            object.insert(QStringLiteral("startOffset"), f.startOffset);
            object.insert(QStringLiteral("endOffset"), f.endOffset);
            array.append(object);
        }
        writeTo(stdout, QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Indented)));
    } else {
        for (const afce::FunctionInfo &f : functions) {
            const QString signature = (f.returnType.isEmpty() ? QString() : f.returnType + QLatin1Char(' '))
                                      + f.name + QLatin1Char('(') + f.parameters + QLatin1Char(')');
            writeTo(stdout, QStringLiteral("%1\t%2\n").arg(f.line).arg(signature));
        }
    }
    if (functions.isEmpty()) {
        printError(QCoreApplication::translate("afce-cli", "no function definitions found in '%1'").arg(nativeName(source)));
        return ExitFailure;
    }
    return ExitOk;
}

int commandImport(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("afce-cli", "Builds the flowchart of a function of a C/C++ source file.\n"
                                        "Prints the flowchart (AFC XML) or, with --generate, source code."));
    parser.addPositionalArgument(QStringLiteral("source"), QCoreApplication::translate("afce-cli", "C or C++ source file (\"-\" = standard input)."));
    const QCommandLineOption functionOption(QStringLiteral("function"),
        QCoreApplication::translate("afce-cli", "Function to import (default: main, otherwise the first one)."), QStringLiteral("name"));
    const QCommandLineOption indexOption(QStringLiteral("index"),
        QCoreApplication::translate("afce-cli", "Index of the function to import (0-based, see the functions command)."), QStringLiteral("n"));
    const QCommandLineOption outputOption(QStringLiteral("output"), QCoreApplication::translate("afce-cli", "Output file (default: standard output)."),
                                          QStringLiteral("file"));
    const QCommandLineOption langOption(QStringLiteral("lang"), QCoreApplication::translate("afce-cli", "Source language: c or cpp (default: by file suffix)."),
                                        QStringLiteral("c|cpp"));
    const QCommandLineOption forStyleOption(QStringLiteral("for-style"),
        QCoreApplication::translate("afce-cli", "How for loops are imported: cstyle (C-style for block), while (init + while loop + step) "
           "or arithmetic (FOR block when possible). Default: cstyle."),
        QStringLiteral("cstyle|while|arithmetic"));
    const QCommandLineOption keepDeclarationsOption(QStringLiteral("keep-declarations"),
        QCoreApplication::translate("afce-cli", "Keep every declaration as a process block."));
    const QCommandLineOption noIoOption(QStringLiteral("no-io"), QCoreApplication::translate("afce-cli", "Do not turn input/output statements into I/O blocks."));
    const QCommandLineOption noCallsOption(QStringLiteral("no-calls"), QCoreApplication::translate("afce-cli", "Do not use subroutine call blocks."));
    const QCommandLineOption noExpandAssignOption(QStringLiteral("no-expand-assign"),
        QCoreApplication::translate("afce-cli", "Keep compound assignments (x += 1, i++) as they are."));
    const QCommandLineOption exactOutputOption(QStringLiteral("exact-output"),
        QCoreApplication::translate("afce-cli", "Keep line breaks and stream manipulators in output blocks."));
    const QCommandLineOption keepMainReturnOption(QStringLiteral("keep-main-return"),
        QCoreApplication::translate("afce-cli", "Keep a trailing 'return 0;' of main."));
    const QCommandLineOption generateOption(QStringLiteral("generate"),
        QCoreApplication::translate("afce-cli", "Print source code generated from the flowchart instead of the flowchart."), QStringLiteral("LANG"));
    const QCommandLineOption diagnosticsOption(QStringLiteral("diagnostics"), QCoreApplication::translate("afce-cli", "Print all diagnostics to stderr."));
    parser.addOptions({functionOption, indexOption, outputOption, langOption, forStyleOption, keepDeclarationsOption,
                       noIoOption, noCallsOption, noExpandAssignOption, exactOutputOption, keepMainReturnOption,
                       generateOption, diagnosticsOption});
    const int parsed = parseCommand(parser, arguments, 1, 1);
    if (parsed >= 0)
        return parsed;

    if (parser.isSet(functionOption) && parser.isSet(indexOption)) {
        printError(QCoreApplication::translate("afce-cli", "--function and --index cannot be used together"));
        return ExitUsage;
    }

    afce::ImportOptions options;
    if (parser.isSet(langOption) && !parseLanguage(parser.value(langOption), &options.language)) {
        printError(QCoreApplication::translate("afce-cli", "invalid --lang value '%1' (expected c or cpp)").arg(parser.value(langOption)));
        return ExitUsage;
    }
    if (parser.isSet(forStyleOption)) {
        const QString style = parser.value(forStyleOption).toLower();
        if (style == QLatin1String("cstyle"))
            options.forStyle = afce::ImportOptions::ForStyle::CStyle;
        else if (style == QLatin1String("while"))
            options.forStyle = afce::ImportOptions::ForStyle::While;
        else if (style == QLatin1String("arithmetic"))
            options.forStyle = afce::ImportOptions::ForStyle::Arithmetic;
        else {
            printError(QCoreApplication::translate("afce-cli", "invalid --for-style value '%1' (expected cstyle, while or arithmetic)").arg(style));
            return ExitUsage;
        }
    }
    options.keepDeclarations = parser.isSet(keepDeclarationsOption);
    options.detectIO = !parser.isSet(noIoOption);
    options.callsAsSubroutine = !parser.isSet(noCallsOption);
    options.expandCompoundAssign = !parser.isSet(noExpandAssignOption);
    options.exactOutput = parser.isSet(exactOutputOption);
    options.omitMainReturn = !parser.isSet(keepMainReturnOption);

    int index = -1;
    if (parser.isSet(indexOption)) {
        bool ok = false;
        index = parser.value(indexOption).toInt(&ok);
        if (!ok || index < 0) {
            printError(QCoreApplication::translate("afce-cli", "invalid --index value '%1'").arg(parser.value(indexOption)));
            return ExitUsage;
        }
    }

    // fail early on an unknown generator
    QString error;
    if (parser.isSet(generateOption)) {
        QString rules;
        if (!resolveGenerator(parser.value(generateOption), &rules, &error)) {
            printError(error);
            return ExitUsage;
        }
    }

    const QString source = parser.positionalArguments().constFirst();
    afce::CodeImporter importer(options);
    if (!parseSource(importer, source, &error)) {
        printError(error);
        return ExitFailure;
    }
    const QList<afce::FunctionInfo> functions = importer.functions();
    if (functions.isEmpty()) {
        printDiagnostics(source, importer.diagnostics(), true);
        printError(QCoreApplication::translate("afce-cli", "no function definitions found in '%1'").arg(nativeName(source)));
        return ExitFailure;
    }

    if (parser.isSet(functionOption)) {
        const QString name = parser.value(functionOption);
        for (int i = 0; i < functions.size() && index < 0; ++i) {
            if (functions.at(i).name == name)
                index = i;
        }
        for (int i = 0; i < functions.size() && index < 0; ++i) {
            if (functions.at(i).name.endsWith(QStringLiteral("::") + name))
                index = i;
        }
        if (index < 0) {
            QStringList names;
            for (const afce::FunctionInfo &f : functions)
                names << f.name;
            printError(QCoreApplication::translate("afce-cli", "function '%1' not found (available: %2)").arg(name, names.join(QStringLiteral(", "))));
            return ExitFailure;
        }
    } else if (index >= 0) {
        if (index >= functions.size()) {
            printError(QCoreApplication::translate("afce-cli", "--index %1 is out of range (%2 functions found)").arg(index).arg(functions.size()));
            return ExitFailure;
        }
    } else {
        index = importer.defaultFunctionIndex();
        if (index < 0)
            index = 0;
    }

    const QDomDocument doc = importer.flowchart(index);
    printDiagnostics(source, importer.diagnostics(), parser.isSet(diagnosticsOption));
    const QDomElement root = doc.documentElement();
    if (root.tagName() != QLatin1String("algorithm")) {
        printError(QCoreApplication::translate("afce-cli", "the importer did not produce a flowchart for '%1'").arg(functions.at(index).name));
        return ExitFailure;
    }

    QString output;
    if (parser.isSet(generateOption)) {
        if (!generateCode(doc, parser.value(generateOption), &output, &error)) {
            printError(error);
            return ExitFailure;
        }
    } else {
        output = doc.toString(2);
    }
    if (!writeOutput(parser.value(outputOption), withTrailingNewline(output), &error)) {
        printError(error);
        return ExitFailure;
    }
    return ExitOk;
}

int commandGenerate(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("afce-cli", "Generates source code from a flowchart."));
    parser.addPositionalArgument(QStringLiteral("file.afc"), QCoreApplication::translate("afce-cli", "Flowchart (\"-\" = standard input)."));
    const QCommandLineOption langOption(QStringLiteral("lang"),
        QCoreApplication::translate("afce-cli", "Generator id (see the languages command) or a path to a .json rule file."), QStringLiteral("LANG"));
    const QCommandLineOption outputOption(QStringLiteral("output"), QCoreApplication::translate("afce-cli", "Output file (default: standard output)."),
                                          QStringLiteral("file"));
    parser.addOptions({langOption, outputOption});
    const int parsed = parseCommand(parser, arguments, 1, 1);
    if (parsed >= 0)
        return parsed;
    if (!parser.isSet(langOption)) {
        printError(QCoreApplication::translate("afce-cli", "--lang is required"));
        return ExitUsage;
    }

    QString error;
    QString rules;
    if (!resolveGenerator(parser.value(langOption), &rules, &error)) {
        printError(error);
        return ExitUsage;
    }
    QDomDocument doc;
    if (!loadFlowchart(parser.positionalArguments().constFirst(), &doc, &error)) {
        printError(error);
        return ExitFailure;
    }
    QString code;
    if (!generateCode(doc, rules, &code, &error)) {
        printError(error);
        return ExitFailure;
    }
    if (!writeOutput(parser.value(outputOption), withTrailingNewline(code), &error)) {
        printError(error);
        return ExitFailure;
    }
    return ExitOk;
}

int commandLanguages(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("afce-cli", "Lists the ids of the available code generators."));
    const QCommandLineOption namesOption(QStringLiteral("names"), QCoreApplication::translate("afce-cli", "Also print the language names."));
    parser.addOption(namesOption);
    const int parsed = parseCommand(parser, arguments, 0, 0);
    if (parsed >= 0)
        return parsed;
    const QStringList files = afce::generatorFiles();
    if (files.isEmpty()) {
        printError(QCoreApplication::translate("afce-cli", "no code generators found"));
        return ExitFailure;
    }
    for (const QString &file : files) {
        if (parser.isSet(namesOption))
            writeTo(stdout, QStringLiteral("%1\t%2\n").arg(generatorId(file), generatorName(file)));
        else
            writeTo(stdout, generatorId(file) + QLatin1Char('\n'));
    }
    return ExitOk;
}

QString usage()
{
    return QCoreApplication::translate("afce-cli", "Usage: afce-cli <command> [options]\n"
              "\n"
              "AFCE command line tool: builds flowcharts from C/C++ code and code from flowcharts.\n"
              "\n"
              "Commands:\n"
              "  functions <source> [--json]         list the functions of a C/C++ source file\n"
              "  import <source> [options]           build the flowchart (AFC XML) of a function\n"
              "  generate <file.afc> --lang LANG     generate source code from a flowchart\n"
              "  languages                           list the code generators\n"
              "\n"
              "Run 'afce-cli <command> --help' for the options of a command.\n"
              "Options: -h, --help (this text), -v, --version.\n");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("afce-cli"));
    QCoreApplication::setApplicationVersion(afce::programVersion());
    afce::setupSearchPaths();

    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() < 2) {
        writeTo(stderr, usage());
        return ExitUsage;
    }
    const QString command = arguments.at(1);
    if (command == QLatin1String("-h") || command == QLatin1String("--help") || command == QLatin1String("help")) {
        writeTo(stdout, usage());
        return ExitOk;
    }
    if (command == QLatin1String("-v") || command == QLatin1String("--version")) {
        writeTo(stdout, QStringLiteral("afce-cli %1\n").arg(afce::programVersion()));
        return ExitOk;
    }

    // the parser of a command sees "afce-cli <command>" as the program name
    QStringList commandArguments = arguments.mid(2);
    commandArguments.prepend(QStringLiteral("afce-cli ") + command);

    if (command == QLatin1String("functions"))
        return commandFunctions(commandArguments);
    if (command == QLatin1String("import"))
        return commandImport(commandArguments);
    if (command == QLatin1String("generate"))
        return commandGenerate(commandArguments);
    if (command == QLatin1String("languages"))
        return commandLanguages(commandArguments);

    printError(QCoreApplication::translate("afce-cli", "unknown command '%1'").arg(command));
    writeTo(stderr, usage());
    return ExitUsage;
}
