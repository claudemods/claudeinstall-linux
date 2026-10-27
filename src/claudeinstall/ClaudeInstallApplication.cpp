/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */
#include "ClaudeInstallApplication.h"

#include "ClaudeInstallConfig.h"
#include "ClaudeInstallVersionX.h"
#include "ClaudeInstallWindow.h"
#include "progresstree/ProgressTreeView.h"

#include "Branding.h"
#include "JobQueue.h"
#include "Settings.h"
#include "ViewManager.h"
#include "locale/TranslationsModel.h"
#include "modulesystem/ModuleManager.h"
#include "utils/Dirs.h"
#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/System.h"
#ifdef WITH_QML
#include "utils/Qml.h"
#endif
#include "utils/Retranslator.h"
#include "viewpages/ViewStep.h"

#include <QDir>
#include <QFileInfo>
#include <QScreen>
#include <QTimer>

/// @brief Convenience for "are the settings in debug mode"
static bool
isDebug()
{
    return ClaudeInstall::Settings::instance() && ClaudeInstall::Settings::instance()->debugMode();
}

ClaudeInstallApplication::ClaudeInstallApplication( int& argc, char* argv[] )
    : QApplication( argc, argv )
    , m_mainwindow( nullptr )
    , m_moduleManager( nullptr )
{
    // Setting the organization name makes the default cache
    // directory -- where ClaudeInstall stores logs, for instance --
    // <org>/<app>/, so we end up with ~/.cache/ClaudeInstall/claudeinstall/
    // which is excessively squidly.
    //
    // setOrganizationName( QStringLiteral( CLAUDEINSTALL_ORGANIZATION_NAME ) );
    setOrganizationDomain( QStringLiteral( CLAUDEINSTALL_ORGANIZATION_DOMAIN ) );
    setApplicationName( QStringLiteral( CLAUDEINSTALL_APPLICATION_NAME ) );
    setApplicationVersion( QStringLiteral( CLAUDEINSTALL_VERSION ) );

    QFont f = font();
    ClaudeInstall::setDefaultFontSize( f.pointSize() );
}

void
ClaudeInstallApplication::init()
{
    Logger::setupLogfile();
    cDebug() << "ClaudeInstall version:" << CLAUDEINSTALL_VERSION;
    cDebug() << Logger::SubEntry << "Using Qt version:" << qVersion();
    cDebug() << Logger::SubEntry << "Build type:" << CMAKE_BUILD_TYPE;
    cDebug() << Logger::SubEntry << "Using settings:" << ClaudeInstall::Settings::instance()->path();
    cDebug() << Logger::SubEntry << "Using log file:" << Logger::logFile();
    cDebug() << Logger::SubEntry << "Languages:" << ClaudeInstall::Locale::availableLanguages();

    if ( !ClaudeInstall::Settings::instance() )
    {
        cError() << "Must create ClaudeInstall::Settings before the application.";
        ::exit( 1 );
    }
    initQmlPath();
    initBranding();

    ClaudeInstall::installTranslator();

    setQuitOnLastWindowClosed( false );
    setWindowIcon( QIcon( ClaudeInstall::Branding::instance()->imagePath( ClaudeInstall::Branding::ProductIcon ) ) );

    cDebug() << Logger::SubEntry << "STARTUP: initSettings, initQmlPath, initBranding done";

    initModuleManager();  //also shows main window

    cDebug() << Logger::SubEntry << "STARTUP: initModuleManager: module init started";
}

ClaudeInstallApplication::~ClaudeInstallApplication()
{
    Logger::CDebug( Logger::LOGVERBOSE ) << "Shutting down ClaudeInstall...";
    Logger::CDebug( Logger::LOGVERBOSE ) << Logger::SubEntry << "Finished shutdown.";
}

ClaudeInstallApplication*
ClaudeInstallApplication::instance()
{
    return qobject_cast< ClaudeInstallApplication* >( QApplication::instance() );
}

ClaudeInstallWindow*
ClaudeInstallApplication::mainWindow()
{
    return m_mainwindow;
}

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
            for ( auto s : ClaudeInstall::extraDataDirs() )
            {
                brandingPaths << ( s + brandingFilename );
            }
        }
        brandingPaths << QDir( CMAKE_INSTALL_FULL_SYSCONFDIR "/claudeinstall/" ).absoluteFilePath( brandingFilename );
        brandingPaths << ClaudeInstall::appDataDir().absoluteFilePath( brandingFilename );
    }

    return brandingPaths;
}

void
ClaudeInstallApplication::initQmlPath()
{
#ifdef WITH_QML
    if ( !ClaudeInstall::initQmlModulesDir() )
    {
        ::exit( EXIT_FAILURE );
    }
#endif
}

