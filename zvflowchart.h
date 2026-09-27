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

#include <QDomDocument>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QStack>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "qflowchartstyle.h"

#define AFC_VERSION "1.3"

class QBlock;
class QFlowChart;
class QKeyEvent;
class QPainter;
class QTimer;

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
      fBranch = nullptr;
      fIndex = -1;
    }
    QPointF point() const { return fPoint; }
    QBlock *branch() const { return fBranch; }
    int index() const { return fIndex; }
    void setPoint(const QPointF &aPoint) { fPoint = aPoint; }
    void setBranch(QBlock *aBranch) { fBranch = aBranch; }
    void setIndex(int aIndex) { fIndex = aIndex; }
    bool isNull() const { return fPoint.isNull() && fBranch == nullptr && fIndex == -1; }
};

Q_DECLARE_TYPEINFO(QInsertionPoint, Q_RELOCATABLE_TYPE);


// A node of the flowchart tree (the algorithm, a branch or a block).
//
// Ownership: a block owns its items. Deleting a block detaches it from its
// parent and deletes all of its items (recursively). insert()/append()
// transfer ownership of the inserted block to this block (detaching it from
// its previous parent first); remove() releases ownership to the caller.
class QBlock : public QObject
{
  Q_OBJECT
  private:
    QFlowChart *fFlowChart;
    // Layout computed by adjustSize() at 100 % zoom (chart pixels): the text
    // lines of the symbol, the text box, the symbol size and, for the
    // algorithm, the END terminator and for a case, the branch labels.
    QStringList fLines;
    QSizeF fTextSize;
    QSizeF fShapeSize;
    double fLineHeight;
    QStringList fEndLines;
    QSizeF fEndTextSize;
    QSizeF fEndShapeSize;
    QList<QStringList> fLabels;
    QList<QSizeF> fLabelSizes;
    double fLabelLineHeight;
    double fLabelZone;
    // gap between the branches of an if / case (zoomed)
    double fGap;

    QString symbolText() const;
    void paintHighlight(QPainter *canvas, bool selected) const;
    QRectF highlightRect() const;

  public:
    QBlock();
    explicit QBlock(const QString & aType);
    ~QBlock() override;
    QHash<QString, QString> attributes;
    double x, y, width, height;
    QList<QBlock *> items;
    QBlock *parent;
    bool isBranch;
    QBlock *root();
    QString type() const { return attributes.value(QStringLiteral("type"), QString()); }
    void setType(const QString & newType) { attributes[QStringLiteral("type")] = newType; }
    int index();
    void insert(int newIndex, QBlock *aBlock);
    // Detaches aBlock (which must be an item of this block); the caller becomes its owner.
    void remove(QBlock *aBlock);
    void append(QBlock *aBlock);
    // Deletes the item at aIndex.
    void deleteObject(int aIndex);
    QBlock * item(int aIndex) const { return items.at(aIndex); }
    // Replaces the item at aIndex by aBlock (taking ownership of it) and deletes the old item.
    void setItem(int aIndex, QBlock *aBlock);
    QFlowChart * flowChart() const { return fFlowChart; }
    // Sets the flowchart of this block and of all of its items.
    void setFlowChart(QFlowChart * aFlowChart);
    // Deletes all items and all attributes except the type.
    void clear();
    // Lays out the block and its items: sizes (text measured at 100 % zoom and
    // scaled by aZoom, so that the layout is proportional to the zoom).
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
    // Converts obsolete attributes (io/ou t1..t8), sets the format version and
    // adds missing mandatory branches (e.g. the "else" branch of an if).
    void makeBackwardCompatibility();
    // Block types this version knows how to lay out and draw.
    static bool isKnownType(const QString &aType);
    // Number of branches a block of the given type must have (0 for simple
    // blocks; the minimum for a case, which has 2 or more).
    static int requiredBranchCount(const QString &aType);
    // True for return, break and continue.
    static bool isJumpType(const QString &aType);
    // Fill colour category of a block type.
    static QFlowChartStyle::Category category(const QString &aType);
    // True if the block is a known type with the right number of branches (and
    // no other items); other blocks are drawn as a generic box.
    bool isWellFormed() const;
    // True if the flow does not continue below the block: a jump (return,
    // break, continue), a branch whose last item terminates, an if / case
    // whose branches all terminate, an algorithm whose body terminates.
    bool terminates() const;
    // Rectangle of the symbol (the start terminator for the algorithm) in
    // widget coordinates; empty for branches.
    QRectF symbolRect() const;
    // Outline of the symbol in widget coordinates (empty for branches).
    QPainterPath symbolPath() const;
    // Box of the symbol text in widget coordinates (every text line fits in it).
    QRectF textRect() const;
    // Draws the symbol of a block type (shape only, no text) into rect with the
    // colours of the style: the same shapes as in the chart (tool panel icons).
    static void drawSymbol(QPainter *canvas, const QString &aType, const QRectF &rect,
                           const QFlowChartStyle &style, double lineWidth);
    // Font of the chart texts when the style sets no family: the application
    // font (a common sans-serif font if that family does not exist).
    static QFont defaultFont();
    // Text of the case branch labels (translated "otherwise" for the last one).
    QString branchLabel(int aIndex) const;
};


