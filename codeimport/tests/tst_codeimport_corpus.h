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

#ifndef TST_CODEIMPORT_CORPUS_H
#define TST_CODEIMPORT_CORPUS_H

#include <QObject>

// Realistic student programs (tests/data): every function of every file
// imports into a valid document without errors, with every option set.
class TestCorpus : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void corpus_data();
    void corpus();
    void allOptionCombinations();
    void cleanupTestCase();

private:
    int m_functions = 0;
    int m_documents = 0;
    int m_blocks = 0;
};

#endif // TST_CODEIMPORT_CORPUS_H
