/****************************************************************************
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

#include "blockcatalog.h"

#include "qflowchartstyle.h"
#include "zvflowchart.h"

#include <QCoreApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace afce {

namespace {

QString wrap(const QString &xml)
{
    return QStringLiteral("<algorithm><branch>%1</branch></algorithm>").arg(xml);
}

// Paints the tool icon of a block type: the chart symbol plus a hint of its
// connections, in the colours of the chart (dark mode aware).
class BlockIconEngine : public QIconEngine
{
public:
    BlockIconEngine(const QString &type, const QPalette &palette)
        : m_type(type), m_palette(palette)
    {
    }

    QIconEngine *clone() const override { return new BlockIconEngine(m_type, m_palette); }
    QString key() const override { return QStringLiteral("afce-block"); }
    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override { return size; }
    QList<QSize> availableSizes(QIcon::Mode, QIcon::State) override
    {
        return {QSize(16, 16), QSize(24, 24), QSize(32, 32), QSize(48, 48), QSize(64, 64)};
    }
    bool isNull() override { return m_type.isEmpty(); }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        const qreal s = scale > 0 ? scale : 1.0;
        QPixmap pm(QSize(qMax(1, qRound(size.width() * s)), qMax(1, qRound(size.height() * s))));
        pm.setDevicePixelRatio(s);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        painter.end();
        return pm;
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override
    {
        const QPalette::ColorGroup group = mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active;
        QPalette palette = m_palette;
        palette.setCurrentColorGroup(group);
        QFlowChartStyle style = QFlowChartStyle::uiStyle(palette);
        // the icons are shown on the window background
        style.setNormalForeground(palette.color(group, QPalette::WindowText));
        style.setNormalBackground(palette.color(group, QPalette::Base));

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        if (mode == QIcon::Disabled)
            painter->setOpacity(0.45);
        const qreal side = qMin(rect.width(), rect.height());
        const QRectF box(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0, side, side);
        paintIcon(painter, box, style);
        painter->restore();
    }

private:
    void paintIcon(QPainter *p, const QRectF &box, const QFlowChartStyle &style) const
    {
        const qreal s = box.width();
        const qreal lw = qBound(1.0, s / 20.0, 2.2);
        const QColor ink = style.normalForeground();
        const QPen pen(ink, lw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        auto at = [&box, s](qreal fx, qreal fy) { return QPointF(box.left() + fx * s, box.top() + fy * s); };
        auto rect = [&box, s](qreal fx, qreal fy, qreal fw, qreal fh) {
            return QRectF(box.left() + fx * s, box.top() + fy * s, fw * s, fh * s);
        };
        auto line = [p, &pen](const QPointF &a, const QPointF &b) {
            p->setPen(pen);
            p->drawLine(a, b);
        };
        auto arrow = [p, &ink, s](const QPointF &tip, const QPointF &dir) {
            const qreal len = s * 0.16, half = s * 0.09;
            const QPointF base = tip - dir * len;
            const QPointF normal(-dir.y(), dir.x());
            p->setPen(Qt::NoPen);
            p->setBrush(ink);
            p->drawPolygon(QPolygonF({tip, base + normal * half, base - normal * half}));
        };
        auto symbol = [p, &style, lw](const QString &type, const QRectF &r) {
            QBlock::drawSymbol(p, type, r, style, lw);
        };
        auto glyph = [p, &ink, s](const QRectF &r, const QString &text, qreal size) {
            QFont font = QBlock::defaultFont();
            font.setPixelSize(qMax(4, qRound(s * size)));
            font.setBold(true);
            p->setFont(font);
            p->setPen(ink);
            p->drawText(r, Qt::AlignCenter, text);
        };
        const QPointF down(0, 1);

        const QString &t = m_type;
        if (t == QLatin1String("process") || t == QLatin1String("call")) {
            symbol(t, rect(0.06, 0.22, 0.88, 0.56));
        } else if (t == QLatin1String("assign")) {
            const QRectF r = rect(0.06, 0.22, 0.88, 0.56);
            symbol(t, r);
            line(at(0.36, 0.44), at(0.64, 0.44));
            line(at(0.36, 0.56), at(0.64, 0.56));
        } else if (t == QLatin1String("io")) {
            symbol(t, rect(0.04, 0.36, 0.92, 0.5));
            line(at(0.5, 0.04), at(0.5, 0.5));
            arrow(at(0.5, 0.64), down);
        } else if (t == QLatin1String("ou")) {
            symbol(t, rect(0.04, 0.14, 0.92, 0.5));
            line(at(0.5, 0.39), at(0.5, 0.8));
            arrow(at(0.5, 0.97), down);
        } else if (t == QLatin1String("if")) {
            line(at(0.1, 0.36), at(0.1, 0.9));
            line(at(0.9, 0.36), at(0.9, 0.9));
            symbol(t, rect(0.1, 0.08, 0.8, 0.56));
        } else if (t == QLatin1String("case")) {
            line(at(0.5, 0.5), at(0.5, 0.66));
            line(at(0.14, 0.66), at(0.86, 0.66));
            for (qreal fx : {0.14, 0.5, 0.86})
                line(at(fx, 0.66), at(fx, 0.94));
            symbol(t, rect(0.16, 0.04, 0.68, 0.46));
        } else if (t == QLatin1String("pre")) {
            // condition first, the body returns to it
            line(at(0.5, 0.0), at(0.5, 0.24));
            line(at(0.5, 0.66), at(0.5, 0.9));
            line(at(0.5, 0.9), at(0.06, 0.9));
            line(at(0.06, 0.9), at(0.06, 0.1));
            line(at(0.06, 0.1), at(0.36, 0.1));
            arrow(at(0.5, 0.1), QPointF(1, 0));
            symbol(t, rect(0.16, 0.24, 0.68, 0.42));
        } else if (t == QLatin1String("post")) {
            // body first, the condition returns to it
            line(at(0.5, 0.02), at(0.5, 0.3));
            line(at(0.14, 0.72), at(0.04, 0.72));
            line(at(0.04, 0.72), at(0.04, 0.14));
            line(at(0.04, 0.14), at(0.4, 0.14));
            arrow(at(0.5, 0.14), QPointF(1, 0));
            symbol(QStringLiteral("process"), rect(0.3, 0.26, 0.4, 0.18));
            symbol(t, rect(0.14, 0.48, 0.72, 0.48));
        } else if (t == QLatin1String("for") || t == QLatin1String("forc") || t == QLatin1String("foreach")) {
            const QRectF r = rect(0.02, 0.22, 0.96, 0.56);
            symbol(t, r);
            const QString text = t == QLatin1String("for") ? QStringLiteral("1..n")
                               : t == QLatin1String("forc") ? QStringLiteral(";;")
                                                             : QStringLiteral("∈");
            glyph(r, text, t == QLatin1String("for") ? 0.24 : 0.32);
        } else if (t == QLatin1String("return")) {
            symbol(t, rect(0.04, 0.3, 0.92, 0.4));
        } else if (t == QLatin1String("break") || t == QLatin1String("continue")) {
            symbol(t, rect(0.06, 0.26, 0.88, 0.48));
        } else {
            QPen dashed = pen;
            dashed.setStyle(Qt::DashLine);
            p->setPen(dashed);
            p->setBrush(Qt::NoBrush);
            p->drawRect(rect(0.1, 0.25, 0.8, 0.5));
        }
    }

    QString m_type;
    QPalette m_palette;
};

} // namespace

// Translation context of the catalog texts.
class BlockCatalog
{
    Q_DECLARE_TR_FUNCTIONS(afce::BlockCatalog)
public:
    static QList<BlockKind> kinds();
    static QString groupTitle(const QString &group);
};

QList<BlockKind> blockKinds()
{
    return BlockCatalog::kinds();
}

QString blockGroupTitle(const QString &group)
{
    return BlockCatalog::groupTitle(group);
}

QList<BlockKind> BlockCatalog::kinds()
{
    const QString basic = QStringLiteral("basic");
    const QString branching = QStringLiteral("branching");
    const QString loops = QStringLiteral("loops");
    const QString jumps = QStringLiteral("jumps");
    return {
        {QStringLiteral("process"), tr("Process"),
         tr("Action: a statement or a sequence of statements"), basic,
         wrap(QStringLiteral("<process text=\"x = x + 1\"/>"))},
        {QStringLiteral("assign"), tr("Assignment"),
         tr("Assigns the value of an expression to a variable"), basic,
         wrap(QStringLiteral("<assign dest=\"x\" src=\"0\"/>"))},
        {QStringLiteral("io"), tr("Input"),
         tr("Reads the values of variables"), basic,
         wrap(QStringLiteral("<io vars=\"x\"/>"))},
        {QStringLiteral("ou"), tr("Output"),
         tr("Prints the values of expressions and texts"), basic,
         wrap(QStringLiteral("<ou vars=\"x\"/>"))},
        {QStringLiteral("call"), tr("Subroutine call"),
         tr("Calls a predefined process: a function or a procedure"), basic,
         wrap(QStringLiteral("<call text=\"proc()\"/>"))},
        {QStringLiteral("if"), tr("Condition (if)"),
         tr("Two-way branching: if the condition is true, the Yes branch is executed, otherwise the No branch"),
         branching, wrap(QStringLiteral("<if cond=\"x &gt; 0\"><branch/><branch/></if>"))},
        {QStringLiteral("case"), tr("Multiple choice (switch)"),
         tr("Executes the branch whose value matches the expression, otherwise the last branch"), branching,
         wrap(QStringLiteral("<case expr=\"x\"><branch value=\"1\"/><branch value=\"2\"/><branch/></case>"))},
        {QStringLiteral("pre"), tr("While loop"),
         tr("Loop with a pre-condition: repeats the body while the condition is true"), loops,
         wrap(QStringLiteral("<pre cond=\"i &lt; n\"><branch/></pre>"))},
        {QStringLiteral("post"), tr("Do-while loop"),
         tr("Loop with a post-condition: executes the body, then repeats it while the condition is true"), loops,
         wrap(QStringLiteral("<post cond=\"i &lt; n\"><branch/></post>"))},
        {QStringLiteral("for"), tr("Counting loop (for)"),
         tr("The variable runs from the start value to the end value inclusive"), loops,
         wrap(QStringLiteral("<for var=\"i\" from=\"1\" to=\"n\"><branch/></for>"))},
        {QStringLiteral("forc"), tr("C-style for loop"),
         tr("for (initialization; condition; step)"), loops,
         wrap(QStringLiteral("<forc init=\"int i = 0\" cond=\"i &lt; n\" step=\"i++\"><branch/></forc>"))},
        {QStringLiteral("foreach"), tr("For-each loop"),
         tr("Repeats the body for every element of a collection or range"), loops,
         wrap(QStringLiteral("<foreach var=\"x\" range=\"items\"><branch/></foreach>"))},
        {QStringLiteral("return"), tr("Return"),
         tr("Ends the algorithm and returns a value"), jumps,
         wrap(QStringLiteral("<return value=\"0\"/>"))},
        {QStringLiteral("break"), tr("Break"),
         tr("Exits the nearest enclosing loop or multiple choice"), jumps,
         wrap(QStringLiteral("<break/>"))},
        {QStringLiteral("continue"), tr("Continue"),
         tr("Goes to the next iteration of the nearest enclosing loop"), jumps,
         wrap(QStringLiteral("<continue/>"))},
    };
}

QString BlockCatalog::groupTitle(const QString &group)
{
    if (group == QLatin1String("basic"))
        return tr("Basic blocks");
    if (group == QLatin1String("branching"))
        return tr("Branching");
    if (group == QLatin1String("loops"))
        return tr("Loops");
    if (group == QLatin1String("jumps"))
        return tr("Jumps");
    return group;
}

QIcon blockIcon(const QString &type, const QPalette &palette)
{
    return QIcon(new BlockIconEngine(type, palette));
}

} // namespace afce
