/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2019-2020 Collabora Ltd
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "MountJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "compat/Variant.h"
#include "partition/Mount.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <algorithm>

using ClaudeInstall::System;
static const auto RunInHost = System::RunLocation::RunInHost;

static QString
diskNameForPartition( const QVariantMap& partition )
{
    static const QRegularExpression mmcPartition( QStringLiteral( "p[0-9]+$" ) );
    static const QRegularExpression plainPartition( QStringLiteral( "[0-9]+$" ) );

    QString name = QFileInfo( partition.value( "device" ).toString() ).fileName();
    // Typical mmc device is mmcblk0p1, nvme looks like nvme0n1p2
    name.remove( name.startsWith( "mmcblk" ) || name.startsWith( "nvme" ) ? mmcPartition : plainPartition );
    return name;
}

/** @brief Checks if @p partition is on an ssd disk */
static bool
isSsdDisk( const QVariantMap& partition )
{
    QFile f( "/sys/block/" + diskNameForPartition( partition ) + "/queue/rotational" );
    return f.open( QIODevice::ReadOnly ) && f.readAll() == "0\n";
}

/// Runs @p command in the host and returns true if it exited normally
static bool
hostCommand( const QStringList& command, const QString& input = QString() )
{
    return System::runCommand( RunInHost, command, QString(), input ).getExitCode() == 0;
}

MountJob::MountJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

MountJob::~MountJob() {}

QString
MountJob::prettyName() const
{
    return tr( "Mounting partitions…", "@status" );
}

QString
MountJob::mountOptions( const QString& filesystem, const QVariantMap& partition, const QString& efiLocation ) const
{
    // Extra mounts can optionally have "options" set, in this case, they override all other settings
    if ( partition.contains( "options" ) )
    {
        return partition.value( "options" ).toStringList().join( ',' );
    }

    // If there are no mount options defined then we use the defaults
    if ( !m_configuration.contains( "mountOptions" ) || m_configuration.value( "mountOptions" ).isNull() )
    {
        return QStringLiteral( "defaults" );
    }
    const QVariantList configured = m_configuration.value( "mountOptions" ).toList();

    // The EFI partition uses special mounting options
    const QString effectiveFilesystem
        = ( !efiLocation.isEmpty() && partition.value( "mountPoint" ).toString() == efiLocation )
        ? QStringLiteral( "efi" )
        : filesystem;

    auto find = [ &configured ]( const QString& fs ) -> QVariantMap
    {
        for ( const auto& o : configured )
        {
            const QVariantMap m = o.toMap();
            if ( m.value( "filesystem" ).toString() == fs )
            {
                return m;
            }
        }
        return QVariantMap();
    };

    QVariantMap options = find( effectiveFilesystem );
    // If there is no match then check for default options
    if ( options.isEmpty() )
    {
        options = find( QStringLiteral( "default" ) );
    }
    // If it is still empty, then fall back to returning defaults
    if ( options.isEmpty() )
    {
        return QStringLiteral( "defaults" );
    }

    QStringList items = options.value( "options" ).toStringList();
    // Append the appropriate options for ssd or hdd if set
    items.append( options.value( isSsdDisk( partition ) ? "ssdOptions" : "hddOptions" ).toStringList() );

    return items.isEmpty() ? QStringLiteral( "defaults" ) : items.join( ',' );
}

