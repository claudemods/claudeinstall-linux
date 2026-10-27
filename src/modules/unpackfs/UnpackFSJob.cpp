/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2014 Daniel Hillenbrand <codeworkx@bbqlinux.org>
 *   SPDX-FileCopyrightText: 2014 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "UnpackFSJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "compat/Variant.h"
#include "partition/Mount.h"
#include "utils/Logger.h"
#include "utils/Runner.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <utility>

#include <sys/stat.h>

using ClaudeInstall::System;
using ClaudeInstall::Utils::Runner;
static const auto RunInHost = System::RunLocation::RunInHost;

/** @brief Like Python's os.path.abspath() */
static QString
absolutePath( const QString& path )
{
    return QDir::cleanPath( QDir::current().absoluteFilePath( path ) );
}

/** @brief Lists the supported filesystems from /proc/filesystems, plus "file" */
static QStringList
supportedFilesystems()
{
    QStringList filesystems { QStringLiteral( "file" ) };
    QFile f( QStringLiteral( "/proc/filesystems" ) );
    if ( f.open( QIODevice::ReadOnly ) )
    {
        const QString text = QString::fromLocal8Bit( f.readAll() ).replace( "nodev", "" ).replace( '\t', "" );
        for ( const QString& line : text.split( '\n' ) )
        {
            filesystems.append( line );
        }
        if ( filesystems.last().isEmpty() )
        {
            filesystems.removeLast();  // the empty piece after the final newline
        }
    }
    return filesystems;
}

/** @brief Gets a nested value out of global storage ("a.b.c"), or an invalid QVariant */
static QVariant
fetchFromGlobalStorage( const QStringList& keys )
{
    QVariant value = ClaudeInstall::JobQueue::instance()->globalStorage()->value( keys.first() );
    if ( !value.isValid() || value.isNull() )
    {
        return QVariant();
    }
    for ( int i = 1; i < keys.size(); ++i )
    {
        const QVariantMap map = value.toMap();
        if ( ClaudeInstall::typeOf( value ) == ClaudeInstall::MapVariantType && map.contains( keys.at( i ) ) )
        {
            value = map.value( keys.at( i ) );
        }
        else
        {
            return QVariant();
        }
    }
    return value;
}

/** @brief The weight of @p entry: 1, or whatever is set if it is sensible */
static int
extractWeight( const QVariantMap& entry )
{
    const QVariant w = entry.value( "weight" );
    if ( w.isValid() && !w.isNull() )
    {
        bool ok = false;
        const int wi = w.toInt( &ok );
        if ( !ok )
        {
            cWarning() << "*weight* setting" << w << "is not valid.";
            return 1;
        }
        return wi > 0 ? wi : 1;
    }
    return 1;
}

/** @brief If the / of the system gets permission 777, change it down to 755
 *
 * Any other permission is left alone. This works around standard behavior
 * from squashfs where permissions are (easily, accidentally) set to 777.
 */
static void
repairRootPermissions( const QString& rootMountPoint )
{
    struct stat st;
    if ( ::stat( rootMountPoint.toLocal8Bit().constData(), &st ) == 0 && ( st.st_mode & 0777 ) == 0777 )
    {
        if ( ::chmod( rootMountPoint.toLocal8Bit().constData(), 0755 ) != 0 )
        {
            cWarning() << "Could not set / to safe permissions";  // but ignore it
        }
    }
}

UnpackFSJob::UnpackFSJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
    , m_status( tr( "Filling up filesystems…", "@status" ) )
{
}

UnpackFSJob::~UnpackFSJob() {}

QString
UnpackFSJob::prettyName() const
{
    return tr( "Filling up filesystems…", "@status" );
}

QString
UnpackFSJob::prettyStatusMessage() const
{
    QMutexLocker l( &m_statusMutex );
    return m_status;
}

void
UnpackFSJob::setStatus( const QString& status )
{
    QMutexLocker l( &m_statusMutex );
    m_status = status;
}

void
UnpackFSJob::reportProgress()
{
    double progressValue = 0.0;

    qint64 currentTotal = 0;
    qint64 currentDone = 0;  // Files count in the current entry
    int completeCount = 0;
    double completeWeight = 0.0;  // This much weight already finished
    for ( const UnpackEntry& entry : std::as_const( m_entries ) )
    {
        if ( entry.total == 0 )
        {
            continue;  // hasn't counted yet
        }
        if ( entry.total == entry.copied )
        {
            completeWeight += entry.weight;
            completeCount++;
        }
        else
        {
            // There is at most *one* entry in-progress
            currentTotal = entry.total;
            currentDone = entry.copied;
            completeWeight += entry.weight * double( currentDone ) / double( currentTotal );
            break;
        }
    }

    if ( currentTotal > 0 )
    {
        progressValue = completeWeight / m_totalWeight;
    }

    setStatus( tr( "Unpacking image %1/%2, file %3/%4" )
                   .arg( completeCount + 1 )
                   .arg( m_entries.size() )
                   .arg( currentDone )
                   .arg( currentTotal ) );
    emit progress( progressValue );
}

