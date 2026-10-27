/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 *   SPDX-FileCopyrightText: 2015 Rohan Garg <rohan@garg.io>
 *   SPDX-FileCopyrightText: 2016-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017 2019, Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "PackagesJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

using namespace Packages;

PackagesJob::PackagesJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

PackagesJob::~PackagesJob() {}

QString
PackagesJob::prettyName() const
{
    return tr( "Install packages.", "@status" );
}

QString
PackagesJob::prettyStatusMessage() const
{
    const int group = m_groupPackages;
    if ( !group )
    {
        if ( m_totalPackages > 0 )
        {
            // Outside the context of an operation
            return tr( "Processing packages (%1 / %2)", "@status" ).arg( m_completedPackages.load() ).arg( m_totalPackages.load() );
        }
        return tr( "Install packages.", "@status" );
    }

    switch ( m_mode )
    {
    case InstallMode:
        return group == 1 ? tr( "Installing one package.", "@status" ) : tr( "Installing %n packages.", "@status", group );
    case RemoveMode:
        return group == 1 ? tr( "Removing one package.", "@status" ) : tr( "Removing %n packages.", "@status", group );
    default:
        // No mode, generic description
        return tr( "Install packages.", "@status" );
    }
}

void
PackagesJob::changeMode( Mode mode )
{
    m_mode = mode;
    emit progress( m_completedPackages * 1.0 / m_totalPackages );
}

void
PackagesJob::runOperations( PackageManager& pkgman, const QVariantMap& entry )
{
    const QString locale
        = ClaudeInstall::JobQueue::instance()->globalStorage()->value( "locale" ).toString();

    // The actions are not iterated in a specific order, so it is recommended to use only one action per entry.
    for ( auto it = entry.constBegin(); it != entry.constEnd(); ++it )
    {
        const QString& key = it.key();
        if ( key == "source" )
        {
            // A string, not a list of packages
            cDebug() << "Package-list from" << it.value().toString();
            continue;
        }

        const QVariantList packageList = substLocale( it.value().toList(), locale );
        m_groupPackages = packageList.size();
        if ( key == "install" )
        {
            changeMode( InstallMode );
            pkgman.operationInstall( packageList );
        }
        else if ( key == "try_install" )
        {
            changeMode( InstallMode );
            pkgman.operationTryInstall( packageList );
        }
        else if ( key == "remove" )
        {
            changeMode( RemoveMode );
            pkgman.operationRemove( packageList );
        }
        else if ( key == "try_remove" )
        {
            changeMode( RemoveMode );
            pkgman.operationTryRemove( packageList );
        }
        else if ( key == "localInstall" )
        {
            changeMode( InstallMode );
            pkgman.operationInstall( packageList, true );
        }
        else
        {
            cWarning() << "Unknown package-operation key" << key;
        }
        m_completedPackages += packageList.size();
        emit progress( m_completedPackages * 1.0 / m_totalPackages );
    }

    m_groupPackages = 0;
    changeMode( NoMode );
}

ClaudeInstall::JobResult
PackagesJob::exec()
{
    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();

    const QString backend = ClaudeInstall::getString( m_configuration, "backend" );
    auto pkgman = PackageManager::create( backend, m_configuration );
    if ( !pkgman )
    {
        return ClaudeInstall::JobResult::error( QStringLiteral( "Bad backend" ), QStringLiteral( "backend=\"%1\"" ).arg( backend ) );
    }

    const bool hasInternet = gs->value( "hasInternet" ).toBool();

    if ( ClaudeInstall::getBool( m_configuration, "skip_if_no_internet", false ) && !hasInternet )
    {
        cWarning() << "Package installation has been skipped: no internet";
        return ClaudeInstall::JobResult::ok();
    }

    auto failure = [ this ]( const CommandFailed& e, const QString& what )
    {
        cWarning() << "Command" << e.command.join( ' ' ) << "failed with exit code" << e.exitCode;
        cDebug() << "output:" << e.output;
        return ClaudeInstall::JobResult::error( tr( "Package Manager error" ),
                                            what.arg( e.command.join( ' ' ) ).arg( e.exitCode ) );
    };

    if ( ClaudeInstall::getBool( m_configuration, "update_db", false ) && hasInternet )
    {
        try
        {
            pkgman->updateDb();
        }
        catch ( const CommandFailed& e )
        {
            return failure( e,
                            tr( "The package manager could not prepare updates. The command <pre>%1</pre> returned error code %2." ) );
        }
    }

    if ( ClaudeInstall::getBool( m_configuration, "update_system", false ) && hasInternet )
    {
        try
        {
            pkgman->updateSystem();
        }
        catch ( const CommandFailed& e )
        {
            return failure( e,
                            tr( "The package manager could not update the system. The command <pre>%1</pre> returned error code %2." ) );
        }
    }

    QVariantList operations = m_configuration.value( "operations" ).toList();
    if ( gs->contains( "packageOperations" ) )
    {
        operations += gs->value( "packageOperations" ).toList();
    }

    const QString locale = gs->value( "locale" ).toString();
    m_mode = NoMode;
    m_totalPackages = 0;
    m_completedPackages = 0;
    for ( const auto& op : operations )
    {
        const QVariantMap map = op.toMap();
        for ( auto it = map.constBegin(); it != map.constEnd(); ++it )
        {
            if ( it.key() != "source" )
            {
                m_totalPackages += substLocale( it.value().toList(), locale ).size();
            }
        }
    }

    if ( !m_totalPackages )
    {
        // Avoids potential divide-by-zero in progress reporting
        return ClaudeInstall::JobResult::ok();
    }

    for ( const auto& op : operations )
    {
        m_groupPackages = 0;
        try
        {
            runOperations( *pkgman, op.toMap() );
        }
        catch ( const CommandFailed& e )
        {
            return failure(
                e,
                tr( "The package manager could not make changes to the installed system. The command <pre>%1</pre> returned error code %2." ) );
        }
    }

    m_mode = NoMode;
    emit progress( 1.0 );

    return ClaudeInstall::JobResult::ok();
}

void
PackagesJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_configuration = configurationMap;
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( PackagesJobFactory, registerPlugin< PackagesJob >(); )
