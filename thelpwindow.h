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

#include <QDockWidget>
#include <QStringList>

class QAction;
class QFrame;
class QTextBrowser;
class QToolBar;

class THelpWindow : public QDockWidget
{
    Q_OBJECT
signals:
    void windowVisibilityChanged();

public:
    QTextBrowser *textBrowser;

    explicit THelpWindow(QWidget *parent = nullptr);

    // Help directories for a locale: <dir>/<locale> then <dir>/en_US for
    // every directory of the "help:" search path (see afce::setupSearchPaths()).
    static QStringList helpSearchPaths(const QString &localeName);

private:
    QFrame *fWidget;
    QToolBar *toolBar;
    QAction *actBack;
    QAction *actForward;
    QAction *actHome;

    void hideEvent(QHideEvent *) override;
    void changeEvent(QEvent *event) override;

public slots:
    void home();
    // Re-reads the help pages for the current default locale.
    void retranslateUi();
};

#endif // THELPWINDOW_H