qint64
UnpackFSJob::doCount( UnpackEntry& entry ) const
{
    qint64 count = 0;
    auto countLines = [ &count ]( const QStringList& command )
    {
        Runner r( command );
        r.setLocation( RunInHost ).enableOutputProcessing();
        QObject::connect( &r, &Runner::output, [ &count ]( QString ) { ++count; } );
        r.run();
    };

    if ( entry.sourcefs == "squashfs" )
    {
        countLines( { "unsquashfs", "-l", entry.source } );
    }
    else if ( entry.sourcefs == "ext4" )
    {
        countLines( { "find", entry.mountPoint, "-type", "f" } );
    }
    else if ( entry.isFile() )
    {
        // Hasn't been mounted, copy directly; find handles both files and directories.
        countLines( { "find", entry.source, "-type", "f" } );
    }

    entry.total = count;
    return count;
}

QString
UnpackFSJob::doMount( UnpackEntry& entry, const QString& base ) const
{
    QString imgBaseName = QFileInfo( entry.source ).completeBaseName();
    if ( imgBaseName.isEmpty() )
    {
        imgBaseName = QFileInfo( entry.source ).fileName();
    }
    const QString imgMountDir = QDir( base ).filePath( imgBaseName );
    QDir().mkpath( imgMountDir );

    // This is where it *would* go (files bail out before actually mounting)
    entry.mountPoint = imgMountDir;

    if ( entry.isFile() )
    {
        return QString();
    }

    int r;
    const QFileInfo source( entry.source );
    if ( source.isDir() )
    {
        r = ClaudeInstall::Partition::mount( entry.source, imgMountDir, QString(), QStringLiteral( "--bind" ) );
    }
    else if ( source.isFile() )
    {
        r = ClaudeInstall::Partition::mount( entry.source, imgMountDir, entry.sourcefs, QStringLiteral( "loop" ) );
    }
    else  // the source is a device
    {
        r = ClaudeInstall::Partition::mount( entry.source, imgMountDir, entry.sourcefs, QString() );
    }

    if ( r != 0 )
    {
        cDebug() << "Failed to mount" << entry.source << "(fs=" << entry.sourcefs << ") (target=" << imgMountDir << ")";
        return tr( "Failed to mount \"%1\" (fs=%2) on \"%3\"." ).arg( entry.source, entry.sourcefs, imgMountDir );
    }
    return QString();
}

