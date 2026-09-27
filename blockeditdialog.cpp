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

#include "blockeditdialog.h"

#include "afceutil.h"
#include "zvflowchart.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>

namespace {

QFont codeFont()
{
    return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

QLineEdit *codeLine(const QString &text, const QString &placeholder)
{
    auto *edit = new QLineEdit(text);
    edit->setFont(codeFont());
    edit->setPlaceholderText(placeholder);
    edit->setMinimumWidth(320);
    return edit;
}

// Sets an attribute; an empty value removes an optional attribute.
void setAttribute(QBlock *block, const QString &name, const QString &value, bool removeIfEmpty = false)
{
    if (removeIfEmpty && value.isEmpty())
        block->attributes.remove(name);
    else
        block->attributes.insert(name, value);
}

QStringList nonEmptyLines(const QString &text)
{
    QStringList result;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString item = line.trimmed();
        if (!item.isEmpty())
            result << item;
    }
    return result;
}

QList<QBlock *> branchesOf(const QBlock *block)
{
    QList<QBlock *> result;
    for (QBlock *child : block->items) {
        if (child->isBranch)
            result << child;
    }
    return result;
}

constexpr int kDefaultBranch = -2;
constexpr int kNewBranch = -1;

} // namespace

bool BlockEditDialog::canEdit(const QBlock *block)
{
    if (!block)
        return false;
    if (block->isBranch)
        return block->parent && canEdit(block->parent);
    static const QStringList editable = {
        QStringLiteral("algorithm"), QStringLiteral("process"), QStringLiteral("assign"), QStringLiteral("io"),
        QStringLiteral("ou"), QStringLiteral("call"), QStringLiteral("if"), QStringLiteral("case"),
        QStringLiteral("pre"), QStringLiteral("post"), QStringLiteral("for"), QStringLiteral("forc"),
        QStringLiteral("foreach"), QStringLiteral("return")};
    if (!editable.contains(block->type()))
        return false;
    // a case needs its branches (the last one is the default branch)
    if (block->type() == QLatin1String("case"))
        return branchesOf(block).size() >= 2;
    return true;
}

