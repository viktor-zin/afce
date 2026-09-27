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

#include "codeeditor.h"

#include <QEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QSet>
#include <QTextBlock>
#include <QToolTip>
#include <QUrl>
#include <QVariantAnimation>
#include <QWidget>

namespace {

bool isDarkPalette(const QPalette &palette)
{
    return palette.color(QPalette::Base).lightness() < 128;
}

QColor mixColors(const QColor &a, const QColor &b, double t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

QColor severityColor(CodeEditor::LineMarker::Severity severity, bool dark)
{
    switch (severity) {
    case CodeEditor::LineMarker::Error:
        return dark ? QColor(0xf1, 0x6a, 0x6a) : QColor(0xd3, 0x2f, 0x2f);
    case CodeEditor::LineMarker::Warning:
        return dark ? QColor(0xe8, 0xb3, 0x39) : QColor(0xe0, 0x8e, 0x00);
    case CodeEditor::LineMarker::Info:
        break;
    }
    return dark ? QColor(0x6c, 0xa8, 0xf0) : QColor(0x1e, 0x73, 0xd8);
}

class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor) : QWidget(editor), fEditor(editor) {}
    QSize sizeHint() const override { return QSize(fEditor->lineNumberAreaWidth(), 0); }

protected:
    void paintEvent(QPaintEvent *event) override { fEditor->lineNumberAreaPaintEvent(event); }
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ToolTip) {
            QHelpEvent *help = static_cast<QHelpEvent *>(event);
            const QString tip = fEditor->lineNumberAreaToolTip(help->pos());
            if (tip.isEmpty())
                QToolTip::hideText();
            else
                QToolTip::showText(help->globalPos(), tip, this);
            return true;
        }
        return QWidget::event(event);
    }

private:
    CodeEditor *fEditor;
};

} // namespace

// ---------------------------------------------------------------------------
// CodeEditor

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent), fLineNumberArea(new LineNumberArea(this)), fLineNumbersVisible(true),
      fCurrentLineHighlight(true), fWasEmpty(true), fFlashLine(0), fFlashAlpha(0), fFlash(new QVariantAnimation(this))
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(4 * QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')));
    setFrameShape(QFrame::StyledPanel);

    fFlash->setStartValue(150);
    fFlash->setEndValue(0);
    fFlash->setDuration(1800);
    fFlash->setEasingCurve(QEasingCurve::InQuad);
    connect(fFlash, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        fFlashAlpha = value.toInt();
        updateExtraSelections();
    });
    connect(fFlash, &QVariantAnimation::finished, this, [this]() {
        fFlashLine = 0;
        updateExtraSelections();
    });

    connect(this, &QPlainTextEdit::blockCountChanged, this, [this]() { updateLineNumberAreaWidth(); });
    connect(this, &QPlainTextEdit::textChanged, this, [this]() {
        if (document()->isEmpty() != fWasEmpty) {
            fWasEmpty = document()->isEmpty();
            updateExtraSelections();
        }
    });
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        updateExtraSelections();
        fLineNumberArea->update();
    });
    updateLineNumberAreaWidth();
    updateExtraSelections();
}

void CodeEditor::setLineNumbersVisible(bool visible)
{
    if (fLineNumbersVisible == visible)
        return;
    fLineNumbersVisible = visible;
    fLineNumberArea->setVisible(visible);
    updateLineNumberAreaWidth();
}

bool CodeEditor::lineNumbersVisible() const
{
    return fLineNumbersVisible;
}

void CodeEditor::setCurrentLineHighlight(bool enabled)
{
    fCurrentLineHighlight = enabled;
    updateExtraSelections();
}

void CodeEditor::highlightLine(int line)
{
    const QTextBlock block = document()->findBlockByNumber(line - 1);
    if (!block.isValid())
        return;
    QTextCursor cursor(block);
    // put the cursor on the first non-blank character of the line
    const QString text = block.text();
    int column = 0;
    while (column < text.size() && text.at(column).isSpace())
        ++column;
    cursor.setPosition(block.position() + column);
    setTextCursor(cursor);
    centerCursor();
    fFlashLine = line;
    fFlash->stop();
    fFlashAlpha = fFlash->startValue().toInt();
    fFlash->start();
    updateExtraSelections();
}

