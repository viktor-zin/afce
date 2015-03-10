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
    QSettings settings("afce", "application");
    QString localeName = settings.value("locale", QLocale::system().name()).toString();
    qDebug() << "Available languages: " << MainWindow::enumLanguages();
    setApplicationLocale(localeName);
#if defined(Q_WS_X11) or defined(Q_OS_LINUX)
    qDebug() << "Application dir path:" << QString(PROGRAM_DATA_DIR);
#else
    qDebug() << "Application dir path:" << app.applicationDirPath();
#endif
    qDebug() << "Version: " << afceVersion();
    qDebug() << "Supported image formats to write: " << QImageWriter::supportedImageFormats();
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("utf-8"));
    MainWindow w;
    w.setLocale(QLocale(localeName));
    w.show();
    return app.exec();
}
