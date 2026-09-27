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

// Tokenizer and token stream tests of the C / C++ importer.

#include "tst_codeimport_tokenizer.h"

#include "importtokenizer.h"

#include <QtTest>

using afce::cimport::Lang;
using afce::cimport::Token;
using afce::cimport::TokenStream;

namespace {

QVector<Token> lex(const QString &source, Lang lang = Lang::Cpp, QList<afce::Diagnostic> *diags = nullptr)
{
    return afce::cimport::tokenize(source, lang, diags);
}

QStringList texts(const QString &source, Lang lang = Lang::Cpp)
{
    QStringList result;
    for (const Token &t : lex(source, lang))
        result << t.text;
    return result;
}

QString kindName(Token::Kind k)
{
    switch (k) {
    case Token::Identifier: return QStringLiteral("id");
    case Token::Keyword: return QStringLiteral("kw");
    case Token::Number: return QStringLiteral("num");
    case Token::String: return QStringLiteral("str");
    case Token::Char: return QStringLiteral("chr");
    case Token::Punct: return QStringLiteral("punct");
    case Token::Preprocessor: return QStringLiteral("pp");
    case Token::Unknown: return QStringLiteral("unknown");
    case Token::End: return QStringLiteral("end");
    }
    return QString();
}

// "kind:text" per token
QStringList describe(const QString &source, Lang lang = Lang::Cpp)
{
    QStringList result;
    for (const Token &t : lex(source, lang))
        result << kindName(t.kind) + QLatin1Char(':') + t.text;
    return result;
}

} // namespace


void TestTokenizer::keywordsDependOnLanguage()
{
    const QString src = QStringLiteral("class new delete template namespace int bool this");
    QCOMPARE(describe(src, Lang::Cpp),
             QStringList({"kw:class", "kw:new", "kw:delete", "kw:template", "kw:namespace", "kw:int", "kw:bool",
                          "kw:this"}));
    QCOMPARE(describe(src, Lang::C),
             QStringList({"id:class", "id:new", "id:delete", "id:template", "id:namespace", "kw:int", "kw:bool",
                          "id:this"}));
    QCOMPARE(describe(QStringLiteral("_Bool restrict override final $x Привет_1"), Lang::C),
             QStringList({"kw:_Bool", "kw:restrict", "id:override", "id:final", "id:$x", "id:Привет_1"}));
}

void TestTokenizer::numbers_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<QStringList>("tokens");
    QTest::newRow("int") << "42" << QStringList({"num:42"});
    QTest::newRow("hex") << "0x1Fu" << QStringList({"num:0x1Fu"});
    QTest::newRow("binary") << "0b1010" << QStringList({"num:0b1010"});
    QTest::newRow("float exponent") << "1.5e-3f" << QStringList({"num:1.5e-3f"});
    QTest::newRow("leading dot") << ".5" << QStringList({"num:.5"});
    QTest::newRow("hex float") << "0x1.8p+3" << QStringList({"num:0x1.8p+3"});
    QTest::newRow("suffixes") << "100ULL 7l" << QStringList({"num:100ULL", "num:7l"});
    QTest::newRow("digit separators") << "1'000'000" << QStringList({"num:1'000'000"});
    QTest::newRow("udl") << "10_km" << QStringList({"num:10_km"});
    QTest::newRow("minus is separate") << "a-1" << QStringList({"id:a", "punct:-", "num:1"});
    QTest::newRow("plus after exponent only") << "1e+5+2" << QStringList({"num:1e+5", "punct:+", "num:2"});
    QTest::newRow("member access") << "x.y" << QStringList({"id:x", "punct:.", "id:y"});
}

void TestTokenizer::numbers()
{
    QFETCH(QString, source);
    QFETCH(QStringList, tokens);
    QCOMPARE(describe(source), tokens);
}

