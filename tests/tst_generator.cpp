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

// Tests of the code generator engine (SourceCodeGenerator) and of the rule files in generators/.
//
//  * engine unit tests with small inline rules;
//  * golden tests: tests/generators/*.afc x generators/*.json compared with
//    tests/generators/expected/<sample>.<id>.txt (AFCE_UPDATE_GOLDEN=1 regenerates them);
//  * syntax tests: the golden outputs are checked by python3 / node / ruby / perl when available;
//  * semantic tests: tests/generators/semantic/*.afc are generated for py, js, ruby, perl, c and cpp,
//    executed and their output compared with the matching .out file.

#include <QtTest>

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>

#include <cstdio>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "afceutil.h"
#include "sourcecodegenerator.h"

namespace {

const QString sourceDir = QStringLiteral(AFCE_SOURCE_DIR);

QString generatorsDir()
{
    return sourceDir + QStringLiteral("/generators");
}

QString samplesDir()
{
    return sourceDir + QStringLiteral("/tests/generators");
}

QDomDocument parseXml(const QString &xml)
{
    QDomDocument doc;
    const auto result = doc.setContent(xml);
    if (!result)
        qWarning() << "XML error:" << result.errorMessage << result.errorLine << result.errorColumn;
    return doc;
}

QString generate(const QByteArray &rule, const QString &body, const QString &algorithmAttributes = QString())
{
    const QString xml = QStringLiteral("<algorithm%1><branch>%2</branch></algorithm>")
                            .arg(algorithmAttributes.isEmpty() ? QString() : QLatin1Char(' ') + algorithmAttributes,
                                 body);
    SourceCodeGenerator generator;
    generator.ruleFromJSON(rule);
    return generator.applyRule(parseXml(xml));
}

QString generateFile(const QString &ruleFile, const QString &afcFile)
{
    QFile f(afcFile);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    QDomDocument doc;
    doc.setContent(f.readAll());
    SourceCodeGenerator generator;
    generator.loadRule(ruleFile);
    return generator.applyRule(doc);
}

QString readText(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

bool writeText(const QString &path, const QString &text)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    f.write(text.toUtf8());
    return true;
}

// Line by line comparison with a readable failure message.
QString firstDifference(const QString &actual, const QString &expected)
{
    const QStringList a = actual.split(QLatin1Char('\n'));
    const QStringList e = expected.split(QLatin1Char('\n'));
    for (int i = 0; i < qMax(a.size(), e.size()); ++i) {
        const QString la = i < a.size() ? a.at(i) : QStringLiteral("<missing>");
        const QString le = i < e.size() ? e.at(i) : QStringLiteral("<missing>");
        if (la != le)
            return QStringLiteral("line %1:\n  actual:   '%2'\n  expected: '%3'").arg(i + 1).arg(la, le);
    }
    return QString();
}

#define COMPARE_TEXT(actual, expected)                                                                               \
    do {                                                                                                             \
        const QString a_ = (actual);                                                                                 \
        const QString e_ = (expected);                                                                               \
        if (a_ != e_)                                                                                                \
            QFAIL(qPrintable(QStringLiteral("text mismatch, ") + firstDifference(a_, e_) + QStringLiteral("\n--- actual:\n") \
                             + a_ + QStringLiteral("--- expected:\n") + e_));                                        \
    } while (false)

struct RunResult {
    bool started = false;
    int exitCode = -1;
    QString out;
    QString err;
};

RunResult run(const QString &program, const QStringList &args, const QString &workDir)
{
    RunResult result;
    QProcess p;
    p.setWorkingDirectory(workDir);
    p.setProcessChannelMode(QProcess::SeparateChannels);
    // The programs print UTF-8 (the samples contain Cyrillic). On Windows Python
    // would encode its output with the ANSI code page of the system instead.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    env.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    p.setProcessEnvironment(env);
    p.start(program, args);
    if (!p.waitForStarted(10000))
        return result;
    result.started = true;
    p.closeWriteChannel();
    if (!p.waitForFinished(60000)) {
        p.kill();
        p.waitForFinished();
        result.err = QStringLiteral("timeout");
        return result;
    }
    result.exitCode = p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1;
    // Windows programs write "\r\n" to text-mode stdout
    result.out = QString::fromUtf8(p.readAllStandardOutput()).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    result.err = QString::fromUtf8(p.readAllStandardError()).replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    return result;
}

QString tool(const QStringList &names)
{
    for (const QString &n : names) {
        const QString path = QStandardPaths::findExecutable(n);
        // Windows: the "python3" in WindowsApps is a Microsoft Store stub, not an interpreter
        if (!path.isEmpty() && !path.contains(QLatin1String("WindowsApps"), Qt::CaseInsensitive))
            return path;
    }
    return QString();
}

const QStringList &blockTypes()
{
    static const QStringList types = {
        QStringLiteral("algorithm"), QStringLiteral("process"), QStringLiteral("assign"), QStringLiteral("io"),
        QStringLiteral("ou"), QStringLiteral("if"), QStringLiteral("pre"), QStringLiteral("post"),
        QStringLiteral("for"), QStringLiteral("forc"), QStringLiteral("foreach"), QStringLiteral("case"),
        QStringLiteral("call"), QStringLiteral("return"), QStringLiteral("break"), QStringLiteral("continue")};
    return types;
}

} // namespace

class Test_generator : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();

    // engine
    void indentationIsRelative();
    void legacyInlineBranches();
    void branchIndexing();
    void emptyBody();
    void templateEmptyAttribute();
    void templateEmptyCombinations();
    void templateShortenedLegacy();
    void templateSingle();
    void defaults();
    void listSplitting();
    void literalPrefix();
    void listHeadTailAndViews();
    void caseBranches();
    void caseTerminatedBranches();
    void caseFirstBranchAndOnlyDefault();
    void caseValueItemsSeeParent();
    void loopContextNearest();
    void loopContextContinueSkipsCase();
    void loopContextThroughCase();
    void loopContextLastInBranch();
    void loopAttributePlaceholders();
    void algorithmPlaceholders();
    void variablePrefix_data();
    void variablePrefix();
    void variablePrefixSkipsNames();
    void operatorMapping_data();
    void operatorMapping();
    void unknownElementComment();
    void noReExpansion();
    void multilineValues();
    void lineDropping();
    void filters_data();
    void filters();
    void ifChain();
    void variablesCollection();
    void typeMapping();
    void templateArrays();
    void legacyRule();
    void legacyInputOutputAttributes();
    void invalidRules();
    void loadRuleByIdAndPath();
    void languageNames();
    void voidParametersAndReturns();
    void attributeWhitespace();
    void typedItemsAndLimit();
    void binaryOperatorsAndCasts_data();
    void binaryOperatorsAndCasts();
    void truthyFilter_data();
    void truthyFilter();
    void literalConversion();
    void prefixSubscriptsCastsAndCalls_data();
    void prefixSubscriptsCastsAndCalls();
    void templateEmptyCases();
    void undeclaredVariables();
    void mainParams();
    void deepNesting();

    // rule files
    void allRulesComplete_data();
    void allRulesComplete();
    void golden_data();
    void golden();
    void goldenSyntax_data();
    void goldenSyntax();
    void semantic_data();
    void semantic();
    void robustness();
};

void Test_generator::initTestCase()
{
    afce::setupSearchPaths();
}

// ---------------------------------------------------------------------------
// engine

void Test_generator::indentationIsRelative()
{
    const QByteArray rule = R"json({
        "additional_settings": {"indentation_preference": "  "},
        "algorithm": {"template": ["begin", "\t%branch1%", "end"]},
        "pre": {"template": ["while %cond%", "\t%branch1%", "\tafter", "wend"]},
        "process": {"template": "%text%"}
    })json";
    const QString body = QStringLiteral(
        "<pre cond='a'><branch><pre cond='b'><branch><pre cond='c'><branch>"
        "<process text='x'/></branch></pre></branch></pre></branch></pre>");
    COMPARE_TEXT(generate(rule, body),
                 QStringLiteral("begin\n"
                                "  while a\n"
                                "    while b\n"
                                "      while c\n"
                                "        x\n"
                                "        after\n"
                                "      wend\n"
                                "      after\n"
                                "    wend\n"
                                "    after\n"
                                "  wend\n"
                                "end\n"));
}

void Test_generator::legacyInlineBranches()
{
    // %branchN% at the end of a line (old rule style) puts the blocks one level deeper,
    // text after the placeholder continues on a new line.
    const QByteArray rule = R"json({
        "additional_settings": {"indentation_preference": "    "},
        "algorithm": {"template": "def main():%branch1%\nmain()"},
        "post": {"template": "do {%branch1%} while (%cond%);"},
        "pre": {"template": "while %cond%:%branch1%\n\tpass"},
        "process": {"template": "%text%"}
    })json";
    const QString body = QStringLiteral(
        "<post cond='a'><branch><pre cond='b'><branch><process text='x'/></branch></pre></branch></post>");
    COMPARE_TEXT(generate(rule, body),
                 QStringLiteral("def main():\n"
                                "    do {\n"
                                "        while b:\n"
                                "            x\n"
                                "            pass\n"
                                "    } while (a);\n"
                                "main()\n"));
}

void Test_generator::branchIndexing()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "if": {"template": ["Y:", "\t%branch1%", "N:", "\t%branch2%", "X:", "\t%branch3%"]},
        "process": {"template": "%text%"}
    })json";
    // whitespace, comments and text between the branches must not shift the numbering
    const QString body = QStringLiteral(
        "<if cond='c'>\n  <!-- yes -->\n  <branch>\n <process text='yes'/>\n </branch>\n  text\n"
        "  <branch><process text='no'/></branch>\n</if>");
    COMPARE_TEXT(generate(rule, body), QStringLiteral("Y:\n  yes\nN:\n  no\nX:\n"));
}

void Test_generator::emptyBody()
{
    const QByteArray rule = R"json({
        "additional_settings": {"indentation_preference": "    ", "empty_body": "pass"},
        "algorithm": {"template": ["def main():", "\t%branch1%"]},
        "if": {"template": ["if %cond%:", "\t%branch1%", "else:", "\t%branch2%"]},
        "process": {"template": "%text%"}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<if cond='a'><branch/><branch><process text='x'/></branch></if>")),
                 QStringLiteral("def main():\n    if a:\n        pass\n    else:\n        x\n"));
    COMPARE_TEXT(generate(rule, QString()), QStringLiteral("def main():\n    pass\n"));
    // a branch whose blocks render to nothing is empty as well
    COMPARE_TEXT(generate(rule, QStringLiteral("<process text=''/>")), QStringLiteral("def main():\n    pass\n"));
    // ... and so is a branch with nothing but comments for unknown blocks
    const QByteArray withComment = QByteArray(rule).replace("\"process\"", "\"comment\": {\"template\": \"# %text%\"}, \"process\"");
    COMPARE_TEXT(generate(withComment, QStringLiteral("<if cond='a'><branch><gizmo/></branch><branch><process text='x'/></branch></if>")),
                 QStringLiteral("def main():\n    if a:\n        # unsupported block \"gizmo\"\n        pass\n    else:\n        x\n"));
}

void Test_generator::templateEmptyAttribute()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "return": {"template": "return %value%;", "template_empty_value": "return;"},
        "io": {"template": "read(%vars%)", "template_empty_vars": "wait()", "list": {"vars": {"glue": ", "}}}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<return value='x + 1'/><return value='  '/><return/>")),
                 QStringLiteral("return x + 1;\nreturn;\nreturn;\n"));
    // a list attribute without items is empty
    COMPARE_TEXT(generate(rule, QStringLiteral("<io vars='a,b'/><io vars=' , '/>")),
                 QStringLiteral("read(a, b)\nwait()\n"));
}

void Test_generator::templateEmptyCombinations()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "forc": {
            "template": "full",
            "template_empty_init": "no init",
            "template_empty_step": "no step",
            "template_empty_init+step": "neither",
            "template_empty_init+cond+step": "forever"
        }
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<forc init='i' cond='c' step='s'><branch/></forc>")),
                 QStringLiteral("full\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<forc init='' cond='c' step='s'><branch/></forc>")),
                 QStringLiteral("no init\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<forc init='i' cond='c' step=''><branch/></forc>")),
                 QStringLiteral("no step\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<forc init='' cond='c' step=''><branch/></forc>")),
                 QStringLiteral("neither\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<forc><branch/></forc>")), QStringLiteral("forever\n"));

    // branch conditions and alphabetical tie-break (branch1 < branch2)
    const QByteArray ifRule = R"json({
        "algorithm": {"template": "%branch1%"},
        "if": {"template": "both", "template_empty_branch1": "only no", "template_empty_branch2": "only yes"},
        "process": {"template": "%text%"}
    })json";
    COMPARE_TEXT(generate(ifRule, QStringLiteral("<if cond='c'><branch><process text='a'/></branch><branch/></if>")),
                 QStringLiteral("only yes\n"));
    COMPARE_TEXT(generate(ifRule, QStringLiteral("<if cond='c'><branch/><branch><process text='a'/></branch></if>")),
                 QStringLiteral("only no\n"));
    COMPARE_TEXT(generate(ifRule, QStringLiteral("<if cond='c'><branch/><branch/></if>")), QStringLiteral("only no\n"));
    // a missing branch counts as empty
    COMPARE_TEXT(generate(ifRule, QStringLiteral("<if cond='c'><branch><process text='a'/></branch></if>")),
                 QStringLiteral("only yes\n"));
}