QString
UnpackFSJob::fileCopy( const QString& sourceIn, UnpackEntry& entry )
{
    QString source = sourceIn;

    // `source` *must* end with '/' otherwise a directory named after the source
    // will be created in `dest`: ie if `source` is "/foo/bar" and `dest` is
    // "/dest", then files will be copied in "/dest/bar".
    if ( !source.endsWith( '/' ) && !QFileInfo( source ).isFile() )
    {
        source += '/';
    }

    QStringList args { "rsync", "-aHAXSr", "--filter=-x trusted.overlay.*" };

    // Excludes for all the extra mounts
    for ( const auto& em : ClaudeInstall::JobQueue::instance()->globalStorage()->value( "extraMounts" ).toList() )
    {
        const QString mountPoint = em.toMap().value( "mountPoint" ).toString();
        if ( !mountPoint.isEmpty() )
        {
            args << "--exclude" << mountPoint + '/';
        }
    }
    if ( !entry.excludeFile.isEmpty() )
    {
        args << "--exclude-from=" + entry.excludeFile;
    }
    for ( const QString& f : std::as_const( entry.exclude ) )
    {
        args << "--exclude" << f;
    }
    args << "--progress" << source << entry.destination;

    // lastCopied trails the number of copied files, and whenever at least 107 more
    // files (fileCountChunk) have been copied, progress is reported and lastCopied
    // is updated. The chunk size isn't "tidy" so that all the digits of the
    // progress-reported number change.
    constexpr const qint64 fileCountChunk = 107;
    qint64 lastCopied = 0;
    qint64 lastTotal = 0;
    QElapsedTimer sinceReport;
    sinceReport.start();

    auto progressCb = [ this, &entry ]( qint64 copied, qint64 total )
    {
        entry.copied = copied;
        if ( total > entry.total )
        {
            entry.total = total;
        }
        reportProgress();
    };

    // rsync outputs progress in parentheses. Each line will have an xfer and a
    // chk item (either ir-chk or to-chk) as follows:
    //   - xfer#x => Interpret it as 'file copy try no. x'
    //   - ir-chk=x/y, where x = files yet to be checked, y = total files calculated so far
    //   - to-chk=x/y, which is similar and happens once the ir-chk phase is over.
    static const QRegularExpression progressLine( QStringLiteral( "xfr#(\\d+), ..-chk=(\\d+)/(\\d+)" ) );

    Runner r( args );
    r.setLocation( RunInHost ).enableOutputProcessing();
    QObject::connect( &r,
                      &Runner::output,
                      [ & ]( QString line )
                      {
                          const auto m = progressLine.match( line );
                          if ( !m.hasMatch() )
                          {
                              return;
                          }
                          const qint64 remaining = m.captured( 2 ).toLongLong();
                          const qint64 totalLocal = m.captured( 3 ).toLongLong();
                          // adjusting the offset so that the progress bar can be drawn continuously
                          const qint64 copied = totalLocal - remaining;

                          if ( copied - lastCopied >= fileCountChunk || sinceReport.elapsed() > 500 )
                          {
                              lastCopied = copied;
                              lastTotal = totalLocal;
                              sinceReport.restart();
                              progressCb( copied, totalLocal );
                          }
                      } );
    const int returnCode = r.run().getExitCode();

    progressCb( lastCopied, lastTotal );  // Push towards 100%

    // Mark this entry as really done
    entry.copied = entry.total;

    // 23 is the return code rsync returns if it cannot write extended
    // attributes (with -X) because the target file system does not support it,
    // e.g., the FAT EFI system partition. We need -X because distributions
    // using file system capabilities and/or SELinux require the extended
    // attributes. But distributions using SELinux may also have SELinux labels
    // set on files under /boot/efi, and rsync complains about those. The only
    // clean way would be to split the rsync into one with -X and
    // --exclude /boot/efi and a separate one without -X for /boot/efi, but
    // only if /boot/efi is actually an EFI system partition. For now, this hack
    // will have to do. See also:
    // https://bugzilla.redhat.com/show_bug.cgi?id=868755#c50
    // for the same issue in Anaconda, which uses a similar workaround.
    if ( returnCode != 0 && returnCode != 23 )
    {
        cWarning() << "rsync failed with error code" << returnCode;
        return tr( "rsync failed with error code %1." ).arg( returnCode );
    }
    return QString();
}

QString
UnpackFSJob::unpackImage( UnpackEntry& entry )
{
    const QString source = entry.isFile() ? entry.source : entry.mountPoint;
    const QString error = fileCopy( source, entry );
    if ( !entry.isFile() && System::runCommand( RunInHost, { "umount", "-l", entry.mountPoint } ).getExitCode() != 0 )
    {
        cWarning() << "Could not unmount" << entry.mountPoint;
    }
    return error;
}

