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

#include "importtestutil.h"

#include "afceutil.h"

#include <QDomElement>
#include <QFile>
#include <QHash>
#include <QSet>

namespace importtest {

namespace {

QStringList &recordedProblems()
{
    static QStringList problems;
    return problems;
}

QList<QDomElement> childElements(const QDomElement &e)
{
    QList<QDomElement> result;
    for (QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.isElement())
            result << n.toElement();
    }
    return result;
}

void serializeBranch(const QDomElement &branch, int indent, QStringList &lines);

void serializeBlock(const QDomElement &e, int indent, QStringList &lines)
{
    const QString pad(indent * 2, QLatin1Char(' '));
    const QString tag = e.tagName();
    const QList<QDomElement> branches = childElements(e);
    auto a = [&](const char *name) { return e.attribute(QString::fromLatin1(name)); };

    if (tag == QLatin1String("process") || tag == QLatin1String("call")) {
        lines << pad + tag + QLatin1Char(' ') + a("text");
    } else if (tag == QLatin1String("assign")) {
        lines << pad + QLatin1String("assign ") + a("dest") + QLatin1String(" := ") + a("src");
    } else if (tag == QLatin1String("io") || tag == QLatin1String("ou")) {
        lines << (a("vars").isEmpty() ? pad + tag : pad + tag + QLatin1Char(' ') + a("vars"));
    } else if (tag == QLatin1String("return")) {
        lines << (e.hasAttribute(QStringLiteral("value")) ? pad + QLatin1String("return ") + a("value")
                                                           : pad + QLatin1String("return"));
    } else if (tag == QLatin1String("break") || tag == QLatin1String("continue")) {
        lines << pad + tag;
    } else if (tag == QLatin1String("if")) {
        lines << pad + QLatin1String("if (") + a("cond") + QLatin1Char(')');
        if (branches.size() > 0)
            serializeBranch(branches.at(0), indent + 1, lines);
        if (branches.size() > 1 && !childElements(branches.at(1)).isEmpty()) {
            lines << pad + QLatin1String("else");
            serializeBranch(branches.at(1), indent + 1, lines);
        }
    } else if (tag == QLatin1String("pre") || tag == QLatin1String("post")) {
        lines << pad + tag + QLatin1String(" (") + a("cond") + QLatin1Char(')');
        if (!branches.isEmpty())
            serializeBranch(branches.first(), indent + 1, lines);
    } else if (tag == QLatin1String("for")) {
        lines << pad + QLatin1String("for ") + a("var") + QLatin1String(" = ") + a("from") + QLatin1String(" .. ")
                + a("to");
        if (!branches.isEmpty())
            serializeBranch(branches.first(), indent + 1, lines);
    } else if (tag == QLatin1String("forc")) {
        lines << pad + QLatin1String("forc (") + a("init") + QLatin1String("; ") + a("cond") + QLatin1String("; ")
                + a("step") + QLatin1Char(')');
        if (!branches.isEmpty())
            serializeBranch(branches.first(), indent + 1, lines);
    } else if (tag == QLatin1String("foreach")) {
        lines << pad + QLatin1String("foreach (") + a("var") + QLatin1String(" : ") + a("range") + QLatin1Char(')');
        if (!branches.isEmpty())
            serializeBranch(branches.first(), indent + 1, lines);
    } else if (tag == QLatin1String("case")) {
        lines << pad + QLatin1String("case (") + a("expr") + QLatin1Char(')');
        for (int i = 0; i < branches.size(); ++i) {
            if (i == branches.size() - 1)
                lines << pad + QLatin1String("  default");
            else
                lines << pad + QLatin1String("  = ") + branches.at(i).attribute(QStringLiteral("value"));
            serializeBranch(branches.at(i), indent + 2, lines);
        }
    } else {
        lines << pad + QLatin1String("?") + tag;
    }
}

void serializeBranch(const QDomElement &branch, int indent, QStringList &lines)
{
    for (const QDomElement &child : childElements(branch))
        serializeBlock(child, indent, lines);
}

struct BlockSpec
{
    QStringList allowed;   // allowed attributes
    QStringList nonEmpty;  // required, non-empty attributes
    QStringList present;   // required attributes (may be empty)
    int branches;          // exact number of branch children; -2: case (>= 2)
};

const QHash<QString, BlockSpec> &blockSpecs()
{
    static const QHash<QString, BlockSpec> specs = {
        {QStringLiteral("process"), {{"text"}, {"text"}, {}, 0}},
        {QStringLiteral("call"), {{"text"}, {"text"}, {}, 0}},
        {QStringLiteral("assign"), {{"dest", "src"}, {"dest", "src"}, {}, 0}},
        {QStringLiteral("io"), {{"vars"}, {"vars"}, {}, 0}},
        {QStringLiteral("ou"), {{"vars"}, {}, {"vars"}, 0}},
        {QStringLiteral("if"), {{"cond"}, {"cond"}, {}, 2}},
        {QStringLiteral("pre"), {{"cond"}, {"cond"}, {}, 1}},
        {QStringLiteral("post"), {{"cond"}, {"cond"}, {}, 1}},
        {QStringLiteral("for"), {{"var", "from", "to"}, {"var", "from", "to"}, {}, 1}},
        {QStringLiteral("forc"), {{"init", "cond", "step"}, {}, {"init", "cond", "step"}, 1}},
        {QStringLiteral("foreach"), {{"var", "range"}, {"var", "range"}, {}, 1}},
        {QStringLiteral("case"), {{"expr"}, {"expr"}, {}, -2}},
        {QStringLiteral("return"), {{"value"}, {}, {}, 0}},
        {QStringLiteral("break"), {{}, {}, {}, 0}},
        {QStringLiteral("continue"), {{}, {}, {}, 0}},
    };
    return specs;
}

void validateNonElements(const QDomElement &e, const QString &path, QStringList &problems)
{
    for (QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.isElement())
            continue;
        if (n.isText() && n.nodeValue().trimmed().isEmpty())
            continue;
        problems << path + QLatin1String(": unexpected non-element child");
    }
}

