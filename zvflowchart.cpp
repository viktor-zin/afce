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

#include "zvflowchart.h"
#include <QtGui>
#include <QDomDocument>
#include <QApplication>

QFlowChart::QFlowChart(QWidget *pObj /* = 0 */) : QWidget(pObj), fZoom(1)
{
  fBuffer = QString();
  fTargetPoint = QInsertionPoint();
  fStatus = Display;
  fRoot = new QBlock();
  root()->setFlowChart(this);
  clear();
  setZoom(1);
}

QFlowChart::~QFlowChart()
{
  clear();
}

void QFlowChart::makeUndo()
{
  QString state = toString();
  undoStack.push(state);
  redoStack.clear();
}
bool QFlowChart::canUndo() const
{
  return !undoStack.isEmpty();
}

bool QFlowChart::canRedo() const
{
  return !redoStack.isEmpty();
}

bool QFlowChart::canPaste() const
{
  QDomDocument doc;
  QClipboard *clp = QApplication::clipboard();
  QString aBuffer = clp->text();
  return doc.setContent(aBuffer, false);
}

void QFlowChart::makeChanged()
{
  emit changed();
}

void QFlowChart::undo()
{
  if (!undoStack.isEmpty())
  {
    QString state = toString();
    redoStack.push(state);
    state = undoStack.pop();
    fromString(state);
    deselectAll();
    emit changed();
  }
}


void QFlowChart::redo()
{
  if (!redoStack.isEmpty())
  {
    QString state = toString();
    undoStack.push(state);
    state = redoStack.pop();
    fromString(state);
    deselectAll();
    emit changed();
  }
}


void QFlowChart::clear()
{
    deselectAll();
    root()->clear();
    root()->attributes.clear();
    root()->setType("algorithm");
    QBlock *branch = new QBlock();
    branch->setType("branch");
    branch->isBranch = true;
    root()->append(branch);
    branch->setFlowChart(root()->flowChart());

//    fDocument->clear();
//    QDomProcessingInstruction xml = fDocument->createProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\" stand-alone=\"yes\"");
//    fDocument->appendChild(xml);

//    QDomElement fRoot = fDocument->createElement("algorithm");
//    fDocument->appendChild(fRoot);
//    QDomElement branch = fDocument->createElement("branch");
//    fRoot.appendChild(branch);
    emit changed();
}

void QFlowChart::selectAll()
{
  fActiveBlock = root();
  emit changed();
  update();
}

void QFlowChart::deselectAll()
{
    fActiveBlock = 0;
    emit changed();
    update();
}

void QFlowChart::paintEvent(QPaintEvent *pEvent)
{
    QPainter canvas(this);
    pEvent->accept();
    canvas.setClipRect(pEvent->rect());
    canvas.setRenderHint(QPainter::Antialiasing, true);
    paintTo(&canvas);
}

void QFlowChart::paintTo(QPainter *canvas)
{
    if (root())
    {
      root()->paint(canvas);
      if (status() == Insertion)
      {
        QFlowChartStyle st = chartStyle();
        for (int i = 0; i < insertionPoints.size(); ++i)
        {
          QInsertionPoint ip = insertionPoints.at(i);
          QPointF p = ip.point();
          canvas->setPen(QPen(st.normalForeground(), 2 * zoom()));
          canvas->setBrush(st.normalForeground());
          canvas->drawEllipse(p, 3 * zoom(), 3 * zoom());
        }
        if (!targetPoint().isNull())
        {
          QPointF p = targetPoint().point();
          canvas->setPen(QPen(st.selectedBackground(), 2 * zoom()));
          canvas->setBrush(st.selectedBackground());
          canvas->drawEllipse(p, 7 * zoom(), 7 * zoom());
        }
      }
    }
}

QDomDocument QFlowChart::document() const
{
  QDomDocument doc("AFC");
  QDomElement r = root()->xmlNode(doc);
  doc.appendChild(r);
  return doc;
}

QBlock * QFlowChart::root() const
{
  return fRoot;
}

QBlock * QFlowChart::activeBlock() const
{
  return fActiveBlock;
}

