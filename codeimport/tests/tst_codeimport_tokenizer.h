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

// Declared in a header: moc cannot lex some raw string literals used by the tests.

#ifndef TST_CODEIMPORT_TOKENIZER_H
#define TST_CODEIMPORT_TOKENIZER_H

#include <QObject>

class TestTokenizer : public QObject
{
    Q_OBJECT

private slots:
    void keywordsDependOnLanguage();
    void numbers_data();
    void numbers();
    void strings_data();
    void strings();
    void rawStringsAreCppOnly();
    void comments();
    void preprocessor();
    void punctuators_data();
    void punctuators();
    void unknownCharacters();
    void lineAndColumn();
    void lineSplices();
    void unterminated();
    void byteOrderMark();
    void bracketMatching();
    void unclosedGroupEndsBeforeImplicitCloser();
    void deepNestingIsIterative();
    void text();
    void templateArguments();
};

#endif // TST_CODEIMPORT_TOKENIZER_H