void CodeEditor::scrollToLine(int line)
{
    const QTextBlock block = document()->findBlockByNumber(qMax(0, line - 1));
    if (!block.isValid())
        return;
    // the vertical scroll bar of QPlainTextEdit counts lines; keep two lines of context
    verticalScrollBar()->setValue(qMax(0, block.firstLineNumber() - 2));
}

QColor CodeEditor::markerColor(LineMarker::Severity severity, const QPalette &palette)
{
    return severityColor(severity, isDarkPalette(palette));
}

void CodeEditor::setLineMarkers(const QList<LineMarker> &markers)
{
    fMarkers = markers;
    updateLineNumberAreaWidth();
    fLineNumberArea->update();
}

int CodeEditor::lineNumberAreaWidth() const
{
    if (!fLineNumbersVisible)
        return 0;
    int digits = 2;
    for (int max = qMax(1, blockCount()); max >= 100; max /= 10)
        ++digits;
    const QFontMetrics fm(font());
    const int markerSpace = fm.height() / 2 + 6;
    return markerSpace + fm.horizontalAdvance(QLatin1Char('9')) * digits + 8;
}

void CodeEditor::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    const QRect cr = contentsRect();
    fLineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        fLineNumberArea->scroll(0, dy);
    else
        fLineNumberArea->update(0, rect.y(), fLineNumberArea->width(), rect.height());
    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth();
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect cr = contentsRect();
    fLineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::changeEvent(QEvent *event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        applyPalette();
    else if (event->type() == QEvent::FontChange) {
        setTabStopDistance(4 * QFontMetricsF(font()).horizontalAdvance(QLatin1Char(' ')));
        updateLineNumberAreaWidth();
    }
}

void CodeEditor::applyPalette()
{
    const QList<CodeHighlighter *> highlighters = document()->findChildren<CodeHighlighter *>();
    for (CodeHighlighter *h : highlighters)
        h->setPalette(palette());
    updateExtraSelections();
    fLineNumberArea->update();
}

void CodeEditor::updateExtraSelections()
{
    QList<QTextEdit::ExtraSelection> selections;
    const QPalette pal = palette();
    const bool dark = isDarkPalette(pal);
    const QColor base = pal.color(QPalette::Base);
    // (QPlainTextEdit shows the placeholder text only when there are no extra selections)
    if (fCurrentLineHighlight && !textCursor().hasSelection() && !document()->isEmpty()) {
        QTextEdit::ExtraSelection current;
        current.format.setBackground(mixColors(base, pal.color(QPalette::Highlight), dark ? 0.16 : 0.09));
        current.format.setProperty(QTextFormat::FullWidthSelection, true);
        current.cursor = textCursor();
        current.cursor.clearSelection();
        selections.append(current);
    }
    if (fFlashLine > 0 && fFlashAlpha > 0) {
        const QTextBlock block = document()->findBlockByNumber(fFlashLine - 1);
        if (block.isValid()) {
            QTextEdit::ExtraSelection flash;
            QColor color = dark ? QColor(0xd9, 0xa4, 0x1a) : QColor(0xff, 0xc8, 0x3d);
            color.setAlpha(fFlashAlpha);
            flash.format.setBackground(color);
            flash.format.setProperty(QTextFormat::FullWidthSelection, true);
            flash.cursor = QTextCursor(block);
            selections.append(flash);
        }
    }
    setExtraSelections(selections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(fLineNumberArea);
    const QPalette pal = palette();
    const bool dark = isDarkPalette(pal);
    const QColor base = pal.color(QPalette::Base);
    const QColor text = pal.color(QPalette::Text);
    painter.fillRect(event->rect(), mixColors(base, text, dark ? 0.04 : 0.035));
    painter.setPen(mixColors(base, text, 0.12));
    painter.drawLine(fLineNumberArea->width() - 1, event->rect().top(), fLineNumberArea->width() - 1,
                     event->rect().bottom());
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor number = mixColors(base, text, 0.45);
    const QColor currentNumber = mixColors(base, text, 0.85);
    const int currentLine = textCursor().blockNumber();
    const QFontMetrics fm(font());
    const int markerSize = qMax(6, fm.height() / 2);
    painter.setFont(font());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(blockNumber == currentLine ? currentNumber : number);
            painter.drawText(0, top, fLineNumberArea->width() - 6, fm.height(), Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(blockNumber + 1));
            // the most severe marker of this line
            int severity = -1;
            for (const LineMarker &m : std::as_const(fMarkers))
                if (m.line == blockNumber + 1)
                    severity = qMax(severity, int(m.severity));
            if (severity >= 0) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(severityColor(LineMarker::Severity(severity), dark));
                painter.drawEllipse(QRectF(4, top + (fm.height() - markerSize) / 2.0, markerSize, markerSize));
            }
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

QString CodeEditor::lineNumberAreaToolTip(const QPoint &pos) const
{
    const QTextCursor cursor = cursorForPosition(QPoint(0, pos.y()));
    const int line = cursor.blockNumber() + 1;
    QStringList tips;
    for (const LineMarker &m : fMarkers)
        if (m.line == line && !m.toolTip.isEmpty())
            tips.append(m.toolTip);
    return tips.join(QLatin1Char('\n'));
}

QString CodeEditor::droppedFile(const QMimeData *source)
{
    if (!source || !source->hasUrls())
        return QString();
    const QList<QUrl> urls = source->urls();
    for (const QUrl &url : urls)
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile())
            return url.toLocalFile();
    return QString();
}