void QFlowChart::setZoom(const double aZoom)
{
  fZoom = aZoom;
  realignObjects();
  if (status() == Insertion)
  {
    fTargetPoint = QInsertionPoint();
    regeneratePoints();
  }
  emit zoomChanged(aZoom);
}

void QFlowChart::setStatus(int aStatus)
{
  fStatus = aStatus;
  if (status() == Insertion)
  {
    setMouseTracking(true);
    regeneratePoints();
    update();
  }
  else
  {
    fTargetPoint = QInsertionPoint();
    setMouseTracking(false);
  }
  emit statusChanged();
  emit changed();
}

void QFlowChart::deleteActiveBlock()
{
  if (activeBlock())
  {
    makeUndo();
    QBlock *tmp = activeBlock();
    fActiveBlock = 0;
    deleteBlock(tmp);
    emit changed();
  }
}
void QFlowChart::realignObjects()
{
  if(root())
  {
    root()->adjustSize(zoom());
    root()->adjustPosition(0,0);
    resize(root()->width, root()->height);
    emit changed();
    update();
  }
}

void QFlowChart::setBuffer(const QString & aBuffer)
{
  QDomDocument doc;
  if(doc.setContent(aBuffer, false))
  {
    fBuffer = aBuffer;
  }
  else
  {
    fBuffer = QString();
  }
}

double QFlowChart::zoom() const
{
  return fZoom;
}

void QFlowChart::mousePressEvent(QMouseEvent *pEvent)
{
  if (status() == Selectable)
  {
    QBlock *block = root()->blockAt(pEvent->x(), pEvent->y());
    if (block)
    {
      if (block->isActive() && ((pEvent->modifiers() & Qt::ControlModifier) != 0))
      {
        if(activeBlock()->parent)
        {
          fActiveBlock = activeBlock()->parent;
        }
        else
        {
          fActiveBlock = block;
        }
      }
      else
      {
        if(block->parent !=0 && block->isBranch)
        {
          fActiveBlock = block->parent;
        }
        else
          fActiveBlock = block;

      }
      emit changed();
      update();
    }
  }
  else if(status() == Insertion)
  {
    QPoint mp = pEvent->pos();
    QInsertionPoint ip = getNearistPoint(mp.x(), mp.y());
    fTargetPoint = ip;
    if(!ip.isNull() && !buffer().isEmpty())
    {
      QDomDocument doc;
      if(doc.setContent(buffer(), false))
      {
        QDomElement algorithm = doc.firstChildElement("algorithm");
        QBlock *branch = ip.branch();
        if(branch)
        {
          makeUndo();
          branch->insertXmlTree(ip.index(), algorithm);
          realignObjects();
          regeneratePoints();
          fActiveBlock = 0;
          emit changed();
        }
      }
    }
    if (!multiInsert()) setStatus(Selectable);
  }
}

void QFlowChart::mouseDoubleClickEvent(QMouseEvent * event)
{
  if(status() == Selectable)
  {
    QBlock *block = root()->blockAt(event->x(), event->y());
    if (block)
    {
      emit changed();
      emit editBlock(block);
    }
  }
}

void QFlowChart::mouseMoveEvent(QMouseEvent *pEvent)
{
  if(status() == Insertion)
  {
    QPoint mp = pEvent->pos();
    QInsertionPoint ip = getNearistPoint(mp.x(), mp.y());
    fTargetPoint = ip;
    repaint();
  }
}

QSize QFlowChart::sizeHint() const
{
  if (root())
  {
    return QSize(root()->width, root()->height);
  }
  else return QSize();

}

void QFlowChart::deleteBlock(QBlock *aBlock)
{
  if (aBlock == root())
  {
    for(int i = 0; i < aBlock->items.size(); ++i)
    {
      aBlock->item(i)->clear();
    }
    realignObjects();
  }
  else if (aBlock->isBranch)
  {
    aBlock->clear();
    realignObjects();
  }
  else
  {
    delete aBlock;
    realignObjects();
  }
  emit changed();
}

QInsertionPoint QFlowChart::getNearistPoint(int x, int y) const
{
  QInsertionPoint result;
  if (insertionPoints.size() > 0)
  {
    result = insertionPoints.at(0);
    double len = calcLength(result.point(), QPointF(x, y));
    for (int i = 0; i < insertionPoints.size(); ++i)
    {
      QInsertionPoint ip = insertionPoints.at(i);
      double tmp = calcLength(ip.point(), QPointF(x, y));
      if (tmp < len)
      {
        result = ip;
        len = tmp;
      }
    }
  }
  return result;
}

