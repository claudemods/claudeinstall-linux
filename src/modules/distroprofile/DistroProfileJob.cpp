/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "DistroProfileJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"
#include "utils/Yaml.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QUrl>

#include <utility>

using ClaudeInstall::JobResult;
using RunLocation = ClaudeInstall::System::RunLocation;

namespace
{
/// Is @p path (after cleaning) @p base or below it?
bool
isInside( const QString& base, const QString& path )
{
    const QString b = QDir::cleanPath( base );
    const QString p = QDir::cleanPath( path );
    return p == b || p.startsWith( b + '/' );
}

ClaudeInstall::ProcessResult
host( const QStringList& args )
{
    return ClaudeInstall::System::runCommand( RunLocation::RunInHost, args );
}

ClaudeInstall::ProcessResult
inTarget( const QStringList& args )
{
    return ClaudeInstall::System::runCommand( RunLocation::RunInTarget, args );
}

JobResult
failed( const QStringList& cmd, const ClaudeInstall::ProcessResult& r )
{
    cWarning() << "Command" << cmd.join( ' ' ) << "failed with exit code" << r.getExitCode();
    cDebug() << r.getOutput();
    return JobResult::error( QCoreApplication::translate( "DistroProfileJob", "Setup step failed" ),
                             QCoreApplication::translate( "DistroProfileJob", "The command <pre>%1</pre> returned error code %2." )
                                 .arg( cmd.join( ' ' ) )
                                 .arg( r.getExitCode() ) );
}

/// Package names that a package manager says it cannot find
QStringList
missingPackages( const QString& output )
{
    static const QRegularExpression re( QStringLiteral( "target not found: (\\S+)" ) );
    QStringList names;
    auto it = re.globalMatch( output );
    while ( it.hasNext() )
    {
        names << it.next().captured( 1 );
    }
    names.removeDuplicates();
    return names;
}

/// Profile ids become file names: allow nothing that could leave the profiles directory
bool
validId( const QString& id )
{
    static const QRegularExpression re( QStringLiteral( "^[A-Za-z0-9][A-Za-z0-9._-]*$" ) );
    return re.match( id ).hasMatch();
}
}  // namespace

DistroProfileJob::DistroProfileJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

DistroProfileJob::~DistroProfileJob() {}

QString
DistroProfileJob::prettyName() const
{
    return m_phase == QLatin1String( "customize" ) ? tr( "Apply the distribution's customisation", "@status" )
                                                   : tr( "Install the distribution", "@status" );
}

QString
DistroProfileJob::prettyStatusMessage() const
{
    QMutexLocker lock( &m_statusMutex );
    return m_status;
}

void
DistroProfileJob::setStatus( const QString& status )
{
    {
        QMutexLocker lock( &m_statusMutex );
        m_status = status;
    }
    // Lines can arrive many times a second: tell the queue and the log about once a second
    if ( !m_clock.isValid() || m_clock.elapsed() - m_lastEmitMs >= 1000 )
    {
        m_lastEmitMs = m_clock.isValid() ? m_clock.elapsed() : 0;
        cDebug() << "Status:" << status;
        emit progress( m_lastProgress );
    }
}

int
DistroProfileJob::runStreaming( const QStringList& cmd, QString& output )
{
    QProcess proc;
    proc.setProcessChannelMode( QProcess::MergedChannels );
    proc.start( cmd.first(), cmd.mid( 1 ) );
    if ( !proc.waitForStarted( 30000 ) )
    {
        output = tr( "Cannot start %1." ).arg( cmd.first() );
        return -1;
    }

    const QString prefix = prettyStatusMessage();
    QString pending;
    auto consume = [ & ]( const QByteArray& chunk )
    {
        output += QString::fromUtf8( chunk );
        pending += QString::fromUtf8( chunk );
        // pacman redraws its progress with carriage returns: every piece is a line
        const QStringList parts = pending.split( QRegularExpression( QStringLiteral( "[\\r\\n]" ) ) );
        pending = parts.last();
        for ( int i = 0; i + 1 < parts.size(); ++i )
        {
            const QString line = parts.at( i ).trimmed();
            if ( !line.isEmpty() )
            {
                setStatus( prefix + QStringLiteral( ": " ) + line.left( 110 ) );
            }
        }
    };
    while ( !proc.waitForFinished( 300 ) )
    {
        consume( proc.readAll() );
    }
    consume( proc.readAll() );
    return proc.exitStatus() == QProcess::NormalExit ? proc.exitCode() : -1;
}

