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

#ifndef APPICONS_H
#define APPICONS_H

#include <QIcon>
#include <QString>

namespace afce {

// A line icon of the user interface (:/images/icons/<name>.svg, 24x24, drawn
// with the placeholder colour #000000). The icon is tinted at paint time with
// the text colour of the application palette (so it follows dark mode) and
// rendered at the device pixel ratio of the screen (crisp on HiDPI).
QIcon uiIcon(const QString &name);

// The application icon (:/images/appicon.svg, and appicon-small.svg for the
// small sizes), rendered for every size the window manager may ask for.
QIcon applicationIcon();

} // namespace afce

#endif // APPICONS_H