void QFlowChart::regeneratePoints()
{
  insertionPoints.clear();
  if(root())
  {
    generatePoints(root());
  }
}

void QFlowChart::generatePoints(QBlock *aBlock)
{
  if(aBlock->isBranch)
  {
    double x = aBlock->x + aBlock->width / 2.0;
    for (int i = 0; i < aBlock->items.size(); ++i)
    {
      double y = aBlock->item(i)->y;
      QInsertionPoint p;
      p.setBranch(aBlock);
      p.setPoint(QPointF(x, y));
      p.setIndex(i);
      insertionPoints.append(p);
      generatePoints(aBlock->item(i));
    }
    QInsertionPoint p;
    p.setBranch(aBlock);
    if(aBlock->items.size() == 0)
      p.setPoint(QPointF(x, aBlock->y + aBlock->height / 2));
    else
      p.setPoint(QPointF(x, aBlock->y + aBlock->height));
    p.setIndex(aBlock->items.size());
    insertionPoints.append(p);
  }
  else
  {
    for (int i = 0; i < aBlock->items.size(); ++i)
    {
      generatePoints(aBlock->item(i));
    }
  }
}

double QFlowChart::calcLength(const QPointF & p1, const QPointF & p2)
{
  return (p1.x() - p2.x()) * (p1.x() - p2.x()) + (p1.y() - p2.y()) * (p1.y() - p2.y());
}
void QFlowChart::setChartStyle(const QFlowChartStyle & aStyle)
{
  fStyle = aStyle;
  emit changed();
  update();
}

void QFlowChart::drawBottomArrow(QPainter *canvas, const QPointF & aPoint, const QSizeF & aSize)
{
  QVector<QPointF> arrow;
  arrow << QPointF(aPoint.x() - aSize.width()/2.0, aPoint.y()-aSize.height()) <<
      aPoint << aPoint << QPointF(aPoint.x() + aSize.width()/2.0, aPoint.y()-aSize.height());
  canvas->drawLines(arrow);
}

QString QFlowChart::toString()
{
  return document().toString(2);
}

void QFlowChart::fromString(const QString & str)
{
  QDomDocument doc;
  if(doc.setContent(str, false))
  {
    root()->setXmlNode(doc.firstChildElement("algorithm"));
    realignObjects();
    emit changed();
  }
}

void QFlowChart::drawRightArrow(QPainter *canvas, const QPointF & aPoint, const QSizeF & aSize)
{
  QVector<QPointF> arrow;
  arrow << QPointF(aPoint.x() - aSize.width(), aPoint.y()-aSize.height()/2);
  arrow << aPoint;
  arrow << aPoint;
  arrow << QPointF(aPoint.x() - aSize.width(), aPoint.y()+aSize.height()/2);
  canvas->drawLines(arrow);
}


/******************************** QBlock ***********************************/


QBlock::QBlock()
{
  parent = 0;
  isBranch = false;
  attributes.clear();
  items.clear();
}

QBlock::~QBlock()
{
  if(parent != 0)
  {
    parent->remove(this);
    clear();
  }
}


//QBlock::QBlock(const QBlock & aBlock)
//{
//  x = aBlock.x;
//  y = aBlock.y;
//  width = aBlock.width;
//  height = aBlock.height;
//  branches = aBlock.branches;
//  attributes = aBlock.attributes;
//}

QBlock * QBlock::root()
{
  if (parent == 0)
  {
    return this;
  }
  else
  {
    return parent->root();
  }
}



int QBlock::index()
{

  if(parent == 0) return -1;
  else return parent->items.indexOf(this);
}

void QBlock::insert(int newIndex, QBlock *aBlock)
{
  if (aBlock->parent != 0)
  {
    aBlock->parent->remove(aBlock);
  }
  if (newIndex < 0 || newIndex >= items.size())
    items.append(aBlock);
  else
    items.insert(newIndex, aBlock);
  aBlock->parent = this;
  aBlock->setFlowChart(flowChart());
}

