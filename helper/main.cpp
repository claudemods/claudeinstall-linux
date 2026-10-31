// SPDX-FileCopyrightText: no
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>

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
                                  QStringLiteral( "path" ),
                                  QDir::currentPath() );
    parser.addOption( dirOption );
    parser.process( app );

    MainWindow window( QDir( parser.value( dirOption ) ).absolutePath() );
    window.show();
    return app.exec();
}
