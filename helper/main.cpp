// SPDX-FileCopyrightText: no
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QMessageBox>

int
main( int argc, char* argv[] )
{
    QApplication app( argc, argv );
    QApplication::setApplicationName( QStringLiteral( "claudeinstall-helper" ) );
    QApplication::setApplicationDisplayName( QStringLiteral( "ClaudeInstall Setup Helper" ) );
    QApplication::setApplicationVersion( QStringLiteral( "1.0" ) );

    QCommandLineParser parser;
    parser.setApplicationDescription( QStringLiteral( "Configure how build.sh builds the ClaudeInstall installer." ) );
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption dirOption( { QStringLiteral( "d" ), QStringLiteral( "dir" ) },
                                  QStringLiteral( "Project directory (where build.sh lives)." ),
                                  QStringLiteral( "path" ) );
    parser.addOption( dirOption );
    parser.process( app );

    // Find the folder that holds build.sh: --dir, else the current folder, else the folder of this
    // program, each also tried with its parents (the helper is built in <project>/build-helper).
    QStringList starts;
    if ( parser.isSet( dirOption ) )
    {
        starts << QDir( parser.value( dirOption ) ).absolutePath();
    }
    starts << QDir::currentPath() << QApplication::applicationDirPath();
    QString projectDir;
    for ( const QString& start : starts )
    {
        QDir d( start );
        for ( int up = 0; up < 4 && projectDir.isEmpty(); ++up )
        {
            if ( d.exists( QStringLiteral( "build.sh" ) ) )
            {
                projectDir = d.absolutePath();
            }
            else if ( !d.cdUp() )
            {
                break;
            }
        }
        if ( !projectDir.isEmpty() )
        {
            break;
        }
    }
    if ( projectDir.isEmpty() )
    {
        QMessageBox::critical( nullptr,
                               QStringLiteral( "ClaudeInstall Setup Helper" ),
                               QStringLiteral( "Could not find build.sh.\n\nStart the helper with  --dir /path/to/project  "
                                               "or run ./build.sh from the project folder." ) );
        return 1;
    }

    MainWindow window( projectDir );
    window.show();
    return app.exec();
}