void TestTokenizer::strings_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<QStringList>("tokens");
    QTest::newRow("plain") << R"("abc")" << QStringList({R"(str:"abc")"});
    QTest::newRow("escapes") << R"("a\"b\\" x)" << QStringList({R"(str:"a\"b\\")", "id:x"});
    QTest::newRow("comment inside") << R"("/* not */ // no")" << QStringList({R"(str:"/* not */ // no")"});
    QTest::newRow("prefixes") << R"(u8"a" u"b" U"c" L"d")"
                              << QStringList({R"(str:u8"a")", R"(str:u"b")", R"(str:U"c")", R"(str:L"d")"});
    // (moc cannot parse raw string literals with custom delimiters, hence the escapes)
    QTest::newRow("raw") << "R\"(a \"quoted\" \\n)\"" << QStringList({"str:R\"(a \"quoted\" \\n)\""});
    QTest::newRow("raw delimiter") << "R\"xy(a )\" b)xy\" z" << QStringList({"str:R\"xy(a )\" b)xy\"", "id:z"});
    QTest::newRow("raw prefixed") << "u8R\"(x)\" LR\"(y)\"" << QStringList({"str:u8R\"(x)\"", "str:LR\"(y)\""});
    QTest::newRow("raw multiline") << "R\"(line1\nline2)\"" << QStringList({"str:R\"(line1\nline2)\""});
    QTest::newRow("udl suffix") << R"("abc"s "x"_y "%d"PRId64)"
                                << QStringList({R"(str:"abc"s)", R"(str:"x"_y)", R"(str:"%d")", "id:PRId64"});
    QTest::newRow("adjacent") << R"("a" "b")" << QStringList({R"(str:"a")", R"(str:"b")"});
    QTest::newRow("char") << R"('a' '\'' '\n' u8'x' L'y')"
                          << QStringList({"chr:'a'", R"(chr:'\'')", R"(chr:'\n')", "chr:u8'x'", "chr:L'y'"});
    QTest::newRow("prefix-like identifiers") << "u8 R L x" << QStringList({"id:u8", "id:R", "id:L", "id:x"});
}

void TestTokenizer::strings()
{
    QFETCH(QString, source);
    QFETCH(QStringList, tokens);
    QCOMPARE(describe(source), tokens);
}

void TestTokenizer::rawStringsAreCppOnly()
{
    QCOMPARE(describe(QStringLiteral("R\"(x)\""), Lang::C), QStringList({"id:R", "str:\"(x)\""}));
}

void TestTokenizer::comments()
{
    QCOMPARE(texts(QStringLiteral("a // comment \"x\"\nb /* multi\nline */ c/**/d")),
             QStringList({"a", "b", "c", "d"}));
    QCOMPARE(texts(QStringLiteral("x = 1 / 2; // done")), QStringList({"x", "=", "1", "/", "2", ";"}));
}

void TestTokenizer::preprocessor()
{
    const QString src = QStringLiteral("#include <stdio.h>\n"
                                       "  #define MAX(a, b) \\\n    ((a) > (b) ? (a) : (b))\n"
                                       "int x; # no directive here\n"
                                       "#if A // comment\n"
                                       "#error \"it's\" fine /* spans\n lines */ tail\n"
                                       "y");
    const QVector<Token> t = lex(src);
    QStringList d;
    for (const Token &tok : t)
        d << kindName(tok.kind) + QLatin1Char(':') + tok.text;
    QCOMPARE(d, QStringList({"pp:#include <stdio.h>", "pp:#define MAX(a, b)     ((a) > (b) ? (a) : (b))", "kw:int",
                             "id:x", "punct:;", "punct:#", "id:no", "id:directive", "id:here", "pp:#if A",
                             "pp:#error \"it's\" fine /* spans\n lines */ tail", "id:y"}));
    // The directive token covers its continuation lines in the source.
    QCOMPARE(src.mid(t.at(1).offset, t.at(1).length),
             QStringLiteral("#define MAX(a, b) \\\n    ((a) > (b) ? (a) : (b))"));
    QCOMPARE(t.at(1).line, 2);
    QCOMPARE(t.at(1).column, 3);
    QCOMPARE(t.last().line, 8);
}

void TestTokenizer::punctuators_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<QStringList>("tokens");
    QTest::newRow("spaceship") << "a<=>b" << QStringList({"a", "<=>", "b"});
    QTest::newRow("shift assign") << "a<<=b>>=c" << QStringList({"a", "<<=", "b", ">>=", "c"});
    QTest::newRow("arrow star") << "p->*m" << QStringList({"p", "->*", "m"});
    QTest::newRow("ellipsis") << "f(...)" << QStringList({"f", "(", "...", ")"});
    QTest::newRow("scope") << "std::vector<std::vector<int>>"
                           << QStringList({"std", "::", "vector", "<", "std", "::", "vector", "<", "int", ">>"});
    QTest::newRow("increments") << "i+++j" << QStringList({"i", "++", "+", "j"});
    QTest::newRow("comparisons") << "a<=b>=c==d!=e" << QStringList({"a", "<=", "b", ">=", "c", "==", "d", "!=", "e"});
    QTest::newRow("logical") << "a&&b||!c&d|e" << QStringList({"a", "&&", "b", "||", "!", "c", "&", "d", "|", "e"});
    QTest::newRow("compound") << "a+=1;b-=2;c*=3;d/=4;e%=5;f&=6;g|=7;h^=8"
                              << QStringList({"a", "+=", "1", ";", "b", "-=", "2", ";", "c", "*=", "3", ";", "d",
                                              "/=", "4", ";", "e", "%=", "5", ";", "f", "&=", "6", ";", "g", "|=",
                                              "7", ";", "h", "^=", "8"});
    QTest::newRow("member pointer") << "a.*b" << QStringList({"a", ".*", "b"});
    QTest::newRow("token paste") << "x ## y" << QStringList({"x", "##", "y"});
    QTest::newRow("ternary") << "a?b:c" << QStringList({"a", "?", "b", ":", "c"});
    QTest::newRow("template close") << "a<::b>" << QStringList({"a", "<", "::", "b", ">"});
}