class QFlowChart : public QWidget
{
  Q_OBJECT
  friend class QBlock;

  protected:
    void paintEvent(QPaintEvent *pEvent) override;
    void mousePressEvent(QMouseEvent *pEvent) override;
    void mouseMoveEvent(QMouseEvent *pEvent) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

  private:
    QBlock *fRoot;
    QBlock *fActiveBlock;
    double fZoom;
    int fStatus;
    QList<QInsertionPoint> insertionPoints;
    QInsertionPoint fTargetPoint;
    QString fBuffer;
    bool fMultiInsert;
    QFlowChartStyle fStyle;
    QStack<QString> undoStack;
    QStack<QString> redoStack;
    // hover and drag and drop of blocks
    QBlock *fHoverBlock;
    QBlock *fDragBlock;     // pressed (and maybe dragged) block
    QPoint fPressPos;
    bool fDragging;
    bool fDragCopy;
    // a fragment dragged from outside (the tool panel): Qt drag and drop
    bool fExternalDrag;
    QPoint fExternalPos;        // last drag position, in chart coordinates
    QTimer *fAutoScrollTimer;   // scrolls the enclosing scroll area near its edges

    void setHoverBlock(QBlock *aBlock);
    void startDrag();
    void cancelDrag();
    void updateDragCursor(Qt::KeyboardModifiers modifiers);
    void updateExternalTarget(const QPoint &pos);
    void endExternalDrag();
    void autoScrollStep();

  public:

    enum {Display, Selectable, Insertion};

    explicit QFlowChart(QWidget *pObj = nullptr);
    ~QFlowChart() override;
    QSize sizeHint() const override;
    QDomDocument document() const;
    QInsertionPoint targetPoint() const { return fTargetPoint; }

    void paintTo(QPainter *canvas);

    // Checks that doc is an AFC flowchart: the root element is <algorithm>
    // and it contains a <branch>. On failure a translated message is stored in errorMessage.
    static bool isValidDocument(const QDomDocument &doc, QString *errorMessage = nullptr);
    // Parses data as an AFC document (see isValidDocument).
    static bool parseDocument(const QByteArray &data, QDomDocument *doc, QString *errorMessage = nullptr);
    // True if aBuffer is an insertable fragment: <algorithm><branch>...</branch></algorithm>.
    static bool isInsertableBuffer(const QString &aBuffer);

    // Replaces the flowchart by doc (validated), clears the selection and the undo history.
    bool loadDocument(const QDomDocument &doc, QString *errorMessage = nullptr);
    // Reads and loads an .afc file.
    bool loadFile(const QString &fileName, QString *errorMessage = nullptr);
    // Writes the flowchart to an .afc file (UTF-8, atomically).
    bool saveFile(const QString &fileName, QString *errorMessage = nullptr) const;

    QBlock * root() const;
    QBlock * activeBlock() const;
    void setActiveBlock(QBlock *aBlock);
    // XML fragment (as used by the clipboard and the insertion buffer) of the active block.
    QString activeBlockXml() const;
    double zoom() const;
    int status() const { return fStatus; }
    void deleteBlock(QBlock *aBlock);
    QInsertionPoint getNearistPoint(int x, int y) const;
    QList<QInsertionPoint> insertionPointList() const { return insertionPoints; }
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
    void clearHistory();
    void makeChanged();
    void makeUndo();
    void makeBackwardCompatibility();

    // The block under the mouse (highlighted in Selectable mode), or nullptr.
    QBlock *hoverBlock() const { return fHoverBlock; }
    // True while a block is dragged with the mouse (insertion points are shown).
    bool isDragging() const { return fDragging; }
    QBlock *draggedBlock() const { return fDragging ? fDragBlock : nullptr; }
    // Moves (or copies) aBlock to the insertion point as one undo step and
    // selects the result. Returns false (nothing changed) if the point is inside
    // the block itself, the move would not change anything or the block cannot
    // be moved (the algorithm or a branch).
    bool dropBlock(QBlock *aBlock, const QInsertionPoint &aPoint, bool copy);
    // Zoom that fits the whole chart into a viewport of the given size
    // (limited to [minZoom, maxZoom]).
    double fitZoom(const QSize &viewport, double minZoom = 0.1, double maxZoom = 4.0) const;

    // MIME type of a flowchart fragment (<algorithm><branch>...</branch></algorithm>)
    // dragged onto the chart, e.g. a block from the tool panel.
    static QString fragmentMimeType();
    // Inserts a fragment at the insertion point as one undo step. Returns false
    // if the fragment is not insertable or the point is not valid.
    bool insertFragment(const QString &xml, const QInsertionPoint &aPoint);
    // True while a fragment from outside is dragged over the chart.
    bool isExternalDragActive() const { return fExternalDrag; }

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
    void zoomToFit(const QSize &viewport);


};

#endif // QFlowChart_H