void Test_generator::templateShortenedLegacy()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "if": {"template": "if %cond%: %branch1%\nelse: %branch2%", "template_shortened": "if %cond%:%branch1%"},
        "process": {"template": "%text%"}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<if cond='c'><branch><process text='a'/></branch><branch/></if>")),
                 QStringLiteral("if c:\n  a\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<if cond='c'><branch><process text='a'/></branch>"
                                               "<branch><process text='b'/></branch></if>")),
                 QStringLiteral("if c:\n  a\nelse:\n  b\n"));
    // an explicit template_empty_branch2 wins over the legacy alias
    const QByteArray both = R"json({
        "algorithm": {"template": "%branch1%"},
        "if": {"template": "full", "template_shortened": "legacy", "template_empty_branch2": "new"}
    })json";
    COMPARE_TEXT(generate(both, QStringLiteral("<if cond='c'><branch/><branch/></if>")), QStringLiteral("new\n"));
}

void Test_generator::templateSingle()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "ou": {"template": "print(%vars%, sep='')", "template_single_vars": "print(%vars%)",
               "template_empty_vars": "print()", "list": {"vars": {"glue": ", "}}}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<ou vars='a, b'/><ou vars='&quot;x, y&quot;'/><ou vars=''/>")),
                 QStringLiteral("print(a, b, sep='')\nprint(\"x, y\")\nprint()\n"));
}

void Test_generator::defaults()
{
    const QByteArray rule = R"json({
        "algorithm": {
            "template": ["%returns% %name%(%params%) {", "\t%branch1%", "}"],
            "template_empty_name": ["program %name%", "\t%branch1%"],
            "defaults": {"name": "main", "returns": "int", "params": "void"}
        },
        "return": {"template": "return %value%;", "defaults": {"value": "0"}}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<return/>")), QStringLiteral("program main\n  return 0;\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<return value='x'/>"), QStringLiteral("name='f'")),
                 QStringLiteral("int f(void) {\n  return x;\n}\n"));
    COMPARE_TEXT(generate(rule, QString(), QStringLiteral("name='g' params='int a' returns='  '")),
                 QStringLiteral("int g(int a) {\n}\n"));
}

void Test_generator::listSplitting()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "ou": {"template": "%vars%", "list": ["vars"], "separator": ",", "glue": "\n",
               "prefix": "put(", "suffix": ") # %$%"}
    })json";
    const QString vars = QStringLiteral("&quot;a, b&quot;, f(x, y),'c,d' , arr[1, 2],, {1, 2},&quot;q\\&quot;u,o&quot;");
    COMPARE_TEXT(generate(rule, QStringLiteral("<ou vars=\"%1\"/>").arg(vars)),
                 QStringLiteral("put(\"a, b\") # \"a, b\"\n"
                                "put(f(x, y)) # f(x, y)\n"
                                "put('c,d') # 'c,d'\n"
                                "put(arr[1, 2]) # arr[1, 2]\n"
                                "put({1, 2}) # {1, 2}\n"
                                "put(\"q\\\"u,o\") # \"q\\\"u,o\"\n"));
}

void Test_generator::literalPrefix()
{
    // legacy element-level keys + literal_prefix / literal_suffix
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "ou": {"template": "%vars%", "list": ["vars"], "glue": "\n",
               "prefix": "printf(\"%d\", ", "suffix": ");",
               "literal_prefix": "printf(\"%s\", ", "literal_suffix": ");"}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<ou vars='&quot;Sum = &quot;, s, &quot;a&quot; &quot;b&quot;, L&quot;w&quot;'/>")),
                 QStringLiteral("printf(\"%s\", \"Sum = \");\n"
                                "printf(\"%d\", s);\n"
                                "printf(\"%d\", \"a\" \"b\");\n"
                                "printf(\"%s\", L\"w\");\n"));
    // char items: char_item wins, otherwise literal_*; %$text% with escapes
    const QByteArray chars = R"json({
        "algorithm": {"template": "%branch1%"},
        "ou": {"template": "WriteLn(%vars%);", "list": {"vars": {"glue": ", ",
               "literal_item": "'%$text%'", "char_item": "chr(%$%)",
               "text_escape": {"'": "''", "\\n": "'#10'", "\\\"": "\""}}}}
    })json";
    COMPARE_TEXT(generate(chars, QStringLiteral("<ou vars='&quot;it&apos;s \\&quot;ok\\&quot;\\n&quot;, &apos;x&apos;, n'/>")),
                 QStringLiteral("WriteLn('it''s \"ok\"'#10'', chr('x'), n);\n"));
}

void Test_generator::listHeadTailAndViews()
{
    // two views of the same attribute (printf format + arguments), head/tail only when not empty
    const QByteArray rule = R"json({
        "algorithm": {"template": ["%decl%", "%branch1%"],
                      "list": {"decl": {"source": "variables", "head": "int ", "glue": ", ", "tail": ";"}}},
        "assign": {"template": "%dest% = %src%;"},
        "ou": {"template": "printf(\"%fmt%\\n\"%args%);",
               "list": {"fmt": {"source": "vars", "item": "%d", "literal_item": "%$text%", "text_escape": {"%": "%%"}},
                        "args": {"source": "vars", "item": ", %$%", "literal_item": ""}}}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<assign dest='s' src='1'/><ou vars='&quot;100% = &quot;, s, &quot;!&quot;'/>")),
                 QStringLiteral("int s;\ns = 1;\nprintf(\"100%% = %d!\\n\", s);\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<ou vars='&quot;hi&quot;'/>")), QStringLiteral("printf(\"hi\\n\");\n"));
}

void Test_generator::caseBranches()
{
    const QByteArray rule = R"json({
        "additional_settings": {"indentation_preference": "    "},
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "%text%;"},
        "break": {"template": "break;"},
        "case": {
            "template": ["switch (%expr%) {", "%branches%", "}"],
            "case_branch": ["\t%value%", "\t\t%body%", "\t\tbreak;"],
            "case_branch_terminated": ["\t%value%", "\t\t%body%"],
            "default_branch": ["\tdefault:", "\t\t%body%"],
            "default_branch_empty": "",
            "list": {"value": {"item": "case %$%:", "glue": "\n"}}
        }
    })json";
    const QString body = QStringLiteral(
        "<case expr='x'>"
        "<branch value='1'><process text='a()'/></branch>"
        "<branch value='2, 3'><process text='b()'/><break/></branch>"
        "<branch value='ignored'><process text='c()'/></branch>"
        "</case>"
        "<case expr='y'><branch value='&quot;s, t&quot;'/><branch/></case>");
    COMPARE_TEXT(generate(rule, body),
                 QStringLiteral("switch (x) {\n"
                                "    case 1:\n"
                                "        a();\n"
                                "        break;\n"
                                "    case 2:\n"
                                "    case 3:\n"
                                "        b();\n"
                                "        break;\n"
                                "    default:\n"
                                "        c();\n"
                                "}\n"
                                "switch (y) {\n"
                                "    case \"s, t\":\n"
                                "        break;\n"
                                "}\n"));
}

void Test_generator::caseTerminatedBranches()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "%text%;"},
        "break": {"template": "break;"},
        "continue": {"template": "continue;"},
        "return": {"template": "return;"},
        "if": {"template": ["if (%cond%) {", "\t%branch1%", "} else {", "\t%branch2%", "}"]},
        "case": {
            "template": ["switch (%expr%) {", "%branches%", "}"],
            "case_branch": ["case %value%:", "\t%body%", "\tbreak;"],
            "case_branch_terminated": ["case %value%:", "\t%body%"],
            "default_branch": ["default:", "\t%body%"]
        }
    })json";
    // ends with return / continue / an if whose branches both jump: no extra break
    COMPARE_TEXT(generate(rule, QStringLiteral(
                     "<case expr='x'><branch value='1'><return/></branch><branch value='2'><continue/></branch>"
                     "<branch value='3'><if cond='c'><branch><break/></branch><branch><return/></branch></if></branch>"
                     "<branch/></case>")),
                 QStringLiteral("switch (x) {\ncase 1:\n  return;\ncase 2:\n  continue;\ncase 3:\n  if (c) {\n    break;\n"
                                "  } else {\n    return;\n  }\ndefault:\n}\n"));
    // a nested case whose branches end with break only leaves the inner case: the outer break stays
    COMPARE_TEXT(generate(rule, QStringLiteral(
                     "<case expr='x'><branch value='1'><case expr='y'><branch value='2'><break/></branch>"
                     "<branch><break/></branch></case></branch><branch/></case>")),
                 QStringLiteral("switch (x) {\ncase 1:\n  switch (y) {\n  case 2:\n    break;\n  default:\n    break;\n  }\n"
                                "  break;\ndefault:\n}\n"));
}

void Test_generator::caseFirstBranchAndOnlyDefault()
{
    const QByteArray rule = R"json({
        "additional_settings": {"indentation_preference": "    ", "empty_body": "pass"},
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "%text%"},
        "case": {
            "template": "%branches%",
            "first_case_branch": ["if %value%:", "\t%body%"],
            "case_branch": ["elif %value%:", "\t%body%"],
            "default_branch": ["else:", "\t%body%"],
            "default_branch_empty": "",
            "default_branch_only": "%body%",
            "list": {"value": {"item": "%expr% == %$%", "glue": " or "}}
        }
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<case expr='x'><branch value='1'/><branch value='2,3'>"
                                               "<process text='b'/></branch><branch><process text='c'/></branch></case>")),
                 QStringLiteral("if x == 1:\n    pass\nelif x == 2 or x == 3:\n    b\nelse:\n    c\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<case expr='x'><branch value='1'><process text='a'/></branch><branch/></case>")),
                 QStringLiteral("if x == 1:\n    a\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<case expr='x'><branch><process text='only'/></branch></case>")),
                 QStringLiteral("only\n"));
}

void Test_generator::caseValueItemsSeeParent()
{
    const QByteArray rule = R"json({
        "additional_settings": {"indentation_preference": "    ", "variable_prefix": "$"},
        "algorithm": {"template": "%branch1%"},
        "case": {
            "template": ["%branches%", "}"],
            "first_case_branch": ["if (%value%) {", "\t%body%"],
            "case_branch": ["} elsif (%value%) {", "\t%body%"],
            "default_branch": ["} else {", "\t%body%"],
            "list": {"value": {"item": "%expr% == %$%", "literal_item": "%expr% eq %$%", "glue": " || "}}
        },
        "process": {"template": "%text%;"}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<case expr='x'><branch value='2, 3'><process text='a = 1'/></branch>"
                                               "<branch value='&quot;q&quot;, N'/><branch><process text='b = x'/></branch></case>")),
                 QStringLiteral("if ($x == 2 || $x == 3) {\n"
                                "    $a = 1;\n"
                                "} elsif ($x eq \"q\" || $x == $N) {\n"
                                "} else {\n"
                                "    $b = $x;\n"
                                "}\n"));
}