bool BlockEditDialog::edit(QBlock *block, QWidget *parent)
{
    if (!block)
        return false;
    if (block->isBranch)
        return block->parent ? edit(block->parent, parent) : false;
    if (!canEdit(block))
        return false;

    const QString t = block->type();
    auto attr = [block](const char *name) { return block->attributes.value(QLatin1String(name)); };

    // declared before the dialog: its signal handlers use them until it is destroyed
    std::function<QString()> problem; // a message if the input is not acceptable
    std::function<void()> apply;

    QDialog dlg(parent);
    dlg.setMinimumWidth(460);
    auto *layout = new QVBoxLayout(&dlg);
    auto *intro = new QLabel(&dlg);
    intro->setWordWrap(true);
    intro->hide();
    layout->addWidget(intro);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    layout->addLayout(form);
    auto *warning = new QLabel(&dlg);
    warning->setWordWrap(true);
    QPalette warningPalette = warning->palette();
    warningPalette.setColor(QPalette::WindowText, QColor(0xD9, 0x53, 0x4F));
    warning->setPalette(warningPalette);
    warning->hide();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    QList<QLineEdit *> lineEdits;

    auto setIntro = [intro](const QString &text) {
        intro->setText(text);
        intro->setVisible(!text.isEmpty());
    };
    auto addLine = [&](const QString &label, const QString &text, const QString &placeholder) {
        QLineEdit *edit = codeLine(text, placeholder);
        form->addRow(label, edit);
        lineEdits << edit;
        return edit;
    };

    if (t == QLatin1String("algorithm")) {
        dlg.setWindowTitle(tr("Algorithm Properties"));
        setIntro(tr("The start terminator shows the name and the parameters of the algorithm "
                    "(BEGIN if it has no name). Code generators use them for the function header."));
        QLineEdit *name = addLine(tr("&Name:"), attr("name"), tr("e.g. main"));
        QLineEdit *params = addLine(tr("&Parameters:"), attr("params"), tr("e.g. int a, int b"));
        QLineEdit *returns = addLine(tr("&Return type:"), attr("returns"), tr("e.g. int"));
        apply = [=]() {
            setAttribute(block, QStringLiteral("name"), name->text().trimmed(), true);
            setAttribute(block, QStringLiteral("params"), params->text().trimmed(), true);
            setAttribute(block, QStringLiteral("returns"), returns->text().trimmed(), true);
        };
    } else if (t == QLatin1String("process")) {
        dlg.setWindowTitle(tr("Process"));
        const QString text = attr("text");
        if (text.contains(QLatin1Char('\n'))) {
            auto *edit = new QPlainTextEdit(text);
            edit->setFont(codeFont());
            edit->setTabChangesFocus(true);
            form->addRow(tr("&Action:"), edit);
            apply = [=]() { setAttribute(block, QStringLiteral("text"), edit->toPlainText()); };
        } else {
            QLineEdit *edit = addLine(tr("&Action:"), text, tr("e.g. x = x + 1"));
            apply = [=]() { setAttribute(block, QStringLiteral("text"), edit->text().trimmed()); };
        }
    } else if (t == QLatin1String("call")) {
        dlg.setWindowTitle(tr("Subroutine Call"));
        QLineEdit *edit = addLine(tr("&Call:"), attr("text"), tr("e.g. swap(a, b)"));
        problem = [=]() { return edit->text().trimmed().isEmpty() ? tr("Enter the subroutine call.") : QString(); };
        apply = [=]() { setAttribute(block, QStringLiteral("text"), edit->text().trimmed()); };
    } else if (t == QLatin1String("assign")) {
        dlg.setWindowTitle(tr("Assignment"));
        const QString symbol = block->flowChart() ? block->flowChart()->chartStyle().assignSymbol() : QStringLiteral(":=");
        setIntro(tr("The block shows: variable %1 value").arg(symbol));
        QLineEdit *dest = addLine(tr("&Variable:"), attr("dest"), tr("e.g. s"));
        QLineEdit *src = addLine(tr("&Value:"), attr("src"), tr("e.g. s + i"));
        problem = [=]() {
            if (dest->text().trimmed().isEmpty())
                return tr("Enter the variable that receives the value.");
            if (src->text().trimmed().isEmpty())
                return tr("Enter the value (an expression).");
            return QString();
        };
        apply = [=]() {
            setAttribute(block, QStringLiteral("dest"), dest->text().trimmed());
            setAttribute(block, QStringLiteral("src"), src->text().trimmed());
        };
    } else if (t == QLatin1String("io") || t == QLatin1String("ou")) {
        const bool input = t == QLatin1String("io");
        dlg.setWindowTitle(input ? tr("Input") : tr("Output"));
        setIntro(input ? tr("Variables to read, one per line:")
                       : tr("Expressions and texts to print, one per line. Texts are written in quotes, "
                            "e.g. \"Sum = \". The items are printed one after another, followed by a line break."));
        auto *edit = new QPlainTextEdit(afce::splitList(attr("vars")).join(QLatin1Char('\n')));
        edit->setFont(codeFont());
        edit->setTabChangesFocus(true);
        edit->setMinimumHeight(120);
        layout->insertWidget(layout->indexOf(intro) + 1, edit);
        edit->setFocus();
        problem = [=]() {
            return nonEmptyLines(edit->toPlainText()).isEmpty()
                       ? (input ? tr("Enter at least one variable.") : tr("Enter at least one item."))
                       : QString();
        };
        QObject::connect(edit, &QPlainTextEdit::textChanged, &dlg, [&problem, warning, buttons]() {
            const QString message = problem ? problem() : QString();
            warning->setText(message);
            warning->setVisible(!message.isEmpty());
            buttons->button(QDialogButtonBox::Ok)->setEnabled(message.isEmpty());
        });
        apply = [=]() { setAttribute(block, QStringLiteral("vars"), afce::joinList(nonEmptyLines(edit->toPlainText()))); };
    } else if (t == QLatin1String("if") || t == QLatin1String("pre") || t == QLatin1String("post")) {
        if (t == QLatin1String("if")) {
            dlg.setWindowTitle(tr("Condition"));
            setIntro(tr("If the condition is true, the Yes branch is executed, otherwise the No branch."));
        } else if (t == QLatin1String("pre")) {
            dlg.setWindowTitle(tr("While Loop"));
            setIntro(tr("The body is repeated while the condition is true (the condition is checked first)."));
        } else {
            dlg.setWindowTitle(tr("Do-While Loop"));
            setIntro(tr("The body is executed, then repeated while the condition is true."));
        }
        QLineEdit *cond = addLine(tr("&Condition:"), attr("cond"), tr("e.g. x > 0"));
        problem = [=]() { return cond->text().trimmed().isEmpty() ? tr("The condition is empty.") : QString(); };
        apply = [=]() { setAttribute(block, QStringLiteral("cond"), cond->text().trimmed()); };
    } else if (t == QLatin1String("for")) {
        dlg.setWindowTitle(tr("Counting Loop"));
        setIntro(tr("The variable runs from the start value to the end value (inclusive), step 1."));
        QLineEdit *var = addLine(tr("&Variable:"), attr("var"), tr("e.g. i"));
        QLineEdit *from = addLine(tr("&From:"), attr("from"), tr("e.g. 1"));
        QLineEdit *to = addLine(tr("&To:"), attr("to"), tr("e.g. n"));
        problem = [=]() {
            if (var->text().trimmed().isEmpty())
                return tr("Enter the loop variable.");
            if (from->text().trimmed().isEmpty() || to->text().trimmed().isEmpty())
                return tr("Enter the start and the end value.");
            return QString();
        };
        apply = [=]() {
            setAttribute(block, QStringLiteral("var"), var->text().trimmed());
            setAttribute(block, QStringLiteral("from"), from->text().trimmed());
            setAttribute(block, QStringLiteral("to"), to->text().trimmed());
        };
    } else if (t == QLatin1String("forc")) {
        dlg.setWindowTitle(tr("C-Style For Loop"));
        setIntro(tr("for (initialization; condition; step). Every part may be empty; "
                    "without a condition the loop repeats until a break or return."));
        QLineEdit *init = addLine(tr("&Initialization:"), attr("init"), tr("e.g. int i = 0"));
        QLineEdit *cond = addLine(tr("&Condition:"), attr("cond"), tr("e.g. i < n"));
        QLineEdit *step = addLine(tr("&Step:"), attr("step"), tr("e.g. i++"));
        apply = [=]() {
            setAttribute(block, QStringLiteral("init"), init->text().trimmed());
            setAttribute(block, QStringLiteral("cond"), cond->text().trimmed());
            setAttribute(block, QStringLiteral("step"), step->text().trimmed());
        };
    } else if (t == QLatin1String("foreach")) {
        dlg.setWindowTitle(tr("For-Each Loop"));
        setIntro(tr("The body is executed for every element of the collection or range."));
        QLineEdit *var = addLine(tr("&Element:"), attr("var"), tr("e.g. x"));
        QLineEdit *range = addLine(tr("&Collection:"), attr("range"), tr("e.g. items"));
        problem = [=]() {
            if (var->text().trimmed().isEmpty())
                return tr("Enter the element variable.");
            if (range->text().trimmed().isEmpty())
                return tr("Enter the collection or range.");
            return QString();
        };
        apply = [=]() {
            setAttribute(block, QStringLiteral("var"), var->text().trimmed());
            setAttribute(block, QStringLiteral("range"), range->text().trimmed());
        };
    } else if (t == QLatin1String("return")) {
        dlg.setWindowTitle(tr("Return"));
        setIntro(tr("Ends the algorithm. The value is optional."));
        QLineEdit *value = addLine(tr("&Value:"), attr("value"), tr("e.g. 0 (optional)"));
        apply = [=]() { setAttribute(block, QStringLiteral("value"), value->text().trimmed(), true); };
    } else if (t == QLatin1String("case")) {
        dlg.setWindowTitle(tr("Multiple Choice"));
        setIntro(tr("The branch whose values contain the value of the expression is executed; "
                    "if none matches, the last branch (otherwise). Several values of a branch are "
                    "separated by commas."));
        QLineEdit *expr = addLine(tr("&Expression:"), attr("expr"), tr("e.g. x"));
        const QList<QBlock *> branches = branchesOf(block);

        auto *list = new QListWidget();
        list->setFont(codeFont());
        list->setMinimumHeight(170);
        list->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                              | QAbstractItemView::SelectedClicked);
        for (int i = 0; i + 1 < branches.size(); ++i) {
            auto *item = new QListWidgetItem(afce::splitList(branches.at(i)->attributes.value(QStringLiteral("value")))
                                                 .join(QStringLiteral(", ")));
            item->setData(Qt::UserRole, i);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            list->addItem(item);
        }
        auto *otherwise = new QListWidgetItem(tr("otherwise (the default branch, always last)"));
        otherwise->setData(Qt::UserRole, kDefaultBranch);
        otherwise->setFlags(Qt::ItemIsEnabled);
        QFont italic = list->font();
        italic.setItalic(true);
        otherwise->setFont(italic);
        otherwise->setForeground(list->palette().brush(QPalette::Disabled, QPalette::Text));
        list->addItem(otherwise);

        auto *add = new QPushButton(tr("&Add"));
        auto *remove = new QPushButton(tr("&Remove"));
        auto *up = new QPushButton(tr("Move &Up"));
        auto *down = new QPushButton(tr("Move &Down"));
        auto *buttonColumn = new QVBoxLayout();
        for (QPushButton *button : {add, remove, up, down})
            buttonColumn->addWidget(button);
        buttonColumn->addStretch();
        auto *row = new QHBoxLayout();
        row->addWidget(list, 1);
        row->addLayout(buttonColumn);
        auto *branchesLabel = new QLabel(tr("&Branch values (double-click to edit):"));
        branchesLabel->setBuddy(list);
        layout->addWidget(branchesLabel);
        layout->addLayout(row);

        auto valueCount = [list]() { return list->count() - 1; };
        auto updateButtons = [=]() {
            const int current = list->currentRow();
            const bool value = current >= 0 && current < valueCount();
            remove->setEnabled(value && valueCount() > 1);
            up->setEnabled(value && current > 0);
            down->setEnabled(value && current < valueCount() - 1);
        };
        QObject::connect(list, &QListWidget::currentRowChanged, &dlg, updateButtons);
        QObject::connect(add, &QPushButton::clicked, &dlg, [=]() {
            const int current = list->currentRow();
            const int at = (current >= 0 && current < valueCount()) ? current + 1 : valueCount();
            auto *item = new QListWidgetItem(QString());
            item->setData(Qt::UserRole, kNewBranch);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            list->insertItem(at, item);
            list->setCurrentItem(item);
            list->editItem(item);
            updateButtons();
        });
        QObject::connect(remove, &QPushButton::clicked, &dlg, [=, &dlg]() {
            const int current = list->currentRow();
            if (current < 0 || current >= valueCount() || valueCount() <= 1)
                return;
            QListWidgetItem *item = list->item(current);
            const int index = item->data(Qt::UserRole).toInt();
            if (index >= 0 && index < branches.size() && !branches.at(index)->items.isEmpty()) {
                const QString name = item->text().isEmpty() ? tr("(empty value)") : item->text();
                if (QMessageBox::question(&dlg, tr("Remove Branch"),
                                          tr("The branch %1 contains blocks. Remove it together with its blocks?").arg(name),
                                          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
                    return;
            }
            delete list->takeItem(current);
            list->setCurrentRow(qMin(current, valueCount() - 1));
            updateButtons();
        });
        auto move = [=](int delta) {
            const int current = list->currentRow();
            const int target = current + delta;
            if (current < 0 || current >= valueCount() || target < 0 || target >= valueCount())
                return;
            QListWidgetItem *item = list->takeItem(current);
            list->insertItem(target, item);
            list->setCurrentRow(target);
            updateButtons();
        };
        QObject::connect(up, &QPushButton::clicked, &dlg, [=]() { move(-1); });
        QObject::connect(down, &QPushButton::clicked, &dlg, [=]() { move(1); });
        QObject::connect(list, &QListWidget::itemChanged, &dlg, [&problem, warning, buttons]() {
            const QString message = problem ? problem() : QString();
            warning->setText(message);
            warning->setVisible(!message.isEmpty());
            buttons->button(QDialogButtonBox::Ok)->setEnabled(message.isEmpty());
        });
        QObject::connect(list->model(), &QAbstractItemModel::rowsRemoved, &dlg, [&problem, warning, buttons]() {
            const QString message = problem ? problem() : QString();
            warning->setText(message);
            warning->setVisible(!message.isEmpty());
            buttons->button(QDialogButtonBox::Ok)->setEnabled(message.isEmpty());
        });
        list->setCurrentRow(0);
        updateButtons();

        problem = [=]() {
            if (expr->text().trimmed().isEmpty())
                return tr("The expression is empty.");
            for (int i = 0; i < valueCount(); ++i) {
                if (afce::splitList(list->item(i)->text()).isEmpty())
                    return tr("Every branch except the last one needs a value.");
            }
            return QString();
        };
        apply = [=]() {
            setAttribute(block, QStringLiteral("expr"), expr->text().trimmed());
            QList<QBlock *> order;
            for (int i = 0; i < valueCount(); ++i) {
                QListWidgetItem *item = list->item(i);
                const int index = item->data(Qt::UserRole).toInt();
                QBlock *branch = (index >= 0 && index < branches.size()) ? branches.at(index)
                                                                         : new QBlock(QStringLiteral("branch"));
                branch->attributes.insert(QStringLiteral("value"), afce::joinList(afce::splitList(item->text())));
                order << branch;
            }
            order << branches.last();
            for (QBlock *branch : branches) {
                if (!order.contains(branch))
                    delete branch;
            }
            // re-append in the new order (this also adopts the new branches)
            for (QBlock *branch : std::as_const(order))
                block->append(branch);
        };
    }

    auto validate = [&problem, warning, buttons]() {
        const QString message = problem ? problem() : QString();
        warning->setText(message);
        warning->setVisible(!message.isEmpty());
        buttons->button(QDialogButtonBox::Ok)->setEnabled(message.isEmpty());
    };
    for (QLineEdit *edit : std::as_const(lineEdits))
        QObject::connect(edit, &QLineEdit::textChanged, &dlg, validate);
    if (!lineEdits.isEmpty()) {
        lineEdits.first()->setFocus();
        lineEdits.first()->selectAll();
    }

    layout->addWidget(warning);
    layout->addStretch();
    layout->addWidget(buttons);
    validate();

    if (!apply || dlg.exec() != QDialog::Accepted)
        return false;

    QFlowChart *chart = block->flowChart();
    if (chart)
        chart->makeUndo();
    apply();
    if (chart) {
        chart->realignObjects();
        chart->makeChanged();
        chart->update();
    }
    return true;
}