bool CodeEditor::canInsertFromMimeData(const QMimeData *source) const
{
    if (!droppedFile(source).isEmpty())
        return true;
    return QPlainTextEdit::canInsertFromMimeData(source);
}

void CodeEditor::insertFromMimeData(const QMimeData *source)
{
    const QString file = droppedFile(source);
    if (!file.isEmpty()) {
        emit fileDropped(file);
        return;
    }
    QPlainTextEdit::insertFromMimeData(source);
}

// ---------------------------------------------------------------------------
// CodeHighlighter

struct CodeHighlighter::Spec
{
    QSet<QString> keywords;
    QSet<QString> types;
    QSet<QString> builtins;
    bool caseSensitive = true;
    QStringList lineComments;                   // "//", "#", "'", ";", "|"
    bool remComments = false;                   // BASIC: REM starts a comment
    QList<QPair<QString, QString>> blockComments; // state = index + 1
    QString quotes = QStringLiteral("\"'");
    bool backslashEscapes = true;               // "a\"b"
    bool tripleQuotes = false;                  // Python """...""" (states 100, 101)
    bool backtickStrings = false;               // JS template literals (state 102)
    bool preprocessor = false;                  // '#' directive at the start of a line
    QString sigils;                             // variable prefixes: $ @ %
};

namespace {

enum { StateTripleDouble = 100, StateTripleSingle = 101, StateBacktick = 102 };

QSet<QString> words(const char *list, bool lower = false)
{
    QSet<QString> result;
    const QStringList items = QString::fromUtf8(list).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &item : items)
        result.insert(lower ? item.toLower() : item);
    return result;
}