void QBlock::remove(QBlock *aBlock)
{
  items.removeAll(aBlock);
  aBlock->parent = 0;
  aBlock->setFlowChart(0);
}

void QBlock::append(QBlock *aBlock)
{
  insert(-1, aBlock);
}

void QBlock::deleteObject(int aIndex)
{
  QBlock *tmp = item(aIndex);
  if(tmp)
  {
    delete tmp;
  }
}


void QBlock::setItem(int aIndex, QBlock *aBlock)
{
  if(items.size() > aIndex && aIndex >= 0)
  {
    if (aBlock->parent != 0)
    {
      aBlock->parent->remove(aBlock);
    }
    QBlock *old = item(aIndex);
    old->parent = 0;
    items.replace(aIndex, aBlock);
  }
}

void QBlock::setFlowChart(QFlowChart * aFlowChart)
{
  fFlowChart = aFlowChart;
}

void QBlock::clear()
{
  while(!items.isEmpty())
  {
    deleteObject(0);
  }
  QString currentType = type();
  attributes.clear();
  setType(currentType);
  items.clear();
}

void QBlock::adjustSize(const double aZoom)
{
  double clientWidth = 0, clientHeight = 0;
  if (isBranch)
  {
    for (int i = 0; i < items.size(); ++i)
    {
      item(i)->adjustSize(aZoom);
      if (clientWidth < item(i)->width) clientWidth = item(i)->width;
      clientHeight += item(i)->height;
    }
    double minWidth = 180 * aZoom;
    double minHeight = 16 * aZoom;
    if (clientHeight < minHeight) clientHeight = minHeight;
    if (clientWidth < minWidth) clientWidth = minWidth;
    height = clientHeight;
    width = clientWidth;

  }
  else
  {
    for (int i = 0; i < items.size(); ++i)
    {

      item(i)->adjustSize(aZoom);
      if (clientHeight < item(i)->height) clientHeight = item(i)->height;
      clientWidth += item(i)->width;
    }
    /* поля по умолчанию */
    topMargin = 60 * aZoom;
    bottomMargin = 60 * aZoom;
    leftMargin = 10 * aZoom;
    rightMargin = 10 * aZoom;
    if (type() == "algorithm")
    {
      topMargin = 40 * aZoom;
      bottomMargin = 50 * aZoom;
    }
    else if (type() == "process" || type() == "assign")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 10 * aZoom;
      clientWidth = 120 * aZoom;
      clientHeight = 60 * aZoom;
    }
    else if (type() == "io")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 10 * aZoom;
      leftMargin = 20 * aZoom;
      rightMargin = 20 * aZoom;
      clientWidth = 120 * aZoom;
      clientHeight = 60 * aZoom;
    }
    else if (type() == "if")
    {
      topMargin = 92 * aZoom;
      bottomMargin = 16 * aZoom;
    }
    else if (type() == "pre")
    {
      topMargin = 108 * aZoom;
      bottomMargin = 32 * aZoom;
    }
    else if (type() == "post")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 96 * aZoom;
    }
    else if (type() == "for")
    {
      topMargin = 108 * aZoom;
      bottomMargin = 32 * aZoom;
    }

    width = leftMargin + clientWidth + rightMargin;
    height = topMargin + clientHeight + bottomMargin;
  }
}

void QBlock::adjustPosition(const double ox, const double oy)
{
  x = ox;
  y = oy;
  if (isBranch)
  {
    double cy = y;
    for (int i = 0; i < items.size(); ++i)
    {
      item(i)->adjustPosition(ox + (width - item(i)->width) / 2, cy);
      cy += item(i)->height;
    }
  }
  else
  {
    double cx = x + leftMargin;
    for (int i = 0; i < items.size(); ++i)
    {
      item(i)->adjustPosition(cx, y + topMargin);
      cx += item(i)->width;
    }
  }
}