QString
DistroProfileJob::expand( const QString& text, const Context& ctx ) const
{
    QString out = text;
    out.replace( QLatin1String( "@USER@" ), ctx.user );
    out.replace( QLatin1String( "@HOME@" ), QStringLiteral( "/home/" ) + ctx.user );
    out.replace( QLatin1String( "@ROOT@" ), ctx.root );
    out.replace( QLatin1String( "@ASSETS@" ), ctx.assets );
    for ( auto it = ctx.extra.constBegin(); it != ctx.extra.constEnd(); ++it )
    {
        out.replace( '@' + it.key() + '@', it.value() );
    }
    return out;
}

bool
DistroProfileJob::loadProfile( const QString& id, QVariantMap& out, QString& error ) const
{
    // A profile may "extends:" another one (family, common settings): the child's
    // keys win, "packagesAdd" is appended, and the step lists are concatenated.
    QStringList chain;
    QString current = id;
    while ( !current.isEmpty() )
    {
        if ( !validId( current ) || chain.contains( current ) || chain.size() > 8 )
        {
            error = tr( "Bad or circular profile name \"%1\"." ).arg( current );
            return false;
        }
        chain.prepend( current );

        bool ok = false;
        const QVariantMap map = ClaudeInstall::YAML::load( m_profilesDir + '/' + current + ".conf", &ok );
        if ( !ok )
        {
            error = tr( "Cannot read profile \"%1\" from %2." ).arg( current, m_profilesDir );
            return false;
        }
        current = ClaudeInstall::getString( map, "extends" );
        if ( chain.size() == 1 )
        {
            out = map;  // the requested profile
        }
    }

    // Merge parent-first so that children override
    QVariantMap merged;
    QStringList packages;
    QVariantList install, customize;
    QVariantMap vars;
    for ( const QString& name : chain )
    {
        const QVariantMap map = ClaudeInstall::YAML::load( m_profilesDir + '/' + name + ".conf" );
        vars.insert( map.value( "vars" ).toMap() );  // @NAME@ placeholders: children override parents
        for ( auto it = map.constBegin(); it != map.constEnd(); ++it )
        {
            merged[ it.key() ] = it.value();
        }
        if ( map.contains( "packages" ) )
        {
            packages = map.value( "packages" ).toStringList();
        }
        packages += map.value( "packagesAdd" ).toStringList();
        install += map.value( "install" ).toList();
        customize += map.value( "customize" ).toList();
    }
    packages.removeDuplicates();
    merged[ "packages" ] = packages;
    merged[ "install" ] = install;
    merged[ "customize" ] = customize;
    merged[ "vars" ] = vars;
    out = merged;
    return true;
}

void
DistroProfileJob::refreshHostPackageLists()
{
    host( { "rm", "-f", "/var/lib/pacman/db.lck" } );
    // -Syy forces a re-download, so a list that went stale is replaced
    setStatus( tr( "Refreshing the package lists", "@status" ) );
    host( { "pacman", "-Syy", "--noconfirm" } );
    if ( !QFileInfo::exists( "/etc/pacman.d/gnupg/trustdb.gpg" ) )
    {
        host( { "pacman-key", "--init" } );
    }
    host( { "pacman-key", "--populate" } );
}

