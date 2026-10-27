/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 * Tags from the Python version of this module:
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2016 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 * Tags for the C++ version of this module:
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "UmountJob.h"

#include "partition/Mount.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include <QCoreApplication>
#include <QDir>
#include <QList>

UmountJob::UmountJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

UmountJob::~UmountJob() {}

QString
UmountJob::prettyName() const
{
    return tr( "Unmounting file systems…", "@status" );
}

static ClaudeInstall::JobResult
unmountTargetMounts( const QString& rootMountPoint )
{
    QDir targetMount( rootMountPoint );
    if ( !targetMount.exists() )
    {
        return ClaudeInstall::JobResult::internalError(
            QCoreApplication::translate( UmountJob::staticMetaObject.className(), "Could not unmount target system." ),
            QCoreApplication::translate( UmountJob::staticMetaObject.className(),
                                         "The target system is not mounted at '%1'." )
                .arg( rootMountPoint ),
            ClaudeInstall::JobResult::GenericError );
    }
    QString targetMountPath = targetMount.absolutePath();
    if ( !targetMountPath.endsWith( '/' ) )
    {
        targetMountPath.append( '/' );
    }

    using MtabInfo = ClaudeInstall::Partition::MtabInfo;
    auto targetMounts = MtabInfo::fromMtabFilteredByPrefix( targetMountPath );
    std::sort( targetMounts.begin(), targetMounts.end(), MtabInfo::mountPointOrder );

    cDebug() << "Read" << targetMounts.count() << "entries from" << targetMountPath;
    for ( const auto& m : std::as_const( targetMounts ) )
    {
        // Returns the program's exit code, so 0 is success and non-0
        // (truthy) is a failure.
        if ( ClaudeInstall::Partition::unmount( m.mountPoint, { "-lv" } ) )
        {
            return ClaudeInstall::JobResult::error(
                QCoreApplication::translate( UmountJob::staticMetaObject.className(),
                                             "Could not unmount target system." ),
                QCoreApplication::translate( UmountJob::staticMetaObject.className(),
                                             "The device '%1' is mounted in the target system. It is mounted at '%2'. "
                                             "The device could not be unmounted." )
                    .arg( m.device, m.mountPoint ) );
        }
    }

    // Last we unmount the root
    if ( ClaudeInstall::Partition::unmount( rootMountPoint, { "-lv" } ) )
    {
        return ClaudeInstall::JobResult::error(
            QCoreApplication::translate( UmountJob::staticMetaObject.className(),
                                         "Could not unmount the root of the target system." ),
            QCoreApplication::translate( UmountJob::staticMetaObject.className(),
                                         "The device mounted at '%1' could not be unmounted." )
                .arg( rootMountPoint ) );
    }

    return ClaudeInstall::JobResult::ok();
}

static ClaudeInstall::JobResult
exportZFSPools()
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    QStringList poolNames;
    {
        // The pools are dictionaries / VariantMaps
        auto zfs_pool_list = gs->value( "zfsPoolInfo" ).toList();
        for ( const auto& v : zfs_pool_list )
        {
            auto m = v.toMap();
            QString poolName = m.value( "poolName" ).toString();
            if ( !poolName.isEmpty() )
            {
                poolNames.append( poolName );
            }
        }
        poolNames.sort();
    }

    for ( const auto& poolName : poolNames )
    {
        auto result = ClaudeInstall::System::runCommand( { "zpool", "export", poolName }, std::chrono::seconds( 30 ) );
        if ( result.getExitCode() )
        {
            cWarning() << "Failed to export pool" << result.getOutput();
        }
    }
    // Exporting ZFS pools does not cause the install to fail
    return ClaudeInstall::JobResult::ok();
}

ClaudeInstall::JobResult
UmountJob::exec()
{
    const auto* sys = ClaudeInstall::System::instance();
    if ( !sys )
    {
        return ClaudeInstall::JobResult::internalError(
            "UMount", tr( "No target system available." ), ClaudeInstall::JobResult::InvalidConfiguration );
    }

    ClaudeInstall::GlobalStorage* gs
        = ClaudeInstall::JobQueue::instance() ? ClaudeInstall::JobQueue::instance()->globalStorage() : nullptr;
    if ( !gs || gs->value( "rootMountPoint" ).toString().isEmpty() )
    {
        return ClaudeInstall::JobResult::internalError(
            "UMount", tr( "No rootMountPoint is set." ), ClaudeInstall::JobResult::InvalidConfiguration );
    }

    // Do the unmounting of target-system filesystems
    {
        auto r = unmountTargetMounts( gs->value( "rootMountPoint" ).toString() );
        if ( !r )
        {
            return r;
        }
    }

    // For ZFS systems, export the pools
    {
        auto r = exportZFSPools();
        if ( !r )
        {
            return r;
        }
    }

    return ClaudeInstall::JobResult::ok();
}

void
UmountJob::setConfigurationMap( const QVariantMap& map )
{
    Q_UNUSED( map )
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( UmountJobFactory, registerPlugin< UmountJob >(); )
