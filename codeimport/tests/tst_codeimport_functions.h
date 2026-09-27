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

#ifndef TST_CODEIMPORT_FUNCTIONS_H
#define TST_CODEIMPORT_FUNCTIONS_H

#include <QObject>

class TestFunctions : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void discovery_data();
    void discovery();
    void lineNumbers();
    void defaultFunction();
    void languageFromFileName();
    void noFunctions();
    void fragment();
    void unclosedBody();
    void deepTopLevelNesting();
    void algorithmAttributes();
};

#endif // TST_CODEIMPORT_FUNCTIONS_H