void
ClaudeInstallApplication::initBranding()
{
    QString brandingComponentName = ClaudeInstall::Settings::instance()->brandingComponentName();
    if ( brandingComponentName.simplified().isEmpty() )
    {
        cError() << "FATAL: branding component not set in settings.conf";
        ::exit( EXIT_FAILURE );
    }

    QString brandingDescriptorSubpath = QString( "branding/%1/branding.desc" ).arg( brandingComponentName );
    QStringList brandingFileCandidatesByPriority = brandingFileCandidates( isDebug(), brandingDescriptorSubpath );

    QFileInfo brandingFile;
    bool found = false;

    foreach ( const QString& path, brandingFileCandidatesByPriority )
    {
        QFileInfo pathFi( path );
        if ( pathFi.exists() && pathFi.isReadable() )
        {
            brandingFile = pathFi;
            found = true;
            break;
        }
    }

    if ( !found || !brandingFile.exists() || !brandingFile.isReadable() )
    {
        cError() << "Cowardly refusing to continue startup without branding."
                 << Logger::DebugList( brandingFileCandidatesByPriority );
        if ( ClaudeInstall::isAppDataDirOverridden() )
        {
            cError() << "FATAL: explicitly configured application data directory is missing" << brandingComponentName;
        }
        else
        {
            cError() << "FATAL: none of the expected branding descriptor file paths exist.";
        }
        ::exit( EXIT_FAILURE );
    }

    new ClaudeInstall::Branding( brandingFile.absoluteFilePath(), this, devicePixelRatio() );
}

void
ClaudeInstallApplication::initModuleManager()
{
    m_moduleManager = new ClaudeInstall::ModuleManager( ClaudeInstall::Settings::instance()->modulesSearchPaths(), this );
    connect( m_moduleManager, &ClaudeInstall::ModuleManager::initDone, this, &ClaudeInstallApplication::initView );
    m_moduleManager->init();
}

/** @brief centers the widget @p w on (a) screen
 *
 * This tries to duplicate the (deprecated) qApp->desktop()->availableGeometry()
 * placement by iterating over screens and putting ClaudeInstall in the first
 * one where it fits; this is *generally* the primary screen.
 *
 * With debugging, it would look something like this (2 screens attached,
 * primary at +1080+240 because I have a very strange X setup). Before
 * being mapped, the ClaudeInstall window is at +0+0 but does have a size.
 * The first screen's geometry includes the offset from the origin in
 * screen coordinates.
 *
 *  Proposed window size: 1024 520
 *  Window QRect(0,0 1024x520)
 *  Screen QRect(1080,240 2560x1440)
 *  Moving QPoint(1848,700)
 *  Screen QRect(0,0 1080x1920)
 *
 */
static void
centerWindowOnScreen( QWidget* w )
{
    QList< QScreen* > screens = qApp->screens();
    QPoint windowCenter = w->rect().center();
    QSize windowSize = w->rect().size();

    for ( const auto* screen : screens )
    {
        QSize screenSize = screen->availableGeometry().size();
        if ( ( screenSize.width() >= windowSize.width() ) && ( screenSize.height() >= windowSize.height() ) )
        {
            w->move( screen->availableGeometry().center() - windowCenter );
            break;
        }
    }
}

void
ClaudeInstallApplication::initView()
{
    cDebug() << "STARTUP: initModuleManager: all modules init done";
    initJobQueue();
    cDebug() << "STARTUP: initJobQueue done";

    m_mainwindow = new ClaudeInstallWindow();  //also creates ViewManager

    connect( m_moduleManager, &ClaudeInstall::ModuleManager::modulesLoaded, this, &ClaudeInstallApplication::initViewSteps );
    connect( m_moduleManager, &ClaudeInstall::ModuleManager::modulesFailed, this, &ClaudeInstallApplication::initFailed );

    QTimer::singleShot( 0, m_moduleManager, &ClaudeInstall::ModuleManager::loadModules );

    if ( ClaudeInstall::Branding::instance() && ClaudeInstall::Branding::instance()->windowPlacementCentered() )
    {
        centerWindowOnScreen( m_mainwindow );
    }
    cDebug() << "STARTUP: ClaudeInstallWindow created; loadModules started";
}

void
ClaudeInstallApplication::initViewSteps()
{
    cDebug() << "STARTUP: loadModules for all modules done";
    m_moduleManager->checkRequirements();
    if ( ClaudeInstall::Branding::instance()->windowMaximize() )
    {
        m_mainwindow->setWindowFlag( Qt::FramelessWindowHint );
        m_mainwindow->showMaximized();
    }
    else
    {
        m_mainwindow->show();
    }

    cDebug() << "STARTUP: Window now visible and ProgressTreeView populated";
    cDebug() << Logger::SubEntry << ClaudeInstall::ViewManager::instance()->viewSteps().count() << "view steps loaded.";
    ClaudeInstall::ViewManager::instance()->onInitComplete();
}

void
ClaudeInstallApplication::initFailed( const QStringList& l )
{
    cError() << "STARTUP: failed modules are" << l;
    m_mainwindow->show();
}

void
ClaudeInstallApplication::initJobQueue()
{
    ClaudeInstall::JobQueue* jobQueue = new ClaudeInstall::JobQueue( this );
    new ClaudeInstall::System( ClaudeInstall::Settings::instance()->doChroot(), this );
    ClaudeInstall::Branding::instance()->setGlobals( jobQueue->globalStorage() );
}