CodeHighlighter::Spec *makeSpec(const QString &language)
{
    auto *s = new CodeHighlighter::Spec;
    const QString lang = language.toLower();
    const char *cKeywords = "auto break case const continue default do else enum extern for goto if inline register "
                            "restrict return sizeof static struct switch typedef union volatile while _Bool _Static_assert "
                            "_Noreturn _Alignas _Alignof _Thread_local NULL true false";
    const char *cTypes = "void char short int long float double signed unsigned bool size_t ssize_t ptrdiff_t wchar_t FILE "
                         "int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t";
    const char *cBuiltins = "printf scanf puts gets getchar putchar fprintf fscanf fgets fputs sprintf sscanf malloc free "
                            "strlen strcpy strcmp strcat memset memcpy abs fabs sqrt pow exit stdin stdout stderr";
    if (lang == QLatin1String("c") || lang == QLatin1String("cpp") || lang == QLatin1String("c++")) {
        s->keywords = words(cKeywords);
        s->types = words(cTypes);
        s->builtins = words(cBuiltins);
        s->lineComments = {QStringLiteral("//")};
        s->blockComments = {{QStringLiteral("/*"), QStringLiteral("*/")}};
        s->preprocessor = true;
        if (lang != QLatin1String("c")) {
            s->keywords += words("alignas alignof and and_eq asm bitand bitor catch class compl concept consteval constexpr "
                                 "constinit const_cast co_await co_return co_yield decltype delete dynamic_cast explicit "
                                 "export friend mutable namespace new noexcept not not_eq nullptr operator or or_eq "
                                 "private protected public reinterpret_cast requires static_assert static_cast template "
                                 "this thread_local throw try typeid typename using virtual xor xor_eq override final");
            s->types += words("char8_t char16_t char32_t string wstring string_view vector array map unordered_map set "
                              "unordered_set list deque pair tuple optional std");
            s->builtins += words("cin cout cerr clog endl getline");
        }
    } else if (lang == QLatin1String("py")) {
        s->keywords = words("False None True and as assert async await break class continue def del elif else except "
                            "finally for from global if import in is lambda nonlocal not or pass raise return try while "
                            "with yield match case");
        s->types = words("int float str bool list dict set tuple bytes object");
        s->builtins = words("print input len range abs min max sum round open enumerate zip map filter sorted reversed "
                            "divmod pow type isinstance");
        s->lineComments = {QStringLiteral("#")};
        s->tripleQuotes = true;
    } else if (lang == QLatin1String("pas")) {
        s->caseSensitive = false;
        s->keywords = words("and array as begin case const div do downto else end file for function goto if "
                            "implementation in inherited interface label mod nil not of or packed procedure program record "
                            "repeat set shl shr then to type unit until uses var while with xor break continue exit true "
                            "false", true);
        s->types = words("integer real boolean char string byte word longint shortint cardinal int64 double single "
                         "extended text", true);
        s->builtins = words("write writeln read readln length inc dec ord chr sqr sqrt abs trunc round random randomize",
                            true);
        s->lineComments = {QStringLiteral("//")};
        s->blockComments = {{QStringLiteral("{"), QStringLiteral("}")}, {QStringLiteral("(*"), QStringLiteral("*)")}};
        s->quotes = QStringLiteral("'");
        s->backslashEscapes = false;
    } else if (lang == QLatin1String("js")) {
        s->keywords = words("break case catch class const continue debugger default delete do else export extends "
                            "finally for function if import in instanceof let new return super switch this throw try "
                            "typeof var void while with yield async await of null undefined true false NaN Infinity");
        s->builtins = words("console log prompt alert Math parseInt parseFloat Number String Array Object JSON process "
                            "require");
        s->lineComments = {QStringLiteral("//")};
        s->blockComments = {{QStringLiteral("/*"), QStringLiteral("*/")}};
        s->backtickStrings = true;
    } else if (lang == QLatin1String("php")) {
        s->caseSensitive = false;
        s->keywords = words("abstract and array as break callable case catch class clone const continue declare default "
                            "do echo else elseif empty enddeclare endfor endforeach endif endswitch endwhile extends final "
                            "finally fn for foreach function global goto if implements include include_once instanceof "
                            "insteadof interface isset list match namespace new or print private protected public readonly "
                            "require require_once return static switch throw trait try unset use var while xor yield true "
                            "false null", true);
        s->types = words("int float string bool void mixed", true);
        s->builtins = words("fgets fscanf printf intval floatval strlen count trim explode implode readline stdin", true);
        s->lineComments = {QStringLiteral("//"), QStringLiteral("#")};
        s->blockComments = {{QStringLiteral("/*"), QStringLiteral("*/")}};
        s->preprocessor = true; // <?php
        s->sigils = QStringLiteral("$");
    } else if (lang == QLatin1String("perl")) {
        s->keywords = words("my our local sub if elsif else unless while until for foreach do last next redo return and "
                            "or not xor eq ne lt gt le ge cmp use no package require");
        s->builtins = words("print printf say chomp chop defined length scalar push pop shift unshift split join keys "
                            "values int abs sqrt undef STDIN STDOUT");
        s->lineComments = {QStringLiteral("#")};
        s->sigils = QStringLiteral("$@%");
    } else if (lang == QLatin1String("ruby")) {
        s->keywords = words("alias and begin break case class def do else elsif end ensure false for if in module next "
                            "nil not or redo rescue retry return self super then true undef unless until when while yield "
                            "loop");
        s->builtins = words("puts print gets p to_i to_f to_s chomp require Integer Float String");
        s->lineComments = {QStringLiteral("#")};
        s->sigils = QStringLiteral("@$");
    } else if (lang == QLatin1String("vbs")) {
        s->caseSensitive = false;
        s->keywords = words("and as byref byval call case class const dim do each else elseif empty end erase error exit "
                            "explicit false for function get if in is let loop mod new next not nothing null on option or "
                            "preserve private property public redim resume select set step sub then to true until wend "
                            "while with xor", true);
        s->builtins = words("msgbox inputbox wscript echo stdout stdin readline writeline cint clng cdbl cstr len mid",
                            true);
        s->lineComments = {QStringLiteral("'")};
        s->remComments = true;
        s->quotes = QStringLiteral("\"");
        s->backslashEscapes = false;
    } else if (lang == QLatin1String("autoit")) {
        s->caseSensitive = false;
        s->keywords = words("and byref case const continuecase continueloop default dim do else elseif endfunc endif "
                            "endselect endswitch endwith enum exit exitloop false for func global if in local next not "
                            "null or redim return select static step switch then to true until volatile wend while with",
                            true);
        s->builtins = words("consolewrite consoleread msgbox inputbox number string stringlen int", true);
        s->lineComments = {QStringLiteral(";")};
        s->blockComments = {{QStringLiteral("#cs"), QStringLiteral("#ce")}};
        s->backslashEscapes = false;
        s->preprocessor = true;
        s->sigils = QStringLiteral("$@");
    } else if (lang == QLatin1String("bas256")) {
        s->caseSensitive = false;
        s->keywords = words("if then else end endif while endwhile do until for to step next print input function "
                            "endfunction subroutine endsubroutine call return exit global dim redim and or not xor true "
                            "false case begin endcase continue goto gosub", true);
        s->builtins = words("int float string length mid abs sqrt rand", true);
        s->lineComments = {QStringLiteral("#")};
        s->remComments = true;
        s->quotes = QStringLiteral("\"");
        s->backslashEscapes = false;
    } else if (lang == QLatin1String("freebasic")) {
        s->caseSensitive = false;
        s->keywords = words("and andalso as byref byval case cast const continue declare dim do else elseif end endif "
                            "enum exit false for function goto if input is let loop mod new next not or orelse print "
                            "return select shared static step sub then to true type until var wend while xor", true);
        s->types = words("integer long single double string boolean byte ubyte short ushort uinteger ulong longint "
                         "ulongint zstring", true);
        s->builtins = words("len mid str val abs sqr int", true);
        s->lineComments = {QStringLiteral("'")};
        s->remComments = true;
        s->blockComments = {{QStringLiteral("/'"), QStringLiteral("'/")}};
        s->quotes = QStringLiteral("\"");
        s->backslashEscapes = false;
    } else if (lang == QLatin1String("e87")) {
        s->caseSensitive = false;
        s->keywords = words("алг нач кон нц кц кц_при если то иначе все выбор при пока для от до шаг раз ввод вывод "
                            "выход нс арг рез аргрез знач утв дано надо и или не да нет исп кон_исп использовать", true);
        s->types = words("цел вещ лог сим лит таб целтаб вещтаб логтаб симтаб литтаб", true);
        s->lineComments = {QStringLiteral("|")};
        s->backslashEscapes = false;
    } else {
        // unknown language: strings, numbers and C-like comments only
        s->lineComments = {QStringLiteral("//"), QStringLiteral("#")};
        s->blockComments = {{QStringLiteral("/*"), QStringLiteral("*/")}};
    }
    return s;
}

