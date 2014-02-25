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

#include <QtGui>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(),
             QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtTranslator);

    QTranslator myappTranslator;
#ifdef Q_WS_X11
    myappTranslator.load("afce_" + QLocale::system().name(), "/usr/share/afce");
#else
    myappTranslator.load("afce_" + QLocale::system().name());
#endif
    app.installTranslator(&myappTranslator);


    qDebug() << "Version: " << afceVersion();
    qDebug() << "Supported image formats to write: " << QImageWriter::supportedImageFormats();
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf-8"));
    MainWindow w;
    w.show();
    return app.exec();
}