void validateBranch(const QDomElement &branch, const QString &path, bool allowValue, QStringList &problems);

void validateBlock(const QDomElement &e, const QString &path, QStringList &problems)
{
    const QString tag = e.tagName();
    const QString here = path + QLatin1Char('/') + tag;
    const auto it = blockSpecs().constFind(tag);
    if (it == blockSpecs().constEnd()) {
        problems << here + QLatin1String(": unknown element");
        return;
    }
    const BlockSpec &spec = it.value();
    const QDomNamedNodeMap attrs = e.attributes();
    for (int i = 0; i < attrs.count(); ++i) {
        const QString name = attrs.item(i).nodeName();
        if (!spec.allowed.contains(name))
            problems << here + QLatin1String(": attribute not allowed: ") + name;
    }
    for (const QString &name : spec.nonEmpty) {
        if (e.attribute(name).trimmed().isEmpty())
            problems << here + QLatin1String(": missing/empty attribute ") + name;
    }
    for (const QString &name : spec.present) {
        if (!e.hasAttribute(name))
            problems << here + QLatin1String(": missing attribute ") + name;
    }
    if (tag == QLatin1String("io") && afce::splitList(e.attribute(QStringLiteral("vars"))).isEmpty())
        problems << here + QLatin1String(": empty input list");
    validateNonElements(e, here, problems);
    const QList<QDomElement> children = childElements(e);
    for (const QDomElement &c : children) {
        if (c.tagName() != QLatin1String("branch"))
            problems << here + QLatin1String(": child is not a branch: ") + c.tagName();
    }
    if (spec.branches >= 0 && children.size() != spec.branches)
        problems << here + QStringLiteral(": expected %1 branches, got %2").arg(spec.branches).arg(children.size());
    if (spec.branches == -2) {
        if (children.size() < 2)
            problems << here + QStringLiteral(": case needs >= 2 branches, got %1").arg(children.size());
        for (int i = 0; i + 1 < children.size(); ++i) {
            if (afce::splitList(children.at(i).attribute(QStringLiteral("value"))).isEmpty())
                problems << here + QStringLiteral(": case branch %1 has no value").arg(i + 1);
        }
    }
    for (int i = 0; i < children.size(); ++i) {
        if (children.at(i).tagName() == QLatin1String("branch"))
            validateBranch(children.at(i), here + QStringLiteral("[%1]").arg(i), spec.branches == -2, problems);
    }
}

void validateBranch(const QDomElement &branch, const QString &path, bool allowValue, QStringList &problems)
{
    const QDomNamedNodeMap attrs = branch.attributes();
    for (int i = 0; i < attrs.count(); ++i) {
        const QString name = attrs.item(i).nodeName();
        if (!(allowValue && name == QLatin1String("value")))
            problems << path + QLatin1String("/branch: attribute not allowed: ") + name;
    }
    validateNonElements(branch, path + QLatin1String("/branch"), problems);
    for (const QDomElement &c : childElements(branch))
        validateBlock(c, path + QLatin1String("/branch"), problems);
}

} // namespace

QString serialize(const QDomDocument &doc)
{
    QStringList lines;
    const QDomElement root = doc.documentElement();
    const QDomElement body = root.firstChildElement(QStringLiteral("branch"));
    serializeBranch(body, 0, lines);
    return lines.join(QLatin1Char('\n'));
}