JobResult
DistroProfileJob::bootstrap( const QString& root, QStringList packages )
{
    refreshHostPackageLists();

    QStringList skipped;
    for ( int attempt = 0;; ++attempt )
    {
        if ( packages.isEmpty() )
        {
            return JobResult::error( tr( "Nothing to install" ), tr( "The profile has no packages left to install." ) );
        }
        const QStringList cmd = QStringList { "pacstrap", "-K", root } + packages;
        setStatus( tr( "Installing %1 packages with pacstrap (attempt %2)", "@status" ).arg( packages.size() ).arg( attempt + 1 ) );
        QString out;
        const int code = runStreaming( cmd, out );
        if ( code == 0 )
        {
            break;
        }

        // Names that vanished from the repositories are left out, not fatal.
        const QStringList missing = missingPackages( out );
        QStringList kept;
        for ( const QString& p : packages )
        {
            if ( !missing.contains( p ) )
            {
                kept << p;
            }
        }
        if ( kept.size() != packages.size() )
        {
            cWarning() << "Skipping packages that are no longer in the repositories:" << missing;
            skipped += missing;
            packages = kept;
            continue;
        }
        if ( attempt >= m_retries )
        {
            cWarning() << "Command" << cmd.join( ' ' ) << "failed with exit code" << code;
            cDebug() << out;
            return JobResult::error( tr( "Setup step failed" ),
                                     tr( "The command <pre>%1</pre> returned error code %2." ).arg( cmd.join( ' ' ) ).arg( code ) );
        }
        setStatus( tr( "pacstrap failed; refreshing the package lists and retrying", "@status" ) );
        cWarning() << "pacstrap failed; refreshing the package lists and retrying";
        refreshHostPackageLists();
        QThread::sleep( 3 );
    }

    ClaudeInstall::JobQueue::instance()->globalStorage()->insert( "distroprofileSkipped", skipped );
    if ( !QFileInfo::exists( root + "/usr/bin/bash" ) )
    {
        return JobResult::error( tr( "Bootstrap failed" ), tr( "/usr/bin/bash is missing in the target after installing packages." ) );
    }
    return JobResult::ok();
}

JobResult
DistroProfileJob::ensureAssets( QString& assetsDir )
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();

    // Already fetched by the other phase
    const QString staged = gs->value( "distroprofileAssets" ).toString();
    if ( !staged.isEmpty() && QFileInfo( staged ).isDir() )
    {
        assetsDir = staged;
        return JobResult::ok();
    }
    // The files are not kept in the repository: a bundle is downloaded instead. A local
    // assetsDir that already has files in it is still honoured (offline use).
    if ( m_assetsUrl.isEmpty() || !QDir( assetsDir ).isEmpty() )
    {
        return JobResult::ok();
    }
    if ( !m_assetsUrl.startsWith( QLatin1String( "https://" ) ) )
    {
        return JobResult::error( tr( "Bad configuration" ), tr( "assetsUrl must use https: %1" ).arg( m_assetsUrl ) );
    }

    const QString dir = QStringLiteral( "/var/tmp/claudeinstall-assets" );
    const QString file = dir + QStringLiteral( ".zip" );
    QDir( dir ).removeRecursively();
    QDir().mkpath( dir );

    const QStringList get { "curl", "-fL", "--retry", "5", "--retry-delay", "3", "--connect-timeout", "30", "-o", file, m_assetsUrl };
    const auto r = host( get );
    if ( r.getExitCode() != 0 )
    {
        return JobResult::error( tr( "Download failed" ), tr( "Could not fetch %1:\n%2" ).arg( m_assetsUrl, r.getOutput() ) );
    }
    if ( !m_assetsSha256.isEmpty() )
    {
        QFile f( file );
        QCryptographicHash hash( QCryptographicHash::Sha256 );
        if ( !f.open( QIODevice::ReadOnly ) || !hash.addData( &f ) || hash.result().toHex() != m_assetsSha256.toLatin1() )
        {
            return JobResult::error( tr( "Download failed" ), tr( "The checksum of %1 does not match." ).arg( m_assetsUrl ) );
        }
    }
    const QStringList unpack { "unzip", "-o", "-q", file, "-d", dir };
    const auto u = host( unpack );
    if ( u.getExitCode() != 0 )
    {
        return JobResult::error( tr( "Unpack failed" ), u.getOutput() );
    }
    // A bundle may hold everything in one top-level folder
    const QStringList top = QDir( dir ).entryList( QDir::AllEntries | QDir::NoDotAndDotDot );
    assetsDir = ( top.size() == 1 && QFileInfo( dir + '/' + top.first() ).isDir() ) ? dir + '/' + top.first() : dir;
    gs->insert( "distroprofileAssets", assetsDir );
    return JobResult::ok();
}

