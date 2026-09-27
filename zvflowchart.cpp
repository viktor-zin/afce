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

#include "zvflowchart.h"
#include "afceutil.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QImage>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QTimer>
#include <QtMath>

namespace {

// Layout of the chart at 100 % zoom, in pixels (everything is scaled by the zoom).
constexpr double kFontPx = 13;        // block texts
constexpr double kLabelPx = 11.5;     // Yes / No, values of case branches
constexpr double kMinW = 120;         // GOST 19.003-80: b = 2a, a = 60 (minimum symbol size)
constexpr double kMinH = 60;
constexpr double kTermMinW = 120;     // terminator (BEGIN, END, return)
constexpr double kTermMinH = 30;
constexpr double kJumpMinW = 88;      // break, continue
constexpr double kJumpMinH = 34;
constexpr double kPadX = 10;          // text padding inside the symbols
constexpr double kPadY = 7;
constexpr double kInLine = 20;        // incoming line (with the arrow) above a symbol
constexpr double kOutLine = 8;        // outgoing line below a simple symbol
constexpr double kSide = 10;          // free space left and right of a symbol
constexpr double kTopPad = 6;         // space above BEGIN and below END
constexpr double kWrapRect = 360;     // maximum text widths (3 x the base width for rectangles)
constexpr double kWrapDiamond = 200;
constexpr double kWrapHexagon = 280;
constexpr double kWrapLabel = 150;
constexpr double kMinBranchW = 100;
constexpr double kMinBranchH = 24;
constexpr double kBranchDrop = 12;    // if / case: from the diamond to the branches
constexpr double kCaseGap = 12;       // case: between the branches
constexpr double kLoopSide = 22;      // loops: margin for the return / exit lines ...
constexpr double kLoopLine = 10;      // ... drawn at this distance from the block edge
constexpr double kLoopTop = 28;       // while: incoming line (the return line joins it)
constexpr double kLoopJoin = 12;      // while: the return line joins the incoming line here
constexpr double kPostTop = 14;       // do-while: incoming line above the body
constexpr double kPostJoin = 7;
constexpr double kLoopBottom = 34;    // loops: return line at bottom - 24, exit line at bottom - 10
constexpr double kCollector = 16;     // if / case: collector line at bottom - 8
constexpr double kArrowLen = 9;
constexpr double kArrowWidth = 7;

enum class Shape { Rect, Call, Parallelogram, Diamond, Hexagon, Terminator, JumpRight, JumpLeft, Generic };

Shape shapeOf(const QString &type)
{
  if (type == QLatin1String("process") || type == QLatin1String("assign"))
    return Shape::Rect;
  if (type == QLatin1String("call"))
    return Shape::Call;
  if (type == QLatin1String("io") || type == QLatin1String("ou"))
    return Shape::Parallelogram;
  if (type == QLatin1String("if") || type == QLatin1String("case") || type == QLatin1String("pre")
      || type == QLatin1String("post"))
    return Shape::Diamond;
  if (type == QLatin1String("for") || type == QLatin1String("forc") || type == QLatin1String("foreach"))
    return Shape::Hexagon;
  if (type == QLatin1String("algorithm") || type == QLatin1String("return"))
    return Shape::Terminator;
  if (type == QLatin1String("break"))
    return Shape::JumpRight;
  if (type == QLatin1String("continue"))
    return Shape::JumpLeft;
  return Shape::Generic;
}

// A 96 dpi device for measuring texts: the chart font is 13 px at 100 % zoom
// whatever the screen resolution is.
QPaintDevice *measureDevice()
{
  static QImage image = [] {
    QImage img(1, 1, QImage::Format_ARGB32_Premultiplied);
    img.setDotsPerMeterX(3780);
    img.setDotsPerMeterY(3780);
    return img;
  }();
  return &image;
}

// The chart font: `pixels` px at 100 % zoom, scaled exactly with the zoom (a
// fractional size, not rounded to whole pixels), so that line breaks and text
// positions are the same on the screen, in pictures and in PDF / print at any
// zoom. The family is the application font unless the style sets one.
// The application font, or a common sans-serif font if its family does not
// exist (e.g. "Sans Serif" of the offscreen platform, which would make Qt
// search all font aliases).
QFont defaultChartFont()
{
  static const QString family = [] {
    const QString app = QApplication::font().family();
    if (app.startsWith(QLatin1Char('.'))) // macOS system font
      return app;
    const QStringList families = QFontDatabase::families();
    if (families.contains(app, Qt::CaseInsensitive))
      return app;
    const QString system = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    if (system.startsWith(QLatin1Char('.')) || families.contains(system, Qt::CaseInsensitive))
      return system;
    for (const char *candidate : {"Helvetica Neue", "Segoe UI", "Noto Sans", "DejaVu Sans", "Liberation Sans",
                                  "Arial", "Helvetica"})
    {
      if (families.contains(QLatin1String(candidate), Qt::CaseInsensitive))
        return QString::fromLatin1(candidate);
    }
    return app;
  }();
  QFont font = QApplication::font();
  font.setFamily(family);
  return font;
}

QFont chartFont(const QString &family, double pixels, double zoom, QPaintDevice *device)
{
  QFont font = defaultChartFont();
  if (!family.isEmpty())
    font.setFamily(family);
  font.setWeight(QFont::Normal);
  font.setItalic(false);
  font.setUnderline(false);
  font.setStrikeOut(false);
  const int dpi = device && device->logicalDpiY() > 0 ? device->logicalDpiY() : 96;
  font.setPointSizeF(qMax(0.5, pixels * zoom) * 72.0 / dpi);
  return device ? QFont(font, device) : font;
}

// Draws text into a rectangle of the chart. The text is laid out at 100 % zoom
// and scaled by the painter: glyph sizes are whole pixels in the font engine,
// so a layout at the zoomed font size would break lines differently at
// different zooms (the screen, PNG / SVG and PDF / print would disagree).
// The painter's font is the chart font at the given zoom.
void drawZoomedText(QPainter *canvas, const QRectF &rect, int flags, const QString &text, double zoom)
{
  if (!(zoom > 0))
    return;
  canvas->save();
  QFont font = canvas->font();
  font.setPointSizeF(font.pointSizeF() / zoom);
  canvas->translate(rect.topLeft());
  canvas->scale(zoom, zoom);
  canvas->setFont(font);
  canvas->drawText(QRectF(0, 0, rect.width() / zoom, rect.height() / zoom), flags, text);
  canvas->restore();
}

struct TextLayout
{
  QStringList lines;
  double width = 0;
  double height = 0;
};

// Breaks a single token that is wider than maxWidth into pieces (anywhere).
QStringList breakToken(const QString &token, const QFontMetricsF &fm, double maxWidth)
{
  QStringList pieces;
  QString rest = token;
  while (!rest.isEmpty() && fm.horizontalAdvance(rest) > maxWidth)
  {
    int lo = 1, hi = int(rest.size()) - 1;
    while (lo < hi)
    {
      const int mid = (lo + hi + 1) / 2;
      if (fm.horizontalAdvance(rest.left(mid)) <= maxWidth)
        lo = mid;
      else
        hi = mid - 1;
    }
    int cut = qMax(1, lo);
    // prefer a natural break after an underscore, a dot, a comma, an operator...
    static const QString breakAfter = QStringLiteral("_.,;:)]}([{-+*/%=<>&|!?\\");
    for (int i = cut; i > qMax(1, cut / 2); --i)
    {
      if (breakAfter.contains(rest.at(i - 1)))
      {
        cut = i;
        break;
      }
    }
    if (cut < rest.size() && rest.at(cut).isLowSurrogate() && cut > 1)
      --cut;
    pieces << rest.left(cut);
    rest = rest.mid(cut);
  }
  if (!rest.isEmpty())
    pieces << rest;
  return pieces;
}

// Word wrap at maxWidth (explicit line breaks are kept); a word longer than
// maxWidth is broken anywhere. Sizes are those of the 100 % font.
TextLayout layoutText(const QString &text, const QFontMetricsF &fm, double maxWidth)
{
  TextLayout result;
  static const QRegularExpression tokenRx(QStringLiteral("\\S+\\s*"));
  QString plain = text;
  plain.replace(QLatin1Char('\t'), QLatin1Char(' '));
  plain.remove(QLatin1Char('\r'));
  const QStringList paragraphs = plain.split(QLatin1Char('\n'));
  for (const QString &paragraph : paragraphs)
  {
    QString line;
    QRegularExpressionMatchIterator it = tokenRx.globalMatch(paragraph);
    bool any = false;
    while (it.hasNext())
    {
      any = true;
      const QString token = it.next().captured();
      const QString candidate = line + token;
      if (!line.isEmpty() && fm.horizontalAdvance(candidate.trimmed()) <= maxWidth)
      {
        line = candidate;
        continue;
      }
      if (!line.isEmpty())
      {
        result.lines << line.trimmed();
        line.clear();
      }
      if (fm.horizontalAdvance(token.trimmed()) > maxWidth)
      {
        QStringList pieces = breakToken(token.trimmed(), fm, maxWidth);
        line = pieces.takeLast() + token.mid(token.trimmed().size());
        result.lines << pieces;
      }
      else
      {
        line = token;
      }
    }
    if (any)
      result.lines << line.trimmed();
    else
      result.lines << QString();
  }
  while (result.lines.size() > 1 && result.lines.last().isEmpty())
    result.lines.removeLast();
  for (const QString &line : std::as_const(result.lines))
    result.width = qMax(result.width, fm.horizontalAdvance(line));
  result.width = qCeil(result.width);
  result.height = result.lines.size() * fm.lineSpacing();
  return result;
}

// Size of a symbol (100 % zoom) that fits a text box of tw x th.
QSizeF shapeSizeFor(Shape shape, double tw, double th)
{
  switch (shape)
  {
    case Shape::Rect:
    case Shape::Generic:
      return QSizeF(qMax(kMinW, tw + 2 * kPadX), qMax(kMinH, th + 2 * kPadY));
    case Shape::Call:
      return QSizeF(qMax(kMinW, tw + 2 * kPadX + 16), qMax(kMinH, th + 2 * kPadY));
    case Shape::Parallelogram:
    {
      const double h = qMax(kMinH, th + 2 * kPadY);
      const double off = qMin(h, 60.0) / 2;
      return QSizeF(qMax(kMinW + 30, tw + 2 * kPadX + off * (h + th) / h), h);
    }
    case Shape::Diamond:
    {
      // the text box (rw x rh) is inscribed: (rw/2)/a + (rh/2)/b <= 1
      const double rw = tw + 2 * 8;
      const double rh = th + 2 * 5;
      const double b = qMax(kMinH / 2, rh);
      const double a = qMax(kMinW / 2, (rw / 2) / (1 - rh / (2 * b)));
      return QSizeF(qCeil(2 * a), qCeil(2 * b));
    }
    case Shape::Hexagon:
    {
      const double h = qMax(kMinH, th + 2 * kPadY);
      const double c = qMin(h / 2, 30.0);
      return QSizeF(qCeil(qMax(kMinW, tw + 2 * kPadX + 2 * c * (th + 4) / h)), h);
    }
    case Shape::Terminator:
    {
      const double h = qMax(kTermMinH, th + 12);
      const double r = qMin(h / 2, 18.0);
      return QSizeF(qCeil(qMax(kTermMinW, tw + 2 * kPadX + r)), h);
    }
    case Shape::JumpRight:
    case Shape::JumpLeft:
    {
      const double h = qMax(kJumpMinH, th + 14);
      const double tip = qMin(h / 2, 16.0);
      return QSizeF(qCeil(qMax(kJumpMinW, tw + 2 * kPadX + tip)), h);
    }
  }
  return QSizeF(kMinW, kMinH);
}

// Outline of a symbol filling r (z = zoom, for the fixed-size details).
QPainterPath shapePath(Shape shape, const QRectF &r, double z, bool rounded)
{
  QPainterPath path;
  const double l = r.left(), t = r.top(), rr = r.right(), b = r.bottom();
  const double cx = r.center().x(), cy = r.center().y(), h = r.height();
  switch (shape)
  {
    case Shape::Rect:
    case Shape::Call:
    case Shape::Generic:
      if (rounded)
      {
        const double radius = qMin(4 * z, h / 6);
        path.addRoundedRect(r, radius, radius);
      }
      else
        path.addRect(r);
      break;
    case Shape::Parallelogram:
    {
      const double off = qMin(h, 60 * z) / 2;
      path.addPolygon(QPolygonF({QPointF(l + off, t), QPointF(rr, t), QPointF(rr - off, b), QPointF(l, b)}));
      path.closeSubpath();
      break;
    }
    case Shape::Diamond:
      path.addPolygon(QPolygonF({QPointF(cx, t), QPointF(rr, cy), QPointF(cx, b), QPointF(l, cy)}));
      path.closeSubpath();
      break;
    case Shape::Hexagon:
    {
      const double c = qMin(qMin(h / 2, 30 * z), r.width() / 4);
      path.addPolygon(QPolygonF({QPointF(l, cy), QPointF(l + c, t), QPointF(rr - c, t), QPointF(rr, cy),
                                 QPointF(rr - c, b), QPointF(l + c, b)}));
      path.closeSubpath();
      break;
    }
    case Shape::Terminator:
    {
      const double radius = qMin(h / 2, 18 * z);
      path.addRoundedRect(r, radius, radius);
      break;
    }
    case Shape::JumpRight:
    {
      const double tip = qMin(h / 2, 16 * z);
      path.addPolygon(QPolygonF({QPointF(l, t), QPointF(rr - tip, t), QPointF(rr, cy), QPointF(rr - tip, b), QPointF(l, b)}));
      path.closeSubpath();
      break;
    }
    case Shape::JumpLeft:
    {
      const double tip = qMin(h / 2, 16 * z);
      path.addPolygon(QPolygonF({QPointF(l + tip, t), QPointF(rr, t), QPointF(rr, b), QPointF(l + tip, b), QPointF(l, cy)}));
      path.closeSubpath();
      break;
    }
  }
  return path;
}

// Horizontal offset of the text centre in a symbol (away from the tip of a jump).
double textShift(Shape shape, double h, double z)
{
  if (shape == Shape::JumpRight)
    return -qMin(h / 2, 16 * z) / 2;
  if (shape == Shape::JumpLeft)
    return qMin(h / 2, 16 * z) / 2;
  return 0;
}

// Draws a symbol with its fill and outline.
void paintShape(QPainter *canvas, Shape shape, const QRectF &r, double z, const QColor &fill,
                const QPen &pen, bool rounded)
{
  canvas->save();
  canvas->setPen(pen);
  canvas->setBrush(fill);
  canvas->drawPath(shapePath(shape, r, z, rounded));
  if (shape == Shape::Call)
  {
    const double inset = qMin(8 * z, r.width() / 8);
    canvas->setBrush(Qt::NoBrush);
    canvas->drawLine(QLineF(r.left() + inset, r.top(), r.left() + inset, r.bottom()));
    canvas->drawLine(QLineF(r.right() - inset, r.top(), r.right() - inset, r.bottom()));
  }
  canvas->restore();
}

// A filled arrow head with the tip at `tip` pointing in direction dir (unit vector).
void paintArrow(QPainter *canvas, const QPointF &tip, const QPointF &dir, double z, const QColor &color)
{
  const double len = kArrowLen * z;
  const double half = kArrowWidth * z / 2;
  const QPointF base = tip - dir * len;
  const QPointF normal(-dir.y(), dir.x());
  canvas->save();
  canvas->setPen(Qt::NoPen);
  canvas->setBrush(color);
  canvas->drawPolygon(QPolygonF({tip, base + normal * half, base - normal * half}));
  canvas->restore();
}

// Text lines centred at `center` (lineHeight and width at 100 % zoom).
void paintLines(QPainter *canvas, const QStringList &lines, const QPointF &center, double lineHeight,
                double width, double zoom)
{
  const double lh = lineHeight * zoom;
  const double w = (width + 8) * zoom;
  const double top = center.y() - lines.size() * lh / 2;
  for (int i = 0; i < lines.size(); ++i)
  {
    const QRectF r(center.x() - w / 2, top + i * lh, w, lh);
    drawZoomedText(canvas, r, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextDontClip,
                   lines.at(i), zoom);
  }
}

// Text lines starting at `anchor` (top-left, or top-right if alignRight).
void paintLabel(QPainter *canvas, const QStringList &lines, const QPointF &anchor, double lineHeight,
                double zoom, bool alignRight)
{
  const double lh = lineHeight * zoom;
  const double w = 400 * zoom;
  for (int i = 0; i < lines.size(); ++i)
  {
    const QRectF r(alignRight ? anchor.x() - w : anchor.x(), anchor.y() + i * lh, w, lh);
    drawZoomedText(canvas, r, (alignRight ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter
                   | Qt::TextSingleLine | Qt::TextDontClip, lines.at(i), zoom);
  }
}

QString styleFamily(const QFlowChart *chart)
{
  return chart ? chart->chartStyle().fontFamily() : QString();
}

// Width of a single-line label at 100 % zoom.
double labelWidth(const QString &family, const QString &text)
{
  const QFont font = chartFont(family, kLabelPx, 1, measureDevice());
  return qCeil(QFontMetricsF(font, measureDevice()).horizontalAdvance(text));
}

// The block a mouse click / hover refers to: branches stand for their block,
// the algorithm only on its start terminator.
QBlock *pickBlock(QBlock *root, const QPoint &pos)
{
  if (!root)
    return nullptr;
  QBlock *block = root->blockAt(pos.x(), pos.y());
  while (block && block->isBranch)
    block = block->parent;
  if (block == root && !root->symbolRect().adjusted(-4, -4, 4, 4).contains(pos))
    return nullptr;
  return block;
}

} // namespace

QFlowChart::QFlowChart(QWidget *pObj /* = nullptr */)
  : QWidget(pObj), fRoot(nullptr), fActiveBlock(nullptr), fZoom(1), fStatus(Display), fMultiInsert(false),
    fHoverBlock(nullptr), fDragBlock(nullptr), fDragging(false), fDragCopy(false),
    fExternalDrag(false), fAutoScrollTimer(new QTimer(this))
{
  fBuffer = QString();
  fTargetPoint = QInsertionPoint();
  fRoot = new QBlock();
  root()->setFlowChart(this);
  setMouseTracking(true);
  setFocusPolicy(Qt::ClickFocus);
  setAcceptDrops(true);
  fAutoScrollTimer->setInterval(30);
  connect(fAutoScrollTimer, &QTimer::timeout, this, &QFlowChart::autoScrollStep);
  clear();
  setZoom(1);
}

QFlowChart::~QFlowChart()
{
  // Do not call clear() here: it emits signals that could reach receivers
  // which are being destroyed together with this widget.
  fActiveBlock = nullptr;
  fHoverBlock = nullptr;
  fDragBlock = nullptr;
  fDragging = false;
  fExternalDrag = false;
  insertionPoints.clear();
  fTargetPoint = QInsertionPoint();
  QBlock *r = fRoot;
  fRoot = nullptr;
  delete r;
}

void QFlowChart::makeUndo()
{
  QString state = toString();
  undoStack.push(state);
  redoStack.clear();
  emit modified();
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
  const QClipboard *clp = QApplication::clipboard();
  return clp && isInsertableBuffer(clp->text());
}

void QFlowChart::clearHistory()
{
  undoStack.clear();
  redoStack.clear();
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
    emit modified();
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
    emit modified();
    emit changed();
  }
}


