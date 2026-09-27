/****************************************************************************
**                                                                         **
** Copyright (C) 2008-2014 Victor Zinkevich. All rights reserved.          **
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

#include "afceutil.h"
#include "flowchartexport.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QLibraryInfo>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>

#include <cstdio>
#include <cstring>

namespace {

void printError(const QString &message)
{
    const QByteArray text = message.toLocal8Bit() + '\n';
    std::fputs(text.constData(), stderr);
    std::fflush(stderr);
}

// Registers the command line options. The descriptions are translated when
// the translators are installed already (they are not for the first parse).
void setupParser(QCommandLineParser &parser)
{
    parser.setApplicationDescription(QCoreApplication::translate("main", "AFCE - Algorithm Flowchart Editor"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", QCoreApplication::translate("main", "Flowchart (.afc) to open."), "[file]");
    parser.addOptions({
        {"import", QCoreApplication::translate("main", "Build the flowchart of a function of a C/C++ source file."), "source"},
        {"function", QCoreApplication::translate("main", "Function to import (default: main, or the first function)."), "name"},
        {"export", QCoreApplication::translate("main", "Export the flowchart to a picture (png, jpg, bmp, svg, pdf, ...) and exit."), "file"},
        {"zoom", QCoreApplication::translate("main", "Zoom factor of the export (default 1)."), "factor", "1"},
        {"monochrome", QCoreApplication::translate("main", "Export classic black-and-white blocks.")},
        {"screenshot", QCoreApplication::translate("main", "Save a screenshot of the main window and exit (for testing)."), "file"},
        {"language", QCoreApplication::translate("main", "User interface language for this run, e.g. ru_RU."), "locale"},
    });
}

// Checks the combination of the parsed options; returns the error message or an empty string.
QString argumentError(const QCommandLineParser &parser)
{
    const QStringList positional = parser.positionalArguments();
    if (positional.size() > 1)
        return QCoreApplication::translate("main", "Too many arguments: only one flowchart file can be opened.");
    const bool import = !parser.value("import").isEmpty();
    // the window opens both in tabs; an export needs exactly one flowchart
    if (!positional.isEmpty() && import && parser.isSet("export"))
        return QCoreApplication::translate("main", "Use either a flowchart file or --import, not both.");
    if (parser.isSet("function") && !import)
        return QCoreApplication::translate("main", "--function requires --import.");
    return QString();
}

// True if the program will not show a window: help, version, headless export,
// screenshot (rendered offscreen) or invalid arguments (only an error message).
// Such runs must work without a display (SSH, CI): they use the offscreen platform.
bool isHeadlessRun(int argc, char *argv[])
{
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);
    QCommandLineParser parser;
    setupParser(parser);
    if (!parser.parse(arguments))
        return true;
    for (const char *option : {"help", "help-all", "version", "export", "screenshot"}) {
        if (parser.isSet(QString::fromLatin1(option)))
            return true;
    }
    return !argumentError(parser).isEmpty();
}

// The platform plugin for a run without a window: "offscreen" (or "minimal") if
// it is installed, followed by the native one as a fallback. Deployed packages
// (macdeployqt, windeployqt) contain only the native plugin: asking Qt for a
// missing plugin would print warnings on every headless run. Called before the
// QApplication exists, so the plugin directories are guessed from argv[0] too.
QByteArray headlessPlatforms(const char *argv0)
{
#if defined(Q_OS_MACOS)
    const QByteArray native = "cocoa";
#elif defined(Q_OS_WIN)
    const QByteArray native = "windows";
#else
    const QByteArray native = "xcb;wayland";
#endif
    QStringList dirs;
    const QStringList pluginPaths = qEnvironmentVariable("QT_PLUGIN_PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const QString &path : pluginPaths)
        dirs << path + QStringLiteral("/platforms");
    dirs << QLibraryInfo::path(QLibraryInfo::PluginsPath) + QStringLiteral("/platforms");
    const QString exeDir = QFileInfo(QString::fromLocal8Bit(argv0)).absolutePath();
    dirs << exeDir + QStringLiteral("/platforms")               // windeployqt
         << exeDir + QStringLiteral("/plugins/platforms")
         << exeDir + QStringLiteral("/../PlugIns/platforms");  // macOS bundle
    for (const char *plugin : {"offscreen", "minimal"}) {
        const QStringList patterns{QStringLiteral("*q%1.*").arg(QLatin1String(plugin)),
                                   QStringLiteral("*q%1d.*").arg(QLatin1String(plugin))};
        for (const QString &dir : std::as_const(dirs)) {
            if (!QDir(dir).entryList(patterns, QDir::Files).isEmpty())
                return QByteArray(plugin) + ';' + native;
        }
    }
    return native;
}

// The interface language: --language (looked up before the real parse, so that
// the parser's messages and the help are translated already) or the saved one.
QString requestedLocale(const QSettings &settings)
{
    QCommandLineParser parser;
    setupParser(parser);
    parser.parse(QCoreApplication::arguments()); // errors are reported by the real parse
    if (parser.isSet("language"))
        return parser.value("language");
    return settings.value("locale").toString();
}

// AFCE_FORCE_DARK=1 (undocumented, for testing): a dark Fusion palette, so that
// the dark mode can be checked on any system and in offscreen screenshots.
void applyForcedDarkPalette()
{
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette p;
    const QColor window(0x2b, 0x2b, 0x2e), base(0x1f, 0x1f, 0x22), text(0xe6, 0xe6, 0xe8),
        button(0x35, 0x35, 0x39), disabled(0x7a, 0x7a, 0x80), highlight(0x2f, 0x7c, 0xf6);
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::ToolTipBase, button);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, disabled);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, QColor(0xff, 0x6b, 0x6b));
    p.setColor(QPalette::Light, QColor(0x4a, 0x4a, 0x50));
    p.setColor(QPalette::Midlight, QColor(0x3e, 0x3e, 0x44));
    p.setColor(QPalette::Mid, QColor(0x26, 0x26, 0x29));
    p.setColor(QPalette::Dark, QColor(0x18, 0x18, 0x1a));
    p.setColor(QPalette::Shadow, Qt::black);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, QColor(0x6c, 0xa8, 0xff));
    p.setColor(QPalette::LinkVisited, QColor(0xb0, 0x8c, 0xff));
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, disabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x50, 0x50, 0x55));
    QApplication::setPalette(p);
}

// Files opened from the Finder (macOS) arrive as QFileOpenEvent.
class FileOpenHandler : public QObject
{
public:
    explicit FileOpenHandler(MainWindow *window) : QObject(window), m_window(window) { }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::FileOpen) {
            const QString file = static_cast<QFileOpenEvent *>(event)->file();
            if (!file.isEmpty()) {
                m_window->requestOpenDocument(file);
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    MainWindow *m_window;
};

} // namespace

int main(int argc, char *argv[])
{
    // Headless runs must not need a display: select the offscreen platform
    // before QApplication is created (unless the user chose a platform).
    if (isHeadlessRun(argc, argv) && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", headlessPlatforms(argc > 0 ? argv[0] : ""));

    QApplication app(argc, argv);
    QApplication::setApplicationName("afce");
    QApplication::setApplicationVersion(afce::programVersion());
    afce::setupSearchPaths();

    if (qEnvironmentVariableIntValue("AFCE_FORCE_DARK") == 1)
        applyForcedDarkPalette();

    QSettings settings("afce", "application");
    setApplicationLocale(resolveApplicationLocale(requestedLocale(settings)));

    QCommandLineParser parser;
    setupParser(parser);
    parser.process(app);
    const QString argError = argumentError(parser);
    if (!argError.isEmpty()) {
        printError(argError);
        return 2;
    }
    const QString documentFile = parser.positionalArguments().value(0);
    const QString importFile = parser.value("import");
    const QString functionName = parser.value("function");
    const bool persistent = !parser.isSet("screenshot");

    // ---- headless export -------------------------------------------------
    if (parser.isSet("export")) {
        const QString output = parser.value("export");
        bool zoomOk = false;
        afce::ExportOptions options;
        options.zoom = parser.value("zoom").toDouble(&zoomOk);
        options.monochrome = parser.isSet("monochrome");
        if (!zoomOk) {
            printError(QCoreApplication::translate("main", "Invalid zoom factor '%1'.").arg(parser.value("zoom")));
            return 2;
        }
        QDomDocument doc;
        QString error;
        if (!importFile.isEmpty()) {
            if (!MainWindow::importFromSource(importFile, functionName, &doc, &error)) {
                printError(error);
                return 1;
            }
        } else if (!documentFile.isEmpty()) {
            QFlowChart chart;
            if (!chart.loadFile(documentFile, &error)) {
                printError(error);
                return 1;
            }
            doc = chart.document();
        } else {
            printError(QCoreApplication::translate("main", "--export needs a flowchart file or --import <source>."));
            return 2;
        }
        if (!afce::exportFlowchart(doc, output, options, &error)) {
            printError(error);
            return 1;
        }
        return 0;
    }

    // ---- GUI -------------------------------------------------------------
    MainWindow w;
    w.setSettingsPersistent(persistent);
    const QString screenshotFile = parser.value("screenshot");
    if (screenshotFile.isEmpty())
        w.readSettings();
    else
        w.resize(1200, 800);
    w.setLocale(QLocale());
    app.installEventFilter(new FileOpenHandler(&w));
    w.show();

    if (!screenshotFile.isEmpty()) {
        // load synchronously (errors go to stderr, no message boxes)
        QString error;
        bool ok = true;
        if (!documentFile.isEmpty())
            ok = w.openDocument(documentFile, &error);
        if (ok && !importFile.isEmpty())
            ok = w.importSource(importFile, functionName, &error);
        if (!ok) {
            printError(error);
            return 1;
        }
        // grab once the window has been shown and laid out
        QTimer::singleShot(300, &w, [&w, screenshotFile]() {
            // an opaque picture: on a screen the platform fills the window background
            const qreal dpr = w.devicePixelRatio();
            QImage shot((QSizeF(w.size()) * dpr).toSize(), QImage::Format_RGB32);
            shot.setDevicePixelRatio(dpr);
            shot.fill(w.palette().color(QPalette::Window));
            QPainter painter(&shot);
            w.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
            painter.end();
            if (!shot.save(screenshotFile)) {
                printError(QCoreApplication::translate("main", "Unable to write file '%1'.")
                               .arg(QDir::toNativeSeparators(screenshotFile)));
                QCoreApplication::exit(1);
                return;
            }
            QCoreApplication::exit(0);
        });
        return app.exec();
    }

    QTimer::singleShot(0, &w, [&w, documentFile, importFile, functionName]() {
        if (!documentFile.isEmpty())
            w.requestOpenDocument(documentFile);
        if (!importFile.isEmpty())
            w.importSource(importFile, functionName);
    });
    return app.exec();
}