JobResult
DistroProfileJob::writeRegdom( const Context& ctx )
{
    QString code = ClaudeInstall::JobQueue::instance()->globalStorage()->value( m_regdomKey ).toString().split( ',' ).first().trimmed().toUpper();
    if ( code == QLatin1String( "AUTO" ) )
    {
        code.clear();
    }
    if ( code.isEmpty() )
    {
        code = m_regdom.toUpper();
    }
    if ( code.isEmpty() )
    {
        // Derive the country from the chosen time zone (zone.tab: CC, coordinates, TZ)
        const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
        const QString zone = gs->value( "locationRegion" ).toString() + '/' + gs->value( "locationZone" ).toString();
        for ( const QString& tab : { QStringLiteral( "/usr/share/zoneinfo/zone.tab" ), QStringLiteral( "/usr/share/zoneinfo/zone1970.tab" ) } )
        {
            QFile f( tab );
            if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
            {
                continue;
            }
            while ( !f.atEnd() )
            {
                const QStringList cols = QString::fromUtf8( f.readLine() ).trimmed().split( '\t' );
                if ( cols.size() >= 3 && !cols[ 0 ].startsWith( '#' ) && cols[ 2 ] == zone )
                {
                    code = cols[ 0 ].split( ',' ).first();
                    break;
                }
            }
            if ( !code.isEmpty() )
            {
                break;
            }
        }
    }
    static const QRegularExpression twoLetters( QStringLiteral( "^[A-Z]{2}$" ) );
    if ( !twoLetters.match( code ).hasMatch() )
    {
        cWarning() << "No wireless regulatory domain could be determined; leaving it unset";
        return JobResult::ok();
    }

    QDir().mkpath( ctx.root + "/etc/conf.d" );
    const QString path = ctx.root + "/etc/conf.d/wireless-regdom";

    // Keep the file the profile copied (the full list of codes): only the active line changes.
    QStringList lines;
    {
        QFile in( path );
        if ( in.open( QIODevice::ReadOnly | QIODevice::Text ) )
        {
            lines = QString::fromUtf8( in.readAll() ).split( '\n' );
        }
    }
    static const QRegularExpression codeLine( QStringLiteral( "^#?\\s*WIRELESS_REGDOM=\"([A-Z0-9]{2})\"\\s*$" ) );
    bool found = false;
    for ( QString& line : lines )
    {
        const auto m = codeLine.match( line );
        if ( m.hasMatch() )
        {
            const bool wanted = m.captured( 1 ) == code;
            found = found || wanted;
            line = QStringLiteral( "%1WIRELESS_REGDOM=\"%2\"" ).arg( wanted ? QString() : QStringLiteral( "#" ), m.captured( 1 ) );
        }
    }
    if ( lines.isEmpty() )
    {
        lines << QStringLiteral( "# Written by ClaudeInstall" );
    }
    if ( !found )
    {
        while ( !lines.isEmpty() && lines.last().isEmpty() )
        {
            lines.removeLast();
        }
        lines << QStringLiteral( "WIRELESS_REGDOM=\"%1\"" ).arg( code );
    }
    if ( !lines.last().isEmpty() )
    {
        lines << QString();
    }

    QFile out( path );
    if ( !out.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) )
    {
        return JobResult::error( tr( "Wireless regulatory domain" ), tr( "Cannot write %1." ).arg( out.fileName() ) );
    }
    out.write( lines.join( '\n' ).toUtf8() );
    cDebug() << "Wireless regulatory domain set to" << code;
    return JobResult::ok();
}

