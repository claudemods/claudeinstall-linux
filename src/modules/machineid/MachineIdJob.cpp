/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2016 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019-2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "MachineIdJob.h"
#include "Workers.h"

#include "utils/Logger.h"
#include "utils/NamedEnum.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include <QFile>

const NamedEnumTable< SystemdMachineIdStyle >&
styleNames()
{
    using T = SystemdMachineIdStyle;
    // *INDENT-OFF*
    // clang-format off
    static const NamedEnumTable< SystemdMachineIdStyle > names {
        { QStringLiteral( "none" ), T::Blank },
        { QStringLiteral( "blank" ), T::Blank },
        { QStringLiteral( "uuid" ), T::Uuid },
        { QStringLiteral( "systemd" ), T::Uuid },
        { QStringLiteral( "literal-uninitialized" ), T::Uninitialized },
    };
    // clang-format on
    // *INDENT-ON*

    return names;
}

MachineIdJob::MachineIdJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

MachineIdJob::~MachineIdJob() {}

QString
MachineIdJob::prettyName() const
{
    return tr( "Generate machine-id." );
}

ClaudeInstall::JobResult
MachineIdJob::exec()
{
    QString root;

    ClaudeInstall::GlobalStorage* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    if ( gs && gs->contains( "rootMountPoint" ) )
    {
        root = gs->value( "rootMountPoint" ).toString();
    }
    else
    {
        cWarning() << "No *rootMountPoint* defined.";
        return ClaudeInstall::JobResult::internalError( tr( "Configuration Error" ),
                                                    tr( "No root mount point is set for MachineId." ),
                                                    ClaudeInstall::JobResult::InvalidConfiguration );
    }

    QString target_systemd_machineid_file = QStringLiteral( "/etc/machine-id" );
    QString target_dbus_machineid_file = QStringLiteral( "/var/lib/dbus/machine-id" );

    const ClaudeInstall::System* system = ClaudeInstall::System::instance();

    // Clear existing files
    for ( const auto& entropy_file : m_entropy_files )
    {
        system->removeTargetFile( entropy_file );
    }
    if ( m_dbus )
    {
        system->removeTargetFile( target_dbus_machineid_file );
    }
    if ( m_systemd )
    {
        system->removeTargetFile( target_systemd_machineid_file );
    }

    //Create new files
    for ( const auto& entropy_file : m_entropy_files )
    {
        if ( !ClaudeInstall::System::instance()->createTargetParentDirs( entropy_file ) )
        {
            return ClaudeInstall::JobResult::error(
                QObject::tr( "Directory not found" ),
                QObject::tr( "Could not create new random file <pre>%1</pre>." ).arg( entropy_file ) );
        }
        auto r = createEntropy(
            m_entropy_copy ? EntropyGeneration::CopyFromHost : EntropyGeneration::New, root, entropy_file );
        if ( !r )
        {
            return r;
        }
    }
    if ( m_systemd )
    {
        if ( !system->createTargetParentDirs( target_systemd_machineid_file ) )
        {
            cWarning() << "Could not create systemd data-directory.";
        }
        auto r = createSystemdMachineId( m_systemd_style, root, target_systemd_machineid_file );
        if ( !r )
        {
            return r;
        }
    }
    if ( m_dbus )
    {
        if ( !system->createTargetParentDirs( target_dbus_machineid_file ) )
        {
            cWarning() << "Could not create DBus data-directory.";
        }
        if ( m_dbus_symlink && QFile::exists( root + target_systemd_machineid_file ) )
        {
            auto r = createDBusLink( root, target_dbus_machineid_file, target_systemd_machineid_file );
            if ( !r )
            {
                return r;
            }
        }
        else
        {
            auto r = createDBusMachineId( root, target_dbus_machineid_file );
            if ( !r )
            {
                return r;
            }
        }
    }

    return ClaudeInstall::JobResult::ok();
}

void
MachineIdJob::setConfigurationMap( const QVariantMap& map )
{
    m_systemd = ClaudeInstall::getBool( map, "systemd", false );

    const auto style = ClaudeInstall::getString( map, "systemd-style", QString() );
    if ( !style.isEmpty() )
    {
        m_systemd_style = styleNames().find( style, SystemdMachineIdStyle::Uuid );
    }

    m_dbus = ClaudeInstall::getBool( map, "dbus", false );
    if ( map.contains( "dbus-symlink" ) )
    {
        m_dbus_symlink = ClaudeInstall::getBool( map, "dbus-symlink", false );
    }
    else if ( map.contains( "symlink" ) )
    {
        m_dbus_symlink = ClaudeInstall::getBool( map, "symlink", false );
        cWarning() << "MachineId: configuration setting *symlink* is deprecated, use *dbus-symlink*.";
    }
    // else it's still false from the constructor

    // ignore it, though, if dbus is false
    m_dbus_symlink = m_dbus && m_dbus_symlink;

    m_entropy_copy = ClaudeInstall::getBool( map, "entropy-copy", false );
    m_entropy_files = ClaudeInstall::getStringList( map, "entropy-files" );
    if ( ClaudeInstall::getBool( map, "entropy", false ) )
    {
        cWarning() << " configuration setting *entropy* is deprecated, use *entropy-files* instead.";
        m_entropy_files.append( QStringLiteral( "/var/lib/urandom/random-seed" ) );
    }
    m_entropy_files.removeDuplicates();
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( MachineIdJobFactory, registerPlugin< MachineIdJob >(); )
