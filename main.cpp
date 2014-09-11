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
#if defined(Q_WS_X11) or defined(Q_OS_LINUX)
    qtTranslator.load(QLocale::system(), "qt", "_", QString(PROGRAM_DATA_DIR) + "locale", ".qm");
#else
    qtTranslator.load(QLocale::system(), "qt", "_", app.applicationDirPath() + "/locale",  ".qm");
#endif
    app.installTranslator(&qtTranslator);

    QTranslator myappTranslator;
#if defined(Q_WS_X11) or defined(Q_OS_LINUX)
    myappTranslator.load("afce_" + QLocale::system().name() + ".qm", QString(PROGRAM_DATA_DIR) + "locale");
#else
    myappTranslator.load(QLocale::system(), "afce", "_", app.applicationDirPath() + "/locale", ".qm");
#endif
    app.installTranslator(&myappTranslator);

    qDebug() << "Detected system locale: " << QLocale::system().name();
#if defined(Q_WS_X11) or defined(Q_OS_LINUX)
    qDebug() << "Application dir path:" << QString(PROGRAM_DATA_DIR);
#else
    qDebug() << "Application dir path:" << app.applicationDirPath();
#endif
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