QVariantList
MountJob::btrfsSubvolumes( const QVariantList& partitions ) const
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();

    // Warn if there's no configuration at all, and empty configurations are
    // replaced by a simple root-only layout.
    if ( !m_configuration.contains( "btrfsSubvolumes" ) )
    {
        cWarning() << "No configuration for btrfsSubvolumes";
    }
    QVariantList subvolumes = m_configuration.value( "btrfsSubvolumes" ).toList();
    if ( subvolumes.isEmpty() )
    {
        const auto make = []( const char* mountPoint, const char* subvolume )
        {
            QVariantMap m;
            m[ "mountPoint" ] = mountPoint;
            m[ "subvolume" ] = subvolume;
            return QVariant( m );
        };
        subvolumes = QVariantList { make( "/", "/@" ), make( "/home", "/@home" ) };
    }

    // Filter out the subvolumes which have a dedicated partition
    QStringList nonRootPartitionMounts;
    for ( const auto& p : partitions )
    {
        const QString m = p.toMap().value( "mountPoint" ).toString();
        if ( !m.isEmpty() && m != "/" )
        {
            nonRootPartitionMounts.append( m );
        }
    }
    QVariantList result;
    for ( const auto& s : subvolumes )
    {
        if ( !nonRootPartitionMounts.contains( s.toMap().value( "mountPoint" ).toString() ) )
        {
            result.append( s );
        }
    }

    // If we have a swap **file**, give it a separate subvolume.
    if ( gs->value( "partitionChoices" ).toMap().value( "swap" ).toString() == "file" )
    {
        const QString swapSubvol = ClaudeInstall::getString( m_configuration, "btrfsSwapSubvol", QStringLiteral( "/@swap" ) );
        QVariantMap m;
        m[ "mountPoint" ] = "/swap";
        m[ "subvolume" ] = swapSubvol;
        result.append( m );
        gs->insert( "btrfsSwapSubvol", swapSubvol );
    }
    return result;
}

QString
MountJob::mountZfs( const QString& rootMountPoint, const QVariantMap& partition ) const
{
    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    const QString mountPoint = partition.value( "mountPoint" ).toString();
    const QString internalError = tr( "Internal error mounting zfs datasets" );

    // Get the list of zpools from global storage
    const QVariantList zfsPoolList = gs->value( "zfsPoolInfo" ).toList();
    if ( zfsPoolList.isEmpty() )
    {
        cWarning() << "Failed to locate zfsPoolInfo data in global storage";
        return internalError;
    }

    // Find the zpool matching this partition
    QString poolName;
    QString dsName;
    for ( const auto& pv : zfsPoolList )
    {
        const QVariantMap pool = pv.toMap();
        if ( pool.value( "mountpoint" ).toString() == mountPoint )
        {
            poolName = pool.value( "poolName" ).toString();
            dsName = pool.value( "dsName" ).toString();
        }
    }
    if ( poolName.isEmpty() )
    {
        cWarning() << "No zpool for mount point" << mountPoint;
        return internalError;
    }

    // import the zpool
    if ( !hostCommand( { "zpool", "import", "-N", "-R", rootMountPoint, poolName } ) )
    {
        return tr( "Failed to import zpool" );
    }

    // Get the encryption information from global storage
    for ( const auto& iv : gs->value( "zfsInfo" ).toList() )
    {
        const QVariantMap info = iv.toMap();
        if ( info.value( "mountpoint" ).toString() == mountPoint && info.value( "encrypted" ).toBool() )
        {
            // The zpool is encrypted, we need to unlock it
            if ( !hostCommand( { "zfs", "load-key", poolName }, info.value( "passphrase" ).toString() ) )
            {
                return tr( "Failed to unlock zpool" );
            }
        }
    }

    if ( mountPoint == "/" )
    {
        // Get the zfs dataset list from global storage
        QVariantList zfs = gs->value( "zfsDatasets" ).toList();
        if ( zfs.isEmpty() )
        {
            cWarning() << "Failed to locate zfs dataset list";
            return internalError;
        }

        std::stable_sort( zfs.begin(),
                          zfs.end(),
                          []( const QVariant& a, const QVariant& b )
                          { return a.toMap().value( "mountpoint" ).toString() < b.toMap().value( "mountpoint" ).toString(); } );
        for ( const auto& dv : zfs )
        {
            const QVariantMap dataset = dv.toMap();
            const QVariant canMount = dataset.value( "canMount" );
            const bool mountable = canMount.toString() == "noauto"
                || ( ClaudeInstall::typeOf( canMount ) == ClaudeInstall::BoolVariantType && canMount.toBool() );
            if ( mountable
                 && !hostCommand( { "zfs", "mount", dataset.value( "zpool" ).toString() + '/' + dataset.value( "dsName" ).toString() } ) )
            {
                return tr( "Failed to set zfs mountpoint" );
            }
        }
    }
    else if ( !hostCommand( { "zfs", "mount", poolName + '/' + dsName } ) )
    {
        return tr( "Failed to set zfs mountpoint" );
    }
    return QString();
}

