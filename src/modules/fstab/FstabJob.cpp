/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2014 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2014 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2016 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "FstabJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <sys/stat.h>

static const char fstabHeader[]
    = "# /etc/fstab: static file system information.\n"
      "#\n"
      "# Use 'blkid' to print the universally unique identifier for a device; this may\n"
      "# be used with UUID= as a more robust way to name devices that works even if\n"
      "# disks are added and removed. See fstab(5).\n"
      "#\n"
      "# <file system>             <mount point>  <type>  <options>  <dump>  <pass>\n";

static const char crypttabHeader[]
    = "# /etc/crypttab: mappings for encrypted partitions.\n"
      "#\n"
      "# Each mapped device will be created in /dev/mapper, so your /etc/fstab\n"
      "# should use the /dev/mapper/<name> paths for encrypted devices.\n"
      "#\n"
      "# See crypttab(5) for the supported syntax.\n"
      "#\n"
      "# NOTE: You need not list your root (/) partition here, but it must be set up\n"
      "#       beforehand by the initramfs (/etc/mkinitcpio.conf). The same applies\n"
      "#       to encrypted swap, which should be set up with mkinitcpio-openswap\n"
      "#       for resume support.\n"
      "#\n"
      "# <name>               <device>                         <password> <options>\n";

/** @brief Turns Parted filesystem names into fstab names */
static QString
fstabFsName( const QString& fs )
{
    const QString lower = fs.toLower();
    if ( lower == "fat16" || lower == "fat32" )
    {
        return QStringLiteral( "vfat" );
    }
    if ( lower == "linuxswap" )
    {
        return QStringLiteral( "swap" );
    }
    return lower;
}

/** @brief Does the sysfs entry for @p diskName say it's an SSD? */
static bool
isSsdDisk( const QString& diskName )
{
    QFile f( "/sys/block/" + diskName + "/queue/rotational" );
    if ( !f.open( QIODevice::ReadOnly ) )
    {
        // Should not happen unless sysfs changes, but better safe than sorry
        return false;
    }
    return f.readAll() == "0\n";
}

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

static QString
padRight( const QString& s, int width )
{
    return s.leftJustified( width, ' ', false );
}

FstabJob::FstabJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

FstabJob::~FstabJob() {}

QString
FstabJob::prettyName() const
{
    return tr( "Writing fstab…", "@status" );
}

bool
FstabJob::findSsdDisks()
{
    QSet< QString > disks;
    for ( const auto& p : m_partitions )
    {
        disks.insert( diskNameForPartition( p.toMap() ) );
    }
    m_ssdDisks.clear();
    for ( const QString& d : disks )
    {
        if ( isSsdDisk( d ) )
        {
            m_ssdDisks.insert( d );
        }
    }
    return true;
}

QString
FstabJob::mountOptions( const QString& mountPoint ) const
{
    for ( const auto& o : m_mountOptionsList )
    {
        const QVariantMap item = o.toMap();
        if ( item.value( "mountpoint" ).toString() == mountPoint )
        {
            return ClaudeInstall::getString( item, "option_string", QStringLiteral( "defaults" ) );
        }
    }
    return QStringLiteral( "defaults" );
}