bool isIdentifierStart(QChar c)
{
    return c.isLetter() || c == QLatin1Char('_');
}

bool isIdentifierChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

bool startsAt(const QString &text, int pos, const QString &what, bool caseSensitive = true)
{
    return QStringView(text).mid(pos).startsWith(what, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
}

} // namespace

CodeHighlighter::CodeHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document), fSpec(makeSpec(QString())), fDark(false)
{
    // the palette of the editor showing the document (if any), else the application palette
    QPalette palette = QGuiApplication::palette();
    for (QObject *o = document ? document->parent() : nullptr; o; o = o->parent()) {
        if (o->isWidgetType()) {
            palette = static_cast<QWidget *>(o)->palette();
            break;
        }
    }
    setPalette(palette);
}

CodeHighlighter::~CodeHighlighter()
{
    delete fSpec;
}

void CodeHighlighter::setLanguage(const QString &generatorId)
{
    if (fLanguage == generatorId)
        return;
    fLanguage = generatorId;
    delete fSpec;
    fSpec = makeSpec(generatorId);
    rehighlight();
}

QString CodeHighlighter::language() const
{
    return fLanguage;
}

void CodeHighlighter::setPalette(const QPalette &palette)
{
    const bool dark = isDarkPalette(palette);
    if (dark == fDark && fKeyword.foreground().style() != Qt::NoBrush)
        return;
    fDark = dark;
    updateFormats();
    rehighlight();
}

