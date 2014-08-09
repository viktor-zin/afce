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

#ifndef QFlowChart_H
#define QFlowChart_H


#include <QtCore>
#include <QtGui>
#include <QWidget>
#include <QDomDocument>
#include "qflowchartstyle.h"

class QBlock;
class QFlowChart;
//class QBranch;

class QInsertionPoint
{
  private:
    QPointF fPoint;
    QBlock *fBranch;
    int fIndex;
  public:
    QInsertionPoint()
    {
      fPoint = QPoint();
      fBranch = 0;
      fIndex = -1;
    }
    QPointF point() const { return fPoint; }
    QBlock *branch() const { return fBranch; }
    int index() const { return fIndex; }
    void setPoint(const QPointF &aPoint) { fPoint = aPoint; }
    void setBranch(QBlock *aBranch) { fBranch = aBranch; }
    void setIndex(int aIndex) { fIndex = aIndex; }
    bool isNull() const { return fPoint.isNull() && fBranch == 0 && fIndex == -1; }
};

Q_DECLARE_TYPEINFO(QInsertionPoint, Q_MOVABLE_TYPE);


class QBlock : public QObject
{
  Q_OBJECT
  private:
    QFlowChart *fFlowChart;

  public:
    QBlock();
    explicit QBlock(const QString & aType) { setType(aType); }
    ~QBlock();
//    QBlock(const QBlock & aBlock);
    QHash<QString, QString> attributes;
    double x, y, width, height;
    QList<QBlock *> items;
    QBlock *parent;
    bool isBranch;
    QBlock *root();
    QString type() const { return attributes.value("type", QString()); }
    void setType(const QString & newType) { attributes["type"] = newType; }
    int index();
    void insert(int newIndex, QBlock *aBlock);
    void remove(QBlock *aBlock);
    void append(QBlock *aBlock);
    void deleteObject(int aIndex);
    QBlock * item(int aIndex) const { return items.at(aIndex); }
    void setItem(int aIndex, QBlock *aBlock);
    QFlowChart * flowChart() const { return fFlowChart; }
    void setFlowChart(QFlowChart * aFlowChart);
    void clear();
    void adjustSize(const double aZoom);
    void adjustPosition(const double ox, const double oy);
    void paint(QPainter *canvas, bool fontSizeInPoints = false) const;
    double zoom() const;
    QBlock * blockAt(int px, int py);
    QDomElement xmlNode(QDomDocument & doc) const;
    void setXmlNode(const QDomElement & node);
    void insertXmlTree(int aIndex, const QDomElement & algorithm);
    bool isActive() const;
    double topMargin;
    double bottomMargin;
    double leftMargin;
    double rightMargin;
    static void drawCaption(QPainter *canvas, const QRectF & rect, const double zoomFactor, const QString & text);
};


class QFlowChart : public QWidget
{
  Q_OBJECT

  protected:
    virtual void paintEvent(QPaintEvent *pEvent);
    virtual void mousePressEvent(QMouseEvent *pEvent);
    virtual void mouseMoveEvent(QMouseEvent *pEvent);
    virtual void mouseDoubleClickEvent(QMouseEvent * event);

  private:
    QBlock *fRoot;
    QBlock *fActiveBlock;
    double fZoom;
    int fStatus;
    virtual QSize sizeHint() const;
    QList<QInsertionPoint> insertionPoints;
    QInsertionPoint fTargetPoint;
    QString fBuffer;
    bool fMultiInsert;
    QFlowChartStyle fStyle;
    QStack<QString> undoStack;
    QStack<QString> redoStack;

  public:

    enum {Display, Selectable, Insertion};

    QFlowChart(QWidget *pObj = 0);
    ~QFlowChart();
    QDomDocument document() const;
    QInsertionPoint targetPoint() const { return fTargetPoint; }

    void paintTo(QPainter *canvas);


    QBlock * root() const;
    QBlock * activeBlock() const;
    double zoom() const;
    int status() const { return fStatus; }
    void deleteBlock(QBlock *aBlock);
    QInsertionPoint getNearistPoint(int x, int y) const;
    void regeneratePoints();
    void generatePoints(QBlock *aBlock); // recursive
    static double calcLength(const QPointF & p1, const QPointF & p2);
    QString buffer() const { return fBuffer; }
    bool multiInsert() const { return fMultiInsert; }
    QFlowChartStyle chartStyle() const { return fStyle; }
    void setChartStyle(const QFlowChartStyle & aStyle);
    static void drawBottomArrow(QPainter *canvas, const QPointF & aPoint, const QSizeF & aSize);
    static void drawRightArrow(QPainter *canvas, const QPointF & aPoint, const QSizeF & aSize);
    QString toString();
    void fromString(const QString & str);
    bool canUndo() const;
    bool canRedo() const;
    bool canPaste() const;
    void makeChanged();
    void makeUndo();

  signals:
    void zoomChanged(const double aZoom);
    void statusChanged();
    void editBlock(QBlock *block);
    void changed();
    void modified();

  public slots:
    void clear();
    void selectAll();
    void deselectAll();
    void setZoom(const double aZoom);
    void setStatus(int aStatus);
    void deleteActiveBlock();
    void realignObjects();
    void setBuffer(const QString & aBuffer);
    void setMultiInsert(bool aValue) { fMultiInsert = aValue; }
    void undo();
    void redo();


};

#endif // QFlowChart_H
