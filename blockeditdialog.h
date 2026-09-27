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

#ifndef BLOCKEDITDIALOG_H
#define BLOCKEDITDIALOG_H

#include <QCoreApplication>

class QBlock;
class QWidget;

class BlockEditDialog
{
    Q_DECLARE_TR_FUNCTIONS(BlockEditDialog)
public:
    // Shows the modal property dialog for any block type (including the algorithm
    // root: name / params / returns). On OK: flowChart()->makeUndo(), applies the
    // changes (including adding / removing case branches), realigns the chart,
    // emits changed() and returns true. Returns false if cancelled or not editable.
    static bool edit(QBlock *block, QWidget *parent);
    // True if edit() shows a dialog for the block (break and continue have no
    // properties; unknown elements cannot be edited).
    static bool canEdit(const QBlock *block);
};

#endif // BLOCKEDITDIALOG_H