void CodeHighlighter::updateFormats()
{
    auto format = [](const QColor &color, bool bold = false, bool italic = false) {
        QTextCharFormat f;
        f.setForeground(color);
        if (bold)
            f.setFontWeight(QFont::DemiBold);
        f.setFontItalic(italic);
        return f;
    };
    if (fDark) {
        fKeyword = format(QColor(0xcf, 0x8e, 0x6d), true);
        fType = format(QColor(0x4e, 0xc9, 0xb0));
        fString = format(QColor(0x9e, 0xce, 0x6a));
        fNumber = format(QColor(0x7a, 0xb8, 0xf5));
        fComment = format(QColor(0x8a, 0x8f, 0x98), false, true);
        fPreprocessor = format(QColor(0xc5, 0x86, 0xc0));
        fFunction = format(QColor(0x56, 0xa8, 0xf5));
        fVariable = format(QColor(0xe0, 0xa8, 0x6a));
    } else {
        fKeyword = format(QColor(0x00, 0x33, 0xb3), true);
        fType = format(QColor(0x00, 0x7a, 0x7a));
        fString = format(QColor(0x06, 0x7d, 0x17));
        fNumber = format(QColor(0x17, 0x50, 0xeb));
        fComment = format(QColor(0x8c, 0x8c, 0x8c), false, true);
        fPreprocessor = format(QColor(0x9e, 0x3d, 0xa8));
        fFunction = format(QColor(0x00, 0x62, 0x7a));
        fVariable = format(QColor(0x87, 0x10, 0x94));
    }
}

