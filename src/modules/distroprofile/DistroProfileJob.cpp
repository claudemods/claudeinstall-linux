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
DistroProfileJob::refreshHostPackageLists() const
{
    host( { "rm", "-f", "/var/lib/pacman/db.lck" } );
    // -Syy forces a re-download, so a list that went stale is replaced
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
        const auto r = ClaudeInstall::System::runCommand( RunLocation::RunInHost, cmd );
        if ( r.getExitCode() == 0 )
        {
            break;
        }

        // Names that vanished from the repositories are left out, not fatal.
        const QStringList missing = missingPackages( r.getOutput() );
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
            return failed( cmd, r );
        }
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
DistroProfileJob::writeRegdom( const Context& ctx )
{
    QString code = m_regdom.toUpper();
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
    QFile out( ctx.root + "/etc/conf.d/wireless-regdom" );
    if ( !out.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) )
    {
        return JobResult::error( tr( "Wireless regulatory domain" ), tr( "Cannot write %1." ).arg( out.fileName() ) );
    }
    QTextStream( &out ) << "# Written by ClaudeInstall\nWIRELESS_REGDOM=\"" << code << "\"\n";
    cDebug() << "Wireless regulatory domain set to" << code;
    return JobResult::ok();
}

JobResult
DistroProfileJob::runStep( const QVariantMap& step, const Context& ctx )
{
    const QString type = ClaudeInstall::getString( step, "type" );
    const bool optional = ClaudeInstall::getBool( step, "optional", false );
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
        const QStringList get { "curl", "-fL", "--retry", "5", "--retry-delay", "3", "--connect-timeout", "30", "-o", file, url };
        const auto r = host( get );
        if ( r.getExitCode() != 0 )
        {
            return soften( failed( get, r ) );
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
        emit progress( double( ++done ) / double( qMax( 1, int( steps.size() ) ) ) );
    }
    return JobResult::ok();
}

JobResult
DistroProfileJob::exec()
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();

    Context ctx;
    ctx.root = gs->value( "rootMountPoint" ).toString();
    ctx.user = gs->value( "username" ).toString();
    ctx.assets = m_assetsDir;
    if ( ctx.root.isEmpty() || !QFileInfo::exists( ctx.root ) )
    {
        return JobResult::error( tr( "No target" ), tr( "globalstorage has no usable \"rootMountPoint\"." ) );
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
    m_regdom = ClaudeInstall::getString( configurationMap, "wirelessRegdom" );
    m_retries = ClaudeInstall::getInteger( configurationMap, "retries", 3 );
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( DistroProfileJobFactory, registerPlugin< DistroProfileJob >(); )