void QFlowChart::clear()
{
    deselectAll();
    root()->clear();
    root()->attributes.clear();
    root()->setType(QStringLiteral("algorithm"));
    root()->append(new QBlock(QStringLiteral("branch")));
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
    fActiveBlock = nullptr;
    emit changed();
    update();
}

void QFlowChart::setActiveBlock(QBlock *aBlock)
{
  fActiveBlock = aBlock;
  emit changed();
  update();
}

void QFlowChart::paintEvent(QPaintEvent *pEvent)
{
    QPainter canvas(this);
    pEvent->accept();
    canvas.setClipRect(pEvent->rect());
    canvas.setRenderHint(QPainter::Antialiasing, true);
    canvas.setRenderHint(QPainter::TextAntialiasing, true);
    paintTo(&canvas);
}

void QFlowChart::paintTo(QPainter *canvas)
{
    if (root())
    {
      const QFlowChartStyle st = chartStyle();
      canvas->fillRect(QRectF(0, 0, root()->width, root()->height), st.canvasColor());
      root()->paint(canvas);
      if (status() == Insertion || fDragging || fExternalDrag)
      {
        const double z = zoom();
        QColor accent = st.selectedBackground();
        canvas->save();
        canvas->setRenderHint(QPainter::Antialiasing, true);
        for (const QInsertionPoint &ip : std::as_const(insertionPoints))
        {
          canvas->setPen(QPen(accent, qMax(1.0, 1.5 * z)));
          canvas->setBrush(st.canvasColor());
          canvas->drawEllipse(ip.point(), 3.5 * z, 3.5 * z);
        }
        if (!targetPoint().isNull())
        {
          const QPointF p = targetPoint().point();
          QColor halo = accent;
          halo.setAlpha(60);
          canvas->setPen(Qt::NoPen);
          canvas->setBrush(halo);
          canvas->drawEllipse(p, 12 * z, 12 * z);
          canvas->setBrush(accent);
          canvas->drawEllipse(p, 8 * z, 8 * z);
          if (!fDragging || fDragCopy)
          {
            // "+": a new block (insertion, copy)
            canvas->setPen(QPen(st.selectedForeground(), qMax(1.0, 2 * z), Qt::SolidLine, Qt::RoundCap));
            canvas->drawLine(QPointF(p.x() - 4 * z, p.y()), QPointF(p.x() + 4 * z, p.y()));
            canvas->drawLine(QPointF(p.x(), p.y() - 4 * z), QPointF(p.x(), p.y() + 4 * z));
          }
        }
        canvas->restore();
      }
    }
}

