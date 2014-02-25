/****************************************************************************
**                                                                         **
** Copyright (C) 2009 Victor Zinkevich. All rights reserved.               **
** Contact: vicking@yandex.ru                                              **
**                                                                         **
** This file is part of the Algorithm Flowchart Editor project.            **
**                                                                         **
** This file may be used under the terms of the GNU                        **
** General Public License versions 2.0 or 3.0 as published by the Free     **
** Software Foundation and appearing in the file LICENSE.TXT included in   **
** the packaging of this file.                                             **
** You can find license at http://www.gnu.org/licenses/gpl.html            **
**                                                                         **
****************************************************************************/

#ifndef ZVCODEGEN_H
#define ZVCODEGEN_H
#include <QtCore>
#include <QDomDocument>

QString xmlToPascal(const QDomDocument & xml);
QString xmlToPascal(const QString & xml);

QString xmlToC(const QDomDocument & xml);
QString xmlToC(const QString & xml);

QString xmlToE87(const QDomDocument & xml);
QString xmlToE87(const QString & xml);


#endif // ZVCODEGEN_H
