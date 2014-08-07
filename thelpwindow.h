/****************************************************************************
**                                                                         **
** Copyright (C) 2009-2014 Victor Zinkevich. All rights reserved.          **
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

#ifndef THELPWINDOW_H
#define THELPWINDOW_H

//#include <QtGui>
#include <QFrame>
#include <QDockWidget>
#include <QToolBar>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QIcon>

class THelpWindow : public QDockWidget
{
    Q_OBJECT
signals:
    void windowVisibilityChanged();

public:
    THelpWindow();
private:
    QFrame *fWidget;
    QToolBar *toolBar;
    QTextBrowser *textBrowser;
    void hideEvent(QHideEvent *);
public slots:
    void home();
};

#endif // THELPWINDOW_H