ClaudeInstall::JobResult
UnpackFSJob::exec()
{
    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    const QString rootMountPoint = gs->value( "rootMountPoint" ).toString();

    if ( rootMountPoint.isEmpty() )
    {
        cWarning() << "No mount point for root partition";
        return ClaudeInstall::JobResult::error( tr( "No mount point for root partition" ),
                                            tr( "globalstorage does not contain a \"rootMountPoint\" key." ) );
    }
    if ( !QFileInfo::exists( rootMountPoint ) )
    {
        cWarning() << "Bad root mount point" << rootMountPoint;
        return ClaudeInstall::JobResult::error( tr( "Bad mount point for root partition" ),
                                            tr( "rootMountPoint is \"%1\", which does not exist." ).arg( rootMountPoint ) );
    }

    if ( m_unpack.isEmpty() )
    {
        cWarning() << "No *unpack* key in job configuration.";
        return ClaudeInstall::JobResult::error( tr( "Bad unpackfs configuration" ), tr( "There is no configuration information." ) );
    }

    const QStringList supported = supportedFilesystems();
    const QString badConfiguration = tr( "Bad unpackfs configuration" );

    // Bail out before we start when there are obvious problems
    //   - unsupported filesystems
    //   - non-existent sources
    //   - missing tools for specific FS
    QVariantList unpack = m_unpack;  // Entries may be changed (marked as skipped)
    for ( int i = 0; i < unpack.size(); ++i )
    {
        QVariantMap entry = unpack.at( i ).toMap();
        const QString source = absolutePath( entry.value( "source" ).toString() );
        const QString sourcefs = entry.value( "sourcefs" ).toString();
        const bool optional = ClaudeInstall::getBool( entry, "optional", false );

        if ( !supported.contains( sourcefs ) )
        {
            cWarning() << "The filesystem for" << source << "(" << sourcefs << ") is not supported by your current kernel";
            cWarning() << " ... modprobe" << sourcefs << "may solve the problem";
            return ClaudeInstall::JobResult::error(
                badConfiguration,
                tr( "The filesystem for \"%1\" (%2) is not supported by your current kernel" ).arg( source, sourcefs ) );
        }
        if ( !QFileInfo::exists( source ) )
        {
            if ( optional )
            {
                cWarning() << "The source filesystem" << source << "does not exist but is marked as optional, skipping";
                entry[ "condition" ] = false;
                unpack[ i ] = entry;
                continue;
            }
            cWarning() << "The source filesystem" << source << "does not exist";
            return ClaudeInstall::JobResult::error( badConfiguration,
                                                tr( "The source filesystem \"%1\" does not exist" ).arg( source ) );
        }
        if ( sourcefs == "squashfs" && QStandardPaths::findExecutable( "unsquashfs" ).isEmpty() )
        {
            cWarning() << "Failed to find unsquashfs";
            return ClaudeInstall::JobResult::error(
                badConfiguration,
                tr( "Failed to find unsquashfs, make sure you have the squashfs-tools package installed." ) + ' '
                    + tr( "Failed to unpack image \"%1\"" ).arg( source ) );
        }
    }

    m_entries.clear();
    bool isFirst = true;
    for ( const auto& ev : std::as_const( unpack ) )
    {
        const QVariantMap entry = ev.toMap();
        const QString source = absolutePath( entry.value( "source" ).toString() );
        const QString sourcefs = entry.value( "sourcefs" ).toString();
        const QString destination = absolutePath( rootMountPoint + entry.value( "destination" ).toString() );

        bool condition = true;
        if ( entry.contains( "condition" ) )
        {
            const QVariant c = entry.value( "condition" );
            if ( ClaudeInstall::typeOf( c ) == ClaudeInstall::BoolVariantType )
            {
                condition = c.toBool();
            }
            else if ( ClaudeInstall::typeOf( c ) == ClaudeInstall::StringVariantType )
            {
                const QString name = c.toString();
                const QVariant value = fetchFromGlobalStorage( name.split( '.' ) );
                if ( !value.isValid() )
                {
                    cWarning() << "Condition key" << name << "not found in global storage, assuming False";
                    condition = false;
                }
                else if ( ClaudeInstall::typeOf( value ) == ClaudeInstall::BoolVariantType )
                {
                    condition = value.toBool();
                }
                else
                {
                    cWarning() << "Condition key" << name << "is not a boolean, assuming True";
                    condition = true;
                }
            }
            else
            {
                cWarning() << "Invalid 'condition' value" << c << ", assuming True";
                condition = true;
            }
        }

        if ( !condition )
        {
            cDebug() << "Skipping unpack of" << source << "due to 'condition' being False";
            continue;
        }

        if ( !QFileInfo( destination ).isDir() && sourcefs != "file" )
        {
            cWarning() << "The destination" << destination << "in the target system is not a directory";
            if ( isFirst )
            {
                return ClaudeInstall::JobResult::error(
                    badConfiguration,
                    tr( "The destination \"%1\" in the target system is not a directory" ).arg( destination ) );
            }
            cDebug() << ".. assuming that the previous targets will create that directory.";
        }

        UnpackEntry e;
        e.source = source;
        e.sourcefs = sourcefs;
        e.destination = destination;
        // Optional settings
        e.exclude = entry.value( "exclude" ).toStringList();
        e.excludeFile = entry.value( "excludeFile" ).toString();
        e.weight = extractWeight( entry );
        m_entries.append( e );

        isFirst = false;
    }

    m_totalWeight = 0;
    for ( const UnpackEntry& e : std::as_const( m_entries ) )
    {
        m_totalWeight += e.weight;
    }
    if ( m_totalWeight < 1 )
    {
        m_totalWeight = 1;
    }

    repairRootPermissions( rootMountPoint );

    QTemporaryDir sourceMountPath;
    ClaudeInstall::JobResult result = ClaudeInstall::JobResult::ok();
    int complete = 0;
    for ( UnpackEntry& entry : m_entries )
    {
        setStatus( tr( "Starting to unpack %1" ).arg( entry.source ) );
        emit progress( double( complete ) / double( m_entries.size() ) );

        QString error = doMount( entry, sourceMountPath.path() );
        if ( error.isEmpty() )
        {
            doCount( entry );  // Fills in the entry.total
            reportProgress();
            error = unpackImage( entry );
        }
        if ( !error.isEmpty() )
        {
            result = ClaudeInstall::JobResult::error( tr( "Failed to unpack image \"%1\"" ).arg( entry.source ), error );
            break;
        }
        complete++;
    }

    repairRootPermissions( rootMountPoint );
    return result;
}

void
UnpackFSJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_unpack = configurationMap.value( "unpack" ).toList();
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( UnpackFSJobFactory, registerPlugin< UnpackFSJob >(); )