void Test_generator::loopContextNearest()
{
    const QByteArray rule = R"json({
        "additional_settings": {"indentation_preference": "  "},
        "algorithm": {"template": "%branch1%"},
        "for": {"template": ["for %var%", "\t%branch1%"]},
        "forc": {"template": ["forc %cond%", "\t%branch1%", "\t%step%"]},
        "pre": {"template": ["pre", "\t%branch1%"]},
        "post": {"template": ["post", "\t%branch1%"]},
        "foreach": {"template": ["foreach", "\t%branch1%"]},
        "if": {"template": ["if", "\t%branch1%"]},
        "continue": {"template": "continue", "template_in_forc": ["%loop.step%", "continue"],
                     "template_in_for": "continue-for %loop.var%"},
        "break": {"template": "break", "template_in_post": "break-post", "template_in_case": "break-case"},
        "case": {"template": ["case", "%branches%"], "case_branch": ["\twhen", "\t\t%body%"],
                 "default_branch": ["\telse", "\t\t%body%"]}
    })json";
    const QString body = QStringLiteral(
        "<forc cond='c' step='i += 1'><branch>"
        "  <for var='k'><branch><if><branch><continue/></branch><branch/></if></branch></for>"
        "  <if><branch><continue/></branch><branch/></if>"
        "  <post><branch><break/><pre><branch><continue/><break/></branch></pre></branch></post>"
        "  <case><branch value='1'><continue/></branch><branch><break/></branch></case>"
        "</branch></forc>"
        "<continue/>");
    COMPARE_TEXT(generate(rule, body),
                 QStringLiteral("forc c\n"
                                "  for k\n"
                                "    if\n"
                                "      continue-for k\n"
                                "  if\n"
                                "    i += 1\n"
                                "    continue\n"
                                "  post\n"
                                "    break-post\n"
                                "    pre\n"
                                "      continue\n"
                                "      break\n"
                                "  case\n"
                                "    when\n"
                                "      i += 1\n"
                                "      continue\n"
                                "    else\n"
                                "      break-case\n"
                                "  i += 1\n"
                                "continue\n"));
}

void Test_generator::loopContextContinueSkipsCase()
{
    // "context" overrides the context set; continue ignores case by default
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "pre": {"template": ["pre", "\t%branch1%"]},
        "case": {"template": "%branches%", "case_branch": "%body%", "default_branch": "%body%"},
        "continue": {"template": "c", "template_in_case": "c-case", "template_in_pre": "c-pre"},
        "break": {"template": "b", "template_in_case": "b-case", "template_in_pre": "b-pre", "context": ["pre"]}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<pre><branch><case><branch value='1'><continue/><break/></branch>"
                                               "<branch/></case></branch></pre>")),
                 QStringLiteral("pre\n  c-pre\n  b-pre\n"));
}

void Test_generator::loopContextThroughCase()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "for": {"template": ["for", "\t%branch1%"]},
        "case": {"template": "%branches%", "case_branch": "%body%", "default_branch": "%body%"},
        "continue": {"template": "continue;", "template_through_case": "continue %levels%;"},
        "break": {"template": "break;", "template_through_case": "never"}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral(
                     "<for><branch><continue/>"
                     "<case><branch value='1'><continue/><break/></branch>"
                     "<branch><case><branch value='2'><continue/></branch><branch/></case></branch></case>"
                     "</branch></for>")),
                 QStringLiteral("for\n  continue;\n  continue 2;\n  break;\n  continue 3;\n"));
}

void Test_generator::loopContextLastInBranch()
{
    // template_in_<type>_last: the jump is the last block of a branch that belongs to the context
    const QByteArray rule = R"json({
        "additional_settings": {"empty_body": "pass"},
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "%text%"},
        "if": {"template": ["if", "\t%branch1%"]},
        "pre": {"template": ["pre", "\t%branch1%"]},
        "case": {"template": "%branches%", "case_branch": ["when", "\t%body%"], "default_branch": ["else", "\t%body%"]},
        "break": {"template": "break", "template_in_case": "unsupported", "template_in_case_last": ""},
        "continue": {"template": "continue", "template_in_pre_last": "# continue"}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral(
                     "<pre><branch><case>"
                     "<branch value='1'><process text='a'/><break/></branch>"
                     "<branch value='2'><break/></branch>"
                     "<branch><if><branch><break/></branch><branch/></if><process text='b'/></branch>"
                     "</case><continue/><if><branch><continue/></branch><branch/></if><continue/></branch></pre>")),
                 QStringLiteral("pre\n  when\n    a\n  when\n    pass\n  else\n    if\n      unsupported\n    b\n"
                                "  continue\n  if\n    continue\n  # continue\n"));
}

void Test_generator::loopAttributePlaceholders()
{
    const QByteArray rule = R"json({
        "additional_settings": {"variable_prefix": "$"},
        "algorithm": {"template": "%branch1%"},
        "post": {"template": ["do", "\t%branch1%", "until %cond%"], "defaults": {"cond": "1"}},
        "continue": {"template": "next", "template_in_post": "next if %loop.cond|paren%; %loop.missing%|%loop.cond|raw%"},
        "break": {"template": "last %loop.cond%"}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<post cond='i &lt; n'><branch><continue/></branch></post>"
                                               "<post><branch><continue/></branch></post><break/>")),
                 QStringLiteral("do\n  next if ($i < $n); |i < n\nuntil $i < $n\n"
                                "do\n  next if 1; |1\nuntil 1\n"
                                "last\n"));
}

void Test_generator::algorithmPlaceholders()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": ["%name%:", "\t%branch1%"], "defaults": {"name": "Main"}},
        "return": {"template": ["%algorithm.name% = %value%", "Exit Function"],
                   "template_empty_algorithm.returns": "Exit Sub"}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<return value='1'/>")), QStringLiteral("Main:\n  Exit Sub\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<return value='1'/>"), QStringLiteral("name='F' returns='Integer'")),
                 QStringLiteral("F:\n  F = 1\n  Exit Function\n"));
}

void Test_generator::variablePrefix_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("identifiers") << "a + b * c1" << "$a + $b * $c1";
    QTest::newRow("underscore") << "x_1 + _y" << "$x_1 + $_y";
    QTest::newRow("double quoted") << "\"a + b\" . c" << "\"a + b\" . $c";
    QTest::newRow("single quoted") << "'x y' . z" << "'x y' . $z";
    QTest::newRow("escaped quote") << "\"say \\\"hi\\\" n\" . n" << "\"say \\\"hi\\\" n\" . $n";
    QTest::newRow("function call") << "f(x) + g (y)" << "f($x) + g ($y)";
    QTest::newRow("keywords") << "count and not done or true" << "$count and not $done or true";
    QTest::newRow("numbers") << "x == 10 + 2.5e3 - 0x1F + .5" << "$x == 10 + 2.5e3 - 0x1F + .5";
    QTest::newRow("already prefixed") << "$already + @arr + $h{k}" << "$already + @arr + $h{$k}";
    QTest::newRow("members") << "obj->field + Foo::bar + Foo::baz()" << "$obj->field + Foo::bar + Foo::baz()";
    QTest::newRow("subscript") << "a[i + 1]" << "$a[$i + 1]";
    QTest::newRow("hash arrow") << "key => value" << "key => $value";
    QTest::newRow("keyword constant") << "PHP_EOL" << "PHP_EOL";
    QTest::newRow("unicode") << "сумма + x" << "$сумма + $x";
    QTest::newRow("empty") << "" << "";
}

void Test_generator::variablePrefix()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    const QByteArray rule = R"json({
        "additional_settings": {"variable_prefix": "$", "keywords": ["and", "or", "not", "true", "PHP_EOL"]},
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "[%text%]"}
    })json";
    QDomDocument doc;
    QDomElement alg = doc.createElement(QStringLiteral("algorithm"));
    doc.appendChild(alg);
    QDomElement branch = doc.createElement(QStringLiteral("branch"));
    alg.appendChild(branch);
    QDomElement process = doc.createElement(QStringLiteral("process"));
    process.setAttribute(QStringLiteral("text"), input);
    branch.appendChild(process);
    SourceCodeGenerator generator;
    generator.ruleFromJSON(rule);
    const QString result = generator.applyRule(doc);
    QCOMPARE(result, QStringLiteral("[%1]\n").arg(expected));
}

void Test_generator::variablePrefixSkipsNames()
{
    const QByteArray rule = R"json({
        "additional_settings": {"variable_prefix": "$", "keywords": ["And"], "case_insensitive": true},
        "algorithm": {"template": ["%returns% %name%(%args%)", "\t%branch1%"],
                      "list": {"args": {"source": "params", "item": "%$name%", "glue": ", "}}},
        "ou": {"template": "echo %vars%;", "list": {"vars": {"glue": ", "}}},
        "if": {"template": ["if %cond%", "\t%branch1%"]}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<ou vars='&quot;x = &quot;, x, &apos;y&apos;, 5'/>"
                                               "<if cond='a AND b'><branch/><branch/></if>"),
                          QStringLiteral("name='sum' params='int a, const char *s' returns='int'")),
                 QStringLiteral("int sum($a, $s)\n  echo \"x = \", $x, 'y', 5;\n  if $a AND $b\n"));
}

void Test_generator::operatorMapping_data()
{
    QTest::addColumn<QString>("language");
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("py and or not") << "py" << "a > 0 && b != 0 || !done" << "a > 0 and b != 0 or not done";
    QTest::newRow("py compact") << "py" << "!x&&y||!(z)" << "not x and y or not (z)";
    QTest::newRow("py words") << "py" << "flag == true || p == NULL || truex" << "flag == True or p == None or truex";
    QTest::newRow("py literals kept") << "py" << "\"a && !b\" + '|' + s" << "\"a && !b\" + '|' + s";
    QTest::newRow("pas parens") << "pas" << "a > 0 && b != 0 || !done" << "(a > 0) and (b <> 0) or not done";
    QTest::newRow("pas nested") << "pas" << "(x % 2 == 0 && y < 10) || f(a == b && c)" << "((x mod 2 = 0) and (y < 10)) or f((a = b) and c)";
    QTest::newRow("pas mod") << "pas" << "a%b" << "a mod b";
    QTest::newRow("pas already") << "pas" << "(a > 0) && ok" << "(a > 0) and ok";
    QTest::newRow("pas assign kept") << "pas" << "x := y" << "x := y";
    QTest::newRow("perl prefix after map") << "perl" << "done == false && x" << "$done == 0 && $x";
    QTest::newRow("e87 unicode") << "e87" << "!a && b != c" << "не a и b <> c";
}

void Test_generator::operatorMapping()
{
    QFETCH(QString, language);
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QFile f(generatorsDir() + QLatin1Char('/') + language + QStringLiteral(".json"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QJsonObject rule = QJsonDocument::fromJson(f.readAll()).object();
    QJsonObject process;
    process.insert(QStringLiteral("template"), QStringLiteral("[%text%]"));
    rule.insert(QStringLiteral("process"), process);
    QJsonObject algorithm;
    algorithm.insert(QStringLiteral("template"), QStringLiteral("%branch1%"));
    rule.insert(QStringLiteral("algorithm"), algorithm);
    QDomDocument doc = parseXml(QStringLiteral("<algorithm><branch><process/></branch></algorithm>"));
    doc.elementsByTagName(QStringLiteral("process")).at(0).toElement().setAttribute(QStringLiteral("text"), input);
    SourceCodeGenerator generator;
    generator.ruleFromJSON(QJsonDocument(rule).toJson());
    QCOMPARE(generator.applyRule(doc).trimmed(), QStringLiteral("[%1]").arg(expected));
}

void Test_generator::unknownElementComment()
{
    const QByteArray withComment = R"json({
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "%text%"},
        "comment": {"template": "// %text%"}
    })json";
    COMPARE_TEXT(generate(withComment, QStringLiteral("<process text='a'/><frobnicate text='x'><branch/></frobnicate><process text='b'/>")),
                 QStringLiteral("a\n// unsupported block \"frobnicate\"\nb\n"));
    // a block type the rule does not know (not only unknown XML) is reported as well
    COMPARE_TEXT(generate(withComment, QStringLiteral("<call text='f()'/>")),
                 QStringLiteral("// unsupported block \"call\"\n"));
    const QByteArray without = R"json({"algorithm": {"template": "%branch1%"}, "process": {"template": "%text%"}})json";
    COMPARE_TEXT(generate(without, QStringLiteral("<process text='a'/><frobnicate/><name/>")), QStringLiteral("a\n"));
    // the comment rule may also be a plain string
    const QByteArray plain = R"json({"algorithm": {"template": "%branch1%"}, "comment": "# %text%"})json";
    COMPARE_TEXT(generate(plain, QStringLiteral("<x/>")), QStringLiteral("# unsupported block \"x\"\n"));
}

void Test_generator::noReExpansion()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "assign": {"template": "%dest% = %src%;"},
        "ou": {"template": "out(%vars%)", "list": {"vars": {"item": "<%$%>", "glue": ","}}}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<assign dest='%src%' src='%dest% + %branch1% + %% + 5%'/>"
                                               "<ou vars='%$%, &quot;%vars%&quot;'/>")),
                 QStringLiteral("%src% = %dest% + %branch1% + %% + 5%;\nout(<%$%>,<\"%vars%\">)\n"));
}