JobResult
DistroProfileJob::runStep( const QVariantMap& step, const Context& ctx )
{
    const QString type = ClaudeInstall::getString( step, "type" );
    const bool optional = ClaudeInstall::getBool( step, "optional", false );

    // "when: NAME" skips the step while the profile variable NAME is empty
    const QString when = ClaudeInstall::getString( step, "when" );
    if ( !when.isEmpty() && ctx.extra.value( when ).isEmpty() )
    {
        return JobResult::ok();
    }

    // Say what is happening, so the progress view and the log are never silent
    {
        QString subject = ClaudeInstall::getString( step, "from" );
        for ( const char* key : { "command", "url", "path", "to" } )
        {
            if ( subject.isEmpty() )
            {
                subject = ClaudeInstall::getString( step, key );
            }
        }
        if ( subject.isEmpty() )
        {
            subject = step.value( "units" ).toStringList().join( ", " );
        }
        subject = expand( subject.simplified(), ctx );
        static const QMap< QString, QString > verbs {
            { "copy", QStringLiteral( "Copying" ) },       { "mkdir", QStringLiteral( "Creating" ) },
            { "remove", QStringLiteral( "Removing" ) },    { "enable", QStringLiteral( "Enabling" ) },
            { "run", QStringLiteral( "Running" ) },        { "runAsUser", QStringLiteral( "Running as the user" ) },
            { "hostRun", QStringLiteral( "Running" ) },    { "download", QStringLiteral( "Downloading" ) },
            { "write", QStringLiteral( "Writing" ) },      { "regdom", QStringLiteral( "Setting the wireless region" ) } };
        setStatus( verbs.value( type, type ) + ' ' + subject.left( 90 ) );
    }

    auto soften = [ & ]( JobResult r ) -> JobResult
    {
        if ( !r && optional )
        {
            cWarning() << "Optional step" << type << "failed, continuing:" << r.message();
            return JobResult::ok();
        }
        return std::move( r );
    };

    if ( type == QLatin1String( "copy" ) )
    {
        // from: relative to the assets directory. to: in the target (or on the host with host: true)
        const bool onHost = ClaudeInstall::getBool( step, "host", false );
        const QString from = ctx.assets + '/' + expand( ClaudeInstall::getString( step, "from" ), ctx );
        const QString toRel = expand( ClaudeInstall::getString( step, "to" ), ctx );
        const QString to = onHost ? toRel : ctx.root + toRel;
        if ( !isInside( ctx.assets, from ) || ( !onHost && !isInside( ctx.root, to ) ) )
        {
            return JobResult::error( tr( "Bad profile" ), tr( "Copy step leaves its directory: %1 -> %2" ).arg( from, to ) );
        }
        if ( !QFileInfo::exists( from ) )
        {
            return soften( JobResult::error( tr( "Missing file" ), tr( "The file %1 is not installed." ).arg( from ) ) );
        }
        QDir().mkpath( QFileInfo( to ).path() );
        const QStringList cmd { "cp", "-a", from, to };
        const auto r = host( cmd );
        return soften( r.getExitCode() == 0 ? JobResult::ok() : failed( cmd, r ) );
    }
    if ( type == QLatin1String( "mkdir" ) )
    {
        const QString dir = ctx.root + expand( ClaudeInstall::getString( step, "path" ), ctx );
        return isInside( ctx.root, dir ) && QDir().mkpath( dir ) ? JobResult::ok()
                                                                  : soften( JobResult::error( tr( "Cannot create directory" ), dir ) );
    }
    if ( type == QLatin1String( "remove" ) )
    {
        const QString path = ctx.root + expand( ClaudeInstall::getString( step, "path" ), ctx );
        if ( !isInside( ctx.root, path ) || QDir::cleanPath( path ) == QDir::cleanPath( ctx.root ) )
        {
            return JobResult::error( tr( "Bad profile" ), tr( "Refusing to remove %1" ).arg( path ) );
        }
        const QStringList cmd { "rm", "-rf", path };
        host( cmd );
        return JobResult::ok();
    }
    if ( type == QLatin1String( "enable" ) )
    {
        for ( const QString& unit : step.value( "units" ).toStringList() )
        {
            const QStringList cmd { "systemctl", "enable", expand( unit, ctx ) };
            const auto r = inTarget( cmd );
            if ( r.getExitCode() != 0 )
            {
                auto res = soften( failed( cmd, r ) );
                if ( !res )
                {
                    return res;
                }
            }
        }
        return JobResult::ok();
    }
    if ( type == QLatin1String( "run" ) || type == QLatin1String( "runAsUser" ) || type == QLatin1String( "hostRun" ) )
    {
        const QString script = expand( ClaudeInstall::getString( step, "command" ), ctx );
        QStringList cmd;
        if ( type == QLatin1String( "hostRun" ) )
        {
            cmd = { "sh", "-c", script };
        }
        else if ( type == QLatin1String( "runAsUser" ) )
        {
            cmd = { "su", "-", ctx.user, "-c", script };
        }
        else
        {
            cmd = { "sh", "-c", script };
        }
        const auto r = type == QLatin1String( "hostRun" ) ? host( cmd ) : inTarget( cmd );
        return soften( r.getExitCode() == 0 ? JobResult::ok() : failed( cmd, r ) );
    }
    if ( type == QLatin1String( "download" ) )
    {
        // Downloads are verified: give sha256, or say insecure: true on purpose.
        const QString url = expand( ClaudeInstall::getString( step, "url" ), ctx );
        const QString sha = ClaudeInstall::getString( step, "sha256" ).toLower();
        const QString destRel = expand( ClaudeInstall::getString( step, "dest" ), ctx );
        const QString dest = ctx.root + destRel;
        if ( !url.startsWith( QLatin1String( "https://" ) ) || !isInside( ctx.root, dest ) )
        {
            return JobResult::error( tr( "Bad profile" ), tr( "Download must use https and stay inside the target: %1" ).arg( url ) );
        }
        if ( sha.isEmpty() && !ClaudeInstall::getBool( step, "insecure", false ) )
        {
            return soften( JobResult::error( tr( "Unverified download" ),
                                             tr( "The profile gives no sha256 for %1 (set insecure: true to allow that)." ).arg( url ) ) );
        }
        QTemporaryDir tmp;
        if ( !tmp.isValid() )
        {
            return JobResult::error( tr( "Download" ), tr( "Cannot create a temporary directory." ) );
        }
        const QString file = tmp.filePath( QFileInfo( QUrl( url ).path() ).fileName() );
        // -# prints a progress bar: its percentage is shown as the status
        const QStringList get { "curl", "-fL", "-#", "--retry", "5", "--retry-delay", "3", "--connect-timeout", "30", "-o", file, url };
        QString curlOutput;
        const int curlCode = runStreaming( get, curlOutput );
        if ( curlCode != 0 )
        {
            cWarning() << "Command" << get.join( ' ' ) << "failed with exit code" << curlCode;
            return soften( JobResult::error( tr( "Download failed" ), tr( "Could not download %1 (curl error %2)." ).arg( url ).arg( curlCode ) ) );
        }
        if ( !sha.isEmpty() )
        {
            QFile f( file );
            QCryptographicHash hash( QCryptographicHash::Sha256 );
            if ( !f.open( QIODevice::ReadOnly ) || !hash.addData( &f ) || hash.result().toHex() != sha.toLatin1() )
            {
                return soften( JobResult::error( tr( "Download" ), tr( "The checksum of %1 does not match." ).arg( url ) ) );
            }
        }
        QDir().mkpath( dest );
        const QStringList unpack = file.endsWith( QLatin1String( ".zip" ) ) ? QStringList { "unzip", "-o", file, "-d", dest }
                                                                              : QStringList { "cp", "-a", file, dest };
        const auto u = host( unpack );
        return soften( u.getExitCode() == 0 ? JobResult::ok() : failed( unpack, u ) );
    }
    if ( type == QLatin1String( "write" ) )
    {
        // A small file written straight from the profile (no helper script needed)
        const QString path = ctx.root + expand( ClaudeInstall::getString( step, "path" ), ctx );
        if ( !isInside( ctx.root, path ) )
        {
            return JobResult::error( tr( "Bad profile" ), tr( "Write step leaves the target: %1" ).arg( path ) );
        }
        QDir().mkpath( QFileInfo( path ).path() );
        QFile out( path );
        const auto flags = QIODevice::WriteOnly | QIODevice::Text
            | ( ClaudeInstall::getBool( step, "append", false ) ? QIODevice::Append : QIODevice::Truncate );
        if ( !out.open( flags ) )
        {
            return soften( JobResult::error( tr( "Cannot write a file" ), path ) );
        }
        out.write( expand( ClaudeInstall::getString( step, "content" ), ctx ).toUtf8() );
        out.close();
        const QString mode = ClaudeInstall::getString( step, "mode" );
        if ( !mode.isEmpty() )
        {
            host( { "chmod", mode, path } );
        }
        return JobResult::ok();
    }
    if ( type == QLatin1String( "regdom" ) )
    {
        return soften( writeRegdom( ctx ) );
    }

    return JobResult::error( tr( "Bad profile" ), tr( "Unknown step type \"%1\"." ).arg( type ) );
}