QDomDocument QFlowChart::document() const
{
  QDomDocument doc(QStringLiteral("AFC"));
  QDomElement r = root()->xmlNode(doc);
  doc.appendChild(r);
  return doc;
}

bool QFlowChart::isValidDocument(const QDomDocument &doc, QString *errorMessage)
{
  const QDomElement algorithm = doc.documentElement();
  if (algorithm.isNull())
  {
    if (errorMessage)
      *errorMessage = tr("The document is empty.");
    return false;
  }
  if (algorithm.tagName() != QLatin1String("algorithm"))
  {
    if (errorMessage)
      *errorMessage = tr("This is not an algorithm flowchart: the root element is <%1> instead of <algorithm>.").arg(algorithm.tagName());
    return false;
  }
  if (algorithm.firstChildElement(QStringLiteral("branch")).isNull())
  {
    if (errorMessage)
      *errorMessage = tr("The algorithm has no body (<branch> element).");
    return false;
  }
  return true;
}

bool QFlowChart::parseDocument(const QByteArray &data, QDomDocument *doc, QString *errorMessage)
{
  QDomDocument tmp;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  const QDomDocument::ParseResult result = tmp.setContent(data);
  const bool parsed = bool(result);
  const QString xmlError = result.errorMessage;
  const qint64 errorLine = result.errorLine;
  const qint64 errorColumn = result.errorColumn;
#else
  // Qt 6.4 (Ubuntu 24.04, Debian 12) has no ParseResult yet
  QString xmlError;
  int errorLine = 0;
  int errorColumn = 0;
  const bool parsed = tmp.setContent(data, &xmlError, &errorLine, &errorColumn);
#endif
  if (!parsed)
  {
    if (errorMessage)
      *errorMessage = tr("Invalid XML (line %1, column %2): %3")
                        .arg(errorLine).arg(errorColumn).arg(xmlError);
    return false;
  }
  if (!isValidDocument(tmp, errorMessage))
    return false;
  if (doc)
    *doc = tmp;
  return true;
}

bool QFlowChart::isInsertableBuffer(const QString &aBuffer)
{
  if (aBuffer.trimmed().isEmpty())
    return false;
  QDomDocument doc;
  if (!doc.setContent(aBuffer))
    return false;
  return isValidDocument(doc);
}

bool QFlowChart::loadDocument(const QDomDocument &doc, QString *errorMessage)
{
  if (!isValidDocument(doc, errorMessage))
    return false;
  fActiveBlock = nullptr;
  fTargetPoint = QInsertionPoint();
  insertionPoints.clear();
  root()->setXmlNode(doc.documentElement());
  clearHistory();
  realignObjects();
  emit changed();
  return true;
}

bool QFlowChart::loadFile(const QString &fileName, QString *errorMessage)
{
  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly))
  {
    if (errorMessage)
      *errorMessage = tr("Unable to open file '%1': %2").arg(QDir::toNativeSeparators(fileName), file.errorString());
    return false;
  }
  const QByteArray data = file.readAll();
  if (file.error() != QFileDevice::NoError)
  {
    if (errorMessage)
      *errorMessage = tr("Unable to read file '%1': %2").arg(QDir::toNativeSeparators(fileName), file.errorString());
    return false;
  }
  QDomDocument doc;
  QString error;
  if (!parseDocument(data, &doc, &error))
  {
    if (errorMessage)
      *errorMessage = tr("File '%1' is not a valid flowchart. %2").arg(QDir::toNativeSeparators(fileName), error);
    return false;
  }
  return loadDocument(doc, errorMessage);
}

bool QFlowChart::saveFile(const QString &fileName, QString *errorMessage) const
{
  QSaveFile file(fileName);
  file.setDirectWriteFallback(true);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    if (errorMessage)
      *errorMessage = tr("Unable to save file '%1': %2").arg(QDir::toNativeSeparators(fileName), file.errorString());
    return false;
  }
  const QByteArray data = document().toString(2).toUtf8();
  if (file.write(data) != data.size() || !file.commit())
  {
    if (errorMessage)
      *errorMessage = tr("Unable to save file '%1': %2").arg(QDir::toNativeSeparators(fileName), file.errorString());
    file.cancelWriting();
    return false;
  }
  return true;
}

QString QFlowChart::activeBlockXml() const
{
  const QBlock *block = activeBlock();
  if (!block)
    return QString();
  QDomDocument doc(QStringLiteral("AFC")); // do not localize!
  QDomElement element = block->xmlNode(doc);
  if (block->isBranch)
  {
    QDomElement alg = doc.createElement(QStringLiteral("algorithm"));
    alg.appendChild(element);
    doc.appendChild(alg);
  }
  else if (element.nodeName() != QLatin1String("algorithm"))
  {
    QDomElement alg = doc.createElement(QStringLiteral("algorithm"));
    QDomElement branch = doc.createElement(QStringLiteral("branch"));
    alg.appendChild(branch);
    branch.appendChild(element);
    doc.appendChild(alg);
  }
  else
  {
    doc.appendChild(element);
  }
  return doc.toString(2);
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
  emit zoomChanged(aZoom);
}

double QFlowChart::fitZoom(const QSize &viewport, double minZoom, double maxZoom) const
{
  if (!root() || !(zoom() > 0) || root()->width <= 0 || root()->height <= 0 || viewport.isEmpty())
    return zoom();
  const double w = root()->width / zoom();
  const double h = root()->height / zoom();
  const double z = qMin(viewport.width() / w, viewport.height() / h);
  return qBound(minZoom, z, maxZoom);
}

void QFlowChart::zoomToFit(const QSize &viewport)
{
  setZoom(fitZoom(viewport));
}

void QFlowChart::setStatus(int aStatus)
{
  if (fDragging)
    cancelDrag();
  fDragBlock = nullptr;
  fStatus = aStatus;
  fHoverBlock = nullptr;
  if (status() == Insertion)
  {
    regeneratePoints();
    if (isVisible())
      setFocus(Qt::OtherFocusReason);
    update();
  }
  else
  {
    fTargetPoint = QInsertionPoint();
    insertionPoints.clear();
    update();
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
    fActiveBlock = nullptr;
    deleteBlock(tmp);
    emit changed();
  }
}
void QFlowChart::realignObjects()
{
  if(root())
  {
    makeBackwardCompatibility();
    root()->adjustSize(zoom());
    root()->adjustPosition(0,0);
    resize(qCeil(root()->width), qCeil(root()->height));
    if (status() == Insertion || fDragging)
    {
      // block positions changed: the old insertion points are stale
      fTargetPoint = QInsertionPoint();
      regeneratePoints();
    }
    emit changed();
    update();
  }
}

void QFlowChart::makeBackwardCompatibility() {
    if(root()) {
        root()->makeBackwardCompatibility();
    }
}


