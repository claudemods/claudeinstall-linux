/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

/*
 * The text-mode installer: the same modules, settings and jobs as the
 * graphical one, shown in a terminal. This program has no Widgets, no QML
 * and no window system; see TuiApplication for how it is driven.
 */

#include "ClaudeInstallConfig.h"
#include "ClaudeInstallVersionX.h"

#include "BrandingInfo.h"
#include "JobQueue.h"
#include "Settings.h"
#include "TuiApplication.h"
#include "utils/Dirs.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"
#include "utils/System.h"

// From 3rdparty/
#include "kdsingleapplication.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <cstdio>
#include <memory>

/// @brief Where the branding descriptor may be, most preferred first (as for the GUI)
static QStringList
brandingFileCandidates( bool assumeBuilddir, const QString& brandingFilename )
{
    QStringList brandingPaths;
    if ( ClaudeInstall::isAppDataDirOverridden() )
    {
        brandingPaths << ClaudeInstall::appDataDir().absoluteFilePath( brandingFilename );
    }
    else
    {
        if ( assumeBuilddir )
        {
            brandingPaths << ( QDir::currentPath() + QStringLiteral( "/src/" ) + brandingFilename );
        }
        if ( ClaudeInstall::haveExtraDirs() )
        {
            for ( const auto& s : ClaudeInstall::extraDataDirs() )
            {
                brandingPaths << ( s + brandingFilename );
            }
        }
        brandingPaths << QDir( CMAKE_INSTALL_FULL_SYSCONFDIR "/claudeinstall/" ).absoluteFilePath( brandingFilename );
        brandingPaths << ClaudeInstall::appDataDir().absoluteFilePath( brandingFilename );
    }
    return brandingPaths;
}

/// @brief Loads the headless branding; returns @c false (after saying why) if there is none
static bool
initBranding( bool isDebug )
{
    const QString component = ClaudeInstall::Settings::instance()->brandingComponentName();
    if ( component.simplified().isEmpty() )
    {
        std::fprintf( stderr, "The branding component is not set in settings.conf.\n" );
        return false;
    }

    const QStringList candidates
        = brandingFileCandidates( isDebug, QStringLiteral( "branding/%1/branding.desc" ).arg( component ) );
    for ( const QString& path : candidates )
    {
        const QFileInfo fi( path );
        if ( fi.exists() && fi.isReadable() )
        {
            if ( ClaudeInstall::BrandingInfo::instance().loadFromDescriptor( fi.absoluteFilePath() ) )
            {
                return true;
            }
            std::fprintf( stderr, "The branding file %s is not usable.\n", qPrintable( fi.absoluteFilePath() ) );
            return false;
        }
    }
    std::fprintf( stderr, "There is no branding file for component %s.\n", qPrintable( component ) );
    return false;
}

/// @brief Handles the command-line arguments; sets @p isDebug and @p keepStderr
static void
handle_args( QCoreApplication& a, bool& isDebug, bool& keepStderr )
{
    QCommandLineOption debugOption( QStringList { "d", "debug" },
                                    "Also look in current directory for configuration. Implies -D8." );
    QCommandLineOption debugLevelOption(
        QStringLiteral( "D" ), "Verbose output for debugging purposes (0-8).", "level" );
    QCommandLineOption debugTxOption( QStringList { "T", "debug-translation" },
                                      "Also look in the current directory for translation." );
    QCommandLineOption configOption(
        QStringList { "c", "config" }, "Configuration directory to use, for testing purposes.", "config" );
    QCommandLineOption xdgOption( QStringList { "X", "xdg-config" }, "Use XDG_{CONFIG,DATA}_DIRS as well." );
    QCommandLineOption stderrOption( QStringList { "S", "keep-stderr" },
                                     "Leave standard error alone (it will scribble on the screen; redirect it)." );

    QCommandLineParser parser;
    parser.setApplicationDescription( "Distribution-independent installer framework, text mode" );
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption( debugOption );
    parser.addOption( debugLevelOption );
    parser.addOption( configOption );
    parser.addOption( xdgOption );
    parser.addOption( debugTxOption );
    parser.addOption( stderrOption );
    parser.process( a );

    unsigned int level = Logger::LOGERROR;
    if ( parser.isSet( debugOption ) )
    {
        level = Logger::LOGVERBOSE;
    }
    else if ( parser.isSet( debugLevelOption ) )
    {
        bool ok = true;
        const int l = parser.value( debugLevelOption ).toInt( &ok );
        level = ( !ok || l < 0 ) ? Logger::LOGVERBOSE : static_cast< unsigned int >( l );
    }
    Logger::setupLogLevel( level );

    if ( parser.isSet( configOption ) )
    {
        ClaudeInstall::setAppDataDir( QDir( parser.value( configOption ) ) );
    }
    if ( parser.isSet( xdgOption ) )
    {
        ClaudeInstall::setXdgDirs();
    }
    ClaudeInstall::setAllowLocalTranslation( parser.isSet( debugOption ) || parser.isSet( debugTxOption ) );

    isDebug = parser.isSet( debugOption );
    keepStderr = parser.isSet( stderrOption );
}

int
main( int argc, char* argv[] )
{
    QCoreApplication a( argc, argv );
    a.setOrganizationDomain( QStringLiteral( CLAUDEINSTALL_ORGANIZATION_DOMAIN ) );
    a.setApplicationName( QStringLiteral( CLAUDEINSTALL_APPLICATION_NAME ) );
    a.setApplicationVersion( QStringLiteral( CLAUDEINSTALL_VERSION ) );

    bool isDebug = false;
    bool keepStderr = false;
    handle_args( a, isDebug, keepStderr );

    std::unique_ptr< KDSingleApplication > possiblyUnique;
    if ( !isDebug )
    {
        possiblyUnique = std::make_unique< KDSingleApplication >();
        if ( !possiblyUnique->isPrimaryInstance() )
        {
            std::fprintf( stderr, "ClaudeInstall is already running.\n" );
            return 87;  // EUSERS on Linux
        }
    }

    ClaudeInstall::Settings::init( isDebug );
    if ( !ClaudeInstall::Settings::instance() || !ClaudeInstall::Settings::instance()->isValid() )
    {
        std::fprintf( stderr, "ClaudeInstall has invalid settings, shutting down.\n" );
        return 78;  // EX_CONFIG on FreeBSD
    }

    Logger::setupLogfile();
    cDebug() << "ClaudeInstall (text mode) version:" << CLAUDEINSTALL_VERSION;
    cDebug() << Logger::SubEntry << "Using settings:" << ClaudeInstall::Settings::instance()->path();
    cDebug() << Logger::SubEntry << "Using log file:" << Logger::logFile();

    if ( !initBranding( isDebug ) )
    {
        return EXIT_FAILURE;
    }
    ClaudeInstall::installTranslator();

    auto* jobQueue = new ClaudeInstall::JobQueue( &a );
    new ClaudeInstall::System( ClaudeInstall::Settings::instance()->doChroot(), &a );
    ClaudeInstall::BrandingInfo::instance().setGlobals( jobQueue->globalStorage() );

    ClaudeInstall::TuiApplication tui;
    if ( !tui.start( !keepStderr ) )
    {
        std::fprintf( stderr, "%s\n", qPrintable( tui.errorString() ) );
        return EXIT_FAILURE;
    }
    return a.exec();
}