bool
FstabJob::crypttabLineInfo( const QVariantMap& partition, CrypttabLine& line ) const
{
    const QString mapperName = partition.value( "luksMapperName" ).toString();
    const QString luksUuid = partition.value( "luksUuid" ).toString();
    if ( mapperName.isEmpty() || luksUuid.isEmpty() )
    {
        return false;
    }

    QString options = m_crypttabOptions;
    // Make sure to not use missing keyfile
    QString password = QFileInfo( QDir( m_rootMountPoint ).filePath( "crypto_keyfile.bin" ) ).isFile()
        ? QStringLiteral( "/crypto_keyfile.bin" )
        : QStringLiteral( "none" );

    bool rootUnencrypted = false;
    bool bootUnencrypted = false;
    for ( const auto& pv : m_partitions )
    {
        const QVariantMap p = pv.toMap();
        const QString mp = p.value( "mountPoint" ).toString();
        if ( mp == "/" && !p.contains( "luksMapperName" ) )
        {
            rootUnencrypted = true;
        }
        if ( mp == "/boot" && !p.contains( "luksMapperName" ) )
        {
            bootUnencrypted = true;
        }
    }

    // Set crypttab password for partition to none and remove crypttab options
    // if root partition was not encrypted
    if ( rootUnencrypted )
    {
        password = QStringLiteral( "none" );
        options.clear();
    }
    // on root partition when /boot is unencrypted
    else if ( partition.value( "mountPoint" ).toString() == "/" && bootUnencrypted )
    {
        password = QStringLiteral( "none" );
        options.clear();
    }

    line.name = mapperName;
    line.device = "UUID=" + luksUuid;
    line.password = password;
    line.options = options;
    return true;
}

bool
FstabJob::generateCrypttab() const
{
    QDir().mkpath( QDir( m_rootMountPoint ).filePath( "etc" ) );
    QFile f( QDir( m_rootMountPoint ).filePath( "etc/crypttab" ) );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
    {
        return false;
    }
    f.write( crypttabHeader );

    for ( const auto& p : m_partitions )
    {
        CrypttabLine line;
        if ( crypttabLineInfo( p.toMap(), line ) )
        {
            f.write( QStringLiteral( "%1 %2 %3 %4\n" )
                         .arg( padRight( line.name, 21 ), padRight( line.device, 45 ), line.password, line.options )
                         .toUtf8() );
        }
    }
    return true;
}

bool
FstabJob::fstabLineInfo( const QVariantMap& partition, FstabLine& line ) const
{
    // Some "fs" names need special handling in /etc/fstab, so remap them.
    const QString filesystem = fstabFsName( partition.value( "fs" ).toString() );
    const QString luksMapperName = partition.value( "luksMapperName" ).toString();
    QString mountPoint = partition.value( "mountPoint" ).toString();
    const QString diskName = diskNameForPartition( partition );
    const bool isSsd = m_ssdDisks.contains( diskName );

    // Swap partitions are called "linuxswap" by parted. That "fs" is visible in GS,
    // but that gets mapped to "swap", above, because that's the spelling needed in /etc/fstab
    if ( mountPoint.isEmpty() && filesystem != "swap" )
    {
        return false;
    }
    if ( mountPoint.isEmpty() )
    {
        mountPoint = QStringLiteral( "swap" );
    }

    if ( filesystem == "swap" && !partition.value( "claimed" ).toBool() )
    {
        cDebug() << "Ignoring foreign swap" << diskName << partition.value( "uuid" ).toString();
        return false;
    }

    QString options = mountOptions( mountPoint );

    int check = 0;
    if ( mountPoint == "/" && filesystem != "btrfs" )
    {
        check = 1;
    }
    else if ( mountPoint != "swap" && filesystem != "btrfs" )
    {
        check = 2;
    }

    if ( mountPoint == "/" )
    {
        m_rootIsSsd = isSsd;
    }

    // If there's a set-and-not-empty subvolume set, add it
    const QString subvol = partition.value( "subvol" ).toString();
    if ( filesystem == "btrfs" && !subvol.isEmpty() )
    {
        options = QStringLiteral( "subvol=%1," ).arg( subvol ) + options;
    }

    QString device;
    if ( !luksMapperName.isEmpty() )
    {
        device = "/dev/mapper/" + luksMapperName;
    }
    else if ( !partition.value( "uuid" ).toString().isEmpty() )
    {
        device = "UUID=" + partition.value( "uuid" ).toString();
    }
    else
    {
        device = partition.value( "device" ).toString();
    }

    if ( device.isEmpty() )
    {
        // We get here when the user mounted a previously encrypted partition
        return false;
    }

    line.device = device;
    line.mountPoint = mountPoint;
    line.fs = filesystem;
    line.options = options;
    line.check = check;
    return true;
}