void QBlock::drawCaption(QPainter *canvas, const QRectF & rect, const double zoomFactor, const QString & text)
{
  QFont font("Sans Serif");
  font.setPixelSize(13 * zoomFactor);
  font = QFont(font, canvas->device());
  QFontMetricsF fontMetrics(font);
  QRectF textRect = fontMetrics.boundingRect(text);
  double tx = rect.x() + rect.width() / 2 - textRect.width() / 2;
  double ty = rect.y() + rect.height() / 2 + fontMetrics.ascent() / 2;
  canvas->setFont(font);
//  canvas->drawRect(tx, ty - textRect.height(), textRect.width(), textRect.height());
  canvas->drawText(QPointF(tx, ty), text);

}

void QBlock::paint(QPainter *canvas, bool fontSizeInPoints) const
{
  if (flowChart())
  {
    QFlowChartStyle st = flowChart()->chartStyle();
    double hcenter = x + width / 2;
    /* в соответствии с ГОСТ 19.003-80 */
    double a = 60 * zoom();
    double b = 2 * a;
    double bottom = y + height;
    double lw = st.lineWidth() * zoom();

//    QFont font = flowChart()->font();
//    font.setPixelSize(13 * zoom());
//    flowChart()->setFont(font);

    QFont font("Tahoma");
    font.setWeight(0);
    if (fontSizeInPoints)
      font.setPointSizeF(10 * zoom());
    else
      font.setPixelSize(13 * zoom());
    font = QFont(font, canvas->device());
    canvas->setFont(font);

    if(flowChart()->status() == QFlowChart::Selectable && isActive())
    {
      canvas->setPen(QPen(st.selectedForeground(), lw));
      canvas->setBrush(QBrush(st.selectedBackground()));
    }
    else
    {
      canvas->setPen(QPen(st.normalForeground(), lw));
      canvas->setBrush(QBrush(st.normalBackground()));
    }

    QPen pen = canvas->pen();
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    canvas->setPen(pen);
    canvas->fillRect(QRectF(x, y, width, height), canvas->brush());

    if (isBranch)
    {
      /* отрисовка ветви */
      QLineF line(hcenter, y-0.5, hcenter, y + height+0.5);
      canvas->drawLine(line);
    }
    else
    {
      if (type() == "algorithm")
      {
        /* алгоритм */
        QBlock *body = item(0);
        Q_ASSERT_X(body != 0, "QBlock::paint()" ,"item(0) == 0. i.e. body of algorithm is nul.");
        QRectF oval(hcenter - b/2, lw, b, a/2);
        canvas->drawRoundedRect(oval, a/4, a/4);
        canvas->drawText(oval, Qt::TextSingleLine | Qt::AlignHCenter | Qt::AlignVCenter, tr("BEGIN"));
//        drawCaption(canvas, oval, zoom(), tr("BEGIN"));
        canvas->drawLine(QLineF(hcenter, y + a/2+lw, hcenter, body->y+0.5));
        canvas->drawLine(QLineF(hcenter, body->y + body->height-0.5, hcenter, bottom - a/2-lw));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, bottom - a/2 - lw),
                                    QSize(6 * zoom(), 12 * zoom()));

        oval = QRectF(hcenter - b/2, bottom - a/2 - lw, b, a/2);
        canvas->drawRoundedRect(oval, a/4, a/4);
        canvas->drawText(oval, Qt::TextSingleLine | Qt::AlignHCenter | Qt::AlignVCenter, tr("END"));

      }
      else if(type() == "process")
      {
        /* процесс */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QRectF rect(hcenter - b/2, y + 16 * zoom(), b, a);
        QRectF textRect(hcenter - b/2 + 4 * zoom(), y + 20 * zoom(), b - 8 * zoom(), a - 8 * zoom());
        canvas->drawRect(rect);
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, y + 16 * zoom()+a, hcenter, bottom+0.5));
      }
      else if(type() == "assign")
      {
        /* присваивание */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QRectF rect(hcenter - b/2, y + 16 * zoom(), b, a);
        QRectF textRect(hcenter - b/2+4, y + 16 * zoom()+4, b-8, a-8);
        canvas->drawRect(rect);
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, QString("%1 := %2").arg(attributes.value("dest", ""), attributes.value("src", "")));
        canvas->drawLine(QLineF(hcenter, y + 16 * zoom()+a, hcenter, bottom+0.5));
      }
      else if(type() == "io")
      {
        /* ввод/вывод */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        par[0] = QPointF(hcenter - b/2 + a/4, y + 16 * zoom());
        par[1] = QPointF(hcenter + b/2 + a/4, y + 16 * zoom());
        par[2] = QPointF(hcenter + b/2 - a/4, y + 16 * zoom() + a);
        par[3] = QPointF(hcenter - b/2 - a/4, y + 16 * zoom() + a);
        canvas->drawPolygon(par, 4);
        QRectF textRect(hcenter - b/2 + a/4 +4, y + 16 * zoom()+4, b-a/2 - 8, a - 8);
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, y + 16 * zoom()+a, hcenter, bottom+0.5));
      }
      else if(type() == "if")
      {
        /* ветвление */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        par[0] = QPointF(hcenter - b/2, y + 16 * zoom() + a/2);
        par[1] = QPointF(hcenter      , y + 16 * zoom()      );
        par[2] = QPointF(hcenter + b/2, y + 16 * zoom() + a/2);
        par[3] = QPointF(hcenter      , y + 16 * zoom() + a  );
        canvas->drawPolygon(par, 4);
        QRectF textRect(hcenter - b/2 + a/4 + 20, y + 16 * zoom()+4 + a/8, b - a/2 - 40, a - 8 - a/4);
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, QString("%1?").arg(attributes.value("cond", "")));
        QBlock *left = item(0);
        Q_ASSERT_X(left != 0, "QBlock::paint()" ,"item(0) == 0. i.e. left branch of IF is nul.");
        QBlock *right = item(1);
        Q_ASSERT_X(right != 0, "QBlock::paint()" ,"item(1) == 0. i.e. right branch of IF is nul.");
        // левая линия
        QPointF line[3];
        line[0] = QPointF(hcenter - b/2, y + 16 * zoom() + a/2);
        line[1] = QPointF(left->x+left->width/2, y + 16 * zoom() + a/2);
        line[2] = QPointF(left->x+left->width/2, left->y);
        canvas->drawPolyline(line, 3);

        canvas->drawText(QPointF(hcenter - b/2 - 24*zoom(), y + 12 * zoom() + a/2), tr("Yes"));

        // правая линия
        line[0] = QPointF(hcenter + b/2, y + 16 * zoom() + a/2);
        line[1] = QPointF(right->x+right->width/2, y + 16 * zoom() + a/2);
        line[2] = QPointF(right->x+right->width/2, right->y);
        canvas->drawPolyline(line, 3);
        canvas->drawText(QPointF(hcenter + b/2 +5*zoom(), y + 12 * zoom() + a/2), tr("No"));

        // соединение
        QPointF collector[4];
        collector[0] = QPointF(left->x + left->width / 2, left->y+left->height);
        collector[1] = QPointF(left->x + left->width / 2, bottom - 8*zoom());
        collector[2] = QPointF(right->x + right->width / 2, bottom - 8*zoom());
        collector[3] = QPointF(right->x + right->width / 2, right->y+right->height);
        canvas->drawPolyline(collector, 4);
        canvas->drawLine(QLineF(hcenter, bottom-8*zoom(), hcenter, bottom+0.5));
      }
      else if(type() == "pre")
      {
        /* цикл с предусловием */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 32 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 32 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        par[0] = QPointF(hcenter - b/2, y + 32 * zoom() + a/2);
        par[1] = QPointF(hcenter      , y + 32 * zoom()      );
        par[2] = QPointF(hcenter + b/2, y + 32 * zoom() + a/2);
        par[3] = QPointF(hcenter      , y + 32 * zoom() + a  );
        canvas->drawPolygon(par, 4);
        QRectF rect(hcenter - b/2, y + 32 * zoom(), b, a);
        canvas->drawText(rect, Qt::AlignCenter | Qt::TextWrapAnywhere, QString("%1?").arg(attributes.value("cond", "")));
        QBlock *left = item(0);
        Q_ASSERT_X(left != 0, "QBlock::paint()" ,"item(0) == 0. i.e. body of PRE-loop is nul.");
        canvas->drawLine(QLineF(hcenter,y + 32 * zoom() + a,hcenter,left->y));

//        // правая линия
        QPointF line[5];
        line[0] = QPointF(hcenter + b/2, y + 32 * zoom() + a/2);
        line[1] = QPointF(x + width - 5*zoom(), y + 32 * zoom() + a/2);
        line[2] = QPointF(x + width - 5*zoom(), bottom - 4 * zoom());
        line[3] = QPointF(hcenter, bottom - 4 * zoom());
        line[4] = QPointF(hcenter, bottom+0.5);
        canvas->drawPolyline(line, 5);
        canvas->drawText(QPointF(hcenter + 4*zoom(), y + 44 * zoom() + a), tr("Yes"));
        canvas->drawText(QPointF(hcenter + b/2 +5*zoom(), y + 28 * zoom() + a/2), tr("No"));

        // соединение
        QPointF collector[5];
        collector[0] = QPointF(hcenter, left->y+left->height);
        collector[1] = QPointF(hcenter, bottom - 28*zoom());
        collector[2] = QPointF(x + 5*zoom(), bottom - 28*zoom());
        collector[3] = QPointF(x + 5*zoom(), y + 8*zoom());
        collector[4] = QPointF(hcenter, y + 8*zoom());
        canvas->drawPolyline(collector, 5);
        QFlowChart::drawRightArrow(canvas, collector[4],
                                    QSize(12 * zoom(), 6 * zoom()));

      }
      else if(type() == "post")
      {
        /* цикл с постусловием */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QBlock *left = item(0);
        Q_ASSERT_X(left != 0, "QBlock::paint()" ,"item(0) == 0. i.e. body of POST-loop is nul.");
        // верх ромба с входяящей стрелкой
        double top = left->y+left->height + 16 * zoom();

        canvas->drawLine(QLineF(hcenter,left->y+left->height,hcenter,top));

        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, top),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        par[0] = QPointF(hcenter - b/2, top + a/2);
        par[1] = QPointF(hcenter      , top      );
        par[2] = QPointF(hcenter + b/2, top + a/2);
        par[3] = QPointF(hcenter      , top + a  );
        canvas->drawPolygon(par, 4);
        QRectF rect(hcenter - b/2, top, b, a);
        canvas->drawText(rect, Qt::AlignCenter | Qt::TextWrapAnywhere, QString("%1?").arg(attributes.value("cond", "")));

        canvas->drawText(QPointF(hcenter - b/2 - 24*zoom(), top - 4* zoom() + a/2), tr("Yes"));
        canvas->drawText(QPointF(hcenter  +4*zoom(), top + 16 * zoom() + a), tr("No"));

        // соединение
        QPointF collector[4];
        collector[0] = QPointF(hcenter - b/2, top + a/2);
        collector[1] = QPointF(x + 5*zoom(), top + a/2);
        collector[2] = QPointF(x + 5*zoom(), y + 8*zoom());
        collector[3] = QPointF(hcenter, y + 8*zoom());
        canvas->drawPolyline(collector, 4);
        QFlowChart::drawRightArrow(canvas, collector[3],
                                    QSize(12 * zoom(), 6 * zoom()));

        // выход
        canvas->drawLine(QLineF(hcenter, top + a, hcenter, bottom+0.5));
      }
      else if(type() == "for")
      {
        /* цикл FOR */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 32 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 32 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF hex[6];
        hex[0] = QPointF(hcenter - b/2, y + 32 * zoom() + a/2);
        hex[1] = QPointF(hcenter - a/2, y + 32 * zoom()      );
        hex[2] = QPointF(hcenter + a/2, y + 32 * zoom()      );
        hex[3] = QPointF(hcenter + b/2, y + 32 * zoom() + a/2);
        hex[4] = QPointF(hcenter + a/2, y + 32 * zoom() + a  );
        hex[5] = QPointF(hcenter - a/2, y + 32 * zoom() + a  );
        canvas->drawPolygon(hex, 6);

        QRectF rect(hcenter - b/2, y + 32 * zoom(), b, a);
        canvas->drawText(rect, Qt::AlignCenter | Qt::TextWrapAnywhere, QString("%1 := %2...%3").arg(attributes.value("var", ""), attributes.value("from", ""), attributes.value("to", "")));
        QBlock *left = item(0);
        Q_ASSERT_X(left != 0, "QBlock::paint()" ,"item(0) == 0. i.e. body of PRE-loop is nul.");
        canvas->drawLine(QLineF(hcenter,y + 32 * zoom() + a,hcenter,left->y));

//        // правая линия
        QPointF line[5];
        line[0] = QPointF(hcenter + b/2, y + 32 * zoom() + a/2);
        line[1] = QPointF(x + width - 5*zoom(), y + 32 * zoom() + a/2);
        line[2] = QPointF(x + width - 5*zoom(), bottom - 4 * zoom());
        line[3] = QPointF(hcenter, bottom - 4 * zoom());
        line[4] = QPointF(hcenter, bottom+0.5);
        canvas->drawPolyline(line, 5);

        // соединение
        QPointF collector[5];
        collector[0] = QPointF(hcenter, left->y+left->height);
        collector[1] = QPointF(hcenter, bottom - 28*zoom());
        collector[2] = QPointF(x + 5*zoom(), bottom - 28*zoom());
        collector[3] = QPointF(x + 5*zoom(), y + 32*zoom() + a/2);
        collector[4] = QPointF(hcenter - b/2, y + 32*zoom() + a/2);
        canvas->drawPolyline(collector, 5);
        QFlowChart::drawRightArrow(canvas, collector[4],
                                    QSize(12 * zoom(), 6 * zoom()));

      }

    }
    //canvas->drawText(x+8, y+12, type());
    for(int i = 0; i < items.size(); ++i)
    {
      item(i)->paint(canvas, fontSizeInPoints);
    }
  }
}