void Test_generator::multilineValues()
{
    const QByteArray rule = R"json({
        "additional_settings": {"indentation_preference": "    "},
        "algorithm": {"template": ["def main():", "\t%branch1%"]},
        "pre": {"template": ["while %cond%:", "\t%branch1%"]},
        "process": {"template": "%text%"}
    })json";
    QDomDocument doc = parseXml(QStringLiteral(
        "<algorithm><branch><pre cond='a'><branch><pre cond='b'><branch><process/></branch></pre></branch></pre></branch></algorithm>"));
    doc.elementsByTagName(QStringLiteral("process")).at(0).toElement().setAttribute(
        QStringLiteral("text"), QStringLiteral("x = 1\r\n\nif x:\n    y = 2\n\tz = 3"));
    SourceCodeGenerator generator;
    generator.ruleFromJSON(rule);
    COMPARE_TEXT(generator.applyRule(doc),
                 QStringLiteral("def main():\n"
                                "    while a:\n"
                                "        while b:\n"
                                "            x = 1\n"
                                "\n"
                                "            if x:\n"
                                "                y = 2\n"
                                "                z = 3\n"));
}

void Test_generator::lineDropping()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": ["top", "", "%branch1%", "", "bottom"]},
        "forc": {"template": ["%init%;", "while %cond%", "\t%branch1%", "\t%step%", "end"]},
        "process": {"template": "%text%;"}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<forc cond='c'><branch><process/></branch></forc>"
                                               "<forc init='i = 0' cond='c' step='i++'><branch/></forc>")),
                 QStringLiteral("top\n\nwhile c\nend\ni = 0;\nwhile c\n  i++\nend\n\nbottom\n"));
}

void Test_generator::filters_data()
{
    QTest::addColumn<QString>("filter");
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("inc number") << "inc" << "10" << "11";
    QTest::newRow("inc negative") << "inc" << "-1" << "0";
    QTest::newRow("inc minus one") << "inc" << "n - 1" << "n";
    QTest::newRow("inc minus one compact") << "inc" << "len(a)-1" << "len(a)";
    QTest::newRow("inc product minus one") << "inc" << "2 * n - 1" << "2 * n";
    QTest::newRow("inc minus ten") << "inc" << "n - 10" << "n - 10 + 1";
    QTest::newRow("inc identifier") << "inc" << "n" << "n + 1";
    QTest::newRow("inc call") << "inc" << "f(n - 1)" << "f(n - 1) + 1";
    QTest::newRow("inc times minus one") << "inc" << "n * -1" << "n * -1 + 1";
    QTest::newRow("inc ternary") << "inc" << "a < b ? a : b" << "(a < b ? a : b) + 1";
    QTest::newRow("inc shift") << "inc" << "n << 1" << "(n << 1) + 1";
    QTest::newRow("inc empty") << "inc" << "" << "";
    QTest::newRow("dec number") << "dec" << "10" << "9";
    QTest::newRow("dec plus one") << "dec" << "n + 1" << "n";
    QTest::newRow("dec identifier") << "dec" << "n" << "n - 1";
    QTest::newRow("paren simple") << "paren" << "x" << "x";
    QTest::newRow("paren member") << "paren" << "a[i].b(c)" << "a[i].b(c)";
    QTest::newRow("paren expr") << "paren" << "a + b" << "(a + b)";
    QTest::newRow("paren wrapped") << "paren" << "(a + b)" << "(a + b)";
    QTest::newRow("paren two groups") << "paren" << "(a) + (b)" << "((a) + (b))";
    QTest::newRow("paren literal") << "paren" << "\"a b\"" << "\"a b\"";
    QTest::newRow("paren negative") << "paren" << "-1" << "(-1)";
    QTest::newRow("operand arithmetic") << "operand" << "a + 1" << "a + 1";
    QTest::newRow("operand bitwise") << "operand" << "a & 1" << "(a & 1)";
    QTest::newRow("nosemi") << "nosemi" << "x = 1; ; " << "x = 1";
    QTest::newRow("name array") << "name" << "int a[10]" << "a";
    QTest::newRow("name pointer") << "name" << "const char *s" << "s";
    QTest::newRow("name pascal") << "name" << "a: Integer" << "a";
    QTest::newRow("name default") << "name" << "int x = 5" << "x";
    QTest::newRow("name reference") << "name" << "const std::vector<int> &v" << "v";
    QTest::newRow("name plain") << "name" << "x" << "x";
    QTest::newRow("type pointer") << "type" << "const char *s" << "const char *";
    QTest::newRow("type pascal") << "type" << "a: Integer" << "Integer";
    QTest::newRow("lhs") << "lhs" << "i = 0" << "i";
    QTest::newRow("lhs comparison") << "lhs" << "i == 0" << "i == 0";
    QTest::newRow("deftype plain") << "deftype:int" << "i" << "int i";
    QTest::newRow("deftype typed") << "deftype:int" << "long i" << "long i";
    QTest::newRow("array identifier") << "array" << "arr" << "@arr";
    QTest::newRow("array literal") << "array" << "[1, 2, 3]" << "(1, 2, 3)";
    QTest::newRow("array expression") << "array" << "(1..10)" << "(1..10)";
    QTest::newRow("chain") << "nosemi|paren" << "a + b;" << "(a + b)";
    QTest::newRow("negate identifier") << "negate" << "done" << "!done";
    QTest::newRow("negate not") << "negate" << "!done" << "done";
    QTest::newRow("negate not parens") << "negate" << "!(a < b)" << "a < b";
    QTest::newRow("negate word not") << "negate" << "not done" << "done";
    QTest::newRow("negate less") << "negate" << "n < 5" << "n >= 5";
    QTest::newRow("negate greater equal") << "negate" << "a + 1 >= b" << "a + 1 < b";
    QTest::newRow("negate equal") << "negate" << "x == 1" << "x != 1";
    QTest::newRow("negate pascal equal") << "negate" << "x = 1" << "x <> 1";
    QTest::newRow("negate pascal not equal") << "negate" << "x <> 1" << "x = 1";
    QTest::newRow("negate string") << "negate" << "s == \"<\"" << "s != \"<\"";
    QTest::newRow("negate logical") << "negate" << "a < b && c" << "!(a < b && c)";
    QTest::newRow("negate words") << "negate" << "a < b or c" << "!(a < b or c)";
    QTest::newRow("negate chained") << "negate" << "a < b < c" << "!(a < b < c)";
    QTest::newRow("negate shift") << "negate" << "a << 1" << "!(a << 1)";
    QTest::newRow("negate call") << "negate" << "f(a < b)" << "!f(a < b)";
    QTest::newRow("negate pointer") << "negate" << "p->next != 0" << "p->next == 0";
    QTest::newRow("cstmt declaration") << "cstmt" << "int i = 0" << "i = 0";
    QTest::newRow("cstmt increments") << "cstmt" << "i++, --j" << "i = i + 1\nj = j - 1";
    QTest::newRow("cstmt compound") << "cstmt" << "k *= a + b; s += x * y; t -= 1" << "k = k * (a + b)\ns = s + x * y\nt = t - 1";
    QTest::newRow("cstmt shift") << "cstmt" << "m <<= 1" << "m = m << 1";
    QTest::newRow("cstmt declarators") << "cstmt" << "int lo = 0, hi = n - 1" << "lo = 0\nhi = n - 1";
    QTest::newRow("cstmt pointer") << "cstmt" << "const char *p = s" << "p = s";
    QTest::newRow("cstmt no initializer") << "cstmt" << "int i" << "";
    QTest::newRow("cstmt kept") << "cstmt" << "a[i] = 0, f(x, y)" << "a[i] = 0\nf(x, y)";
    QTest::newRow("cstmt pascal") << "cstmt:pascal" << "int i = 0, j++" << "i := 0;\nj := j + 1";
    QTest::newRow("cstmt colon") << "cstmt:colon" << "i = 0" << "i := 0";
    QTest::newRow("cstmt decl") << "cstmt:decl" << "int i = 0, j = 1" << "i = 0, j = 1";
    QTest::newRow("cstmt decl keeps ++") << "cstmt:decl" << "i++, j += 2" << "i++, j += 2";
    QTest::newRow("callparens bare") << "callparens" << "init" << "init()";
    QTest::newRow("callparens call") << "callparens" << "f(x)" << "f(x)";
    QTest::newRow("colonassign") << "colonassign" << "i = 0" << "i := 0";
    QTest::newRow("colonassign pascal") << "colonassign" << "i := i + 1" << "i := i + 1";
    QTest::newRow("colonassign list") << "colonassign" << "a = 1; b[i] = 2" << "a := 1; b[i] := 2";
    QTest::newRow("colonassign comparison") << "colonassign" << "i <= 0" << "i <= 0";
    QTest::newRow("lvalue declaration") << "lvalue" << "long k" << "k";
    QTest::newRow("lvalue pointer") << "lvalue" << "int *p" << "p";
    QTest::newRow("lvalue long long") << "lvalue" << "unsigned long long n" << "n";
    QTest::newRow("lvalue plain") << "lvalue" << "k" << "k";
    QTest::newRow("lvalue subscript") << "lvalue" << "a[i]" << "a[i]";
    QTest::newRow("lvalue assignment") << "lvalue" << "x = 5" << "x = 5";
    QTest::newRow("lvalue then deftype") << "lvalue|deftype:let" << "long k" << "let k";
    QTest::newRow("deftype subscript") << "lvalue|deftype:let" << "a[i]" << "a[i]";
    QTest::newRow("semicolon single") << "semicolon" << "a = 1" << "a = 1;";
    QTest::newRow("semicolon kept") << "semicolon" << "a = 1;" << "a = 1;";
    QTest::newRow("semicolon lines") << "semicolon" << "a = 1\nb = 2" << "a = 1;\nb = 2;";
    QTest::newRow("semicolon block") << "semicolon" << "if (x) {\n    y = 1\n}" << "if (x) {\n    y = 1;\n}";
    QTest::newRow("semicolon control") << "semicolon" << "for (i = 0; i < n; i++)\n    s += i" << "for (i = 0; i < n; i++)\n    s += i;";
    QTest::newRow("semicolon else") << "semicolon" << "if (x)\n    a()\nelse\n    b()" << "if (x)\n    a();\nelse\n    b();";
    QTest::newRow("semicolon initializer") << "semicolon" << "int a[] = {1, 2}" << "int a[] = {1, 2};";
    QTest::newRow("semicolon braced init") << "semicolon" << "std::vector<int> v{1, 2}" << "std::vector<int> v{1, 2};";
    QTest::newRow("semicolon continued") << "semicolon" << "s = a +\n    b" << "s = a +\n    b;";
    QTest::newRow("semicolon continued next") << "semicolon" << "s = a\n    + b" << "s = a\n    + b;";
    QTest::newRow("semicolon arguments") << "semicolon" << "x = f(a,\n  b)" << "x = f(a,\n  b);";
    QTest::newRow("semicolon comments") << "semicolon" << "// note\nx = 1\n# define" << "// note\nx = 1;\n# define";
    QTest::newRow("semicolon label") << "semicolon" << "case 1:\nx = 2" << "case 1:\nx = 2;";
    QTest::newRow("semicolon pascal") << "semicolon" << "if a then\nbegin\n  b := 1\nend" << "if a then\nbegin\n  b := 1;\nend;";
    QTest::newRow("semicolon empty") << "semicolon" << "" << "";
    QTest::newRow("semicolon blank lines") << "semicolon" << "a = 1\n\nb = 2" << "a = 1;\n\nb = 2;";
    QTest::newRow("array ref") << "array:ref" << "arr" << "@$arr";
    QTest::newRow("array ref literal") << "array:ref" << "[1, 2]" << "(1, 2)";
}

void Test_generator::filters()
{
    QFETCH(QString, filter);
    QFETCH(QString, input);
    QFETCH(QString, expected);
    const QByteArray rule = QStringLiteral(R"json({
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "[%text|FILTER%]"}
    })json").replace(QStringLiteral("FILTER"), filter).toUtf8();
    QDomDocument doc = parseXml(QStringLiteral("<algorithm><branch><process/></branch></algorithm>"));
    doc.elementsByTagName(QStringLiteral("process")).at(0).toElement().setAttribute(QStringLiteral("text"), input);
    SourceCodeGenerator generator;
    generator.ruleFromJSON(rule);
    QCOMPARE(generator.applyRule(doc), QStringLiteral("[%1]\n").arg(expected));
}