void CodeHighlighter::highlightBlock(const QString &text)
{
    const Spec &s = *fSpec;
    const int n = text.size();
    int i = 0;
    setCurrentBlockState(0);
    int state = previousBlockState();

    // continuation of a multi-line comment / string
    auto closeMultiLine = [&](int from) -> int {
        if (state >= 1 && state <= s.blockComments.size()) {
            const QString &end = s.blockComments.at(state - 1).second;
            const int pos = text.indexOf(end, from, s.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
            if (pos < 0) {
                setFormat(from, n - from, fComment);
                setCurrentBlockState(state);
                return n;
            }
            setFormat(from, pos + end.size() - from, fComment);
            state = 0;
            return pos + end.size();
        }
        if (state == StateTripleDouble || state == StateTripleSingle || state == StateBacktick) {
            const QString end = state == StateTripleDouble ? QStringLiteral("\"\"\"")
                              : state == StateTripleSingle ? QStringLiteral("'''") : QStringLiteral("`");
            int pos = from;
            while (pos < n) {
                if (text.at(pos) == QLatin1Char('\\')) {
                    pos += 2;
                    continue;
                }
                if (startsAt(text, pos, end))
                    break;
                ++pos;
            }
            if (pos >= n) {
                setFormat(from, n - from, fString);
                setCurrentBlockState(state);
                return n;
            }
            setFormat(from, pos + end.size() - from, fString);
            state = 0;
            return pos + end.size();
        }
        state = 0;
        return from;
    };
    i = closeMultiLine(0);

    // preprocessor directive at the start of the line
    if (s.preprocessor && i == 0) {
        int j = 0;
        while (j < n && text.at(j).isSpace())
            ++j;
        if (j < n && text.at(j) == QLatin1Char('#') && !s.lineComments.contains(QStringLiteral("#")) && !startsAt(text, j, QStringLiteral("#cs"), false)
                && !startsAt(text, j, QStringLiteral("#ce"), false)) {
            int k = j + 1;
            while (k < n && text.at(k).isSpace())
                ++k;
            while (k < n && isIdentifierChar(text.at(k)))
                ++k;
            setFormat(j, k - j, fPreprocessor);
            const QString directive = text.mid(j + 1, k - j - 1).trimmed();
            i = k;
            if (directive == QLatin1String("include")) {
                while (i < n && text.at(i).isSpace())
                    ++i;
                if (i < n && text.at(i) == QLatin1Char('<')) {
                    const int close = text.indexOf(QLatin1Char('>'), i);
                    const int end = close < 0 ? n : close + 1;
                    setFormat(i, end - i, fString);
                    i = end;
                }
            }
        } else if (startsAt(text, j, QStringLiteral("<?php")) || startsAt(text, j, QStringLiteral("?>"))) {
            const int len = text.at(j) == QLatin1Char('<') ? 5 : 2;
            setFormat(j, len, fPreprocessor);
            i = j + len;
        }
    }

    while (i < n) {
        const QChar c = text.at(i);
        if (c.isSpace()) {
            ++i;
            continue;
        }
        // block comments
        bool matched = false;
        for (int k = 0; k < s.blockComments.size(); ++k) {
            if (startsAt(text, i, s.blockComments.at(k).first, s.caseSensitive)) {
                state = k + 1;
                const int start = i;
                i = closeMultiLine(i + s.blockComments.at(k).first.size());
                setFormat(start, i - start, fComment);
                matched = true;
                break;
            }
        }
        if (matched)
            continue;
        // line comments
        for (const QString &lc : s.lineComments) {
            if (startsAt(text, i, lc)) {
                setFormat(i, n - i, fComment);
                return;
            }
        }
        // strings
        if (s.tripleQuotes && (startsAt(text, i, QStringLiteral("\"\"\"")) || startsAt(text, i, QStringLiteral("'''")))) {
            state = c == QLatin1Char('"') ? StateTripleDouble : StateTripleSingle;
            const int start = i;
            i = closeMultiLine(i + 3);
            setFormat(start, i - start, fString);
            continue;
        }
        if (s.backtickStrings && c == QLatin1Char('`')) {
            state = StateBacktick;
            const int start = i;
            i = closeMultiLine(i + 1);
            setFormat(start, i - start, fString);
            continue;
        }
        if (s.quotes.contains(c)) {
            int j = i + 1;
            while (j < n) {
                const QChar d = text.at(j);
                if (s.backslashEscapes && d == QLatin1Char('\\')) {
                    j += 2;
                    continue;
                }
                if (d == c) {
                    if (!s.backslashEscapes && j + 1 < n && text.at(j + 1) == c) { // doubled quote
                        j += 2;
                        continue;
                    }
                    ++j;
                    break;
                }
                ++j;
            }
            j = qMin(j, n);
            setFormat(i, j - i, fString);
            i = j;
            continue;
        }
        // numbers
        if (c.isDigit() || (c == QLatin1Char('.') && i + 1 < n && text.at(i + 1).isDigit())) {
            int j = i + 1;
            while (j < n) {
                const QChar d = text.at(j);
                if (d.isLetterOrNumber() || d == QLatin1Char('_') || d == QLatin1Char('\'')) {
                    ++j;
                } else if (d == QLatin1Char('.') && !(j + 1 < n && text.at(j + 1) == QLatin1Char('.'))) {
                    ++j;
                } else if ((d == QLatin1Char('+') || d == QLatin1Char('-'))
                           && QStringLiteral("eEpP").contains(text.at(j - 1))
                           && !text.mid(i, 2).startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
                    ++j;
                } else {
                    break;
                }
            }
            setFormat(i, j - i, fNumber);
            i = j;
            continue;
        }
        // variables with a sigil ($x, @list, %hash, @Macro)
        if (!s.sigils.isEmpty() && s.sigils.contains(c) && i + 1 < n && isIdentifierStart(text.at(i + 1))) {
            int j = i + 1;
            while (j < n && isIdentifierChar(text.at(j)))
                ++j;
            setFormat(i, j - i, fVariable);
            i = j;
            continue;
        }
        // words
        if (isIdentifierStart(c)) {
            int j = i + 1;
            while (j < n && isIdentifierChar(text.at(j)))
                ++j;
            const QString word = text.mid(i, j - i);
            const QString key = s.caseSensitive ? word : word.toLower();
            if (s.remComments && key.compare(QLatin1String("rem"), Qt::CaseInsensitive) == 0) {
                setFormat(i, n - i, fComment);
                return;
            }
            if (s.keywords.contains(key)) {
                setFormat(i, j - i, fKeyword);
            } else if (s.types.contains(key)) {
                setFormat(i, j - i, fType);
            } else {
                int k = j;
                while (k < n && text.at(k) == QLatin1Char(' '))
                    ++k;
                if (s.builtins.contains(key) || (k < n && text.at(k) == QLatin1Char('(')))
                    setFormat(i, j - i, fFunction);
            }
            i = j;
            continue;
        }
        ++i;
    }
    if (state != 0)
        setCurrentBlockState(state);
}
