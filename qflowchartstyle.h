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

#ifndef QFLOWCHARTSTYLE_H
#define QFLOWCHARTSTYLE_H

#include <QColor>
#include <QMetaType>
#include <QString>

class QPalette;

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
    bool fMonochrome;

  public:
    // Fill colour categories of blocks.
    enum Category { Terminator, Action, InputOutput, Decision, Loop, Jump, CategoryCount };

  private:
    QString fAssignSymbol;
    QString fFontFamily;
    QColor fCanvasColor;
    QColor fFill[CategoryCount];

  public:
    QFlowChartStyle();

    // Light "document" style (black lines and text on white) used for export and printing,
    // independent of the UI palette.
    static QFlowChartStyle documentStyle(bool monochrome = false);
    // Style for the editor derived from the widget palette (follows dark mode).
    static QFlowChartStyle uiStyle(const QPalette &palette, bool monochrome = false);

    QColor normalBackground() const { return fNormalBackground; }
    QColor normalForeground() const { return fNormalForeground; }
    QColor selectedBackground() const { return fSelectedBackground; }
    QColor selectedForeground() const { return fSelectedForeground; }
    double lineWidth() const { return fLineWidth; }
    QColor normalMarker() const { return fNormalMarker; }
    QColor selectedMarker() const { return fSelectedMarker; }
    double fontSize() const { return fFontSize; }
    bool monochrome() const { return fMonochrome; }
    // Symbol shown between dest and src of assign blocks (":=", "=", "\u2190").
    QString assignSymbol() const { return fAssignSymbol; }
    // Font family of block texts (empty = application default font).
    QString fontFamily() const { return fFontFamily; }
    QColor canvasColor() const { return fCanvasColor; }
    QColor fillColor(Category aCategory) const { return fFill[aCategory]; }

    void setNormalBackground(const QColor & aValue) { fNormalBackground = aValue; }
    void setNormalForeground(const QColor & aValue) { fNormalForeground = aValue; }
    void setSelectedBackground(const QColor & aValue) { fSelectedBackground = aValue; }
    void setSelectedForeground(const QColor & aValue) { fSelectedForeground = aValue; }
    void setLineWidth(const double aValue) { fLineWidth = aValue; }
    void setNormalMarker(const QColor & aValue) { fNormalMarker = aValue; }
    void setSelectedMarker(const QColor & aValue) { fSelectedMarker = aValue; }
    void setFontSize(const double aValue) { fFontSize = aValue; }
    void setMonochrome(bool aValue) { fMonochrome = aValue; }
    void setAssignSymbol(const QString &aValue) { fAssignSymbol = aValue; }
    void setFontFamily(const QString &aValue) { fFontFamily = aValue; }
    void setCanvasColor(const QColor &aValue) { fCanvasColor = aValue; }
    void setFillColor(Category aCategory, const QColor &aValue) { fFill[aCategory] = aValue; }
};

Q_DECLARE_TYPEINFO(QFlowChartStyle, Q_RELOCATABLE_TYPE);

#endif // QFLOWCHARTSTYLE_H