void Test_generator::ifChain()
{
    const QByteArray rule = R"json({
        "additional_settings": {"indentation_preference": "    "},
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "%text%;"},
        "if": {
            "template": ["if (%cond%) {", "\t%branch1%", "} else {", "\t%branch2%", "}"],
            "template_empty_branch2": ["if (%cond%) {", "\t%branch1%", "}"],
            "chain": {"head": ["if (%cond%) {", "\t%branch1%"], "else_if": ["} else if (%cond%) {", "\t%branch1%"],
                      "else": ["} else {", "\t%branch2%"], "end": "}"}
        }
    })json";
    const QString chain = QStringLiteral(
        "<if cond='a'><branch><process text='x'/></branch><branch>"
        "  <if cond='b'><branch><process text='y'/></branch><branch>"
        "    <if cond='c'><branch/><branch><process text='z'/></branch></if>"
        "  </branch></if>"
        "</branch></if>");
    COMPARE_TEXT(generate(rule, chain),
                 QStringLiteral("if (a) {\n    x;\n} else if (b) {\n    y;\n} else if (c) {\n} else {\n    z;\n}\n"));
    // no final else
    COMPARE_TEXT(generate(rule, QStringLiteral("<if cond='a'><branch/><branch><if cond='b'><branch/><branch/></if></branch></if>")),
                 QStringLiteral("if (a) {\n} else if (b) {\n}\n"));
    // an else branch with more than one block is not a chain
    COMPARE_TEXT(generate(rule, QStringLiteral("<if cond='a'><branch/><branch><if cond='b'><branch/><branch/></if>"
                                               "<process text='w'/></branch></if>")),
                 QStringLiteral("if (a) {\n} else {\n    if (b) {\n    }\n    w;\n}\n"));
}

void Test_generator::variablesCollection()
{
    const QByteArray rule = R"json({
        "additional_settings": {"keywords": ["Result"]},
        "algorithm": {"template": "vars: %variables%"},
        "foreach": {"template": "", "declares": ["%var|name%", "%var|name%_i"]}
    })json";
    const QString body = QStringLiteral(
        "<assign dest='s' src='0'/><assign dest='a[i]' src='0'/><io vars='n, m, arr[2]'/>"
        "<for var='i' from='1' to='n'><branch><assign dest='s' src='s + i'/></branch></for>"
        "<forc init='j = 0' cond='j &lt; n' step='j++'><branch/></forc><forc init='int k = 0'><branch/></forc>"
        "<foreach var='x' range='arr'><branch/></foreach>"
        "<assign dest='p' src='1'/><assign dest='Result' src='1'/><assign dest='sum' src='1'/>"
        "<process text='count = 0; k2 = count\nint z; w == 1'/>");
    COMPARE_TEXT(generate(rule, body, QStringLiteral("name='sum' params='int p, q'")),
                 QStringLiteral("vars: s,n,m,i,j,k,x,x_i,count,k2\n"));
    // case insensitive languages declare each variable once
    const QByteArray ci = R"json({
        "additional_settings": {"case_insensitive": true},
        "algorithm": {"template": "%variables%", "list": {"variables": {"glue": " "}}}
    })json";
    COMPARE_TEXT(generate(ci, QStringLiteral("<assign dest='Total' src='0'/><assign dest='total' src='1'/>"
                                             "<foreach var='int v' range='r'><branch/></foreach>")),
                 QStringLiteral("Total v\n"));
}

void Test_generator::typeMapping()
{
    const QByteArray rule = R"json({
        "additional_settings": {"types": {"": "Integer", "int": "Integer", "char*": "String", "double": "Real"}},
        "algorithm": {"template": ["function %name%%args%: %returns|maptype%;", "\t%branch1%"],
                      "list": {"args": {"source": "params", "item": "%$name%: %$type|maptype%", "glue": "; ",
                                        "head": "(", "tail": ")"}}}
    })json";
    COMPARE_TEXT(generate(rule, QString(), QStringLiteral("name='f' params='int a, const char *s, n, double &amp;d, Foo x' returns='double'")),
                 QStringLiteral("function f(a: Integer; s: String; n: Integer; d: Real; x: Foo): Real;\n"));
    COMPARE_TEXT(generate(rule, QString(), QStringLiteral("name='g' returns='int'")), QStringLiteral("function g: Integer;\n"));
}

void Test_generator::templateArrays()
{
    const QByteArray rule = R"json({
        "algorithm": {"template": ["a", "\t%branch1%", "b"]},
        "process": {"template": ["%text%", "\tdone"]},
        "additional_settings": {"indentation_preference": "\t", "empty_body": ["x", "y"]},
        "if": {"template": ["if", "\t%branch1%"]}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<process text='p'/><if><branch/></if>")),
                 QStringLiteral("a\n\tp\n\t\tdone\n\tif\n\t\tx\n\t\ty\nb\n"));
}

void Test_generator::legacyRule()
{
    // the rule format of AFCE <= 0.9.8 (the original py.json and php.json keys)
    const QByteArray rule = R"json({
        "name": {"en_US": "Python"},
        "algorithm": {"template": "def main():%branch1%\nmain()"},
        "if": {"template": "if %cond%:%branch1%\nelse: %branch2%", "template_shortened": "if %cond%:%branch1%"},
        "post": {"template": "while True:%branch1%\n\tif %cond%:\n\t\tbreak"},
        "io": {"template": "%vars%", "list": ["vars"], "separator": ",", "glue": "\n",
               "prefix": "$", "suffix": " = $_GET[\"%$%\"];"},
        "ou": {"template": "print(%vars%)", "list": ["vars"], "separator": ",", "glue": ", "},
        "assign": {"template": "%dest% = %src%"},
        "additional_settings": {"indentation_preference": "    "}
    })json";
    const QString body = QStringLiteral(
        "<io vars='a,b'/>"
        "<post cond='a &gt; b'><branch><if cond='a'><branch><assign dest='a' src='1'/></branch><branch/></if>"
        "<if cond='b'><branch><ou vars='a,b'/></branch><branch><ou vars='b'/></branch></if></branch></post>");
    COMPARE_TEXT(generate(rule, body),
                 QStringLiteral("def main():\n"
                                "    $a = $_GET[\"a\"];\n"
                                "    $b = $_GET[\"b\"];\n"
                                "    while True:\n"
                                "        if a:\n"
                                "            a = 1\n"
                                "        if b:\n"
                                "            print(a, b)\n"
                                "        else:\n"
                                "            print(b)\n"
                                "        if a > b:\n"
                                "            break\n"
                                "main()\n"));
}

void Test_generator::legacyInputOutputAttributes()
{
    // io/ou of files older than 0.9.7: t1..t8 instead of vars
    const QByteArray rule = R"json({
        "algorithm": {"template": ["%variables%", "%branch1%"]},
        "io": {"template": "read(%vars%)", "list": {"vars": {"glue": ", "}}},
        "ou": {"template": "write(%vars%)", "template_single_vars": "writeln(%vars%)", "list": {"vars": {"glue": ", "}}}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<io t1='a' t2='b' t3='' t4='' t5='' t6='' t7='' t8=''/>"
                                               "<ou t1='a + b' t2=''/><ou vars='x' t1='y'/>")),
                 QStringLiteral("a,b\nread(a, b)\nwriteln(a + b)\nwrite(x, y)\n"));
}

void Test_generator::invalidRules()
{
    SourceCodeGenerator generator;
    QVERIFY(!generator.isLoaded());
    generator.ruleFromJSON("{ not json");
    QVERIFY(!generator.isLoaded());
    QVERIFY(!generator.errorString().isEmpty());
    QCOMPARE(generator.applyRule(parseXml(QStringLiteral("<algorithm><branch><process text='a'/></branch></algorithm>"))),
             QString());
    generator.ruleFromJSON("[1, 2]");
    QVERIFY(!generator.isLoaded());
    generator.loadRule(QStringLiteral("/nonexistent/rules.json"));
    QVERIFY(!generator.isLoaded());
    QVERIFY(generator.errorString().contains(QStringLiteral("rules.json")));
    generator.ruleFromJSON(R"json({"algorithm": {"template": "%branch1%"}})json");
    QVERIFY(generator.isLoaded());
    QVERIFY(generator.errorString().isEmpty());
    // documents without an algorithm, empty documents, broken templates
    QCOMPARE(generator.applyRule(QDomDocument()), QString());
    QCOMPARE(generator.applyRule(parseXml(QStringLiteral("<other/>"))), QString());
    generator.ruleFromJSON(R"json({"algorithm": {"template": "%branch1% %%% %unterminated %$% %|x% %.a%"}})json");
    QCOMPARE(generator.applyRule(parseXml(QStringLiteral("<algorithm/>"))), QStringLiteral(" %% %unterminated  %|x% %.a%\n"));
}

void Test_generator::loadRuleByIdAndPath()
{
    const QDomDocument doc = parseXml(QStringLiteral("<algorithm><branch><assign dest='a' src='1'/></branch></algorithm>"));
    SourceCodeGenerator byId;
    byId.loadRule(QStringLiteral("c"));
    QVERIFY2(byId.isLoaded(), qPrintable(byId.errorString()));
    SourceCodeGenerator bySearchPath;
    bySearchPath.loadRule(QStringLiteral("generators:c.json"));
    QVERIFY2(bySearchPath.isLoaded(), qPrintable(bySearchPath.errorString()));
    SourceCodeGenerator byPath;
    byPath.loadRule(generatorsDir() + QStringLiteral("/c.json"));
    QVERIFY2(byPath.isLoaded(), qPrintable(byPath.errorString()));
    QCOMPARE(byId.applyRule(doc), byPath.applyRule(doc));
    QCOMPARE(bySearchPath.applyRule(doc), byPath.applyRule(doc));
    QVERIFY(byPath.applyRule(doc).contains(QStringLiteral("    a = 1;\n")));
    // every embedded generator can be found
    QVERIFY(afce::generatorFiles().size() >= 13);
}

void Test_generator::languageNames()
{
    SourceCodeGenerator generator;
    generator.ruleFromJSON(R"json({"name": {"en_US": "Pascal", "ru_RU": "Паскаль"}})json");
    QCOMPARE(generator.languageName(QLocale(QStringLiteral("ru_RU"))), QStringLiteral("Паскаль"));
    QCOMPARE(generator.languageName(QLocale(QStringLiteral("ru_UA"))), QStringLiteral("Паскаль"));
    QCOMPARE(generator.languageName(QLocale(QStringLiteral("de_DE"))), QStringLiteral("Pascal"));
    generator.ruleFromJSON(R"json({"name": {"ru_RU": "Только"}})json");
    QCOMPARE(generator.languageName(QLocale(QStringLiteral("de_DE")), QStringLiteral("id")), QStringLiteral("id"));
    generator.ruleFromJSON(R"json({})json");
    QCOMPARE(generator.languageName(QLocale(), QStringLiteral("x")), QStringLiteral("x"));
}

void Test_generator::voidParametersAndReturns()
{
    // the importer writes "void" for f(void) and for void functions: both mean "none"
    const QByteArray rule = R"json({
        "algorithm": {"template": ["%returns% %name%(%args%)", "\t%branch1%"],
                      "template_empty_returns": ["sub %name%(%args%)", "\t%branch1%"],
                      "template_empty_params": ["%returns% %name%()", "\t%branch1%"],
                      "template_empty_params+returns": ["proc %name%", "\t%branch1%"],
                      "list": {"args": {"source": "params", "item": "%$name%", "glue": ", "}}},
        "return": {"template": "return %value%", "template_empty_algorithm.returns": "exit sub"}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<return value='1'/>"), QStringLiteral("name='f' params='void' returns='void'")),
                 QStringLiteral("proc f\n  exit sub\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<return value='1'/>"), QStringLiteral("name='f' params=' void ' returns='int'")),
                 QStringLiteral("int f()\n  return 1\n"));
    COMPARE_TEXT(generate(rule, QStringLiteral("<return value='1'/>"), QStringLiteral("name='f' params='int a' returns=' void'")),
                 QStringLiteral("sub f(a)\n  exit sub\n"));
    // pointers are real types
    COMPARE_TEXT(generate(rule, QStringLiteral("<return value='p'/>"), QStringLiteral("name='f' params='void *p' returns='void *'")),
                 QStringLiteral("void * f(p)\n  return p\n"));
    // C and C++ keep their own spelling
    COMPARE_TEXT(generateFile(generatorsDir() + QStringLiteral("/c.json"), samplesDir() + QStringLiteral("/void.afc")).section(QLatin1Char('\n'), 3, 3),
                 QStringLiteral("void beep(void) {"));
}