QStringList validate(const QDomDocument &doc)
{
    QStringList problems;
    const QDomElement root = doc.documentElement();
    if (root.isNull() || root.tagName() != QLatin1String("algorithm")) {
        problems << QStringLiteral("root element is not <algorithm>");
        return problems;
    }
    if (root.attribute(QStringLiteral("version")) != QLatin1String("1.3"))
        problems << QStringLiteral("algorithm: version is not 1.3");
    const QDomNamedNodeMap attrs = root.attributes();
    for (int i = 0; i < attrs.count(); ++i) {
        const QString name = attrs.item(i).nodeName();
        if (name != QLatin1String("version") && name != QLatin1String("name") && name != QLatin1String("params")
            && name != QLatin1String("returns")) {
            problems << QLatin1String("algorithm: attribute not allowed: ") + name;
        }
    }
    validateNonElements(root, QStringLiteral("algorithm"), problems);
    const QList<QDomElement> children = childElements(root);
    if (children.size() != 1 || children.first().tagName() != QLatin1String("branch")) {
        problems << QStringLiteral("algorithm: expected exactly one <branch> child");
        return problems;
    }
    validateBranch(children.first(), QStringLiteral("algorithm"), false, problems);

    // The document must survive a serialization round trip.
    QDomDocument reparsed;
    if (!reparsed.setContent(doc.toString()))
        problems << QStringLiteral("document does not reparse");
    return problems;
}

afce::ImportOptions options(const QString &spec)
{
    afce::ImportOptions o;
    const QStringList words = spec.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString w : words) {
        w = w.trimmed();
        if (w == QLatin1String("c"))
            o.language = afce::ImportOptions::Language::C;
        else if (w == QLatin1String("cpp"))
            o.language = afce::ImportOptions::Language::Cpp;
        else if (w == QLatin1String("for=cstyle"))
            o.forStyle = afce::ImportOptions::ForStyle::CStyle;
        else if (w == QLatin1String("for=while"))
            o.forStyle = afce::ImportOptions::ForStyle::While;
        else if (w == QLatin1String("for=arithmetic"))
            o.forStyle = afce::ImportOptions::ForStyle::Arithmetic;
        else if (w == QLatin1String("keepdecl"))
            o.keepDeclarations = true;
        else if (w == QLatin1String("noio"))
            o.detectIO = false;
        else if (w == QLatin1String("nocalls"))
            o.callsAsSubroutine = false;
        else if (w == QLatin1String("noexpand"))
            o.expandCompoundAssign = false;
        else if (w == QLatin1String("exact"))
            o.exactOutput = true;
        else if (w == QLatin1String("keepreturn"))
            o.omitMainReturn = false;
        else
            qFatal("unknown option word: %s", qPrintable(w));
    }
    return o;
}

QString Result::diagnosticsText() const
{
    QStringList lines;
    for (const afce::Diagnostic &d : diagnostics) {
        const char *sev = d.severity == afce::Diagnostic::Error ? "error"
            : d.severity == afce::Diagnostic::Warning           ? "warning"
                                                                : "info";
        lines << QStringLiteral("%1:%2: %3: %4").arg(d.line).arg(d.column).arg(QLatin1String(sev), d.message);
    }
    return lines.join(QLatin1Char('\n'));
}

int Result::count(afce::Diagnostic::Severity severity) const
{
    int n = 0;
    for (const afce::Diagnostic &d : diagnostics) {
        if (d.severity == severity)
            ++n;
    }
    return n;
}

Result import(const QString &source, const afce::ImportOptions &options, const QString &function,
              const QString &fileName)
{
    Result r;
    afce::CodeImporter importer(options);
    r.parsed = importer.parse(source, fileName);
    int index = importer.defaultFunctionIndex();
    if (!function.isNull()) {
        index = -1;
        const QList<afce::FunctionInfo> fns = importer.functions();
        for (int i = 0; i < fns.size(); ++i) {
            if (fns.at(i).name == function) {
                index = i;
                break;
            }
        }
    }
    r.doc = importer.flowchart(index);
    r.diagnostics = importer.diagnostics();
    r.chart = serialize(r.doc);
    r.problems = validate(r.doc);
    for (const QString &p : std::as_const(r.problems))
        recordedProblems() << p + QLatin1String("\n  source: ") + source.left(300);
    return r;
}

QStringList takeRecordedProblems()
{
    const QStringList result = recordedProblems();
    recordedProblems().clear();
    return result;
}

Result body(const QString &statements, const QString &optionSpec)
{
    afce::ImportOptions o = options(optionSpec);
    if (o.language == afce::ImportOptions::Language::Auto)
        o.language = afce::ImportOptions::Language::Cpp;
    return import(QStringLiteral("void f()\n{\n") + statements + QStringLiteral("\n}\n"), o);
}

QString readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

} // namespace importtest
