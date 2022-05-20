/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2016-2022 <tsujan2000@gmail.com>
 *
 * FeatherNotes is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FeatherNotes is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <csignal> // signal.h in C
#include <QStringList>
#include <QApplication>
#include <QLibraryInfo>
#include <QTranslator>
#include <QTextStream>
#include "fn.h"

void handleQuitSignals (const std::vector<int>& quitSignals)
{
    auto handler = [](int sig) ->void {
        Q_UNUSED (sig);
        QCoreApplication::quit();
    };

    for (int sig : quitSignals )
        signal (sig, handler); // handle these signals by quitting gracefully
}

int main(int argc, char *argv[])
{
    const QString name = "FeatherNotes";
    const QString version = "0.11.0";
    const QString option = QString::fromUtf8 (argv[1]);
    if (option == "--help" || option == "-h")
    {
        QTextStream out (stdout);
        out << "FeatherNotes - Lightweight Qt hierarchical notes-manager\n"\
               "\nUsage:\n	feathernotes [options] [file] "\
               "Open the specified file\nOptions:\n"\
               "--version or -v   Show version information and exit.\n"\
               "--help            Show this help and exit.\n"\
               "-m, --min         Start minimized.\n"\
               "-t, --tray        Start iconified to tray if there is a tray.\n\n";
        return 0;
    }
    else if (option == "--version" || option == "-v")
    {
        QTextStream out (stdout);
        out << name << " " << version << Qt::endl;
        return 0;
    }

    QApplication app (argc, argv);
    app.setApplicationName (name);
    app.setApplicationVersion (version);

#if not defined (Q_OS_WIN)
    handleQuitSignals ({SIGQUIT, SIGINT, SIGTERM, SIGHUP}); // -> https://en.wikipedia.org/wiki/Unix_signal
#endif

#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    app.setAttribute (Qt::AA_UseHighDpiPixmaps, true);
#endif

    QStringList langs (QLocale::system().uiLanguages());
    QString lang; // bcp47Name() doesn't work under vbox
    if (!langs.isEmpty())
        lang = langs.first().replace ('-', '_');

    QTranslator qtTranslator;
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
    if (!qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath)))
#else
    if (!qtTranslator.load ("qt_" + lang, QLibraryInfo::path (QLibraryInfo::TranslationsPath)))
#endif
    { // shouldn't be needed
        if (!langs.isEmpty())
        {
            lang = langs.first().split (QLatin1Char ('_')).first();
#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))
            qtTranslator.load ("qt_" + lang, QLibraryInfo::location (QLibraryInfo::TranslationsPath));
#else
            (void)qtTranslator.load ("qt_" + lang, QLibraryInfo::path (QLibraryInfo::TranslationsPath));
#endif
        }
    }
    app.installTranslator (&qtTranslator);

    QTranslator FPTranslator;

#if (QT_VERSION < QT_VERSION_CHECK(6,0,0))

#if defined (Q_OS_HAIKU)
    FPTranslator.load ("feathernotes_" + lang, qApp->applicationDirPath() + "/translations");
#elif defined (Q_OS_WIN)
    FPTranslator.load ("feathernotes_" + lang, qApp->applicationDirPath() + "\\..\\data\\translations\\translations");
#else
    FPTranslator.load ("feathernotes_" + lang, QStringLiteral (DATADIR) + "/feathernotes/translations");
#endif

#else

#if defined (Q_OS_HAIKU)
    (void)FPTranslator.load ("feathernotes_" + lang, qApp->applicationDirPath() + "/translations");
#elif defined (Q_OS_WIN)
    (void)FPTranslator.load ("feathernotes_" + lang, qApp->applicationDirPath() + "\\..\\data\\translations\\translations");
#else
    (void)FPTranslator.load ("feathernotes_" + lang, QStringLiteral (DATADIR) + "/feathernotes/translations");
#endif

#endif

    app.installTranslator (&FPTranslator);

    QStringList message;
    if (argc > 1)
    {
        message << QString::fromUtf8 (argv[1]);
        if (argc > 2)
            message << QString::fromUtf8 (argv[2]);
    }

    FeatherNotes::FN w (message);

    QObject::connect (&app, &QCoreApplication::aboutToQuit, &w, &FeatherNotes::FN::quitting);

    return app.exec();
}