void QFlowChart::setBuffer(const QString & aBuffer)
{
  if(isInsertableBuffer(aBuffer))
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

void QFlowChart::setHoverBlock(QBlock *aBlock)
{
  if (fHoverBlock != aBlock)
  {
    fHoverBlock = aBlock;
    update();
  }
}

void QFlowChart::startDrag()
{
  fDragging = true;
  fDragCopy = false;
  fHoverBlock = nullptr;
  fTargetPoint = QInsertionPoint();
  regeneratePoints();
  setCursor(Qt::ClosedHandCursor);
  update();
}

void QFlowChart::cancelDrag()
{
  fDragging = false;
  fDragCopy = false;
  fTargetPoint = QInsertionPoint();
  insertionPoints.clear();
  unsetCursor();
  update();
}

void QFlowChart::updateDragCursor(Qt::KeyboardModifiers modifiers)
{
  if (!fDragging)
    return;
  const bool copy = (modifiers & (Qt::ControlModifier | Qt::AltModifier)) != 0;
  if (copy != fDragCopy)
  {
    fDragCopy = copy;
    setCursor(copy ? Qt::DragCopyCursor : Qt::ClosedHandCursor);
    update();
  }
}

bool QFlowChart::dropBlock(QBlock *aBlock, const QInsertionPoint &aPoint, bool copy)
{
  QBlock *branch = aPoint.branch();
  if (!aBlock || !branch || aBlock == root() || aBlock->isBranch || !aBlock->parent || !branch->isBranch)
    return false;
  if (aBlock->flowChart() != this || branch->flowChart() != this)
    return false;
  // never into itself
  for (const QBlock *b = branch; b; b = b->parent)
  {
    if (b == aBlock)
      return false;
  }
  int index = aPoint.index();
  if (index < 0 || index > branch->items.size())
    index = int(branch->items.size());
  if (!copy && aBlock->parent == branch)
  {
    const int current = int(branch->items.indexOf(aBlock));
    if (index == current || index == current + 1)
      return false;
    if (current < index)
      --index;
  }
  makeUndo();
  QBlock *result = aBlock;
  if (copy)
  {
    QDomDocument doc;
    result = new QBlock();
    result->setFlowChart(this);
    result->setXmlNode(aBlock->xmlNode(doc));
  }
  branch->insert(index, result);
  fActiveBlock = result;
  realignObjects();
  emit changed();
  update();
  return true;
}

void QFlowChart::mousePressEvent(QMouseEvent *pEvent)
{
  const QPoint mp = pEvent->position().toPoint();
  if (status() == Selectable)
  {
    if (pEvent->button() != Qt::LeftButton && pEvent->button() != Qt::RightButton)
      return;
    QBlock *block = pickBlock(root(), mp);
    if (block)
    {
      if (block->isActive() && activeBlock() && ((pEvent->modifiers() & Qt::ControlModifier) != 0))
      {
        // Ctrl + click on the selection selects the enclosing branch / block
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
        fActiveBlock = block;
      }
    }
    else
    {
      fActiveBlock = nullptr;
    }
    // a pressed block may be dragged to another place
    fDragBlock = nullptr;
    QBlock *active = activeBlock();
    if (pEvent->button() == Qt::LeftButton && active && active != root() && !active->isBranch && active->parent
        && QRectF(active->x, active->y, active->width, active->height).contains(mp))
    {
      fDragBlock = active;
      fPressPos = mp;
    }
    emit changed();
    update();
  }
  else if(status() == Insertion)
  {
    if (pEvent->button() == Qt::RightButton)
    {
      setStatus(Selectable);
      return;
    }
    if (pEvent->button() != Qt::LeftButton)
      return;
    QInsertionPoint ip = getNearistPoint(mp.x(), mp.y());
    fTargetPoint = ip;
    if (!ip.isNull() && !buffer().isEmpty() && insertFragment(buffer(), ip))
      regeneratePoints();
    if (!multiInsert()) setStatus(Selectable);
  }
}

QString QFlowChart::fragmentMimeType()
{
  return QStringLiteral("application/x-afce-fragment");
}

bool QFlowChart::insertFragment(const QString &xml, const QInsertionPoint &aPoint)
{
  QBlock *branch = aPoint.branch();
  if (!branch || !branch->isBranch || branch->flowChart() != this)
    return false;
  QDomDocument doc;
  if (!doc.setContent(xml) || !isValidDocument(doc))
    return false;
  int index = aPoint.index();
  if (index < 0 || index > branch->items.size())
    index = int(branch->items.size());
  const qsizetype before = branch->items.size();
  makeUndo();
  branch->insertXmlTree(index, doc.firstChildElement(QStringLiteral("algorithm")));
  // select the (first) inserted block
  fActiveBlock = branch->items.size() > before ? branch->items.value(index, nullptr) : nullptr;
  realignObjects();
  emit changed();
  update();
  return true;
}

void QFlowChart::updateExternalTarget(const QPoint &pos)
{
  fExternalPos = pos;
  const QInsertionPoint ip = getNearistPoint(pos.x(), pos.y());
  const double reach = qMax(48.0, 64 * zoom());
  fTargetPoint = (!ip.isNull() && calcLength(ip.point(), QPointF(pos)) <= reach * reach) ? ip : QInsertionPoint();
  update();
}

void QFlowChart::endExternalDrag()
{
  fExternalDrag = false;
  fAutoScrollTimer->stop();
  fTargetPoint = QInsertionPoint();
  if (status() != Insertion)
    insertionPoints.clear();
  update();
}

void QFlowChart::autoScrollStep()
{
  if (!fExternalDrag)
  {
    fAutoScrollTimer->stop();
    return;
  }
  QAbstractScrollArea *area = nullptr;
  for (QWidget *w = parentWidget(); w && !area; w = w->parentWidget())
    area = qobject_cast<QAbstractScrollArea *>(w);
  if (!area)
    return;
  QWidget *viewport = area->viewport();
  const QPoint vp = viewport->mapFromGlobal(QCursor::pos());
  constexpr int margin = 36;
  auto step = [](int pos, int size) {
    if (pos < margin)
      return -qMax(2, (margin - pos) / 2);
    if (pos > size - margin)
      return qMax(2, (pos - (size - margin)) / 2);
    return 0;
  };
  const int dx = (vp.x() >= -margin && vp.x() <= viewport->width() + margin) ? step(vp.x(), viewport->width()) : 0;
  const int dy = (vp.y() >= -margin && vp.y() <= viewport->height() + margin) ? step(vp.y(), viewport->height()) : 0;
  if (dx == 0 && dy == 0)
    return;
  QScrollBar *h = area->horizontalScrollBar();
  QScrollBar *v = area->verticalScrollBar();
  const int oldH = h->value(), oldV = v->value();
  h->setValue(oldH + dx);
  v->setValue(oldV + dy);
  if (h->value() != oldH || v->value() != oldV)
    updateExternalTarget(mapFromGlobal(QCursor::pos()));
}

void QFlowChart::dragEnterEvent(QDragEnterEvent *event)
{
  const QMimeData *mime = event->mimeData();
  if (status() == Display || !mime || !mime->hasFormat(fragmentMimeType())
      || !isInsertableBuffer(QString::fromUtf8(mime->data(fragmentMimeType()))))
  {
    event->ignore();
    return;
  }
  if (status() == Insertion)
    setStatus(Selectable);
  fExternalDrag = true;
  setHoverBlock(nullptr);
  regeneratePoints();
  event->setDropAction(Qt::CopyAction);
  event->accept();
  updateExternalTarget(event->position().toPoint());
  fAutoScrollTimer->start();
}

void QFlowChart::dragMoveEvent(QDragMoveEvent *event)
{
  if (!fExternalDrag)
  {
    event->ignore();
    return;
  }
  updateExternalTarget(event->position().toPoint());
  event->setDropAction(Qt::CopyAction);
  event->accept();
}

void QFlowChart::dragLeaveEvent(QDragLeaveEvent *event)
{
  if (fExternalDrag)
    endExternalDrag();
  QWidget::dragLeaveEvent(event);
}

void QFlowChart::dropEvent(QDropEvent *event)
{
  if (!fExternalDrag)
  {
    event->ignore();
    return;
  }
  updateExternalTarget(event->position().toPoint());
  const QInsertionPoint target = fTargetPoint;
  const QString xml = QString::fromUtf8(event->mimeData()->data(fragmentMimeType()));
  endExternalDrag();
  if (!target.isNull() && insertFragment(xml, target))
  {
    event->setDropAction(Qt::CopyAction);
    event->accept();
    setFocus(Qt::MouseFocusReason);
  }
  else
  {
    event->ignore();
  }
}

void QFlowChart::mouseReleaseEvent(QMouseEvent *event)
{
  if (fDragging)
  {
    const QPoint mp = event->position().toPoint();
    const QInsertionPoint target = getNearistPoint(mp.x(), mp.y());
    const bool copy = fDragCopy || (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) != 0;
    const double reach = qMax(48.0, 64 * zoom());
    QBlock *block = fDragBlock;
    cancelDrag();
    fDragBlock = nullptr;
    if (!target.isNull() && calcLength(target.point(), QPointF(mp)) <= reach * reach)
      dropBlock(block, target, copy);
    setHoverBlock(pickBlock(root(), mp));
  }
  fDragBlock = nullptr;
  QWidget::mouseReleaseEvent(event);
}

void QFlowChart::mouseDoubleClickEvent(QMouseEvent * event)
{
  if (fDragging)
    cancelDrag();
  fDragBlock = nullptr;
  if(status() == Selectable && event->modifiers() == Qt::NoModifier)
  {
    const QPoint mp = event->position().toPoint();
    // the start terminator stands for the algorithm (name, parameters)
    QBlock *block = pickBlock(root(), mp);
    if (block)
    {
      emit changed();
      emit editBlock(block);
    }
  }
}

void QFlowChart::mouseMoveEvent(QMouseEvent *pEvent)
{
  const QPoint mp = pEvent->position().toPoint();
  if(status() == Insertion)
  {
    QInsertionPoint ip = getNearistPoint(mp.x(), mp.y());
    fTargetPoint = ip;
    update();
    return;
  }
  if (status() != Selectable)
    return;
  if (fDragBlock)
  {
    if (!fDragging && (mp - fPressPos).manhattanLength() >= QApplication::startDragDistance())
      startDrag();
    if (fDragging)
    {
      updateDragCursor(pEvent->modifiers());
      const QInsertionPoint ip = getNearistPoint(mp.x(), mp.y());
      const double reach = qMax(48.0, 64 * zoom());
      fTargetPoint = (!ip.isNull() && calcLength(ip.point(), QPointF(mp)) <= reach * reach) ? ip : QInsertionPoint();
      update();
      return;
    }
  }
  setHoverBlock(pickBlock(root(), mp));
}

void QFlowChart::keyPressEvent(QKeyEvent *event)
{
  if (event->key() == Qt::Key_Escape)
  {
    if (fDragging)
    {
      cancelDrag();
      fDragBlock = nullptr;
      event->accept();
      return;
    }
    if (status() == Insertion)
    {
      setStatus(Selectable);
      event->accept();
      return;
    }
  }
  if (fDragging)
    updateDragCursor(event->modifiers());
  QWidget::keyPressEvent(event);
}

void QFlowChart::keyReleaseEvent(QKeyEvent *event)
{
  if (fDragging)
    updateDragCursor(event->modifiers());
  QWidget::keyReleaseEvent(event);
}

void QFlowChart::leaveEvent(QEvent *event)
{
  setHoverBlock(nullptr);
  QWidget::leaveEvent(event);
}

void QFlowChart::changeEvent(QEvent *event)
{
  // the captions (BEGIN, END, Yes, No, ...) are translated when laying out
  if (event->type() == QEvent::LanguageChange)
    realignObjects();
  QWidget::changeEvent(event);
}

QSize QFlowChart::sizeHint() const
{
  if (root())
  {
    return QSize(qCeil(root()->width), qCeil(root()->height));
  }
  else return QSize();

}

void QFlowChart::deleteBlock(QBlock *aBlock)
{
  if (!aBlock)
    return;
  // a deleted block clears the selection itself if it (or one of its items) was selected
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
  // a dragged block cannot be dropped into itself
  if (fDragging && aBlock == fDragBlock)
    return;
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
  // the font and the assignment symbol change the block sizes
  realignObjects();
}

void QFlowChart::drawBottomArrow(QPainter *canvas, const QPointF & aPoint, const QSizeF & aSize)
{
  const QPolygonF arrow({QPointF(aPoint.x() - aSize.width() / 2.0, aPoint.y() - aSize.height()), aPoint,
                         QPointF(aPoint.x() + aSize.width() / 2.0, aPoint.y() - aSize.height())});
  canvas->save();
  canvas->setBrush(canvas->pen().color());
  canvas->setPen(Qt::NoPen);
  canvas->drawPolygon(arrow);
  canvas->restore();
}

QString QFlowChart::toString()
{
  return document().toString(2);
}

void QFlowChart::fromString(const QString & str)
{
  QDomDocument doc;
  if(doc.setContent(str))
  {
    fActiveBlock = nullptr;
    root()->setXmlNode(doc.firstChildElement(QStringLiteral("algorithm")));
    realignObjects();
    emit changed();
  }
}

void QFlowChart::drawRightArrow(QPainter *canvas, const QPointF & aPoint, const QSizeF & aSize)
{
  const QPolygonF arrow({QPointF(aPoint.x() - aSize.width(), aPoint.y() - aSize.height() / 2), aPoint,
                         QPointF(aPoint.x() - aSize.width(), aPoint.y() + aSize.height() / 2)});
  canvas->save();
  canvas->setBrush(canvas->pen().color());
  canvas->setPen(Qt::NoPen);
  canvas->drawPolygon(arrow);
  canvas->restore();
}


/******************************** QBlock ***********************************/


QBlock::QBlock()
  : QBlock(QString())
{
}

QBlock::QBlock(const QString &aType)
  : fFlowChart(nullptr), fLineHeight(0), fLabelLineHeight(0), fLabelZone(0), fGap(0),
    x(0), y(0), width(0), height(0), parent(nullptr), isBranch(false),
    topMargin(0), bottomMargin(0), leftMargin(0), rightMargin(0)
{
    if (!aType.isEmpty())
    {
      setType(aType);
      isBranch = (aType == QLatin1String("branch"));
    }
}

QBlock::~QBlock()
{
  if (fFlowChart)
  {
    // never leave a dangling selection, hover or drag behind
    if (fFlowChart->fActiveBlock == this)
      fFlowChart->fActiveBlock = nullptr;
    if (fFlowChart->fHoverBlock == this)
      fFlowChart->fHoverBlock = nullptr;
    if (fFlowChart->fDragBlock == this)
    {
      fFlowChart->fDragBlock = nullptr;
      fFlowChart->fDragging = false;
      fFlowChart->insertionPoints.clear();
      fFlowChart->fTargetPoint = QInsertionPoint();
    }
  }
  if (parent)
  {
    parent->items.removeAll(this);
    parent = nullptr;
  }
  const QList<QBlock *> children = items;
  items.clear();
  for (QBlock *child : children)
  {
    child->parent = nullptr;
    delete child;
  }
}

bool QBlock::isKnownType(const QString &aType)
{
  static const QStringList known = {
    QStringLiteral("algorithm"), QStringLiteral("branch"), QStringLiteral("process"),
    QStringLiteral("assign"), QStringLiteral("io"), QStringLiteral("ou"), QStringLiteral("if"),
    QStringLiteral("pre"), QStringLiteral("post"), QStringLiteral("for"), QStringLiteral("forc"),
    QStringLiteral("foreach"), QStringLiteral("case"), QStringLiteral("call"), QStringLiteral("return"),
    QStringLiteral("break"), QStringLiteral("continue")
  };
  return known.contains(aType);
}

int QBlock::requiredBranchCount(const QString &aType)
{
  if (aType == QLatin1String("if") || aType == QLatin1String("case"))
    return 2;
  if (aType == QLatin1String("algorithm") || aType == QLatin1String("pre") ||
      aType == QLatin1String("post") || aType == QLatin1String("for") ||
      aType == QLatin1String("forc") || aType == QLatin1String("foreach"))
    return 1;
  return 0;
}

bool QBlock::isJumpType(const QString &aType)
{
  return aType == QLatin1String("return") || aType == QLatin1String("break") || aType == QLatin1String("continue");
}

QFlowChartStyle::Category QBlock::category(const QString &aType)
{
  switch (shapeOf(aType))
  {
    case Shape::Terminator:
      return aType == QLatin1String("return") ? QFlowChartStyle::Jump : QFlowChartStyle::Terminator;
    case Shape::Parallelogram:
      return QFlowChartStyle::InputOutput;
    case Shape::Diamond:
      return QFlowChartStyle::Decision;
    case Shape::Hexagon:
      return QFlowChartStyle::Loop;
    case Shape::JumpLeft:
    case Shape::JumpRight:
      return QFlowChartStyle::Jump;
    default:
      return QFlowChartStyle::Action;
  }
}

bool QBlock::isWellFormed() const
{
  if (isBranch)
    return true;
  const QString t = type();
  if (!isKnownType(t))
    return false;
  for (const QBlock *child : items)
  {
    if (!child->isBranch)
      return false;
  }
  const int required = requiredBranchCount(t);
  if (t == QLatin1String("case") || t == QLatin1String("algorithm"))
    return items.size() >= required;
  return items.size() == required;
}

bool QBlock::terminates() const
{
  const QString t = type();
  if (isJumpType(t))
    return true;
  if (isBranch)
    return !items.isEmpty() && items.last()->terminates();
  if (!isWellFormed())
    return false;
  if (t == QLatin1String("if") || t == QLatin1String("case"))
  {
    for (const QBlock *branch : items)
    {
      if (!branch->terminates())
        return false;
    }
    return true;
  }
  if (t == QLatin1String("algorithm"))
    return !items.isEmpty() && items.first()->terminates();
  return false;
}

void QBlock::makeBackwardCompatibility() {

    // it supports obsoletted attributes t1, t2, ..., t8
    // and converts to attribute vars with comma delemited values
    // versions before 0.9.7
    if(type() == QLatin1String("io") || type() == QLatin1String("ou")) {
        QStringList sl;
        for(int i = 1; i <= 8; ++i) {
            QString attr = QStringLiteral("t%1").arg(i);
            if(!attributes.value(attr).isEmpty())
                sl << attributes.value(attr);
            attributes.remove(attr);
        }
        if(!sl.empty()) {
            QStringList vars = afce::splitList(attributes.value(QStringLiteral("vars")));
            vars << sl;
            attributes.insert(QStringLiteral("vars"), afce::joinList(vars));
        }
    }

    if(type() == QLatin1String("algorithm")) {
        attributes.insert(QStringLiteral("version"), QStringLiteral(AFC_VERSION));
    }

    // malformed or hand-written files: add the branches the renderer relies on
    // (a case gets its default branch)
    const int required = requiredBranchCount(type());
    if (required > 0) {
        int branches = 0;
        for (const QBlock *child : std::as_const(items)) {
            if (child->isBranch)
                ++branches;
        }
        for (; branches < required; ++branches) {
            append(new QBlock(QStringLiteral("branch")));
        }
    }

    for(int i = 0; i < items.size(); ++i) {
        items.at(i)->makeBackwardCompatibility();
    }
}

QBlock * QBlock::root()
{
  if (parent == nullptr)
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

  if(parent == nullptr) return -1;
  else return parent->items.indexOf(this);
}

void QBlock::insert(int newIndex, QBlock *aBlock)
{
  if (!aBlock || aBlock == this)
    return;
  if (aBlock->parent != nullptr)
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
  if (!aBlock)
    return;
  items.removeAll(aBlock);
  aBlock->parent = nullptr;
  aBlock->setFlowChart(nullptr);
}

void QBlock::append(QBlock *aBlock)
{
  insert(-1, aBlock);
}

void QBlock::deleteObject(int aIndex)
{
  if (aIndex < 0 || aIndex >= items.size())
    return;
  delete item(aIndex); // the destructor removes it from items
}


void QBlock::setItem(int aIndex, QBlock *aBlock)
{
  if(aBlock && items.size() > aIndex && aIndex >= 0)
  {
    QBlock *old = item(aIndex);
    if (old == aBlock)
      return;
    if (aBlock->parent != nullptr)
    {
      aBlock->parent->remove(aBlock);
      aIndex = items.indexOf(old); // removing aBlock may have shifted the items
    }
    items.replace(aIndex, aBlock);
    aBlock->parent = this;
    aBlock->setFlowChart(flowChart());
    old->parent = nullptr;
    delete old;
  }
}

void QBlock::setFlowChart(QFlowChart * aFlowChart)
{
  if (fFlowChart && fFlowChart != aFlowChart)
  {
    // a block leaving a chart is no longer selected, hovered or dragged there
    if (fFlowChart->fActiveBlock == this)
      fFlowChart->fActiveBlock = nullptr;
    if (fFlowChart->fHoverBlock == this)
      fFlowChart->fHoverBlock = nullptr;
    if (fFlowChart->fDragBlock == this && !fFlowChart->fDragging)
      fFlowChart->fDragBlock = nullptr;
  }
  fFlowChart = aFlowChart;
  for (QBlock *child : std::as_const(items))
    child->setFlowChart(aFlowChart);
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

QString QBlock::branchLabel(int aIndex) const
{
  if (aIndex < 0 || aIndex >= items.size())
    return QString();
  if (aIndex == items.size() - 1)
    return tr("otherwise");
  return afce::splitList(item(aIndex)->attributes.value(QStringLiteral("value"))).join(QStringLiteral(", "));
}

QString QBlock::symbolText() const
{
  const QString t = type();
  auto attr = [this](const char *name) { return attributes.value(QLatin1String(name)); };
  if (!isWellFormed())
    return QStringLiteral("<%1>").arg(t);
  if (t == QLatin1String("process") || t == QLatin1String("call"))
    return attr("text");
  if (t == QLatin1String("assign"))
  {
    const QString symbol = flowChart() ? flowChart()->chartStyle().assignSymbol() : QStringLiteral(":=");
    return QStringLiteral("%1 %2 %3").arg(attr("dest"), symbol, attr("src"));
  }
  if (t == QLatin1String("io") || t == QLatin1String("ou"))
  {
    const QString list = afce::splitList(attr("vars")).join(QStringLiteral(", "));
    if (t == QLatin1String("io"))
      return list.isEmpty() ? tr("Input") : tr("Input: %1").arg(list);
    return list.isEmpty() ? tr("Output") : tr("Output: %1").arg(list);
  }
  if (t == QLatin1String("if") || t == QLatin1String("pre") || t == QLatin1String("post"))
  {
    const QString cond = attr("cond").trimmed();
    return cond.endsWith(QLatin1Char('?')) ? cond : cond + QLatin1Char('?');
  }
  if (t == QLatin1String("case"))
    return attr("expr").trimmed();
  if (t == QLatin1String("for"))
  {
    const QString symbol = flowChart() ? flowChart()->chartStyle().assignSymbol() : QStringLiteral(":=");
    return QStringLiteral("%1 %2 %3 … %4").arg(attr("var").trimmed(), symbol, attr("from").trimmed(),
                                                   attr("to").trimmed());
  }
  if (t == QLatin1String("forc"))
  {
    const QString init = attr("init").trimmed(), cond = attr("cond").trimmed(), step = attr("step").trimmed();
    if (init.isEmpty() && cond.isEmpty() && step.isEmpty())
      return QStringLiteral("for (;;)");
    return QStringLiteral("%1; %2; %3").arg(init, cond, step).trimmed();
  }
  if (t == QLatin1String("foreach"))
    return tr("%1 in %2").arg(attr("var").trimmed(), attr("range").trimmed());
  if (t == QLatin1String("return"))
  {
    const QString value = attr("value").trimmed();
    return value.isEmpty() ? tr("return") : tr("return %1").arg(value);
  }
  if (t == QLatin1String("break"))
    return tr("break");
  if (t == QLatin1String("continue"))
    return tr("continue");
  if (t == QLatin1String("algorithm"))
  {
    const QString name = attr("name").trimmed();
    if (name.isEmpty())
      return tr("BEGIN");
    return QStringLiteral("%1(%2)").arg(name, attr("params").trimmed());
  }
  return QStringLiteral("<%1>").arg(t);
}

void QBlock::adjustSize(const double aZoom)
{
  const double z = aZoom;
  fGap = 0;
  fLines.clear();
  fTextSize = QSizeF();
  fShapeSize = QSizeF();
  fEndLines.clear();
  fEndTextSize = QSizeF();
  fEndShapeSize = QSizeF();
  fLabels.clear();
  fLabelSizes.clear();
  fLabelZone = 0;
  topMargin = bottomMargin = leftMargin = rightMargin = 0;

  for (QBlock *child : std::as_const(items))
    child->adjustSize(aZoom);

  if (isBranch)
  {
    double clientWidth = 0, clientHeight = 0;
    for (const QBlock *child : std::as_const(items))
    {
      clientWidth = qMax(clientWidth, child->width);
      clientHeight += child->height;
    }
    width = qMax(clientWidth, kMinBranchW * z);
    height = qMax(clientHeight, kMinBranchH * z);
    return;
  }

  const QString family = styleFamily(flowChart());
  const QFont font = chartFont(family, kFontPx, 1, measureDevice());
  const QFontMetricsF fm(font, measureDevice());
  fLineHeight = fm.lineSpacing();
  const QString t = type();
  const Shape shape = isWellFormed() ? shapeOf(t) : Shape::Generic;

  auto layout = [&](const QString &text, double maxWidth) {
    const TextLayout tl = layoutText(text, fm, maxWidth);
    fLines = tl.lines;
    fTextSize = QSizeF(tl.width, tl.height);
  };
  auto maxChildHeight = [this]() {
    double h = 0;
    for (const QBlock *child : std::as_const(items))
      h = qMax(h, child->height);
    return h;
  };

  if (shape == Shape::Generic)
  {
    /* unknown (or malformed) element: a generic box with the element name,
       its children (if any) side by side below it */
    layout(symbolText(), kWrapRect);
    fShapeSize = shapeSizeFor(Shape::Generic, fTextSize.width(), fTextSize.height());
    topMargin = kInLine * z;
    if (items.isEmpty())
    {
      bottomMargin = kOutLine * z;
      leftMargin = rightMargin = kSide * z;
      width = (fShapeSize.width() + 2 * kSide) * z;
      height = (kInLine + fShapeSize.height() + kOutLine) * z;
      return;
    }
    fGap = kCaseGap * z;
    double client = 0;
    for (const QBlock *child : std::as_const(items))
      client += child->width;
    client += fGap * (items.size() - 1);
    topMargin = (kInLine + fShapeSize.height() + 2 * kBranchDrop) * z;
    bottomMargin = kCollector * z;
    width = qMax(client, fShapeSize.width() * z) + 2 * kSide * z;
    leftMargin = rightMargin = (width - client) / 2;
    height = topMargin + maxChildHeight() + bottomMargin;
    return;
  }

  if (t == QLatin1String("algorithm"))
  {
    layout(symbolText(), kWrapRect);
    fShapeSize = shapeSizeFor(Shape::Terminator, fTextSize.width(), fTextSize.height());
    const TextLayout end = layoutText(tr("END"), fm, kWrapRect);
    fEndLines = end.lines;
    fEndTextSize = QSizeF(end.width, end.height);
    fEndShapeSize = shapeSizeFor(Shape::Terminator, end.width, end.height);
    const QBlock *body = item(0);
    // extra bodies of malformed files are placed to the right of the first one
    fGap = kSide * z;
    double client = 0;
    for (const QBlock *child : std::as_const(items))
      client += child->width;
    client += fGap * (items.size() - 1);
    const double termHalf = (qMax(fShapeSize.width(), fEndShapeSize.width()) / 2 + kSide) * z;
    leftMargin = qMax(kSide * z, termHalf - body->width / 2);
    rightMargin = qMax(kSide * z, body->width / 2 + termHalf - client);
    topMargin = (kTopPad + fShapeSize.height()) * z;
    bottomMargin = terminates() ? kTopPad * z : (kInLine + fEndShapeSize.height() + kTopPad) * z;
    width = leftMargin + client + rightMargin;
    height = topMargin + maxChildHeight() + bottomMargin;
    return;
  }

  if (items.isEmpty())
  {
    // simple blocks: the symbol with the incoming line above it
    double wrap = kWrapRect;
    QString text = symbolText();
    if (t == QLatin1String("forc") && fm.horizontalAdvance(text) > kWrapHexagon)
      text.replace(QStringLiteral("; "), QStringLiteral(";\n"));
    layout(text, wrap);
    fShapeSize = shapeSizeFor(shape, fTextSize.width(), fTextSize.height());
    topMargin = kInLine * z;
    bottomMargin = kOutLine * z;
    leftMargin = rightMargin = kSide * z;
    width = (fShapeSize.width() + 2 * kSide) * z;
    height = (kInLine + fShapeSize.height() + kOutLine) * z;
    return;
  }

  const QString yes = tr("Yes");
  const QString no = tr("No");

  if (t == QLatin1String("if"))
  {
    layout(symbolText(), kWrapDiamond);
    fShapeSize = shapeSizeFor(Shape::Diamond, fTextSize.width(), fTextSize.height());
    const QBlock *left = item(0);
    const QBlock *right = item(1);
    const double a = fShapeSize.width() / 2 * z;
    // the branch lines leave the diamond vertices outwards, with room for Yes / No
    const double dl = qMax(22.0, labelWidth(family, yes) + 12) * z;
    const double dr = qMax(22.0, labelWidth(family, no) + 12) * z;
    fGap = qMax(0.0, qMax(2 * (a + dl) - right->width, 2 * (a + dr) - left->width));
    topMargin = (kInLine + fShapeSize.height() + kBranchDrop) * z;
    bottomMargin = (terminates() ? 4 : kCollector) * z;
    width = left->width + fGap + right->width;
    height = topMargin + maxChildHeight() + bottomMargin;
    return;
  }

  if (t == QLatin1String("case"))
  {
    layout(symbolText(), kWrapDiamond);
    fShapeSize = shapeSizeFor(Shape::Diamond, fTextSize.width(), fTextSize.height());
    const QFont labelFont = chartFont(family, kLabelPx, 1, measureDevice());
    const QFontMetricsF lfm(labelFont, measureDevice());
    fLabelLineHeight = lfm.lineSpacing();
    double labelHeight = 0;
    fGap = kCaseGap * z;
    double client = 0;
    for (int i = 0; i < items.size(); ++i)
    {
      const TextLayout label = layoutText(branchLabel(i), lfm, kWrapLabel);
      fLabels << label.lines;
      fLabelSizes << QSizeF(label.width, label.height);
      labelHeight = qMax(labelHeight, label.height);
      // the label is written right of the branch line
      QBlock *branch = item(i);
      branch->width = qMax(branch->width, 2 * (label.width + 10) * z);
      client += branch->width;
    }
    client += fGap * (items.size() - 1);
    fLabelZone = labelHeight + 6;
    topMargin = (kInLine + fShapeSize.height() + kBranchDrop + fLabelZone) * z;
    bottomMargin = (terminates() ? 4 : kCollector) * z;
    width = qMax(client, (fShapeSize.width() + 2 * kSide) * z);
    leftMargin = rightMargin = (width - client) / 2;
    height = topMargin + maxChildHeight() + bottomMargin;
    return;
  }

  // loops: one body; the return line runs on the left, the exit line on the right
  const QBlock *body = item(0);
  double half = body->width / 2;
  if (t == QLatin1String("pre"))
  {
    layout(symbolText(), kWrapDiamond);
    fShapeSize = shapeSizeFor(Shape::Diamond, fTextSize.width(), fTextSize.height());
    half = qMax(half, (fShapeSize.width() / 2 + labelWidth(family, no) + 14) * z);
    topMargin = (kLoopTop + fShapeSize.height() + 4) * z;
    bottomMargin = kLoopBottom * z;
  }
  else if (t == QLatin1String("post"))
  {
    layout(symbolText(), kWrapDiamond);
    fShapeSize = shapeSizeFor(Shape::Diamond, fTextSize.width(), fTextSize.height());
    half = qMax(half, (fShapeSize.width() / 2 + labelWidth(family, yes) + 14) * z);
    topMargin = kPostTop * z;
    bottomMargin = (kInLine + fShapeSize.height() + kInLine) * z;
  }
  else
  {
    QString text = symbolText();
    if (t == QLatin1String("forc") && fm.horizontalAdvance(text) > kWrapHexagon)
      text.replace(QStringLiteral("; "), QStringLiteral(";\n"));
    layout(text, kWrapHexagon);
    fShapeSize = shapeSizeFor(Shape::Hexagon, fTextSize.width(), fTextSize.height());
    half = qMax(half, (fShapeSize.width() / 2 + 16) * z);
    topMargin = (kInLine + fShapeSize.height() + 4) * z;
    bottomMargin = kLoopBottom * z;
  }
  width = 2 * half + 2 * kLoopSide * z;
  leftMargin = rightMargin = (width - body->width) / 2;
  height = topMargin + body->height + bottomMargin;
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
      cx += item(i)->width + fGap;
    }
  }
}

