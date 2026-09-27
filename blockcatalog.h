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

#ifndef BLOCKCATALOG_H
#define BLOCKCATALOG_H

#include <QIcon>
#include <QList>
#include <QPalette>
#include <QString>

namespace afce {

// A block type that can be inserted from the tool panel.
struct BlockKind
{
    QString type;         // element name: process, assign, io, ou, if, case, pre, post, for, forc, foreach, call, return, break, continue
    QString title;        // translated short name
    QString description;  // translated tooltip / status tip
    QString group;        // "basic" | "branching" | "loops" | "jumps"
    QString xmlTemplate;  // insertion buffer: <algorithm><branch>...default block...</branch></algorithm>
};

// Block kinds in tool panel order (strings are translated at call time).
QList<BlockKind> blockKinds();
// Translated caption of a group ("basic", "branching", "loops", "jumps").
QString blockGroupTitle(const QString &group);
// Icon painted with the same shapes/colours as the chart (crisp at any DPI).
QIcon blockIcon(const QString &type, const QPalette &palette);

} // namespace afce

#endif // BLOCKCATALOG_H