JobResult
DistroProfileJob::runSteps( const QVariantList& steps, const Context& ctx )
{
    int done = 0;
    for ( const QVariant& s : steps )
    {
        auto r = runStep( s.toMap(), ctx );
        if ( !r )
        {
            return r;
        }
        m_lastProgress = double( ++done ) / double( qMax( 1, int( steps.size() ) ) );
        emit progress( m_lastProgress );
    }
    return JobResult::ok();
}

JobResult
DistroProfileJob::exec()
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    m_clock.start();
    setStatus( prettyName() );

    Context ctx;
    ctx.root = gs->value( "rootMountPoint" ).toString();
    ctx.user = gs->value( "username" ).toString();
    ctx.assets = m_assetsDir;
    if ( ctx.root.isEmpty() || !QFileInfo::exists( ctx.root ) )
    {
        return JobResult::error( tr( "No target" ), tr( "globalstorage has no usable \"rootMountPoint\"." ) );
    }
    auto fetched = ensureAssets( ctx.assets );
    if ( !fetched )
    {
        return fetched;
    }

    // The chooser stores a comma-separated list of ids; one distro is installed.
    const QString id = gs->value( m_profileKey ).toString().split( ',' ).first().trimmed();
    if ( id.isEmpty() )
    {
        return JobResult::error( tr( "No distribution selected" ), tr( "Nothing was chosen for \"%1\"." ).arg( m_profileKey ) );
    }

    QVariantMap profile;
    QString error;
    if ( !loadProfile( id, profile, error ) )
    {
        return JobResult::error( tr( "Bad profile" ), error );
    }
    cDebug() << "Distribution profile" << id << "phase" << m_phase;
    const QVariantMap vars = profile.value( "vars" ).toMap();
    for ( auto it = vars.constBegin(); it != vars.constEnd(); ++it )
    {
        ctx.extra.insert( it.key(), it.value().toString() );
    }

    if ( m_phase == QLatin1String( "customize" ) )
    {
        if ( ctx.user.isEmpty() )
        {
            return JobResult::error( tr( "No user" ), tr( "The customisation needs the user that the users module creates." ) );
        }
        return runSteps( profile.value( "customize" ).toList(), ctx );
    }

    // install
    auto r = runSteps( profile.value( "install" ).toList(), ctx );
    if ( !r )
    {
        return r;
    }
    QStringList packages = profile.value( "packages" ).toStringList();
    packages += gs->value( "extraPackages" ).toString().split( ' ', Qt::SkipEmptyParts );
    packages.removeDuplicates();
    return bootstrap( ctx.root, packages );
}

void
DistroProfileJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_configuration = configurationMap;
    m_profilesDir = ClaudeInstall::getString( configurationMap, "profilesDir", QStringLiteral( "/usr/share/claudeinstall/profiles" ) );
    m_assetsDir = ClaudeInstall::getString( configurationMap, "assetsDir", QStringLiteral( "/usr/share/claudeinstall/needed-files" ) );
    m_profileKey = ClaudeInstall::getString( configurationMap, "profileKey", QStringLiteral( "packagechooser_distro" ) );
    m_phase = ClaudeInstall::getString( configurationMap, "phase", QStringLiteral( "install" ) );
    m_assetsUrl = ClaudeInstall::getString( configurationMap, "assetsUrl" );
    m_assetsSha256 = ClaudeInstall::getString( configurationMap, "assetsSha256" ).toLower();
    m_regdomKey = ClaudeInstall::getString( configurationMap, "regdomKey", QStringLiteral( "packagechooser_regdom" ) );
    m_regdom = ClaudeInstall::getString( configurationMap, "wirelessRegdom" );
    m_retries = ClaudeInstall::getInteger( configurationMap, "retries", 3 );
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( DistroProfileJobFactory, registerPlugin< DistroProfileJob >(); )
