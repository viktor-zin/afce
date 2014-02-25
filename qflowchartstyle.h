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

#ifndef QFLOWCHARTSTYLE_H
#define QFLOWCHARTSTYLE_H

#include <QtGui>

class QFlowChartStyle
{
  private:
    QColor fNormalBackground;
    QColor fNormalForeground;
    QColor fSelectedBackground;
    QColor fSelectedForeground;
    double fLineWidth;
    QColor fNormalMarker;
    QColor fSelectedMarker;
    double fFontSize;

  public:
    QFlowChartStyle();
    ~QFlowChartStyle() {}

    QColor normalBackground() const { return fNormalBackground; }
    QColor normalForeground() const { return fNormalForeground; }
    QColor selectedBackground() const { return fSelectedBackground; }
    QColor selectedForeground() const { return fSelectedForeground; }
    double lineWidth() const { return fLineWidth; }
    QColor normalMarker() const { return fNormalMarker; }
    QColor selectedMarker() const { return fSelectedMarker; }
    double fontSize() const { return fFontSize; }

    void setNormalBackground(const QColor & aValue) { fNormalBackground = aValue; }
    void setNormalForeground(const QColor & aValue) { fNormalForeground = aValue; }
    void setSelectedBackground(const QColor & aValue) { fSelectedBackground = aValue; }
    void setSelectedForeground(const QColor & aValue) { fSelectedForeground = aValue; }
    void setLineWidth(const double aValue) { fLineWidth = aValue; }
    void setNormalMarker(const QColor & aValue) { fNormalMarker = aValue; }
    void setSelectedMarker(const QColor & aValue) { fSelectedMarker = aValue; }
    void setFontSize(const double aValue) { fFontSize = aValue; }
};

Q_DECLARE_TYPEINFO(QFlowChartStyle, Q_MOVABLE_TYPE);

#endif // QFLOWCHARTSTYLE_H
