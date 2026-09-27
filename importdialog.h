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

#ifndef IMPORTDIALOG_H
#define IMPORTDIALOG_H

#include "codeimporter.h"

#include <QDialog>
#include <QDomDocument>
#include <QList>
#include <QString>
#include <QStringList>

class ImportDialogPrivate;

// "Import from source code" dialog: code editor, language, function selector,
// options, diagnostics and a live flowchart preview.
class ImportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImportDialog(QWidget *parent = nullptr);
    ~ImportDialog() override;

    // Pre-fills the editor (e.g. from drag and drop or the command line).
    // fileName (optional) is used for the language detection (Auto) and the title.
    void setSource(const QString &code, const QString &fileName = QString());
    // Loads a source file into the editor; shows an error and returns false on failure.
    bool loadFile(const QString &fileName);
    // Preselects a function by name (if present).
    void selectFunction(const QString &name);

    // After exec() == Accepted: one AFC document per imported function
    // (one for "Import", every function for "Import all functions").
    QList<QDomDocument> importedDocuments() const;
    // Suggested tab titles, parallel to importedDocuments() (e.g. "main", "sum").
    QStringList importedTitles() const;

    // The code in the editor and the file it was loaded from (empty for pasted code).
    QString source() const;
    QString fileName() const;
    // The options chosen in the dialog.
    afce::ImportOptions options() const;
    void setOptions(const afce::ImportOptions &options);
    // Name of the function shown in the preview (empty for a code fragment or none).
    QString currentFunction() const;
    // Parses / rebuilds the preview now instead of waiting for the debounce timer.
    void updatePreview();
    // The error message of the last failed loadFile().
    QString lastError() const;

    // Decodes a source file: UTF-8 (with or without BOM), UTF-16/32 with BOM,
    // otherwise Windows-1251 (Cyrillic) or Latin-1 by a heuristic.
    static QString decodeSource(const QByteArray &data);
    // Testing: no message boxes (see lastError()) and no QSettings access.
    static void setQuietMode(bool quiet);

signals:
    // The live preview has been rebuilt (after the debounce delay or updatePreview()).
    void previewUpdated();

protected:
    void done(int result) override;
    void changeEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class ImportDialogPrivate;
    ImportDialogPrivate *d;
};

#endif // IMPORTDIALOG_H