QRectF QBlock::symbolRect() const
{
  if (isBranch)
    return QRectF();
  const double z = zoom();
  const double w = fShapeSize.width() * z;
  const double h = fShapeSize.height() * z;
  const QString t = type();
  const bool wellFormed = isWellFormed();
  if (wellFormed && t == QLatin1String("algorithm"))
  {
    const double cx = items.isEmpty() ? x + width / 2 : item(0)->x + item(0)->width / 2;
    return QRectF(cx - w / 2, y + kTopPad * z, w, h);
  }
  double top = y + kInLine * z;
  if (wellFormed && t == QLatin1String("pre"))
    top = y + kLoopTop * z;
  else if (wellFormed && t == QLatin1String("post") && !items.isEmpty())
    top = item(0)->y + item(0)->height + kInLine * z;
  return QRectF(x + width / 2 - w / 2, top, w, h);
}

QPainterPath QBlock::symbolPath() const
{
  if (isBranch)
    return QPainterPath();
  const Shape shape = isWellFormed() ? shapeOf(type()) : Shape::Generic;
  const bool rounded = flowChart() && !flowChart()->chartStyle().monochrome();
  return shapePath(shape, symbolRect(), zoom(), rounded);
}

QRectF QBlock::textRect() const
{
  if (isBranch)
    return QRectF();
  const double z = zoom();
  const QRectF symbol = symbolRect();
  const Shape shape = isWellFormed() ? shapeOf(type()) : Shape::Generic;
  const QSizeF size = fTextSize * z;
  const QPointF center(symbol.center().x() + textShift(shape, symbol.height(), z), symbol.center().y());
  return QRectF(center.x() - size.width() / 2, center.y() - size.height() / 2, size.width(), size.height());
}