void Test_generator::attributeWhitespace()
{
    // line breaks inside expressions and lists must not break line-oriented languages;
    // process text and string literals are kept as written
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "assign": {"template": "%dest% = %src%"},
        "process": {"template": "%text%"},
        "if": {"template": ["if %cond%:", "\t%branch1%"]},
        "ou": {"template": "print(%vars%)", "list": {"vars": {"glue": ", "}}}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<assign dest='x' src='1 +&#10;  2'/><if cond='x &gt; 1 &amp;&amp;&#10;x &lt; 5'>"
                                               "<branch><ou vars='x,&#10;&quot;a  b&quot;'/></branch></if>"
                                               "<process text='a = 1&#10;b = 2'/>")),
                 QStringLiteral("x = 1 + 2\nif x > 1 && x < 5:\n  print(x, \"a  b\")\na = 1\nb = 2\n"));
}

void Test_generator::typedItemsAndLimit()
{
    // item_<kind> templates are chosen by the heuristic kind of an expression item; kinds come from
    // typed parameters, C declarations in process blocks and assigned expressions
    const QByteArray rule = R"json({
        "algorithm": {"template": ["%incl%", "%branch1%"],
                      "list": {"incl": {"source": "variables_real", "item": "#include <math.h>", "limit": 1}}},
        "process": {"template": "%text%;"},
        "assign": {"template": "%dest% = %src%;"},
        "ou": {"template": "printf(\"%fmt%\"%args%);",
               "list": {"fmt": {"source": "vars", "item": "%d", "item_real": "%g", "item_string": "%s", "item_char": "%c",
                                "item_bool": "%b", "literal_item": "%$text%", "glue": " "},
                        "args": {"source": "vars", "item": ", %$%", "literal_item": ""}}},
        "io": {"template": "%vars%", "list": {"vars": {"item": "read_int(%$%)", "item_real": "read_real(%$%)", "glue": "\n"}}}
    })json";
    const QString body = QStringLiteral(
        "<process text='char name[20]; double *p; std::string t; unsigned long u'/>"
        "<assign dest='r' src='x * 2'/><assign dest='r2' src='r + 1'/><assign dest='b' src='n &gt; 0'/>"
        "<io vars='r, n'/>"
        "<ou vars='s, c, x, n, name, name[0], r, r2, b, f[1], t, p, u, unknown, g(x), sqrt(n), &quot;lit&quot;, &apos;q&apos;, "
        "(double) n / 2, n / 2, c + 1, s[0], b ? 1.5 : 2'/>");
    COMPARE_TEXT(generate(rule, body, QStringLiteral("name='f' params='const char *s, char c, double x, int n, float f[]'")),
                 QStringLiteral("#include <math.h>\n"
                                "char name[20]; double *p; std::string t; unsigned long u;\n"
                                "r = x * 2;\nr2 = r + 1;\nb = n > 0;\nread_real(r)\nread_int(n)\n"
                                "printf(\"%s %c %g %d %s %c %g %g %b %g %s %d %d %d %d %g lit q %g %d %d %c %g\", s, c, x, n, name, "
                                "name[0], r, r2, b, f[1], t, p, u, unknown, g(x), sqrt(n), (double) n / 2, n / 2, c + 1, s[0], "
                                "b ? 1.5 : 2);\n"));
    // no real variable: no include
    COMPARE_TEXT(generate(rule, QStringLiteral("<assign dest='i' src='1'/>")), QStringLiteral("i = 1;\n"));
}

void Test_generator::binaryOperatorsAndCasts_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    // context: n int, x real, s string (see below)
    QTest::newRow("mod") << "a % b" << "Mod(a, b)";
    QTest::newRow("mod left chain") << "a * b % c" << "Mod(a * b, c)";
    QTest::newRow("mod then multiply") << "a % b * c" << "Mod(a, b) * c";
    QTest::newRow("mod additive") << "a + b % c - d" << "a + Mod(b, c) - d";
    QTest::newRow("mod unary") << "-a % -b" << "Mod(-a, -b)";
    QTest::newRow("mod parenthesized") << "(a + b) % c" << "Mod(a + b, c)";
    QTest::newRow("mod nested") << "a[i % n] % m" << "Mod(a[Mod(i, n)], m)";
    QTest::newRow("mod call args") << "f(n % 2, g(x))" << "f(Mod(n, 2), g(x))";
    QTest::newRow("mod members") << "p->x % 2 + q.y % 3" << "Mod(p->x, 2) + Mod(q.y, 3)";
    QTest::newRow("mod postfix") << "i++ % 2" << "Mod(i++, 2)";
    QTest::newRow("mod literal kept") << "\"x % y\" + a % b" << "\"x % y\" + Mod(a, b)";
    QTest::newRow("mod compound kept") << "a %= b" << "a %= b";
    QTest::newRow("mod statements") << "x = n / 2; y = n % 3" << "x = n div 2; y = Mod(n, 3)";
    QTest::newRow("mod after keyword") << "return n % 2" << "return Mod(n, 2)";
    QTest::newRow("mod condition") << "if (n % 2 == 0) { x = n % 3; }" << "if (Mod(n, 2) == 0) { x = Mod(n, 3); }";
    QTest::newRow("unbalanced block kept") << "if (n % 2 == 0) {" << "if (n % 2 == 0) {";
    QTest::newRow("int division") << "n / 2" << "n div 2";
    QTest::newRow("int division chain") << "n / 2 * 3" << "(n div 2) * 3";
    QTest::newRow("int division twice") << "n / 2 / 2" << "(n div 2) div 2";
    QTest::newRow("int division product") << "n * 3 / 2" << "n * 3 div 2";
    QTest::newRow("real division") << "x / 2" << "x / 2";
    QTest::newRow("real literal") << "n / 2.0" << "n / 2.0";
    QTest::newRow("real exponent") << "n / 1e3" << "n / 1e3";
    QTest::newRow("real function") << "sqrt(n) / 2" << "sqrt(n) / 2";
    QTest::newRow("string division") << "s / 2" << "s / 2";
    QTest::newRow("unknown is int") << "total / count" << "total div count";
    QTest::newRow("cast real") << "(double) n / 2" << "n / 2";
    QTest::newRow("cast int") << "(int) x / 2" << "Int(x) div 2";
    QTest::newRow("cast parenthesized") << "(int) (x * 10) % 7" << "Mod(Int(x * 10), 7)";
    QTest::newRow("cast unsigned") << "(unsigned int) x" << "Int(x)";
    QTest::newRow("cast pointer") << "(char *) p" << "p";
    QTest::newRow("cast unknown type kept") << "(Foo) x" << "(Foo) x";
    QTest::newRow("not a cast") << "(n) - 1" << "(n) - 1";
    QTest::newRow("sizeof") << "sizeof(int) * 2 / n" << "sizeof(int) * 2 div n";
    QTest::newRow("other syntax kept") << "a ** b % c" << "a ** b % c";
    QTest::newRow("floor division kept") << "a // b % c" << "a // b % c";
    QTest::newRow("unbalanced kept") << "((a % b" << "((a % b";
    QTest::newRow("ternary") << "n ? a % b : c" << "n ? Mod(a, b) : c";
}

void Test_generator::binaryOperatorsAndCasts()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    const QByteArray rule = R"json({
        "additional_settings": {
            "operators": {"%": {"binary": "Mod(%1, %2)"}, "/": {"binary": "%1 div %2", "when": "int"}, "&&": "and"},
            "casts": {"int": "Int(%1)", "real": "%1", "string": "%1", "pointer": "%1"}
        },
        "algorithm": {"template": "%branch1%"},
        "assign": {"template": ""},
        "process": {"template": "[%text%]"}
    })json";
    QDomDocument doc = parseXml(QStringLiteral("<algorithm><branch><assign dest='n' src='1'/><assign dest='x' src='1.5'/>"
                                               "<assign dest='s' src='&quot;a&quot;'/><process/></branch></algorithm>"));
    doc.elementsByTagName(QStringLiteral("process")).at(0).toElement().setAttribute(QStringLiteral("text"), input);
    SourceCodeGenerator generator;
    generator.ruleFromJSON(rule);
    QCOMPARE(generator.applyRule(doc), QStringLiteral("[%1]\n").arg(expected));
}

void Test_generator::truthyFilter_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    // n int, x real, b bool, s string, c char, p typed and q untyped parameters, "unknown" never assigned
    QTest::newRow("int") << "n" << "n != 0";
    QTest::newRow("not int") << "!n" << "n == 0";
    QTest::newRow("bool") << "b" << "b";
    QTest::newRow("not bool") << "!b" << "!b";
    QTest::newRow("real") << "x" << "x != 0";
    QTest::newRow("string") << "s" << "s";
    QTest::newRow("char") << "c" << "c";
    QTest::newRow("unknown") << "unknown" << "unknown";
    QTest::newRow("call") << "f(n)" << "f(n)";
    QTest::newRow("arithmetic") << "n % 2" << "n % 2 != 0";
    QTest::newRow("bitwise") << "n & 1" << "(n & 1) != 0";
    QTest::newRow("and") << "n && !b" << "n != 0 && !b";
    QTest::newRow("or") << "n || x > 1" << "n != 0 || x > 1";
    QTest::newRow("not group") << "!(n - 3)" << "!(n - 3 != 0)";
    QTest::newRow("group") << "(n)" << "(n != 0)";
    QTest::newRow("nested") << "n > 0 && (x || b)" << "n > 0 && (x != 0 || b)";
    QTest::newRow("one") << "1" << "true";
    QTest::newRow("zero") << "0" << "false";
    QTest::newRow("not zero") << "!0" << "true";
    QTest::newRow("hex") << "0x10" << "true";
    QTest::newRow("parameters") << "p && q" << "p != 0 && q != 0";
    QTest::newRow("comparison") << "n == 1" << "n == 1";
    QTest::newRow("literal") << "\"a || b\"" << "\"a || b\"";
    QTest::newRow("subscript unknown") << "a[n]" << "a[n]";
    QTest::newRow("empty") << "" << "";
}

void Test_generator::truthyFilter()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    const QByteArray rule = R"json({
        "algorithm": {"template": "%branch1%"},
        "assign": {"template": ""},
        "process": {"template": "[%text|truthy%]"},
        "if": {"template": "if %cond|truthy|negate% then"}
    })json";
    QDomDocument doc = parseXml(QStringLiteral(
        "<algorithm params='int p, q'><branch><assign dest='n' src='1'/><assign dest='x' src='1.5'/><assign dest='b' src='true'/>"
        "<assign dest='s' src='&quot;a&quot;'/><assign dest='c' src='&apos;c&apos;'/><process/></branch></algorithm>"));
    doc.elementsByTagName(QStringLiteral("process")).at(0).toElement().setAttribute(QStringLiteral("text"), input);
    SourceCodeGenerator generator;
    generator.ruleFromJSON(rule);
    QCOMPARE(generator.applyRule(doc), QStringLiteral("[%1]\n").arg(expected));
    // negation after truthiness
    COMPARE_TEXT(generate(rule, QStringLiteral("<assign dest='n' src='1'/><if cond='n'/><if cond='!n &amp;&amp; m &gt; 1'/>")),
                 QStringLiteral("if n == 0 then\nif !(n == 0 && m > 1) then\n"));
}

void Test_generator::literalConversion()
{
    // "literals": string and char literals of expressions in the target notation (Pascal, BASIC)
    const QByteArray rule = R"json({
        "additional_settings": {
            "literals": {
                "string": {"item": "'%$text%'", "text_escape": {"'": "''", "\\\"": "\"", "\\n": "'#10'"}},
                "char": {"item": "chr(%$%)"}
            },
            "operators": {"==": "="}
        },
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "[%text%]"},
        "ou": {"template": "write(%vars%)", "list": {"vars": {"glue": ", ", "literal_item": "<%$text%>"}}}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<process text='s = &quot;it&apos;s \\&quot;ok\\&quot;\\n&quot; + &apos;x&apos;'/>"
                                               "<process text='c == &apos;y&apos; &amp;&amp; t == &quot;%x%&quot;'/>"
                                               "<process text='u = &quot;unterminated'/>"
                                               "<ou vars='&quot;a&quot;, &quot;b&quot; + s'/>")),
                 QStringLiteral("[s = 'it''s \"ok\"'#10'' + chr('x')]\n"
                                "[c = chr('y') && t = '%x%']\n"
                                "[u = \"unterminated]\n"
                                "write(<a>, 'b' + s)\n"));
    // without a "char" entry chars use the "string" one
    const QByteArray stringOnly = R"json({
        "additional_settings": {"literals": {"string": {"item": "\"%$text%\"", "text_escape": {"\"": "\"\""}}}},
        "algorithm": {"template": "%branch1%"},
        "assign": {"template": "%dest% = %src%"}
    })json";
    COMPARE_TEXT(generate(stringOnly, QStringLiteral("<assign dest='c' src='&apos;&quot;&apos;'/>")),
                 QStringLiteral("c = \"\"\"\"\n"));
}

