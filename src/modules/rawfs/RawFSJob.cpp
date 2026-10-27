/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd <arnaud.ferraris@collabora.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "RawFSJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include <QFile>
#include <QFileInfo>

#include <numeric>

#include <sys/stat.h>

namespace
{
/** @brief One raw copy: @p source (image, device, mount point) to @p destination device */
struct RawFSItem
{
    QString source;
    QString destination;
    QString filesystem;
    bool resize = false;
};

/** @brief Is @p path a mount point? (same test as Python's os.path.ismount) */
bool
isMount( const QString& path )
{
    struct stat st, parent;
    if ( ::stat( path.toLocal8Bit().constData(), &st ) != 0
         || ::stat( ( path + QStringLiteral( "/.." ) ).toLocal8Bit().constData(), &parent ) != 0 )
    {
        return false;
    }
    return st.st_dev != parent.st_dev || st.st_ino == parent.st_ino;
}

qint64
readSysfsNumber( const QString& path )
{
    QFile f( path );
    if ( !f.open( QIODevice::ReadOnly ) )
    {
        return -1;
    }
    bool ok = false;
    const qint64 n = f.readLine().trimmed().toLongLong( &ok );
    return ok ? n : -1;
}

/** @brief Gets a filesystem's total size and block size, in bytes.
 *
 * For block devices, the block size is the device's block size.
 * For other files (fs images), block size is 1 byte.
 * @return false if the size could not be determined.
 */
bool
getDeviceSize( const QString& device, qint64& size, qint64& blockSize )
{
    struct stat st;
    if ( ::stat( device.toLocal8Bit().constData(), &st ) != 0 )
    {
        return false;
    }

    if ( S_ISBLK( st.st_mode ) )
    {
        // Find the base device by chopping characters off the partition name
        const QString partition = QFileInfo( device ).fileName();
        QString baseDevice;
        for ( QString tmp = partition; !tmp.isEmpty(); )
        {
            tmp.chop( 1 );
            if ( !tmp.isEmpty() && QFileInfo::exists( "/sys/block/" + tmp ) )
            {
                baseDevice = tmp;
                break;
            }
        }
        blockSize = readSysfsNumber( "/sys/block/" + baseDevice + "/queue/hw_sector_size" );
        const qint64 sectors = readSysfsNumber( "/sys/block/" + baseDevice + '/' + partition + "/size" );
        if ( blockSize < 0 || sectors < 0 )
        {
            return false;
        }
        size = sectors * blockSize;
    }
    else
    {
        size = QFileInfo( device ).size();
        blockSize = 1;
    }
    return true;
}

RawFSItem
makeItem( const QVariantMap& config, const QString& device, const QString& fs, bool bogus )
{
    cDebug() << "Adding an entry for raw copy of" << config.value( "source" ).toString() << "to" << device;

    RawFSItem item;
    const QString configured = ClaudeInstall::getString( config, "source" );
    item.source = QFileInfo( configured ).canonicalFilePath();
    if ( item.source.isEmpty() )
    {
        item.source = configured;
    }

    // If source is a mount point, look for the actual device mounted on it
    if ( !bogus && isMount( item.source ) )
    {
        QFile procMounts( QStringLiteral( "/proc/mounts" ) );
        if ( procMounts.open( QIODevice::ReadOnly ) )
        {
            while ( !procMounts.atEnd() )
            {
                const QStringList fields
                    = QString::fromLocal8Bit( procMounts.readLine() ).split( ' ', Qt::SkipEmptyParts );
                if ( fields.contains( item.source ) )
                {
                    item.source = fields.first();
                    break;
                }
            }
        }
    }

    item.destination = device;
    item.filesystem = fs;
    item.resize = ClaudeInstall::getBool( config, "resize", false );
    return item;
}
}  // namespace

RawFSJob::RawFSJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

RawFSJob::~RawFSJob() {}

QString
RawFSJob::prettyName() const
{
    return tr( "Installing data…", "@status" );
}

