/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Benjamin Vaudour <benjamin.vaudour@yahoo.fr>
 *   SPDX-FileCopyrightText: 2014-2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2014, 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "DisplayManagerJob.h"

#include "DisplayManager.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "compat/Variant.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

#include <memory>
#include <optional>
#include <vector>

using namespace DisplayManagers;

DisplayManagerJob::DisplayManagerJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

DisplayManagerJob::~DisplayManagerJob() {}

QString
DisplayManagerJob::prettyName() const
{
    return tr( "Configuring display managers…", "@status" );
}

ClaudeInstall::JobResult
DisplayManagerJob::exec()
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();

    // We acquire a list of displaymanagers, either from config or (overridden)
    // from globalstorage. This module will try to set up (including autologin)
    // all the displaymanagers in the list, in that specific order. Most distros
    // will probably only ship one displaymanager.
    // If a displaymanager is in the list but not installed, a debugging message
    // is printed and the entry ignored.
    QStringList displaymanagers = ClaudeInstall::getStringList( m_configuration, "displaymanagers" );
    if ( gs->contains( "displayManagers" ) )
    {
        displaymanagers = gs->value( "displayManagers" ).toStringList();
    }
    if ( ClaudeInstall::getBool( m_configuration, "sysconfigSetup", false ) )
    {
        displaymanagers = QStringList { QStringLiteral( "sysconfig" ) };
    }

    if ( displaymanagers.isEmpty() )
    {
        return ClaudeInstall::JobResult::error(
            tr( "No display managers selected for the displaymanager module." ),
            tr( "The displaymanagers list is empty or undefined in both globalstorage and displaymanager.conf." ) );
    }

    // Get instances that are actually installed
    const QString rootMountPoint = gs->value( "rootMountPoint" ).toString();
    std::vector< std::unique_ptr< DisplayManager > > dmImpl;
    const QStringList dmNames = displaymanagers;
    for ( const QString& dm : dmNames )
    {
        auto instance = DisplayManager::create( dm, rootMountPoint );
        if ( instance && instance->haveDm() )
        {
            dmImpl.push_back( std::move( instance ) );
        }
        else
        {
            if ( !instance )
            {
                cDebug() << dm << "has no implementation class.";
            }
            cDebug() << dm << "selected but not installed";
            displaymanagers.removeOne( dm );
        }
    }

    if ( dmImpl.empty() )
    {
        cWarning() << "No display managers selected for the displaymanager module. "
                      "The list is empty after checking for installed display managers.";
        return ClaudeInstall::JobResult::ok();
    }

    // Pick up remaining settings
    std::optional< DesktopEnvironment > defaultDesktopEnvironment;
    if ( m_configuration.contains( "defaultDesktopEnvironment" ) )
    {
        const QVariantMap entry = m_configuration.value( "defaultDesktopEnvironment" ).toMap();
        defaultDesktopEnvironment
            = DesktopEnvironment( ClaudeInstall::getString( entry, "executable" ), ClaudeInstall::getString( entry, "desktopFile" ) );
        // Adjust if executable is bad, but desktopFile isn't.
        if ( !defaultDesktopEnvironment->updateFromDesktopFile( rootMountPoint ) )
        {
            cWarning() << "The configured default desktop environment," << defaultDesktopEnvironment->desktopFile
                       << ", can not be found.";
        }
    }
    else
    {
        defaultDesktopEnvironment = findDesktopEnvironment( rootMountPoint );
    }

    const bool enableBasicSetup = ClaudeInstall::getBool( m_configuration, "basicSetup", false );

    const QVariant autoLoginUser = gs->value( "autoLoginUser" );
    const bool doAutologin = autoLoginUser.isValid() && !autoLoginUser.isNull();
    const QString username = autoLoginUser.toString();
    if ( doAutologin )
    {
        cDebug() << "Setting up autologin for user" << username << ".";
    }
    else
    {
        cDebug() << "Unsetting autologin.";
    }

    gs->insert( "displayManagers", displaymanagers );

    // Do the actual configuration and collect messages
    QStringList dmSetupMessages;
    const DesktopEnvironment* de = defaultDesktopEnvironment ? &*defaultDesktopEnvironment : nullptr;
    for ( const auto& dm : dmImpl )
    {
        const QVariant specific = m_configuration.value( dm->name() );
        if ( ClaudeInstall::typeOf( specific ) == ClaudeInstall::MapVariantType )
        {
            dm->configure( specific.toMap() );
        }

        Result message;
        if ( enableBasicSetup )
        {
            message = dm->basicSetup();
        }
        if ( de && !message )
        {
            message = dm->desktopEnvironmentSetup( *de );
        }
        if ( !message )
        {
            message = dm->greeterSetup();
        }
        if ( !message )
        {
            message = dm->setAutologin( username, doAutologin, de );
        }

        if ( message )
        {
            dmSetupMessages << QStringLiteral( "%1: %2" ).arg( message->title, message->details );
        }
    }

    if ( !dmSetupMessages.isEmpty() )
    {
        return ClaudeInstall::JobResult::error( tr( "Display manager configuration was incomplete" ),
                                            dmSetupMessages.join( '\n' ) );
    }
    return ClaudeInstall::JobResult::ok();
}

void
DisplayManagerJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_configuration = configurationMap;
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( DisplayManagerJobFactory, registerPlugin< DisplayManagerJob >(); )