QString
MountJob::mountPartition( const QString& rootMountPoint,
                          const QVariantMap& partition,
                          const QVariantList& partitions,
                          const QString& efiLocation,
                          QVariantList& mountOptionsList ) const
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();

    const QString rawMountPoint = partition.value( "mountPoint" ).toString();
    if ( rawMountPoint.isEmpty() )
    {
        return QString();
    }

    // Create mount point by concatenation because rawMountPoint starts with a '/'.
    const QString mountPoint = rootMountPoint + rawMountPoint;
    QDir().mkpath( mountPoint );

    // Ensure that the created directory has the correct SELinux context on
    // SELinux-enabled systems. This fails harmlessly everywhere else.
    if ( System::runCommand( RunInHost, { "chcon", "--reference=" + rawMountPoint, mountPoint } ).getExitCode() != 0 )
    {
        cDebug() << "chcon did not change the SELinux context of" << mountPoint;
    }

    QString fstype = partition.value( "fs" ).toString().toLower();
    if ( fstype == "unformatted" )
    {
        return QString();
    }
    if ( fstype == "fat16" || fstype == "fat32" )
    {
        fstype = QStringLiteral( "vfat" );
    }

    auto deviceFor = []( const QVariantMap& p )
    {
        return p.contains( "luksMapperName" ) ? QDir( "/dev/mapper" ).filePath( p.value( "luksMapperName" ).toString() )
                                              : p.value( "device" ).toString();
    };
    QString device = deviceFor( partition );

    auto mountOptionsEntry = []( const QString& mp, const QString& options )
    {
        QVariantMap m;
        m[ "mountpoint" ] = mp;
        m[ "option_string" ] = options;
        return m;
    };

    if ( fstype == "zfs" )
    {
        const QString error = mountZfs( rootMountPoint, partition );
        if ( !error.isEmpty() )
        {
            return error;
        }
    }
    else
    {
        const QString options = mountOptions( fstype, partition, efiLocation );
        if ( ClaudeInstall::Partition::mount( device, mountPoint, fstype, options ) != 0 )
        {
            cWarning() << "Cannot mount" << device;
        }
        mountOptionsList.append( mountOptionsEntry( rawMountPoint, options ) );
    }

    // Special handling for btrfs subvolumes. Create the subvolumes listed in mount.conf
    if ( fstype == "btrfs" && rawMountPoint == "/" )
    {
        // Root has been mounted to btrfs volume -> create subvolumes from configuration
        const QVariantList subvolumes = btrfsSubvolumes( partitions );

        // Store created list in global storage so it can be used in the fstab module
        gs->insert( "btrfsSubvolumes", subvolumes );
        // Create the subvolumes that are in the completed list
        for ( const auto& sv : subvolumes )
        {
            const QVariantMap s = sv.toMap();
            const QString subvolume = s.value( "subvolume" ).toString();
            if ( subvolume.isEmpty() )
            {
                continue;
            }
            QDir().mkpath( rootMountPoint + QFileInfo( subvolume ).path() );
            if ( !hostCommand( { "btrfs", "subvolume", "create", rootMountPoint + subvolume } ) )
            {
                return tr( "Failed to create the btrfs subvolume <pre>%1</pre>." ).arg( subvolume );
            }
            if ( s.value( "mountPoint" ).toString() == "/" )
            {
                // insert the root subvolume into global storage
                gs->insert( "btrfsRootSubvolume", subvolume );
            }
        }
        if ( !hostCommand( { "umount", "-v", rootMountPoint } ) )
        {
            return tr( "Failed to unmount <pre>%1</pre> to mount the btrfs subvolumes." ).arg( rootMountPoint );
        }

        device = deviceFor( partition );

        // Mount the subvolumes
        const QString swapSubvol = ClaudeInstall::getString( m_configuration, "btrfsSwapSubvol", QStringLiteral( "/@swap" ) );
        for ( const auto& sv : subvolumes )
        {
            const QVariantMap s = sv.toMap();
            const QString subvolume = s.value( "subvolume" ).toString();
            const QString subvolumeMountPoint = s.value( "mountPoint" ).toString();

            const QString optionsNoSubvol = subvolume == swapSubvol ? mountOptions( "btrfs_swap", partition )
                                                                    : mountOptions( fstype, partition );

            // Only add subvol= argument if we are not mounting the entire filesystem
            const QString option
                = subvolume.isEmpty() ? optionsNoSubvol : QStringLiteral( "subvol=%1,%2" ).arg( subvolume, optionsNoSubvol );
            mountOptionsList.append( mountOptionsEntry( subvolumeMountPoint, optionsNoSubvol ) );
            if ( ClaudeInstall::Partition::mount( device, rootMountPoint + subvolumeMountPoint, fstype, option ) != 0 )
            {
                cWarning() << "Cannot mount" << device;
            }
        }
    }
    return QString();
}

