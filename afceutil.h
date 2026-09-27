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

#ifndef AFCEUTIL_H
#define AFCEUTIL_H

#include <QString>
#include <QStringList>

namespace afce {

// Splits a list attribute (io/ou "vars", case branch "value"). Separators that
// appear inside "..." or '...' literals (backslash escapes honoured) or inside
// (), [] or {} are not treated as separators. Items are trimmed, empty items
// are dropped.
QStringList splitList(const QString &list, QChar separator = QLatin1Char(','));

// Joins items for storage in a list attribute (separator ",").
QString joinList(const QStringList &items);

// Collapses whitespace runs (including newlines) outside string/char literals
// into a single space and trims the result.
QString simplifyCode(const QString &code);

// Registers the QDir search-path prefixes "generators:" and "help:".
// Order: user data dir (the same for afce and afce-cli: ~/Library/Application
// Support/afce, ~/.local/share/afce, %APPDATA%/afce), application dir, macOS bundle Resources, Linux
// PROGRAM_DATA_DIR, then the embedded resources ":/generators" / ":/help".
// Requires a Q(Core)Application instance.
void setupSearchPaths();

// Absolute paths of all available generator rule files (*.json), unique by
// base name; a file found earlier in the "generators:" search path wins.
QStringList generatorFiles();

// Program version (from CMake project version).
QString programVersion();

} // namespace afce

#endif // AFCEUTIL_H
