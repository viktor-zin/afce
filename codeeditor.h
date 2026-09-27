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
#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QColor>
#include <QList>
#include <QPalette>
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class QVariantAnimation;

// Plain-text code editor with a line-number area (monospace system font),
// current-line highlight and a tab width of 4 spaces. Follows the palette (dark mode).
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    // A mark in the line-number area, e.g. an import diagnostic.
    struct LineMarker
    {
        enum Severity { Info, Warning, Error };
        int line = 0;               // 1-based
        Severity severity = Warning;
        QString toolTip;
    };

    explicit CodeEditor(QWidget *parent = nullptr);
    void setLineNumbersVisible(bool visible);
    bool lineNumbersVisible() const;
    // Moves the cursor to a 1-based line and highlights it briefly.
    void highlightLine(int line);
    // Scrolls so that a 1-based line is near the top, without moving the cursor.
    void scrollToLine(int line);

    // Coloured dots (with tool tips) in the line-number area.
    void setLineMarkers(const QList<LineMarker> &markers);
    QList<LineMarker> lineMarkers() const { return fMarkers; }
    // Highlighting of the current line (on by default).
    void setCurrentLineHighlight(bool enabled);
    // Colour of a marker severity, readable on the palette's Base colour.
    static QColor markerColor(LineMarker::Severity severity, const QPalette &palette);

    // Used by the line-number area.
    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    QString lineNumberAreaToolTip(const QPoint &pos) const;

signals:
    // A local file was dropped onto the editor (it is not inserted as text).
    void fileDropped(const QString &fileName);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool canInsertFromMimeData(const QMimeData *source) const override;
    void insertFromMimeData(const QMimeData *source) override;

private:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect &rect, int dy);
    void updateExtraSelections();
    void applyPalette();
    static QString droppedFile(const QMimeData *source);

    QWidget *fLineNumberArea;
    bool fLineNumbersVisible;
    bool fCurrentLineHighlight;
    bool fWasEmpty;
    QList<LineMarker> fMarkers;
    int fFlashLine;          // 1-based, 0 = none
    int fFlashAlpha;         // current alpha of the flash highlight
    QVariantAnimation *fFlash;
};

// Simple syntax highlighter: keywords, types, strings, numbers, comments,
// preprocessor lines. Readable in light and dark palettes.
class CodeHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit CodeHighlighter(QTextDocument *document);
    ~CodeHighlighter() override;
    // Language = generator id (c, cpp, py, pas, js, php, perl, ruby, vbs, autoit, bas256, freebasic, e87).
    void setLanguage(const QString &generatorId);
    QString language() const;
    // Chooses the colours for this palette (CodeEditor calls it on palette changes).
    void setPalette(const QPalette &palette);

    struct Spec;

protected:
    void highlightBlock(const QString &text) override;
private:
    void updateFormats();

    QString fLanguage;
    Spec *fSpec;
    bool fDark;
    QTextCharFormat fKeyword, fType, fString, fNumber, fComment, fPreprocessor, fFunction, fVariable;
};

#endif // CODEEDITOR_H