QRectF QBlock::highlightRect() const
{
  const double z = zoom();
  const QString t = type();
  if (!isBranch && (items.isEmpty() || (t == QLatin1String("algorithm") && flowChart()
                                        && flowChart()->activeBlock() != this)))
    return symbolRect().adjusted(-6 * z, -6 * z, 6 * z, 6 * z);
  double top = y + 3 * z;
  if (!isBranch && t != QLatin1String("post") && t != QLatin1String("algorithm"))
    top = y + kInLine * z / 2;
  return QRectF(x + 3 * z, top, width - 6 * z, y + height - 3 * z - top);
}

void QBlock::paintHighlight(QPainter *canvas, bool selected) const
{
  const QFlowChartStyle st = flowChart()->chartStyle();
  const double z = zoom();
  QColor fill = st.selectedBackground();
  fill.setAlpha(selected ? 38 : 20);
  QColor outline = st.selectedBackground();
  outline.setAlpha(selected ? 200 : 90);
  canvas->save();
  canvas->setPen(QPen(outline, qMax(1.0, 1.25 * z)));
  canvas->setBrush(fill);
  const double radius = 7 * z;
  canvas->drawRoundedRect(highlightRect(), radius, radius);
  canvas->restore();
}

void QBlock::drawCaption(QPainter *canvas, const QRectF & rect, const double zoomFactor, const QString & text)
{
  const QFont font = chartFont(QString(), kFontPx, zoomFactor, canvas->device());
  QFontMetricsF fontMetrics(font);
  QRectF textRect = fontMetrics.boundingRect(text);
  double tx = rect.x() + rect.width() / 2 - textRect.width() / 2;
  double ty = rect.y() + rect.height() / 2 + fontMetrics.ascent() / 2;
  canvas->setFont(font);
  canvas->drawText(QPointF(tx, ty), text);

}

