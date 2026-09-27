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

#ifndef TST_CODEIMPORT_H
#define TST_CODEIMPORT_H

#include <QObject>

// Statement mapping rules and import options.
class TestCodeImport : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void mapping_data();
    void mapping();
    void diagnostics_data();
    void diagnostics();
    void mainReturn();
    void printfFormatParser();
    void deepNesting_data();
    void deepNesting();
    void optionsRoundTrip();
    void repeatedFlowchartCalls();
    void forWhileParameterShadowing();
};

#endif // TST_CODEIMPORT_H