ClaudeInstall::JobResult
MountJob::exec()
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    const QVariantList partitions = gs->value( "partitions" ).toList();

    if ( partitions.isEmpty() )
    {
        cWarning() << "partitions is empty";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ), tr( "No partitions are defined for <pre>%1</pre> to use." ).arg( "mount" ) );
    }

    // Find existing swap partitions that are part of the installation and enable them now
    for ( const auto& pv : partitions )
    {
        const QVariantMap p = pv.toMap();
        if ( p.value( "fs" ).toString() != "linuxswap" || !p.value( "claimed" ).toBool() )
        {
            continue;
        }
        const QString fsName = p.value( "fsName" ).toString();
        QString swapDevice;
        if ( fsName == "linuxswap" )
        {
            swapDevice = p.value( "device" ).toString();
        }
        else if ( fsName == "luks" || fsName == "luks2" )
        {
            swapDevice = "/dev/mapper/" + p.value( "luksMapperName" ).toString();
        }
        if ( !swapDevice.isEmpty() && !hostCommand( { "swapon", swapDevice } ) )
        {
            cWarning() << "Failed to enable swap for device" << swapDevice;
            break;
        }
    }

    QTemporaryDir rootDir( QDir::temp().filePath( "claudeinstall-root-XXXXXX" ) );
    if ( !rootDir.isValid() )
    {
        return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                            tr( "Could not create a temporary directory for the root mount point." ) );
    }
    rootDir.setAutoRemove( false );
    const QString rootMountPoint = rootDir.path();

    // Guard against missing keys (generally a sign that the config file is bad)
    QVariantList extraMounts = m_configuration.value( "extraMounts" ).toList();
    if ( extraMounts.isEmpty() )
    {
        cWarning() << "No extra mounts defined. Does mount.conf exist?";
    }

    QString efiLocation;
    if ( gs->value( "firmwareType" ).toString() == "efi" )
    {
        efiLocation = gs->value( "efiSystemPartition" ).toString();
    }
    else
    {
        QVariantList kept;
        for ( const auto& m : extraMounts )
        {
            if ( !ClaudeInstall::getBool( m.toMap(), "efi", false ) )
            {
                kept.append( m );
            }
        }
        extraMounts = kept;
    }

    // Add extra mounts to the partitions list and sort by mount points.
    // This way, we ensure / is mounted before the rest, and every mount point
    // is created on the right partition (e.g. if a partition is to be mounted
    // under /tmp, we make sure /tmp is mounted before the partition)
    QVariantList mountable;
    for ( const auto& p : partitions + extraMounts )
    {
        if ( !p.toMap().value( "mountPoint" ).toString().isEmpty() )
        {
            mountable.append( p );
        }
    }
    std::stable_sort( mountable.begin(),
                      mountable.end(),
                      []( const QVariant& a, const QVariant& b )
                      { return a.toMap().value( "mountPoint" ).toString() < b.toMap().value( "mountPoint" ).toString(); } );

    // mountOptionsList will be inserted into global storage for use in fstab later
    QVariantList mountOptionsList;
    for ( const auto& p : mountable )
    {
        const QString error = mountPartition( rootMountPoint, p.toMap(), partitions, efiLocation, mountOptionsList );
        if ( !error.isEmpty() )
        {
            return ClaudeInstall::JobResult::error( tr( "zfs mounting error" ), error );
        }
    }

    if ( mountOptionsList.isEmpty() )
    {
        cWarning() << "No mount options defined," << partitions.size() << "partitions," << mountable.size() << "mountable";
    }

    gs->insert( "rootMountPoint", rootMountPoint );
    gs->insert( "mountOptionsList", mountOptionsList );

    // Remember the extra mounts for the unpackfs module
    gs->insert( "extraMounts", extraMounts );

    return ClaudeInstall::JobResult::ok();
}

void
MountJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_configuration = configurationMap;
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( MountJobFactory, registerPlugin< MountJob >(); )
