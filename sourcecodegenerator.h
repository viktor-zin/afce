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

#ifndef SOURCECODEGENERATOR_H
#define SOURCECODEGENERATOR_H

#include <QObject>
#include <QByteArray>
#include <QDomDocument>
#include <QJsonObject>
#include <QLocale>
#include <QString>

/*
 * Flowchart -> source code generator driven by JSON rule files
 * (generators/<id>.json; the generator id is the file's base name).
 *
 * ===========================================================================
 *  RULE FILE FORMAT
 * ===========================================================================
 *
 * {
 *   "name": { "en_US": "C", "ru_RU": "...", ... },    display name (locale -> text)
 *   "additional_settings": {
 *     "indentation_preference": "    ",  one indentation unit (default: two spaces)
 *     "empty_body": "pass",              lines emitted for an empty branch (string or array of lines)
 *     "variable_prefix": "$",            prefix for identifiers in expressions (Perl, PHP, AutoIt): not
 *                                        inside string literals, not for names followed by "(" or "::",
 *                                        not after "$", "@", "->", "::", not for numbers and "keywords"
 *     "keywords": ["and", ...],          words never prefixed, never declared as variables and never
 *                                        given a default type by "deftype"
 *     "case_insensitive": true,          keywords and collected variables compare ignoring case
 *     "variable_prefix_skip": [...],     attributes never transformed (default: ["name", "returns"])
 *     "operators": {"&&": "and", ...},   token replacements applied to expressions before the prefix
 *                                        (outside string literals; longest match first; keys starting
 *                                        with a letter match whole words only; word replacements are
 *                                        kept apart from neighbouring identifiers by a space)
 *     "logical_parens": true,            before "operators": wrap operands of && / || that contain a
 *                                        comparison in parentheses (Pascal precedence)
 *     "types": {"int": "Integer", "": "Integer", ...}   used by the "maptype" filter ("" = no type)
 *   },
 *   "comment": { "template": "// %text%" },   renders blocks the rule does not know (%text% = message)
 *   "<block type>": { ...element rule... }     one per XML element: algorithm, process, assign, io, ou,
 *                                              if, pre, post, for, forc, foreach, case, call, return,
 *                                              break, continue
 * }
 *
 * ---------------------------------------------------------------------------
 *  Element rule keys
 * ---------------------------------------------------------------------------
 * Every template may be a string or an array of strings (lines joined with "\n").
 * Template selection, first match wins:
 *  1. "template_through_case"  the nearest loop is reached through one or more "case" blocks that are
 *                              not part of the context (continue inside switch, PHP "continue 2");
 *                              %levels% = number of those case blocks + 1.
 *  2. "template_in_<type>_last" the block is the last one of a branch that belongs directly to the
 *                              context block (e.g. a no-op break at the end of a case branch).
 *  3. "template_in_<type>"     the nearest context block is of <type> (pre, post, for, forc, foreach,
 *                              case). The context of an element is its nearest ancestor whose type is
 *                              in "context" (default: loops + case; for "continue": loops only).
 *  4. "template_empty_<c>"     condition <c> is empty; conditions can be combined with "+"
 *                              ("template_empty_params+returns"). A condition is
 *                                <attr>           attribute missing, blank or an empty list (tested
 *                                                 BEFORE "defaults" are applied),
 *                                branch<N>        the N-th <branch> child has no blocks,
 *                                loop.<attr>      attribute of the context block,
 *                                algorithm.<attr> attribute of the <algorithm> root,
 *                                variables[_int|_real|_bool|_string|_char]  (algorithm) nothing collected.
 *                              The key with the most conditions wins; ties go to the alphabetically
 *                              first key.
 *     "template_shortened"     legacy alias of "template_empty_branch2" (an explicit key wins).
 *  5. "template_single_<attr>" list attribute <attr> has exactly one item.
 *  6. "template"
 *
 *  "context": ["pre", ...]     see 3.
 *  "defaults": {"attr": "v"}   value used for a missing/blank attribute.
 *  "declares": ["%dest%"]      templates naming the variables a block introduces (see %variables%).
 *                              Defaults: assign -> dest, for -> var, foreach -> name of var,
 *                              forc -> variables assigned in "init" (C declarations included),
 *                              io -> every item of vars, process -> left sides of simple assignments
 *                              ("a = 1; b = 2"). Only plain identifiers are collected; parameters,
 *                              the algorithm name and keywords are skipped.
 *  "list"                      list attributes, split with afce::splitList():
 *                                legacy: ["vars"] - uses the element-level keys below;
 *                                new:    {"<placeholder>": {<list config>}, ...}; the placeholder may
 *                                        differ from the attribute ("source"), so one attribute can be
 *                                        rendered in several ways (e.g. printf format and arguments).
 *    list config keys (missing keys fall back to the same keys at element level):
 *      "source"       attribute(s) to split, "a,b" concatenates several (default: the placeholder)
 *      "separator"    item separator inside the attribute (default ",")
 *      "glue"         text between rendered items (default "")
 *      "head"/"tail"  text around the joined result, only when at least one item was rendered
 *      "item"         item template for expressions; default prefix + "%$%" + suffix
 *      "prefix"/"suffix"
 *      "literal_item" / "literal_prefix" / "literal_suffix"   string literal items ("..." or '...')
 *      "char_item" / "char_prefix" / "char_suffix"            single-quoted items (fallback: literal_*)
 *      "text_escape"  {"from": "to", ...} applied (one pass, longest match first) to %$text%
 *    Items that render to an empty string are dropped.
 *
 *  case: "%branches%" expands to the rendered branches (the LAST branch is the default branch):
 *  "first_case_branch"         first non-default branch (if/elif emulation); default "case_branch"
 *  "case_branch"               non-default branches: %value% = the branch's "value" list rendered with
 *                              the "value" list config (item templates see the case attributes, e.g.
 *                              "%expr% == %$%"), %body% = the branch blocks
 *  "default_branch"            the default branch
 *  "default_branch_empty"      default branch without blocks ("" = omit it)
 *  "default_branch_only"       the default branch when there is no other branch
 *  Each of them + "_terminated" is preferred when the branch ends with a jump (no extra "break;").
 *
 *  if: "chain": {"head", "else_if", "else", "end"} renders else-if chains flat when an else branch
 *  consists of exactly one "if": "head" for the outer block, "else_if" for every nested if, "else"
 *  for the else branch of the last one (when not empty), "end" once - all at the same level.
 *
 * ---------------------------------------------------------------------------
 *  Placeholders
 * ---------------------------------------------------------------------------
 *  %attr%              attribute value (after defaults); expressions pass "operators" and the prefix
 *  %branchN%           blocks of the N-th <branch> child (1-based; whitespace/comments are ignored)
 *  %branches% %body%   case branches / blocks of the current case branch
 *  %value%             value list of the current case branch
 *  %loop.attr%         attribute of the context block (see 3.)
 *  %algorithm.attr%    attribute of the <algorithm> root
 *  %variables%         (algorithm) collected variables, comma separated - usable as a list "source";
 *  %variables_int% %variables_real% %variables_bool% %variables_string% %variables_char%
 *                      the same split by a heuristic type (literals, comparisons, "/", real literals,
 *                      sqrt(...), copies of typed variables; default int)
 *  %levels%            see 1.
 *  %text%              the message of the "comment" rule
 *  list items:  %$% item, %$name% declared name ("int a[]" -> a, "a: Integer" -> a, "x = 5" -> x),
 *               %$type% declared type, %$text% contents of a string literal (after text_escape),
 *               %$index% 1-based index; the element's own attributes stay available.
 *  %%                  a literal percent sign ("%" not followed by "name%" is kept as well)
 *
 *  Filters: %name|filter|filter:arg%, applied to the raw value, before operators and prefix:
 *    raw          no operators/prefix             nosemi   strip trailing ';'
 *    paren        (...) unless simple             operand  (...) unless simple or arithmetic
 *    negate       logical negation: "!x" -> "x", "a < b" -> "a >= b", else "!(...)" (C notation,
 *                 translated by "operators")
 *    inc / dec    "+ 1" / "- 1" with folding ("10" -> "11", "n - 1" -> "n")
 *    name / type  declarator name / type          lhs      left side of the first "="
 *    deftype:T    "T x" when the value is a bare non-keyword identifier
 *    maptype      look the type up in additional_settings.types
 *    colonassign  "x = e" -> "x := e" for each ';'-separated statement
 *    cstmt[:mode] C for-header statements -> plain assignments ("int i = 0" -> "i = 0", "i++" ->
 *                 "i = i + 1", "k *= a + b" -> "k = k * (a + b)"; declarations without initializer
 *                 dropped), one per line; mode "colon": "x := e"; "pascal": "x := e" joined by ";\n";
 *                 "decl": only declarations are rewritten, joined by ", " (C-like for headers)
 *    callparens   "name" -> "name()" for a bare identifier (call blocks)
 *    array        Perl: "a" -> "@a", "[1, 2]" -> "(1, 2)"
 *
 * ---------------------------------------------------------------------------
 *  Layout rules
 * ---------------------------------------------------------------------------
 *  "\t" in a template = one indentation unit relative to the element's own level. A block
 *  placeholder alone on a line puts the blocks at that line's indentation; otherwise the text before it
 *  becomes a line, the blocks follow one level deeper and the text after it starts a new line.
 *  Values are inserted verbatim (never re-expanded); continuation lines of multi-line values keep the
 *  indentation of the template line. A template line is dropped when all its values are empty and
 *  only blanks/';' remain, or when it contained values and produced only blank lines; intentionally
 *  empty template lines are kept. Trailing whitespace is removed; the result ends with one line break.
 */
class SourceCodeGenerator : public QObject
{
    Q_OBJECT
public:
    explicit SourceCodeGenerator(QObject *parent = nullptr);
    ~SourceCodeGenerator() override;

    // Generates code for the <algorithm> element of the document.
    QString applyRule(const QDomDocument &xml);

    // True when a rule object has been loaded successfully.
    bool isLoaded() const;
    // Translated description of the last load/parse error (empty when none).
    QString errorString() const;
    // The loaded rule object.
    QJsonObject rule() const;

    // Display name of the loaded rule ("name" object: locale, then en_US, then fallback).
    QString languageName(const QLocale &locale = QLocale(), const QString &fallback = QString()) const;
    static QString languageName(const QJsonObject &rule, const QLocale &locale, const QString &fallback);

public slots:
    // Accepts a path, a search-path name ("generators:c.json") or a bare generator id ("c").
    void loadRule(const QString &fileName);
    void ruleFromJSON(const QByteArray &json);

private:
    QJsonObject m_rule;
    QString m_error;
    bool m_loaded = false;
};

#endif // SOURCECODEGENERATOR_H