void TestTokenizer::punctuators()
{
    QFETCH(QString, source);
    QFETCH(QStringList, tokens);
    QCOMPARE(texts(source), tokens);
}

void TestTokenizer::unknownCharacters()
{
    QCOMPARE(describe(QStringLiteral("a @ ` \\ b")),
             QStringList({"id:a", "unknown:@", "unknown:`", "unknown:\\", "id:b"}));
}

void TestTokenizer::lineAndColumn()
{
    const QString src = QStringLiteral("int main()\r\n{\n\tx = 1; /* c\n */ y\n}");
    const QVector<Token> t = lex(src);
    QStringList d;
    for (const Token &tok : t)
        d << QStringLiteral("%1@%2:%3+%4").arg(tok.text).arg(tok.line).arg(tok.column).arg(tok.offset);
    QCOMPARE(d, QStringList({"int@1:1+0", "main@1:5+4", "(@1:9+8", ")@1:10+9", "{@2:1+12", "x@3:2+15", "=@3:4+17",
                             "1@3:6+19", ";@3:7+20", "y@4:5+31", "}@5:1+33"}));
    for (const Token &tok : t)
        QCOMPARE(src.mid(tok.offset, tok.length), tok.text);
}

void TestTokenizer::lineSplices()
{
    const QString src = QStringLiteral("int lo\\\nng_name = 1;\nchar *s = \"a\\\nb\";\nz");
    const QVector<Token> t = lex(src);
    QCOMPARE(t.at(1).text, QStringLiteral("long_name"));
    QCOMPARE(t.at(1).offset, 4);
    QCOMPARE(t.at(1).length, 11); // includes the splice
    QCOMPARE(src.mid(t.at(1).offset, t.at(1).length), QStringLiteral("lo\\\nng_name"));
    QCOMPARE(t.at(1).line, 1);
    QCOMPARE(t.at(2).line, 2);
    QCOMPARE(t.at(2).column, 9);
    QCOMPARE(t.at(9).text, QStringLiteral("\"ab\""));
    QCOMPARE(t.last().text, QStringLiteral("z"));
    QCOMPARE(t.last().line, 5);
    QCOMPARE(t.last().column, 1);
    // CRLF splice
    const QVector<Token> u = lex(QStringLiteral("a\\\r\nb c"));
    QCOMPARE(u.size(), 2);
    QCOMPARE(u.at(0).text, QStringLiteral("ab"));
    QCOMPARE(u.at(1).offset, 6);
}

void TestTokenizer::unterminated()
{
    QList<afce::Diagnostic> diags;
    const QVector<Token> t = lex(QStringLiteral("x = \"abc\ny = 'q\nz /* never closed"), Lang::Cpp, &diags);
    QCOMPARE(t.size(), 7);
    QCOMPARE(t.at(2).text, QStringLiteral("\"abc"));
    QCOMPARE(t.at(3).text, QStringLiteral("y"));
    QCOMPARE(t.at(5).text, QStringLiteral("'q"));
    QCOMPARE(t.at(6).text, QStringLiteral("z"));
    QCOMPARE(diags.size(), 3);
    QCOMPARE(diags.at(0).line, 1);
    QCOMPARE(diags.at(1).line, 2);
    QCOMPARE(diags.at(2).line, 3);
    QCOMPARE(diags.at(2).column, 3);
    // unterminated raw string runs to the end of input
    const QVector<Token> r = lex(QStringLiteral("R\"x(abc\n)\" y"));
    QCOMPARE(r.size(), 1);
    QCOMPARE(r.at(0).kind, Token::String);
    // lone backslash at the very end
    QCOMPARE(lex(QStringLiteral("\"a\\")).size(), 1);
    QCOMPARE(lex(QStringLiteral("#define X \\")).size(), 1);
}