void Test_generator::prefixSubscriptsCastsAndCalls_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("subscript") << "arr[i] + x" << "$arr->[$i] + $x";
    QTest::newRow("nested subscript") << "m[a[i]][j]" << "$m->[$a->[$i]][$j]";
    QTest::newRow("prefixed subscript kept") << "$arr[0]" << "$arr[0]";
    QTest::newRow("cast") << "(double) sum / n" << "(double) $sum / $n";
    QTest::newRow("cast without space") << "(int)x" << "(int)$x";
    QTest::newRow("parenthesized variable") << "(n) * 2" << "($n) * 2";
    QTest::newRow("builtin names are variables") << "length = array + index" << "$length = $array + $index";
    QTest::newRow("list operator string") << "print \"x\", n" << "print \"x\", $n";
    QTest::newRow("list operator array") << "push @a, n" << "push @a, $n";
    QTest::newRow("list operator scalar") << "chomp $line" << "chomp $line";
    QTest::newRow("word operators") << "a and b or not c" << "$a and $b or not $c";
    QTest::newRow("call") << "f (x)" << "f ($x)";
}

void Test_generator::prefixSubscriptsCastsAndCalls()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    const QByteArray rule = R"json({
        "additional_settings": {"variable_prefix": "$", "keywords": ["and", "or", "not"], "prefix_subscript": "->"},
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "[%text%]"}
    })json";
    QDomDocument doc = parseXml(QStringLiteral("<algorithm><branch><process/></branch></algorithm>"));
    doc.elementsByTagName(QStringLiteral("process")).at(0).toElement().setAttribute(QStringLiteral("text"), input);
    SourceCodeGenerator generator;
    generator.ruleFromJSON(rule);
    QCOMPARE(generator.applyRule(doc), QStringLiteral("[%1]\n").arg(expected));
}

void Test_generator::templateEmptyCases()
{
    // "template_empty_cases": a case without regular branches (hand-made files)
    const QByteArray rule = R"json({
        "additional_settings": {"indentation_preference": "  "},
        "algorithm": {"template": "%branch1%"},
        "process": {"template": "%text%"},
        "case": {"template": ["case %expr%", "%branches%", "end"], "template_empty_cases": "%branches%",
                 "case_branch": ["when %value%", "\t%body%"], "default_branch": ["else", "\t%body%"],
                 "default_branch_only": "%body%", "list": {"value": {"glue": ", "}}}
    })json";
    COMPARE_TEXT(generate(rule, QStringLiteral("<case expr='x'><branch><process text='a'/></branch></case>"
                                               "<case expr='y'/>"
                                               "<case expr='z'><branch value='1'><process text='b'/></branch><branch/></case>")),
                 QStringLiteral("a\ncase z\nwhen 1\n  b\nelse\nend\n"));
}

void Test_generator::undeclaredVariables()
{
    // %undeclared...%: the variables C-like languages must declare themselves - not the ones the
    // chart declares (C declarations in process blocks), not loop variables declared by their header
    const QByteArray rule = R"json({
        "algorithm": {"template": ["int: %undeclared_int%", "real: %undeclared_real%", "bool: %undeclared_bool%",
                                   "string: %undeclared_string%", "char: %undeclared_char%", "all: %undeclared%",
                                   "variables: %variables%", "reals: %variables_real%"]}
    })json";
    const QString body = QStringLiteral(
        "<process text='int a = 1, b; double d&#10;e = 2'/>"
        "<assign dest='a' src='2'/><assign dest='f' src='1.5'/><assign dest='g' src='&quot;x&quot;'/>"
        "<assign dest='h' src='&apos;c&apos;'/><assign dest='ok' src='a &gt; 0'/><assign dest='q' src='d * 2'/>"
        "<forc init='int i = 0, i2 = 1' cond='i &lt; 3' step='i++'><branch/></forc>"
        "<forc init='j = 0' cond='j &lt; 3' step='j++'><branch/></forc>"
        "<for var='k' from='1' to='3'><branch/></for><for var='long k2' from='1' to='3'><branch/></for>"
        "<foreach var='int v' range='arr'><branch/></foreach><foreach var='w' range='name'><branch/></foreach>"
        "<assign dest='k' src='5'/><io vars='m, p'/>");
    COMPARE_TEXT(generate(rule, body, QStringLiteral("name='calc' params='double p'")),
                 QStringLiteral("int: e,j,k,m\nreal: f,q\nbool: ok\nstring: g\nchar: h\nall: e,f,g,h,ok,q,j,k,m\n"
                                "variables: e,f,g,h,ok,q,i,i2,j,k,k2,v,w,m\nreals: f,q\n"));
}

void Test_generator::mainParams()
{
    // %main_params%: the parameters of the program's entry point (name empty or "main")
    const QByteArray rule = R"json({
        "algorithm": {"template": ["def %name%(%params%)", "%entry%"], "defaults": {"name": "main"},
                      "list": {"entry": {"source": "main_params", "item": "%name%(argc, argv)", "limit": 1}}}
    })json";
    COMPARE_TEXT(generate(rule, QString(), QStringLiteral("params='int argc, char **argv'")),
                 QStringLiteral("def main(int argc, char **argv)\nmain(argc, argv)\n"));
    COMPARE_TEXT(generate(rule, QString(), QStringLiteral("name='main' params='int argc'")),
                 QStringLiteral("def main(int argc)\nmain(argc, argv)\n"));
    COMPARE_TEXT(generate(rule, QString(), QStringLiteral("name='f' params='int argc'")), QStringLiteral("def f(int argc)\n"));
    COMPARE_TEXT(generate(rule, QString(), QStringLiteral("name='main' params='void'")), QStringLiteral("def main()\n"));
}

void Test_generator::deepNesting()
{
    // hand-made documents: deep block nesting, long else-if chains and deeply nested parentheses must
    // not overflow the stack; blocks below the limit are replaced by a comment
    QString deep = QStringLiteral("<algorithm><branch>");
    for (int i = 0; i < 3000; ++i)
        deep += QStringLiteral("<if cond='a'><branch>");
    deep += QStringLiteral("<assign dest='x' src='1'/>");
    for (int i = 0; i < 3000; ++i)
        deep += QStringLiteral("</branch><branch/></if>");
    deep += QStringLiteral("</branch></algorithm>");
    QString chain = QStringLiteral("<algorithm><branch>");
    for (int i = 0; i < 3000; ++i)
        chain += QStringLiteral("<if cond='a == %1'><branch><assign dest='x' src='%1'/></branch><branch>").arg(i);
    chain += QStringLiteral("<assign dest='x' src='-1'/>");
    for (int i = 0; i < 3000; ++i)
        chain += QStringLiteral("</branch></if>");
    chain += QStringLiteral("</branch></algorithm>");
    const QString parens = QStringLiteral("<algorithm><branch><assign dest='x' src='%1 %% 2'/><if cond='%1'><branch/></if>"
                                          "</branch></algorithm>")
                               .arg(QString(20000, QLatin1Char('(')) + QLatin1Char('a') + QString(20000, QLatin1Char(')')));
    const QStringList rules = QDir(generatorsDir()).entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
    for (const QString &r : rules) {
        SourceCodeGenerator generator;
        generator.loadRule(generatorsDir() + QLatin1Char('/') + r);
        const QString deepCode = generator.applyRule(parseXml(deep));
        QVERIFY2(deepCode.contains(QStringLiteral("nesting too deep")), qPrintable(r));
        // rules with an "if" chain render else-if chains flat, whatever their length
        const QString chainCode = generator.applyRule(parseXml(chain));
        if (generator.rule().value(QLatin1String("if")).toObject().contains(QLatin1String("chain"))) {
            QVERIFY2(chainCode.contains(QStringLiteral("2999")), qPrintable(r));
            QVERIFY2(!chainCode.contains(QStringLiteral("nesting too deep")), qPrintable(r));
        } else {
            QVERIFY2(chainCode.contains(QStringLiteral("nesting too deep")), qPrintable(r));
        }
        QVERIFY2(!generator.applyRule(parseXml(parens)).isEmpty(), qPrintable(r));
    }
    // Python: the too deep block leaves an empty body behind
    SourceCodeGenerator py;
    py.loadRule(QStringLiteral("py"));
    const QString code = py.applyRule(parseXml(deep));
    QVERIFY(code.contains(QRegularExpression(QStringLiteral("# nesting too deep\n +pass\n"))));
}

// ---------------------------------------------------------------------------
// rule files

void Test_generator::allRulesComplete_data()
{
    QTest::addColumn<QString>("file");
    const QStringList files = QDir(generatorsDir()).entryList(QStringList() << QStringLiteral("*.json"), QDir::Files, QDir::Name);
    QCOMPARE(files.size(), 13);
    for (const QString &f : files)
        QTest::newRow(qPrintable(f)) << generatorsDir() + QLatin1Char('/') + f;
}

void Test_generator::allRulesComplete()
{
    QFETCH(QString, file);
    QFile f(file);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
    const QJsonObject rule = doc.object();
    QVERIFY(!rule.value(QLatin1String("name")).toObject().value(QLatin1String("en_US")).toString().isEmpty());
    QVERIFY(rule.value(QLatin1String("comment")).isObject());
    for (const QString &type : blockTypes()) {
        QVERIFY2(rule.value(type).isObject(), qPrintable(type));
        const QJsonObject r = rule.value(type).toObject();
        QVERIFY2(r.contains(QLatin1String("template")), qPrintable(type));
    }
    const QJsonObject c = rule.value(QLatin1String("case")).toObject();
    QVERIFY(c.contains(QLatin1String("case_branch")) || c.contains(QLatin1String("first_case_branch")));
    QVERIFY(c.contains(QLatin1String("default_branch")));
}

void Test_generator::golden_data()
{
    QTest::addColumn<QString>("sample");
    QTest::addColumn<QString>("id");
    const QStringList samples = QDir(samplesDir()).entryList(QStringList() << QStringLiteral("*.afc"), QDir::Files, QDir::Name);
    QVERIFY(samples.size() >= 10);
    const QStringList rules = QDir(generatorsDir()).entryList(QStringList() << QStringLiteral("*.json"), QDir::Files, QDir::Name);
    for (const QString &s : samples) {
        for (const QString &r : rules) {
            const QString id = QFileInfo(r).completeBaseName();
            QTest::newRow(qPrintable(QFileInfo(s).completeBaseName() + QLatin1Char('.') + id))
                << samplesDir() + QLatin1Char('/') + s << id;
        }
    }
}

void Test_generator::golden()
{
    QFETCH(QString, sample);
    QFETCH(QString, id);
    const QString code = generateFile(generatorsDir() + QLatin1Char('/') + id + QStringLiteral(".json"), sample);
    QVERIFY(!code.isEmpty() || sample.endsWith(QLatin1String("empty.afc")));
    const QString expectedFile = samplesDir() + QStringLiteral("/expected/") + QFileInfo(sample).completeBaseName()
                                 + QLatin1Char('.') + id + QStringLiteral(".txt");
    if (qgetenv("AFCE_UPDATE_GOLDEN") == "1") {
        QVERIFY(writeText(expectedFile, code));
        return;
    }
    QVERIFY2(QFile::exists(expectedFile), qPrintable(expectedFile + QStringLiteral(" is missing (run with AFCE_UPDATE_GOLDEN=1)")));
    COMPARE_TEXT(code, readText(expectedFile));
}

// Golden outputs must at least compile where a syntax checker is available.
void Test_generator::goldenSyntax_data()
{
    QTest::addColumn<QString>("id");
    QTest::newRow("py") << QStringLiteral("py");
    QTest::newRow("js") << QStringLiteral("js");
    QTest::newRow("ruby") << QStringLiteral("ruby");
    QTest::newRow("perl") << QStringLiteral("perl");
}

