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

#ifndef TST_CODEIMPORT_FUZZ_H
#define TST_CODEIMPORT_FUZZ_H

#include <QObject>

// Robustness: truncated and mutated sources, token soup, odd characters,
// deep nesting and a large synthetic file. The importer must never crash,
// hang or assert and must always produce a valid document.
class TestFuzz : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void truncations_data();
    void truncations();
    void tokenMutations_data();
    void tokenMutations();
    void tokenSoup();
    void oddCharacters();
    void deepNestingEverywhere_data();
    void deepNestingEverywhere();
    void largeSyntheticFile();
    void cleanupTestCase();

private:
    int m_cases = 0;
};

#endif // TST_CODEIMPORT_FUZZ_H