QFont QBlock::defaultFont()
{
  return defaultChartFont();
}

void QBlock::drawSymbol(QPainter *canvas, const QString &aType, const QRectF &rect,
                        const QFlowChartStyle &style, double lineWidth)
{
  const Shape shape = shapeOf(aType);
  const QColor fill = style.monochrome() ? style.normalBackground() : style.fillColor(category(aType));
  const QPen pen(style.normalForeground(), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  canvas->save();
  canvas->setRenderHint(QPainter::Antialiasing, true);
  paintShape(canvas, shape, rect, 1, fill, pen, !style.monochrome());
  canvas->restore();
}

void QBlock::paint(QPainter *canvas, bool fontSizeInPoints) const
{
  Q_UNUSED(fontSizeInPoints);
  if (!flowChart())
    return;
  const QFlowChart *chart = flowChart();
  const QFlowChartStyle st = chart->chartStyle();
  const double z = zoom();
  const double hc = x + width / 2;
  const double bottom = y + height;
  const double lw = qMax(0.05, st.lineWidth() * z);
  const QColor ink = st.normalForeground();
  const QColor accent = st.selectedBackground();
  const bool selected = chart->status() == QFlowChart::Selectable && chart->activeBlock() == this;
  const bool hovered = chart->status() == QFlowChart::Selectable && !selected && !chart->isDragging()
                       && chart->hoverBlock() == this;
  if (selected || hovered)
    paintHighlight(canvas, selected);

  const QString family = st.fontFamily();
  const QFont textFont = chartFont(family, kFontPx, z, canvas->device());
  const QFont labelFont = chartFont(family, kLabelPx, z, canvas->device());
  QColor labelColor = ink;
  labelColor.setAlphaF(0.85f);
  const QPen linePen(ink, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  const QPen shapePen(selected ? accent : ink, selected ? lw * 1.5 : lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
  const bool rounded = !st.monochrome();
  const QString t = type();
  const bool wellFormed = isWellFormed();
  const Shape shape = wellFormed ? shapeOf(t) : Shape::Generic;

  canvas->save();
  canvas->setPen(linePen);
  canvas->setBrush(Qt::NoBrush);

  auto line = [&](double x1, double y1, double x2, double y2) {
    canvas->drawLine(QLineF(x1, y1, x2, y2));
  };
  auto polyline = [&](const QList<QPointF> &points) {
    canvas->drawPolyline(QPolygonF(points));
  };
  const QPointF down(0, 1), right(1, 0);
  // a line ending with an arrow: the line stops inside the arrow head
  auto arrowLine = [&](const QPointF &from, const QPointF &tip, const QPointF &dir) {
    canvas->drawLine(QLineF(from, tip - dir * (kArrowLen * z * 0.6)));
    paintArrow(canvas, tip, dir, z, ink);
  };
  auto fillOf = [&](const QString &aType) {
    return st.monochrome() ? st.normalBackground() : st.fillColor(category(aType));
  };
  auto drawShape = [&](Shape s, const QRectF &r, const QString &aType) {
    paintShape(canvas, s, r, z, fillOf(aType), shapePen, rounded);
  };
  auto drawText = [&](const QStringList &lines, const QPointF &center, double textWidth) {
    canvas->save();
    canvas->setPen(ink);
    canvas->setFont(textFont);
    paintLines(canvas, lines, center, fLineHeight, textWidth, z);
    canvas->restore();
  };
  auto drawLabel = [&](const QString &text, const QPointF &anchor, bool alignRight) {
    canvas->save();
    canvas->setPen(labelColor);
    canvas->setFont(labelFont);
    QFontMetricsF lfm(chartFont(family, kLabelPx, 1, measureDevice()), measureDevice());
    paintLabel(canvas, QStringList(text), anchor, lfm.lineSpacing(), z, alignRight);
    canvas->restore();
  };

  const double sw = fShapeSize.width() * z;
  const double sh = fShapeSize.height() * z;

  if (isBranch)
  {
    /* an empty branch is a straight line (if and case draw it themselves as
       one line from the fork to the collector: separate segments would show
       darker dots where their round caps overlap) */
    const bool drawnByParent = parent && !parent->isBranch && parent->isWellFormed()
                               && (parent->type() == QLatin1String("if") || parent->type() == QLatin1String("case"));
    if (items.isEmpty() && !drawnByParent)
      line(hc, y, hc, bottom);
  }
  else if (shape == Shape::Generic)
  {
    /* unknown (or malformed) element: a dashed box with the element name */
    const QRectF rect(hc - sw / 2, y + kInLine * z, sw, sh);
    arrowLine(QPointF(hc, y), QPointF(hc, rect.top()), down);
    if (items.isEmpty())
    {
      line(hc, rect.bottom(), hc, bottom);
    }
    else
    {
      const QBlock *first = item(0);
      const QBlock *last = item(items.size() - 1);
      const double busTop = rect.bottom() + kBranchDrop * z;
      const double busBottom = bottom - kCollector * z / 2;
      line(hc, rect.bottom(), hc, busTop);
      line(qMin(hc, first->x + first->width / 2), busTop, qMax(hc, last->x + last->width / 2), busTop);
      line(qMin(hc, first->x + first->width / 2), busBottom, qMax(hc, last->x + last->width / 2), busBottom);
      for (const QBlock *child : std::as_const(items))
      {
        const double cx = child->x + child->width / 2;
        line(cx, busTop, cx, child->y);
        line(cx, child->y + child->height, cx, busBottom);
      }
      line(hc, busBottom, hc, bottom);
    }
    QPen dashed = shapePen;
    dashed.setStyle(Qt::DashLine);
    canvas->save();
    canvas->setPen(dashed);
    canvas->setBrush(st.canvasColor());
    canvas->drawRect(rect);
    canvas->restore();
    drawText(fLines, rect.center(), fTextSize.width());
  }
  else if (t == QLatin1String("algorithm"))
  {
    const QBlock *body = item(0);
    const double bc = body->x + body->width / 2;
    const QRectF start(bc - sw / 2, y + kTopPad * z, sw, sh);
    drawShape(Shape::Terminator, start, t);
    drawText(fLines, start.center(), fTextSize.width());
    if (!terminates())
    {
      const double ew = fEndShapeSize.width() * z;
      const double eh = fEndShapeSize.height() * z;
      const QRectF end(bc - ew / 2, bottom - (kTopPad * z) - eh, ew, eh);
      arrowLine(QPointF(bc, body->y + body->height), QPointF(bc, end.top()), down);
      paintShape(canvas, Shape::Terminator, end, z, fillOf(t), linePen, rounded);
      drawText(fEndLines, end.center(), fEndTextSize.width());
    }
  }
  else if (items.isEmpty())
  {
    /* simple blocks: process, assign, call, io, ou, return, break, continue */
    const QRectF rect(hc - sw / 2, y + kInLine * z, sw, sh);
    arrowLine(QPointF(hc, y), QPointF(hc, rect.top()), down);
    // a jump ends the flow line
    if (!isJumpType(t))
      line(hc, rect.bottom(), hc, bottom);
    drawShape(shape, rect, t);
    drawText(fLines, QPointF(rect.center().x() + textShift(shape, sh, z), rect.center().y()), fTextSize.width());
  }
  else if (t == QLatin1String("if"))
  {
    const QRectF diamond(hc - sw / 2, y + kInLine * z, sw, sh);
    const double midY = diamond.center().y();
    const QBlock *left = item(0);
    const QBlock *right = item(1);
    const double lx = left->x + left->width / 2;
    const double rx = right->x + right->width / 2;
    arrowLine(QPointF(hc, y), QPointF(hc, diamond.top()), down);
    // collector: only the branches that do not end with a jump
    const bool lt = left->terminates();
    const bool rt = right->terminates();
    const double collector = bottom - kCollector * z / 2;
    // an empty branch is one line from the diamond down to the collector
    polyline({QPointF(diamond.left(), midY), QPointF(lx, midY),
              QPointF(lx, left->items.isEmpty() ? collector : left->y)});
    polyline({QPointF(diamond.right(), midY), QPointF(rx, midY),
              QPointF(rx, right->items.isEmpty() ? collector : right->y)});
    if (!lt && !left->items.isEmpty())
      line(lx, left->y + left->height, lx, collector);
    if (!rt && !right->items.isEmpty())
      line(rx, right->y + right->height, rx, collector);
    if (!lt || !rt)
    {
      line(lt ? hc : lx, collector, rt ? hc : rx, collector);
      line(hc, collector, hc, bottom);
    }
    drawShape(Shape::Diamond, diamond, t);
    drawText(fLines, diamond.center(), fTextSize.width());
    drawLabel(tr("Yes"), QPointF(diamond.left() - 4 * z, midY - 2 * z - 15 * z), true);
    drawLabel(tr("No"), QPointF(diamond.right() + 4 * z, midY - 2 * z - 15 * z), false);
  }
  else if (t == QLatin1String("case"))
  {
    const QRectF diamond(hc - sw / 2, y + kInLine * z, sw, sh);
    const double bus = diamond.bottom() + kBranchDrop * z;
    arrowLine(QPointF(hc, y), QPointF(hc, diamond.top()), down);
    line(hc, diamond.bottom(), hc, bus);
    const QBlock *first = item(0);
    const QBlock *last = item(items.size() - 1);
    line(qMin(hc, first->x + first->width / 2), bus, qMax(hc, last->x + last->width / 2), bus);
    const double collector = bottom - kCollector * z / 2;
    double cmin = hc, cmax = hc;
    bool open = false;
    for (const QBlock *branch : std::as_const(items))
    {
      const double cx = branch->x + branch->width / 2;
      if (branch->items.isEmpty())
        line(cx, bus, cx, collector); // an empty branch: one line down to the collector
      else
        line(cx, bus, cx, branch->y);
      if (!branch->terminates())
      {
        open = true;
        cmin = qMin(cmin, cx);
        cmax = qMax(cmax, cx);
        if (!branch->items.isEmpty())
          line(cx, branch->y + branch->height, cx, collector);
      }
    }
    if (open)
    {
      line(cmin, collector, cmax, collector);
      line(hc, collector, hc, bottom);
    }
    drawShape(Shape::Diamond, diamond, t);
    drawText(fLines, diamond.center(), fTextSize.width());
    canvas->save();
    canvas->setPen(labelColor);
    canvas->setFont(labelFont);
    for (int i = 0; i < items.size() && i < fLabels.size(); ++i)
    {
      const QBlock *branch = item(i);
      paintLabel(canvas, fLabels.at(i), QPointF(branch->x + branch->width / 2 + 5 * z, bus + 3 * z),
                 fLabelLineHeight, z, false);
    }
    canvas->restore();
  }
  else if (t == QLatin1String("pre"))
  {
    const QBlock *body = item(0);
    const QRectF diamond(hc - sw / 2, y + kLoopTop * z, sw, sh);
    const double midY = diamond.center().y();
    const double exitX = x + width - kLoopLine * z;
    const double backX = x + kLoopLine * z;
    arrowLine(QPointF(hc, y), QPointF(hc, diamond.top()), down);
    line(hc, diamond.bottom(), hc, body->y);
    // exit (condition false)
    polyline({QPointF(diamond.right(), midY), QPointF(exitX, midY), QPointF(exitX, bottom - 10 * z),
              QPointF(hc, bottom - 10 * z), QPointF(hc, bottom)});
    // return to the condition
    if (!body->terminates())
    {
      polyline({QPointF(hc, body->y + body->height), QPointF(hc, bottom - 24 * z), QPointF(backX, bottom - 24 * z),
                QPointF(backX, y + kLoopJoin * z), QPointF(hc - lw, y + kLoopJoin * z)});
      paintArrow(canvas, QPointF(hc - lw / 2, y + kLoopJoin * z), right, z, ink);
    }
    drawShape(Shape::Diamond, diamond, t);
    drawText(fLines, diamond.center(), fTextSize.width());
    drawLabel(tr("Yes"), QPointF(hc + 5 * z, diamond.bottom() + 1 * z), false);
    drawLabel(tr("No"), QPointF(diamond.right() + 4 * z, midY - 2 * z - 15 * z), false);
  }
  else if (t == QLatin1String("post"))
  {
    const QBlock *body = item(0);
    const QRectF diamond(hc - sw / 2, body->y + body->height + kInLine * z, sw, sh);
    const double midY = diamond.center().y();
    const double backX = x + kLoopLine * z;
    line(hc, y, hc, body->y);
    if (!body->terminates())
      arrowLine(QPointF(hc, body->y + body->height), QPointF(hc, diamond.top()), down);
    // repeat while the condition is true
    polyline({QPointF(diamond.left(), midY), QPointF(backX, midY), QPointF(backX, y + kPostJoin * z),
              QPointF(hc - lw, y + kPostJoin * z)});
    paintArrow(canvas, QPointF(hc - lw / 2, y + kPostJoin * z), right, z, ink);
    line(hc, diamond.bottom(), hc, bottom);
    drawShape(Shape::Diamond, diamond, t);
    drawText(fLines, diamond.center(), fTextSize.width());
    drawLabel(tr("Yes"), QPointF(diamond.left() - 4 * z, midY - 2 * z - 15 * z), true);
    drawLabel(tr("No"), QPointF(hc + 5 * z, diamond.bottom() + 1 * z), false);
  }
  else
  {
    /* for, forc, foreach: the hexagon ("preparation") */
    const QBlock *body = item(0);
    const QRectF hexagon(hc - sw / 2, y + kInLine * z, sw, sh);
    const double midY = hexagon.center().y();
    const double exitX = x + width - kLoopLine * z;
    const double backX = x + kLoopLine * z;
    arrowLine(QPointF(hc, y), QPointF(hc, hexagon.top()), down);
    line(hc, hexagon.bottom(), hc, body->y);
    polyline({QPointF(hexagon.right(), midY), QPointF(exitX, midY), QPointF(exitX, bottom - 10 * z),
              QPointF(hc, bottom - 10 * z), QPointF(hc, bottom)});
    if (!body->terminates())
    {
      polyline({QPointF(hc, body->y + body->height), QPointF(hc, bottom - 24 * z), QPointF(backX, bottom - 24 * z),
                QPointF(backX, midY)});
      arrowLine(QPointF(backX, midY), QPointF(hexagon.left(), midY), right);
    }
    drawShape(Shape::Hexagon, hexagon, t);
    drawText(fLines, hexagon.center(), fTextSize.width());
  }
  canvas->restore();

  for(int i = 0; i < items.size(); ++i)
  {
    item(i)->paint(canvas, fontSizeInPoints);
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
  if (!rect.contains(px, py)) return nullptr;
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
  QStringList sl = attributes.keys();
  sl.sort();
  for (const QString &name : std::as_const(sl))
  {
    if (name != QLatin1String("type"))
    {
      self.setAttribute(name, attributes.value(name));
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
    if(da.name() != QLatin1String("type"))
    {
      attributes.insert(da.name(), da.value());
    }
  }
  isBranch = (type() == QLatin1String("branch"));
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
    QDomElement branch = algorithm.firstChildElement(QStringLiteral("branch"));
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