void Test_generator::goldenSyntax()
{
    QFETCH(QString, id);
    QString program;
    QStringList args;
    QString suffix;
    if (id == QLatin1String("py")) {
        program = tool({QStringLiteral("python3"), QStringLiteral("python")});
        args << QStringLiteral("-m") << QStringLiteral("py_compile");
        suffix = QStringLiteral(".py");
    } else if (id == QLatin1String("js")) {
        program = tool({QStringLiteral("node")});
        args << QStringLiteral("--check");
        suffix = QStringLiteral(".js");
    } else if (id == QLatin1String("ruby")) {
        program = tool({QStringLiteral("ruby")});
        args << QStringLiteral("-c");
        suffix = QStringLiteral(".rb");
    } else if (id == QLatin1String("perl")) {
        program = tool({QStringLiteral("perl")});
        args << QStringLiteral("-c");
        suffix = QStringLiteral(".pl");
    }
    if (program.isEmpty())
        QSKIP("toolchain not available");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QStringList samples = QDir(samplesDir()).entryList(QStringList() << QStringLiteral("*.afc"), QDir::Files, QDir::Name);
    for (const QString &s : samples) {
        QString code = generateFile(generatorsDir() + QLatin1Char('/') + id + QStringLiteral(".json"),
                                    samplesDir() + QLatin1Char('/') + s);
        // the samples use variables they never assign (inputs, "items", functions): relax strict mode
        if (id == QLatin1String("perl"))
            code.replace(QStringLiteral("use strict;\n"), QString());
        const QString file = dir.filePath(QFileInfo(s).completeBaseName() + suffix);
        QVERIFY(writeText(file, code));
        const RunResult r = run(program, QStringList(args) << file, dir.path());
        QVERIFY2(r.started, qPrintable(program));
        QVERIFY2(r.exitCode == 0, qPrintable(s + QStringLiteral(":\n") + r.err + r.out + QStringLiteral("\n") + code));
    }
}

void Test_generator::semantic_data()
{
    QTest::addColumn<QString>("sample");
    QTest::addColumn<QString>("id");
    const QString dir = samplesDir() + QStringLiteral("/semantic/");
    const QStringList common = {QStringLiteral("sem_loops"), QStringLiteral("sem_case"), QStringLiteral("sem_return")};
    const QStringList languages = {QStringLiteral("py"), QStringLiteral("js"), QStringLiteral("ruby"),
                                   QStringLiteral("perl"), QStringLiteral("c"), QStringLiteral("cpp")};
    for (const QString &s : common) {
        for (const QString &l : languages)
            QTest::newRow(qPrintable(s + QLatin1Char('.') + l)) << dir + s << l;
    }
    for (const QString &l : {QStringLiteral("py"), QStringLiteral("js"), QStringLiteral("ruby"), QStringLiteral("perl")})
        QTest::newRow(qPrintable(QStringLiteral("sem_foreach.") + l)) << dir + QStringLiteral("sem_foreach") << l;
    for (const QString &l : languages)
        QTest::newRow(qPrintable(QStringLiteral("sem_cfor.") + l)) << dir + QStringLiteral("sem_cfor") << l;
    QTest::newRow("sem_foreach_cpp.cpp") << dir + QStringLiteral("sem_foreach_cpp") << QStringLiteral("cpp");
    QTest::newRow("sem_foreach_c.c") << dir + QStringLiteral("sem_foreach_c") << QStringLiteral("c");
    // regression samples of the generator review
    const QStringList everywhere = {QStringLiteral("sem_intdiv"), QStringLiteral("sem_strings"),
                                    QStringLiteral("sem_default_only"), QStringLiteral("sem_utf8"),
                                    QStringLiteral("sem_forvar"), QStringLiteral("sem_multiline"),
                                    QStringLiteral("sem_mainargs")};
    for (const QString &s : everywhere) {
        for (const QString &l : languages)
            QTest::newRow(qPrintable(s + QLatin1Char('.') + l)) << dir + s << l;
    }
    for (const QString &s : {QStringLiteral("sem_arrays"), QStringLiteral("sem_void")}) {
        for (const QString &l : {QStringLiteral("py"), QStringLiteral("js"), QStringLiteral("ruby"), QStringLiteral("perl")})
            QTest::newRow(qPrintable(s + QLatin1Char('.') + l)) << dir + s << l;
    }
    QTest::newRow("sem_cdecl.c") << dir + QStringLiteral("sem_cdecl") << QStringLiteral("c");
    QTest::newRow("sem_cdecl.cpp") << dir + QStringLiteral("sem_cdecl") << QStringLiteral("cpp");
}

void Test_generator::semantic()
{
    QFETCH(QString, sample);
    QFETCH(QString, id);

    QFile f(sample + QStringLiteral(".afc"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QDomDocument doc;
    QVERIFY(doc.setContent(f.readAll()));

    SourceCodeGenerator generator;
    generator.loadRule(generatorsDir() + QLatin1Char('/') + id + QStringLiteral(".json"));
    QVERIFY(generator.isLoaded());
    const QString code = generator.applyRule(doc);
    const QString expected = readText(sample + QStringLiteral(".out"));
    QVERIFY(!expected.isEmpty());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    RunResult result;
    auto runScript = [&](const QStringList &names, const QString &file, const QStringList &extra = QStringList()) -> bool {
        const QString program = tool(names);
        if (program.isEmpty())
            return false;
        writeText(dir.filePath(file), code);
        result = run(program, QStringList(extra) << dir.filePath(file), dir.path());
        return true;
    };
    auto compileAndRun = [&](const QStringList &names, const QString &file, const QStringList &flags) -> bool {
        const QString compiler = tool(names);
        if (compiler.isEmpty())
            return false;
        writeText(dir.filePath(file), code);
#ifdef Q_OS_WIN
        const QString exe = dir.filePath(QStringLiteral("prog.exe"));
#else
        const QString exe = dir.filePath(QStringLiteral("prog"));
#endif
        const RunResult build = run(compiler, QStringList(flags) << dir.filePath(file) << QStringLiteral("-o") << exe, dir.path());
        if (build.exitCode != 0) {
            result = build;
            result.err = QStringLiteral("compilation failed:\n") + build.err;
            return true;
        }
        result = run(exe, QStringList(), dir.path());
        return true;
    };

    bool available = false;
    // C and C++ programs declare their variables themselves (the generator does it)
    const QStringList warnings = {QStringLiteral("-Wall"), QStringLiteral("-Wextra"), QStringLiteral("-Werror"),
                                  QStringLiteral("-Wno-unused-variable"), QStringLiteral("-Wno-unused-but-set-variable"),
                                  QStringLiteral("-Wno-unused-parameter")
#ifdef Q_OS_WIN
                                  // the MSVC runtime headers deprecate scanf & co.
                                  , QStringLiteral("-D_CRT_SECURE_NO_WARNINGS")
#endif
    };
    if (id == QLatin1String("py"))
        available = runScript({QStringLiteral("python3"), QStringLiteral("python")}, QStringLiteral("prog.py"));
    else if (id == QLatin1String("js"))
        available = runScript({QStringLiteral("node")}, QStringLiteral("prog.js"));
    else if (id == QLatin1String("ruby"))
        available = runScript({QStringLiteral("ruby")}, QStringLiteral("prog.rb"));
    else if (id == QLatin1String("perl"))
        available = runScript({QStringLiteral("perl")}, QStringLiteral("prog.pl"));
    else if (id == QLatin1String("c"))
        available = compileAndRun({QStringLiteral("clang"), QStringLiteral("cc"), QStringLiteral("gcc")}, QStringLiteral("prog.c"),
                                  QStringList() << QStringLiteral("-std=c11") << warnings);
    else if (id == QLatin1String("cpp"))
        available = compileAndRun({QStringLiteral("clang++"), QStringLiteral("c++"), QStringLiteral("g++")}, QStringLiteral("prog.cpp"),
                                  QStringList() << QStringLiteral("-std=c++17") << warnings);
    if (!available)
        QSKIP("toolchain not available");

    const QString context = QStringLiteral("\n--- program:\n") + code + QStringLiteral("--- stderr:\n") + result.err;
    QVERIFY2(result.started, qPrintable(context));
    QVERIFY2(result.exitCode == 0, qPrintable(QStringLiteral("exit code %1").arg(result.exitCode) + context));
    if (result.out != expected)
        QFAIL(qPrintable(QStringLiteral("output mismatch, ") + firstDifference(result.out, expected) + context
                         + QStringLiteral("--- stdout:\n") + result.out));
}

// Random (often malformed) flowcharts with hostile attribute values must never crash or hang.
void Test_generator::robustness()
{
    const QStringList types = {QStringLiteral("process"), QStringLiteral("assign"), QStringLiteral("io"),
                               QStringLiteral("ou"), QStringLiteral("if"), QStringLiteral("pre"),
                               QStringLiteral("post"), QStringLiteral("for"), QStringLiteral("forc"),
                               QStringLiteral("foreach"), QStringLiteral("case"), QStringLiteral("call"),
                               QStringLiteral("return"), QStringLiteral("break"), QStringLiteral("continue"),
                               QStringLiteral("frobnicate"), QStringLiteral("branch"), QStringLiteral("algorithm")};
    const QStringList attributes = {QStringLiteral("text"), QStringLiteral("dest"), QStringLiteral("src"),
                                    QStringLiteral("vars"), QStringLiteral("cond"), QStringLiteral("var"),
                                    QStringLiteral("from"), QStringLiteral("to"), QStringLiteral("init"),
                                    QStringLiteral("step"), QStringLiteral("range"), QStringLiteral("expr"),
                                    QStringLiteral("value"), QStringLiteral("name"), QStringLiteral("params"),
                                    QStringLiteral("returns")};
    const QStringList values = {
        QString(), QStringLiteral("x"), QStringLiteral("a + b"), QStringLiteral("%x%"), QStringLiteral("%branch1%"),
        QStringLiteral("%%"), QStringLiteral("%$%"), QStringLiteral("\"s, t\", 'c', 1"), QStringLiteral("'"),
        QStringLiteral("\"unterminated"), QStringLiteral("("), QStringLiteral(")("), QStringLiteral("a[i"),
        QStringLiteral("!"), QStringLiteral("!(a < b"), QStringLiteral("a && (b || c"), QStringLiteral("x = 1; y = 2"),
        QStringLiteral("\n\n"), QStringLiteral("i++"), QStringLiteral("n - 1"), QStringLiteral("-1"),
        QStringLiteral("f(x, y)"), QStringLiteral("Foo::bar"), QStringLiteral("$x @y"), QStringLiteral("мир < 2"),
        QStringLiteral("\\"), QStringLiteral("a<<b>>c"), QStringLiteral("p->q = 5"), QStringLiteral(",,,"),
        QStringLiteral("int a[10], const char *s"), QStringLiteral("\t\tx"), QStringLiteral("a ? b : c"),
        QStringLiteral("1..10"), QStringLiteral("[1, 2]"), QStringLiteral("not not x"), QStringLiteral("=")};
    const QStringList rules = QDir(generatorsDir()).entryList(QStringList() << QStringLiteral("*.json"), QDir::Files);
    QList<QSharedPointer<SourceCodeGenerator>> generators;
    for (const QString &r : rules) {
        auto g = QSharedPointer<SourceCodeGenerator>::create();
        g->loadRule(generatorsDir() + QLatin1Char('/') + r);
        QVERIFY(g->isLoaded());
        generators << g;
    }
    QRandomGenerator random(20260922);
    std::function<void(QDomDocument &, QDomElement &, int)> fill = [&](QDomDocument &doc, QDomElement &parent, int depth) {
        const int count = random.bounded(depth > 4 ? 2 : 5);
        for (int i = 0; i < count; ++i) {
            QDomElement e = doc.createElement(types.at(random.bounded(types.size())));
            const int attrs = random.bounded(4);
            for (int a = 0; a < attrs; ++a)
                e.setAttribute(attributes.at(random.bounded(attributes.size())), values.at(random.bounded(values.size())));
            const int branches = random.bounded(4);
            for (int b = 0; b < branches && depth < 7; ++b) {
                QDomElement branch = doc.createElement(QStringLiteral("branch"));
                if (random.bounded(3) == 0)
                    branch.setAttribute(QStringLiteral("value"), values.at(random.bounded(values.size())));
                fill(doc, branch, depth + 1);
                e.appendChild(branch);
            }
            parent.appendChild(e);
        }
    };
    QElapsedTimer timer;
    timer.start();
    for (int n = 0; n < 150; ++n) {
        QDomDocument doc;
        QDomElement algorithm = doc.createElement(QStringLiteral("algorithm"));
        for (const QString &attr : {QStringLiteral("name"), QStringLiteral("params"), QStringLiteral("returns")}) {
            if (random.bounded(2))
                algorithm.setAttribute(attr, values.at(random.bounded(values.size())));
        }
        doc.appendChild(algorithm);
        QDomElement body = doc.createElement(QStringLiteral("branch"));
        algorithm.appendChild(body);
        fill(doc, body, 0);
        for (const auto &g : std::as_const(generators)) {
            const QString code = g->applyRule(doc);
            QVERIFY(code.isEmpty() || code.endsWith(QLatin1Char('\n')));
        }
    }
    QVERIFY2(timer.elapsed() < 60000, "generation is too slow");
}

int main(int argc, char *argv[])
{
    // Unbuffered: a test that dies (or is killed) would otherwise lose its log.
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    Test_generator tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_generator.moc"