ClaudeInstall::JobResult
RawFSJob::exec()
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    QVariantList partitions = gs->value( "partitions" ).toList();

    if ( partitions.isEmpty() )
    {
        cWarning() << "partitions is empty";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ), tr( "No partitions are defined for <pre>%1</pre> to use." ).arg( "rawfs" ) );
    }

    QList< RawFSItem > filesystems;
    cDebug() << "Copying" << partitions.size() << "raw partitions.";
    for ( const auto& p : partitions )
    {
        const QVariantMap partition = p.toMap();
        const QString mountPoint = partition.value( "mountPoint" ).toString();
        if ( mountPoint.isEmpty() )
        {
            continue;
        }
        for ( const auto& t : m_targets )
        {
            const QVariantMap target = t.toMap();
            if ( target.value( "mountPoint" ).toString() == mountPoint )
            {
                filesystems.append( makeItem(
                    target, partition.value( "device" ).toString(), partition.value( "fs" ).toString(), m_bogus ) );
            }
        }
    }

    for ( int index = 0; index < filesystems.size(); ++index )
    {
        const RawFSItem& item = filesystems.at( index );
        cDebug() << "Copying" << item.source << "to" << item.destination;

        if ( !m_bogus )
        {
            qint64 srcSize = 0, srcBlock = 1, destSize = 0, destBlock = 1;
            if ( !getDeviceSize( item.source, srcSize, srcBlock ) || !getDeviceSize( item.destination, destSize, destBlock ) )
            {
                return ClaudeInstall::JobResult::error(
                    tr( "Cannot determine size" ),
                    tr( "The size of %1 or of %2 could not be determined." ).arg( item.source, item.destination ) );
            }
            if ( destSize < srcSize )
            {
                return ClaudeInstall::JobResult::error(
                    tr( "Not enough free space" ),
                    tr( "%1 partition is too small to copy %2 on it" ).arg( item.destination, item.source ) );
            }

            // Compute transfer block size (100x the LCM of the block sizes seems a good fit)
            const qint64 blockSize = 100 * std::lcm( srcBlock, destBlock );

            QFile src( item.source );
            QFile dest( item.destination );
            if ( !src.open( QIODevice::ReadOnly ) || !dest.open( QIODevice::WriteOnly ) )
            {
                return ClaudeInstall::JobResult::error(
                    tr( "Cannot copy data" ),
                    tr( "Could not open %1 for reading or %2 for writing." ).arg( item.source, item.destination ) );
            }

            qint64 count = 0;
            QByteArray buffer( static_cast< int >( blockSize ), Qt::Uninitialized );
            for ( ;; )
            {
                const qint64 n = src.read( buffer.data(), buffer.size() );
                if ( n <= 0 )
                {
                    break;
                }
                if ( dest.write( buffer.constData(), n ) != n )
                {
                    return ClaudeInstall::JobResult::error( tr( "Cannot copy data" ),
                                                        tr( "Writing to %1 failed." ).arg( item.destination ) );
                }
                count += n;
                if ( srcSize > 0 )
                {
                    emit progress( ( double( count ) / double( srcSize ) + index ) / filesystems.size() );
                }
            }
            src.close();
            dest.close();

            if ( item.resize && item.filesystem.contains( "ext" ) )
            {
                cDebug() << "Resizing filesystem on" << item.destination;
                using ClaudeInstall::System;
                System::runCommand( System::RunLocation::RunInHost, { "e2fsck", "-f", "-y", item.destination } );
                System::runCommand( System::RunLocation::RunInHost, { "resize2fs", item.destination } );
            }
        }

        // Record the new UUID (and where the data came from) in the partition
        for ( auto& p : partitions )
        {
            QVariantMap partition = p.toMap();
            if ( partition.value( "device" ).toString() != item.destination )
            {
                continue;
            }
            const auto r = ClaudeInstall::System::runCommand(
                ClaudeInstall::System::RunLocation::RunInHost, { "blkid", "-s", "UUID", "-o", "value", item.destination } );
            if ( r.getExitCode() == 0 )
            {
                const QString uuid = r.getOutput().trimmed();
                cDebug() << "Setting" << item.destination << "UUID to" << ( uuid.isEmpty() ? "<empty>" : uuid );
                partition[ "uuid" ] = uuid;
                partition[ "source" ] = item.source;
                p = partition;
            }
        }
        gs->remove( "partitions" );
        gs->insert( "partitions", partitions );
    }

    return ClaudeInstall::JobResult::ok();
}

void
RawFSJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_targets = configurationMap.value( "targets" ).toList();
    m_bogus = ClaudeInstall::getBool( configurationMap, "bogus", false );
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( RawFSJobFactory, registerPlugin< RawFSJob >(); )