static QByteArray
formatFstabLine( const QString& device, const QString& mountPoint, const QString& fs, const QString& options, int check )
{
    return QStringLiteral( "%1 %2 %3 %4 0 %5\n" )
        .arg( padRight( device, 41 ), padRight( mountPoint, 14 ), padRight( fs, 7 ), padRight( options, 10 ) )
        .arg( check )
        .toUtf8();
}

bool
FstabJob::generateFstab() const
{
    QDir().mkpath( QDir( m_rootMountPoint ).filePath( "etc" ) );
    QFile f( QDir( m_rootMountPoint ).filePath( "etc/fstab" ) );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
    {
        return false;
    }
    f.write( fstabHeader );

    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    for ( const auto& pv : m_partitions )
    {
        const QVariantMap partition = pv.toMap();
        const QString fs = partition.value( "fs" ).toString();
        FstabLine line;

        // Special treatment for btrfs subvolumes
        if ( fs == "btrfs" && partition.value( "mountPoint" ).toString() == "/" )
        {
            // Subvolume list has been created in mount.conf and curated in mount module,
            // so all subvolumes here should be safe to add to fstab
            for ( const auto& sv : gs->value( "btrfsSubvolumes" ).toList() )
            {
                const QVariantMap s = sv.toMap();
                QVariantMap entry = partition;
                entry[ "mountPoint" ] = s.value( "mountPoint" );
                entry[ "subvol" ] = s.value( "subvolume" );
                if ( fstabLineInfo( entry, line ) )
                {
                    f.write( formatFstabLine( line.device, line.mountPoint, line.fs, line.options, line.check ) );
                }
            }
        }
        else if ( fs != "zfs" )  // zfs partitions don't need an entry in fstab
        {
            if ( fstabLineInfo( partition, line ) )
            {
                f.write( formatFstabLine( line.device, line.mountPoint, line.fs, line.options, line.check ) );
            }
        }
    }

    if ( m_rootIsSsd )
    {
        // Old behavior was to mount /tmp as tmpfs
        // New behavior is to use tmpOptions to decide
        // if mounting /tmp as tmpfs and which options to use
        QVariantMap ssd = m_tmpOptions.value( "ssd" ).toMap();
        if ( ssd.isEmpty() )
        {
            ssd = m_tmpOptions.value( "default" ).toMap();
        }
        // Default to true to mimic old behavior
        if ( ClaudeInstall::getBool( ssd, "tmpfs", true ) )
        {
            const QString options = ClaudeInstall::getString( ssd, "options", QStringLiteral( "defaults,noatime,mode=1777" ) );
            // Mount /tmp on a tmpfs
            f.write( formatFstabLine( "tmpfs", "/tmp", "tmpfs", options, 0 ) );
        }
    }
    return true;
}

bool
FstabJob::createMountPoints() const
{
    for ( const auto& pv : m_partitions )
    {
        const QString mp = pv.toMap().value( "mountPoint" ).toString();
        if ( !mp.isEmpty() )
        {
            QDir().mkpath( m_rootMountPoint + mp );
        }
    }
    return true;
}

QString
FstabJob::createSwapfile( bool rootBtrfs )
{
    // The swapfile-creation covers progress from 0.2 to 0.5
    emit progress( 0.2 );

    QString swapfilePath;
    if ( rootBtrfs )
    {
        // btrfs swapfiles must reside on a subvolume that is not snapshotted to prevent file system corruption
        swapfilePath = QDir( m_rootMountPoint ).filePath( "swap/swapfile" );
        QFile( swapfilePath ).open( QIODevice::WriteOnly | QIODevice::Truncate );
        // No Copy-on-Write, no compression
        ClaudeInstall::System::runCommand( ClaudeInstall::System::RunLocation::RunInHost, { "chattr", "+C", "+m", swapfilePath } );
    }
    else
    {
        swapfilePath = QDir( m_rootMountPoint ).filePath( "swapfile" );
        QFile( swapfilePath ).open( QIODevice::WriteOnly | QIODevice::Truncate );
    }

    // Create the swapfile; swapfiles are small-ish
    const QByteArray zeroes( 16384, '\0' );
    const qint64 desiredSize = qint64( 512 ) * 1024 * 1024;  // 512MiB
    {
        QFile f( swapfilePath );
        if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
        {
            return tr( "Could not create the swap file <pre>%1</pre>." ).arg( swapfilePath );
        }
        qint64 total = 0;
        while ( total < desiredSize )
        {
            const qint64 chunk = f.write( zeroes );
            if ( chunk < 1 )
            {
                cDebug() << "Short write on" << swapfilePath << ", cancelling.";
                break;
            }
            emit progress( 0.2 + 0.3 * ( double( total ) / double( desiredSize ) ) );
            total += chunk;
        }
    }
    ::chmod( swapfilePath.toLocal8Bit().constData(), 0600 );
    ClaudeInstall::System::runCommand( ClaudeInstall::System::RunLocation::RunInHost, { "mkswap", swapfilePath } );
    emit progress( 0.5 );
    return QString();
}