void TestTokenizer::byteOrderMark()
{
    QCOMPARE(texts(QString(QChar(0xFEFF)) + QStringLiteral("int x;")), QStringList({"int", "x", ";"}));
}

void TestTokenizer::bracketMatching()
{
    const TokenStream ts(QStringLiteral("f ( a [ 1 ] , { b } ) ; ( x ] ) } ("), Lang::Cpp);
    //                                   0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17
    QVERIFY(ts.isOpener(1));
    QVERIFY(ts.matched(1));
    QCOMPARE(ts.close(1), 10);
    QCOMPARE(ts.after(1), 11);
    QCOMPARE(ts.contentEnd(1), 10);
    QCOMPARE(ts.close(3), 5);
    QCOMPARE(ts.close(7), 9);
    QCOMPARE(ts.opener(10), 1);
    // "( x ] )": ']' has no opener, '(' closes normally
    QCOMPARE(ts.opener(14), -1);
    QVERIFY(ts.matched(12));
    QCOMPARE(ts.close(12), 15);
    // stray '}' and unclosed '(' at the end
    QCOMPARE(ts.opener(16), -1);
    QVERIFY(!ts.matched(17));
    QCOMPARE(ts.close(17), 17);
    QCOMPARE(ts.after(17), 18);
    QCOMPARE(ts.contentEnd(17), 18);
    // non-openers
    QCOMPARE(ts.after(0), 1);
    QCOMPARE(ts.close(0), 0);
    // out of range access yields the end sentinel
    QCOMPARE(ts.at(-1).kind, Token::End);
    QCOMPARE(ts.at(1000).kind, Token::End);
}

void TestTokenizer::unclosedGroupEndsBeforeImplicitCloser()
{
    const TokenStream ts(QStringLiteral("{ f ( a ; } x"), Lang::Cpp);
    //                                   0 1 2 3 4 5 6
    QVERIFY(ts.matched(0));
    QCOMPARE(ts.close(0), 5);
    QVERIFY(!ts.matched(2));
    QCOMPARE(ts.close(2), 4);
    QCOMPARE(ts.after(2), 5);
    QCOMPARE(ts.contentEnd(2), 5);
    // ')' never closes across '{'
    const TokenStream u(QStringLiteral("( { ) }"), Lang::Cpp);
    QCOMPARE(u.opener(2), -1);
    QVERIFY(u.matched(1));
    QVERIFY(!u.matched(0));
}

void TestTokenizer::deepNestingIsIterative()
{
    const int depth = 20000;
    const QString src = QString(depth, QLatin1Char('(')) + QString(depth, QLatin1Char(')'));
    const TokenStream ts(src, Lang::Cpp);
    QCOMPARE(ts.size(), 2 * depth);
    QCOMPARE(ts.close(0), 2 * depth - 1);
    QCOMPARE(ts.close(depth - 1), depth);
}

void TestTokenizer::text()
{
    const TokenStream ts(QStringLiteral("a  =  b /* c */ +\n\tf( x ,y );// end\n#define Q 1\nz\"  s  \"R\"(a\n b)\""),
                         Lang::Cpp);
    QCOMPARE(ts.text(0, ts.size()), QStringLiteral("a = b + f( x ,y ); z\"  s  \"R\"(a\n b)\""));
    QCOMPARE(ts.text(2, 4), QStringLiteral("b +"));
    QCOMPARE(ts.text(3, 3), QString());
    QCOMPARE(ts.text(-5, 1), QStringLiteral("a"));
    QCOMPARE(ts.text(QVector<int>({0, 2, 4})), QStringLiteral("a b f"));
    const TokenStream glued(QStringLiteral("a/**/b"), Lang::Cpp);
    QCOMPARE(glued.text(0, 2), QStringLiteral("a b"));
}

void TestTokenizer::templateArguments()
{
    const TokenStream ts(QStringLiteral("map<string, vector<int>> m; a < b; f<(x > 1)>(y); v<g(a, b)>"), Lang::Cpp);
    //                                   0  1 2     3 4     5 6  7  8 9 10 11 12 13 14 15 16...
    QCOMPARE(ts.skipTemplateArgs(1, ts.size()), 8);
    QCOMPARE(ts.skipTemplateArgs(11, ts.size()), -1); // a < b;
    QCOMPARE(ts.skipTemplateArgs(15, ts.size()), 22);
    QCOMPARE(ts.skipTemplateArgs(0, ts.size()), -1);  // not '<'
    QVERIFY(ts.skipTemplateArgs(27, ts.size()) > 0);
}

QTEST_GUILESS_MAIN(TestTokenizer)
