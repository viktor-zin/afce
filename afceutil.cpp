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

#include "afceutil.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

namespace afce {

namespace {

// A single quote between two alphanumerics that follows a digit is a C++14
// digit separator (1'000'000), not the start of a character literal.
bool isDigitSeparator(const QString &s, int i)
{
    if (i <= 0 || i + 1 >= s.size())
        return false;
    if (!s.at(i - 1).isLetterOrNumber() || !s.at(i + 1).isLetterOrNumber())
        return false;
    int j = i - 1;
    while (j >= 0 && (s.at(j).isLetterOrNumber() || s.at(j) == QLatin1Char('\'')))
        --j;
    return s.at(j + 1).isDigit();
}

// The per-user data directory of AFCE: ~/Library/Application Support/afce,
// ~/.local/share/afce, %APPDATA%/afce. QStandardPaths::AppDataLocation appends
// the organization and application names, but the directory must be the same
// for every program of the project (afce, afce-cli, tests), so it is looked up
// for the application "afce" without an organization.
QString userDataDir()
{
    const QString appName = QCoreApplication::applicationName();
    const QString orgName = QCoreApplication::organizationName();
    const QString afce = QStringLiteral("afce");
    if (appName == afce && orgName.isEmpty())
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QCoreApplication::setOrganizationName(QString());
    QCoreApplication::setApplicationName(afce);
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QCoreApplication::setApplicationName(appName);
    QCoreApplication::setOrganizationName(orgName);
    return dir;
}

} // namespace

QStringList splitList(const QString &list, QChar separator)
{
    QStringList result;
    QString current;
    int depth = 0;
    QChar quote;

    auto flush = [&]() {
        const QString item = current.trimmed();
        if (!item.isEmpty())
            result << item;
        current.clear();
    };

    for (int i = 0; i < list.size(); ++i) {
        const QChar c = list.at(i);
        if (!quote.isNull()) {
            current += c;
            if (c == QLatin1Char('\\') && i + 1 < list.size())
                current += list.at(++i);
            else if (c == quote)
                quote = QChar();
            continue;
        }
        if (c == QLatin1Char('"') || (c == QLatin1Char('\'') && !isDigitSeparator(list, i))) {
            quote = c;
            current += c;
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{'))
            ++depth;
        else if ((c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) && depth > 0)
            --depth;
        if (c == separator && depth == 0) {
            flush();
            continue;
        }
        current += c;
    }
    flush();
    return result;
}

QString joinList(const QStringList &items)
{
    return items.join(QLatin1Char(','));
}

QString simplifyCode(const QString &code)
{
    QString result;
    result.reserve(code.size());
    QChar quote;
    bool pendingSpace = false;

    for (int i = 0; i < code.size(); ++i) {
        const QChar c = code.at(i);
        if (!quote.isNull()) {
            result += c;
            if (c == QLatin1Char('\\') && i + 1 < code.size())
                result += code.at(++i);
            else if (c == quote)
                quote = QChar();
            continue;
        }
        if (c.isSpace()) {
            pendingSpace = true;
            continue;
        }
        if (pendingSpace && !result.isEmpty())
            result += QLatin1Char(' ');
        pendingSpace = false;
        if (c == QLatin1Char('"') || (c == QLatin1Char('\'') && !isDigitSeparator(code, i)))
            quote = c;
        result += c;
    }
    return result;
}

void setupSearchPaths()
{
    QStringList bases;
    bases << userDataDir();
    const QString appDir = QCoreApplication::applicationDirPath();
    bases << appDir;
#ifdef Q_OS_MACOS
    bases << QDir(appDir).filePath(QStringLiteral("../Resources"));
#endif
#ifdef PROGRAM_DATA_DIR
    bases << QString::fromUtf8(PROGRAM_DATA_DIR);
#endif

    for (const QString &sub : {QStringLiteral("generators"), QStringLiteral("help")}) {
        QStringList paths;
        for (const QString &base : std::as_const(bases)) {
            if (!base.isEmpty())
                paths << QDir::cleanPath(QDir(base).filePath(sub));
        }
        paths << QStringLiteral(":/") + sub;
        QDir::setSearchPaths(sub, paths);
    }
}

QStringList generatorFiles()
{
    QStringList result;
    QSet<QString> seen;
    const QStringList paths = QDir::searchPaths(QStringLiteral("generators"));
    for (const QString &path : paths) {
        const QDir dir(path);
        const QStringList entries = dir.entryList(QStringList() << QStringLiteral("*.json"), QDir::Files, QDir::Name);
        for (const QString &entry : entries) {
            const QString id = QFileInfo(entry).completeBaseName();
            if (seen.contains(id))
                continue;
            seen.insert(id);
            result << dir.absoluteFilePath(entry);
        }
    }
    return result;
}

QString programVersion()
{
#ifdef PROGRAM_VERSION
    return QStringLiteral(PROGRAM_VERSION);
#else
    return QString();
#endif
}

} // namespace afce