ClaudeInstall::JobResult
FstabJob::exec()
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    m_partitions = gs->value( "partitions" ).toList();
    m_rootMountPoint = gs->value( "rootMountPoint" ).toString();

    if ( m_partitions.isEmpty() )
    {
        cWarning() << "partitions is empty";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ), tr( "No partitions are defined for <pre>%1</pre> to use." ).arg( "fstab" ) );
    }
    if ( m_rootMountPoint.isEmpty() )
    {
        cWarning() << "rootMountPoint is empty";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ), tr( "No root mount point is given for <pre>%1</pre> to use." ).arg( "fstab" ) );
    }

    // This follows the GS settings from the partition module's Config object
    bool swapFile = false;
    bool rootBtrfs = false;
    if ( gs->value( "partitionChoices" ).toMap().value( "swap" ).toString() == "file" )
    {
        swapFile = true;
        for ( const auto& pv : m_partitions )
        {
            const QVariantMap p = pv.toMap();
            if ( p.value( "mountPoint" ).toString() == "/" )
            {
                rootBtrfs = p.value( "fs" ).toString().toLower() == "btrfs";
                break;  // only the first root partition counts
            }
        }

        // There's no formatted partition for it, so we'll sneak in an entry
        QVariantMap swapEntry;
        swapEntry[ "fs" ] = "swap";
        swapEntry[ "mountPoint" ] = QVariant();
        swapEntry[ "claimed" ] = true;
        swapEntry[ "device" ] = rootBtrfs ? "/swap/swapfile" : "/swapfile";
        swapEntry[ "uuid" ] = QVariant();
        m_partitions.append( swapEntry );
    }

    emit progress( 0.1 );
    m_mountOptionsList = gs->value( "mountOptionsList" ).toList();

    // We rely on mount_options having a default; if there wasn't one,
    // bail out with a meaningful error.
    if ( m_mountOptionsList.isEmpty() )
    {
        cWarning() << "No mount options defined," << m_partitions.size() << "partitions";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ),
            tr( "No <pre>%1</pre> configuration is given for <pre>%2</pre> to use." ).arg( "mountOptions", "fstab" ) );
    }

    if ( swapFile )
    {
        emit progress( 0.2 );
        const QString error = createSwapfile( rootBtrfs );
        if ( !error.isEmpty() )
        {
            emit progress( 1.0 );
            return ClaudeInstall::JobResult::error( tr( "Configuration Error" ), error );
        }
    }

    emit progress( 0.5 );
    findSsdDisks();
    const bool ok = generateFstab() && generateCrypttab() && createMountPoints();
    emit progress( 1.0 );

    if ( !ok )
    {
        return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                            tr( "Could not write fstab or crypttab in the target." ) );
    }
    return ClaudeInstall::JobResult::ok();
}

void
FstabJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_crypttabOptions = ClaudeInstall::getString( configurationMap, "crypttabOptions", QStringLiteral( "luks" ) );
    m_tmpOptions = configurationMap.value( "tmpOptions" ).toMap();
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( FstabJobFactory, registerPlugin< FstabJob >(); )
