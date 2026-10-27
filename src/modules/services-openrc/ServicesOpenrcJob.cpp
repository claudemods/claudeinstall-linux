/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2016 Artoo <artoo@manjaro.org>
 *   SPDX-FileCopyrightText: 2017 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ServicesOpenrcJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "compat/Variant.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include <QCoreApplication>
#include <QFileInfo>

ServicesOpenrcJob::ServicesOpenrcJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

ServicesOpenrcJob::~ServicesOpenrcJob() {}

QString
ServicesOpenrcJob::prettyName() const
{
    return tr( "Configure OpenRC services", "@status" );
}

static QString
failureDescription( const QString& state, const QString& name, const QString& runlevel )
{
    if ( state == "add" )
    {
        return QCoreApplication::translate( "ServicesOpenrcJob", "Cannot add service %1 to run-level %2." )
            .arg( name, runlevel );
    }
    if ( state == "del" )
    {
        return QCoreApplication::translate( "ServicesOpenrcJob", "Cannot remove service %1 from run-level %2." )
            .arg( name, runlevel );
    }
    return QCoreApplication::translate( "ServicesOpenrcJob",
                                        "Unknown service-action <code>%1</code> for service %2 in run-level %3." )
        .arg( state, name, runlevel );
}

ClaudeInstall::JobResult
ServicesOpenrcJob::update( const QString& state, const QVariantList& services ) const
{
    for ( const auto& svc : services )
    {
        QString name;
        QString runlevel = QStringLiteral( "default" );
        bool mandatory = false;
        if ( ClaudeInstall::typeOf( svc ) == ClaudeInstall::StringVariantType )
        {
            name = svc.toString();
        }
        else
        {
            const QVariantMap m = svc.toMap();
            name = ClaudeInstall::getString( m, "name" );
            runlevel = ClaudeInstall::getString( m, "runlevel", runlevel );
            mandatory = ClaudeInstall::getBool( m, "mandatory", false );
        }

        const QString servicePath = m_root + m_initdDir + '/' + name;
        const QString runlevelPath = m_root + m_runlevelsDir + '/' + runlevel;

        if ( !QFileInfo::exists( servicePath ) )
        {
            cWarning() << "Target service" << name << "does not exist in" << m_initdDir;
            if ( mandatory )
            {
                return ClaudeInstall::JobResult::error(
                    tr( "Target service does not exist" ),
                    failureDescription( state, name, runlevel ) + ' '
                        + tr( "The path for service %1 is <code>%2</code>, which does not exist." )
                              .arg( name, servicePath ) );
            }
            continue;
        }
        if ( !QFileInfo::exists( runlevelPath ) )
        {
            cWarning() << "Target runlevel" << runlevel << "does not exist for" << name;
            if ( mandatory )
            {
                return ClaudeInstall::JobResult::error(
                    tr( "Target runlevel does not exist" ),
                    failureDescription( state, name, runlevel ) + ' '
                        + tr( "The path for runlevel %1 is <code>%2</code>, which does not exist." )
                              .arg( runlevel, runlevelPath ) );
            }
            continue;
        }

        const int ec = ClaudeInstall::System::instance()->targetEnvCall( { "rc-update", state, name, runlevel } );
        if ( ec != 0 )
        {
            cWarning() << "Cannot" << state << "service" << name << "to" << runlevel;
            cWarning() << "rc-update returned error code" << ec;
            if ( mandatory )
            {
                return ClaudeInstall::JobResult::error(
                    tr( "Cannot modify service" ),
                    failureDescription( state, name, runlevel ) + ' '
                        + tr( "<code>rc-update %1</code> call in chroot returned error code %2." )
                              .arg( state )
                              .arg( ec ) );
            }
        }
    }
    return ClaudeInstall::JobResult::ok();
}

ClaudeInstall::JobResult
ServicesOpenrcJob::exec()
{
    auto r = update( QStringLiteral( "add" ), m_add );
    if ( !r )
    {
        return r;
    }
    return update( QStringLiteral( "del" ), m_del );
}

void
ServicesOpenrcJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_root = ClaudeInstall::JobQueue::instance()->globalStorage()->value( "rootMountPoint" ).toString();
    m_add = configurationMap.value( "services" ).toList();
    m_del = configurationMap.value( "disable" ).toList();
    m_initdDir = ClaudeInstall::getString( configurationMap, "initdDir" );
    m_runlevelsDir = ClaudeInstall::getString( configurationMap, "runlevelsDir" );
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( ServicesOpenrcJobFactory, registerPlugin< ServicesOpenrcJob >(); )
