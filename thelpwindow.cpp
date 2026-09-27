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

#include "thelpwindow.h"

#include "appicons.h"

#include <QAction>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QIcon>
#include <QLocale>
#include <QTextBrowser>
#include <QToolBar>
#include <QVBoxLayout>


THelpWindow::THelpWindow(QWidget *parent)
  : QDockWidget(parent)
{
  fWidget = new QFrame(this);
  setWidget(fWidget);
  toolBar = new QToolBar;
  toolBar->setObjectName("help_toolbar");
  textBrowser = new QTextBrowser;
  textBrowser->setOpenExternalLinks(true);
  QVBoxLayout *vl = new QVBoxLayout;
  vl->addWidget(toolBar);
  vl->addWidget(textBrowser);
  widget()->setLayout(vl);

  toolBar->setIconSize(QSize(16, 16));
  vl->setContentsMargins(4, 2, 4, 4);
  vl->setSpacing(2);
  textBrowser->setFrameShape(QFrame::NoFrame);
  actBack = toolBar->addAction(afce::uiIcon(QStringLiteral("back")), QString());
  actForward = toolBar->addAction(afce::uiIcon(QStringLiteral("forward")), QString());
  toolBar->addSeparator();
  actHome = toolBar->addAction(afce::uiIcon(QStringLiteral("home")), QString());
  actBack->setEnabled(false);
  actForward->setEnabled(false);
  connect(actBack, &QAction::triggered, textBrowser, &QTextBrowser::backward);
  connect(actForward, &QAction::triggered, textBrowser, &QTextBrowser::forward);
  connect(actHome, &QAction::triggered, this, &THelpWindow::home);
  connect(textBrowser, &QTextBrowser::backwardAvailable, actBack, &QAction::setEnabled);
  connect(textBrowser, &QTextBrowser::forwardAvailable, actForward, &QAction::setEnabled);

  retranslateUi();
}

QStringList THelpWindow::helpSearchPaths(const QString &localeName)
{
  QStringList result;
  const QStringList dirs = QDir::searchPaths(QStringLiteral("help"));
  for (const QString &dir : dirs)
  {
    const QDir base(dir);
    const QString localized = base.filePath(localeName);
    if (!localeName.isEmpty() && QFileInfo::exists(localized) && !result.contains(localized))
      result << localized;
    const QString english = base.filePath(QStringLiteral("en_US"));
    if (QFileInfo::exists(english) && !result.contains(english))
      result << english;
  }
  return result;
}

void THelpWindow::retranslateUi()
{
  actBack->setText(tr("Back"));
  actForward->setText(tr("Forward"));
  actHome->setText(tr("Home"));
  textBrowser->setSearchPaths(helpSearchPaths(QLocale().name()));
  // show the current page in the new language
  if (textBrowser->source().isEmpty())
    home();
  else
    textBrowser->reload();
}

void THelpWindow::changeEvent(QEvent *event)
{
  if (event->type() == QEvent::LanguageChange)
    retranslateUi();
  QDockWidget::changeEvent(event);
}

void THelpWindow::hideEvent(QHideEvent *)
{
    emit windowVisibilityChanged();
}

void THelpWindow::home()
{
  const QUrl index(QStringLiteral("index.html"));
  if (textBrowser->source() == index)
    textBrowser->reload();
  else
    textBrowser->setSource(index);
}
