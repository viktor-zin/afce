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

#include "qflowchartstyle.h"

#include <QPalette>

namespace {

// Pastel fills for light backgrounds (screen and documents): readable with
// dark lines and text, and still light when printed.
void setLightFills(QFlowChartStyle &st)
{
    st.setFillColor(QFlowChartStyle::Terminator, QColor(0xD8, 0xF0, 0xDF));
    st.setFillColor(QFlowChartStyle::Action, QColor(0xDC, 0xEA, 0xFB));
    st.setFillColor(QFlowChartStyle::InputOutput, QColor(0xEB, 0xE3, 0xFA));
    st.setFillColor(QFlowChartStyle::Decision, QColor(0xFF, 0xEE, 0xCC));
    st.setFillColor(QFlowChartStyle::Loop, QColor(0xD4, 0xF0, 0xEC));
    st.setFillColor(QFlowChartStyle::Jump, QColor(0xFB, 0xDC, 0xE1));
}

// Muted fills for dark backgrounds (light lines and text).
void setDarkFills(QFlowChartStyle &st)
{
    st.setFillColor(QFlowChartStyle::Terminator, QColor(0x2B, 0x47, 0x37));
    st.setFillColor(QFlowChartStyle::Action, QColor(0x29, 0x3C, 0x57));
    st.setFillColor(QFlowChartStyle::InputOutput, QColor(0x3E, 0x33, 0x58));
    st.setFillColor(QFlowChartStyle::Decision, QColor(0x55, 0x44, 0x22));
    st.setFillColor(QFlowChartStyle::Loop, QColor(0x1E, 0x46, 0x48));
    st.setFillColor(QFlowChartStyle::Jump, QColor(0x57, 0x2D, 0x37));
}

void setPlainFills(QFlowChartStyle &st, const QColor &color)
{
    for (int c = 0; c < QFlowChartStyle::CategoryCount; ++c)
        st.setFillColor(QFlowChartStyle::Category(c), color);
}

} // namespace

QFlowChartStyle::QFlowChartStyle()
    : fNormalBackground(Qt::white),
      fNormalForeground(QColor(0x1F, 0x23, 0x28)),
      fSelectedBackground(QColor(0x2F, 0x6F, 0xDE)),
      fSelectedForeground(Qt::white),
      fLineWidth(1.5),
      fNormalMarker(QColor(0x2F, 0x6F, 0xDE)),
      fSelectedMarker(QColor(0x2F, 0x6F, 0xDE)),
      fFontSize(10),
      fMonochrome(false),
      fAssignSymbol(QStringLiteral(":=")),
      fCanvasColor(Qt::white)
{
    setLightFills(*this);
}

QFlowChartStyle QFlowChartStyle::documentStyle(bool monochrome)
{
    QFlowChartStyle st;
    st.setNormalBackground(Qt::white);
    st.setCanvasColor(Qt::white);
    st.setMonochrome(monochrome);
    if (monochrome) {
        // classic GOST 19.701-90: black lines and text, white symbols
        st.setNormalForeground(Qt::black);
        setPlainFills(st, Qt::white);
    } else {
        st.setNormalForeground(QColor(0x1F, 0x23, 0x28));
        setLightFills(st);
    }
    return st;
}

QFlowChartStyle QFlowChartStyle::uiStyle(const QPalette &palette, bool monochrome)
{
    QFlowChartStyle st;
    const QColor base = palette.color(QPalette::Base);
    const QColor text = palette.color(QPalette::Text);
    const bool dark = base.lightnessF() < 0.5;
    st.setNormalBackground(base);
    st.setCanvasColor(base);
    st.setNormalForeground(text);
    QColor accent = palette.color(QPalette::Highlight);
    if (!accent.isValid() || accent == base)
        accent = dark ? QColor(0x5C, 0x9C, 0xFF) : QColor(0x2F, 0x6F, 0xDE);
    st.setSelectedBackground(accent);
    st.setSelectedForeground(palette.color(QPalette::HighlightedText));
    st.setNormalMarker(accent);
    st.setSelectedMarker(accent);
    st.setMonochrome(monochrome);
    if (monochrome)
        setPlainFills(st, base);
    else if (dark)
        setDarkFills(st);
    else
        setLightFills(st);
    return st;
}