double QBlock::zoom() const
{
  if (flowChart())
  {
    return flowChart()->zoom();
  }
  else
  {
    return 1;
  }
}

QBlock * QBlock::blockAt(int px, int py)
{
  QRectF rect(x, y, width, height);
  if (!rect.contains(px, py)) return 0;
  else
  {
    for (int i = 0; i < items.size(); ++i)
    {
      QBlock *tmp = item(i)->blockAt(px, py);
      if (tmp) return tmp;
    }
    return this;
  }
}

QDomElement QBlock::xmlNode(QDomDocument & doc) const
{
  QDomElement self = doc.createElement(type());
  QList<QString> sl = attributes.uniqueKeys();
  for (int i = 0; i < sl.size(); ++i)
  {
    if (sl.at(i) != "type")
    {
      self.setAttribute(sl.at(i), attributes.value(sl.at(i), QString()));
    }
  }


  for (int i = 0; i < items.size(); ++i)
  {
    QDomElement child = item(i)->xmlNode(doc);
    self.appendChild(child);
  }
  return self;
}

void QBlock::setXmlNode(const QDomElement & node)
{
  clear();
  setType(node.nodeName());
  QDomNamedNodeMap attrs = node.attributes();
  for (int i = 0; i < attrs.size(); ++i)
  {
    QDomAttr da = attrs.item(i).toAttr();
    if(da.name() != "type")
    {
      attributes.insert(da.name(), da.value());
    }
  }
  if (type() == "branch") isBranch = true;
  else isBranch = false;
  QDomNodeList children = node.childNodes();
  for(int i = 0; i < children.size(); ++i)
  {
    if (children.at(i).isElement())
    {
      QDomElement child = children.at(i).toElement();
      QBlock *block = new QBlock();
      block->setFlowChart(flowChart());
      block->setXmlNode(child);
      append(block);
    }
  }
}

void QBlock::insertXmlTree(int aIndex, const QDomElement & algorithm)
{
  if (isBranch)
  {
    QDomElement branch = algorithm.firstChildElement("branch");
    if(!branch.isNull())
    {
      QDomNodeList children = branch.childNodes();
      int ind = aIndex;
      for(int i = 0; i < children.size(); ++i)
      {
        if (children.at(i).isElement())
        {
          QDomElement child = children.at(i).toElement();
          QBlock *block = new QBlock();
          block->setFlowChart(flowChart());
          block->setXmlNode(child);
          insert(ind, block);
          ind++;
        }
      }
    }
  }
}

bool QBlock::isActive() const
{
  if(flowChart())
  {
    if (flowChart()->activeBlock() == this) return true;
    else if (parent)
    {
      return parent->isActive();
    }
    else
    {
      return false;
    }

  }
  else
    return false;
}
