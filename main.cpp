/****************************************************************************
**                                                                         **
** Copyright (C) 2008-2014 Victor Zinkevich. All rights reserved.          **
** Contact: vicking@yandex.ru                                              **
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
#if defined(Q_WS_X11) or defined(Q_OS_LINUX)
    myappTranslator.load("afce_" + QLocale::system().name(), "/usr/share/afce");
#else
    myappTranslator.load("afce_" + QLocale::system().name(), app.applicationDirPath());
#endif
    app.installTranslator(&myappTranslator);

    qDebug() << "Detected system locale: " << QLocale::system().name();
    qDebug() << "Version: " << afceVersion();
    qDebug() << "Supported image formats to write: " << QImageWriter::supportedImageFormats();
#if QT_VERSION < 0x050000
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf-8"));
#endif
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("utf-8"));
    MainWindow w;
    w.show();
    return app.exec();
}
