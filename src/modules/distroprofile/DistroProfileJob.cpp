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
#include "utils/String.h"
#include "utils/System.h"
#include "utils/Variant.h"
#include "utils/Yaml.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QUrl>

#include <QVector>

#include <crypt.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

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

/// Runs @p args in the target through chroot(8) without writing the command, its input or its
/// output to the log (used for the final account reset, whose input is the password).
int
quietInTarget( const QString& root, const QStringList& args, const QString& stdInput = QString(), QString* output = nullptr )
{
    QProcess process;
    process.setProgram( QStringLiteral( "chroot" ) );
    process.setArguments( QStringList { root } + args );
    process.setProcessChannelMode( QProcess::MergedChannels );
    process.start();
    if ( !process.waitForStarted() )
    {
        return -1;
    }
    if ( !stdInput.isEmpty() )
    {
        process.write( stdInput.toUtf8() );
    }
    process.closeWriteChannel();
    if ( !process.waitForFinished( 60000 ) )
    {
        process.kill();
        return -1;
    }
    if ( output )
    {
        *output = QString::fromUtf8( process.readAllStandardOutput() );
    }
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

/// Runs @p args in the target as @p uid / @p gid, without PAM (chroot --userspec), with a clean login environment
int
asUserInTarget( const QString& root, const QString& uid, const QString& gid, const QString& user, const QString& home,
                const QString& shell, const QStringList& args )
{
    QProcess process;
    process.setProgram( QStringLiteral( "chroot" ) );
    process.setArguments( QStringList { QStringLiteral( "--userspec=%1:%2" ).arg( uid, gid ), root, "/usr/bin/env", "-i",
                                        "HOME=" + home, "USER=" + user, "LOGNAME=" + user, "SHELL=" + shell,
                                        "PATH=/usr/local/bin:/usr/bin:/bin", "TERM=dumb" }
                          + args );
    process.setProcessChannelMode( QProcess::MergedChannels );
    process.start();
    if ( !process.waitForStarted() )
    {
        return -1;
    }
    process.closeWriteChannel();
    if ( !process.waitForFinished( 60000 ) )
    {
        process.kill();
        return -1;
    }
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

/// The enabled display manager (from display-manager.service), or empty
QString
displayManager( const QString& root )
{
    const QFileInfo link( root + QStringLiteral( "/etc/systemd/system/display-manager.service" ) );
    if ( !link.isSymLink() )
    {
        return QString();
    }
    QString name = QFileInfo( link.symLinkTarget() ).fileName();
    name.chop( name.endsWith( QLatin1String( ".service" ) ) ? 8 : 0 );
    return name;
}

/// Logs @p user in through the new system's own PAM stack (service @p service), the way its login
/// screen does. Empty when the login works; PAM's reason otherwise. @p ran is false when the
/// helper is not installed.
QString
pamLogin( const QString& root, const QString& service, const QString& user, const QString& password, bool& ran, bool authOnly = false )
{
#ifndef LOGINCHECK_PATH
#define LOGINCHECK_PATH "/usr/lib/claudeinstall/claudeinstall-logincheck"
#endif
    ran = false;
    const QString helper = QStringLiteral( LOGINCHECK_PATH );
    if ( !QFileInfo( helper ).isExecutable() )
    {
        return QString();
    }
    const QString copy = QStringLiteral( "/tmp/claudeinstall-logincheck" );
    QDir().mkpath( root + QStringLiteral( "/tmp" ) );
    QFile::remove( root + copy );
    if ( !QFile::copy( helper, root + copy ) )
    {
        return QString();
    }
    QFile::setPermissions( root + copy, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner );
    QString output;
    QStringList args { copy, service, user };
    if ( authOnly )
    {
        args << QStringLiteral( "--auth-only" );
    }
    const int code = quietInTarget( root, args, password + QLatin1Char( '\n' ), &output );
    QFile::remove( root + copy );
    ran = true;
    if ( code == 0 )
    {
        return QString();
    }
    output = output.trimmed().replace( QLatin1Char( '\n' ), QStringLiteral( "; " ) );
    return output.isEmpty() ? QStringLiteral( "PAM login failed (exit code %1)" ).arg( code ) : output;
}

/// Tries every part of the login on its own through the new system's PAM, so the error says
/// which one refuses: pam_unix (password), pam_shells, pam_nologin, pam_faillock, then the
/// system-auth and system-login stacks. One line per part; @p failing gets the keys that fail.
QStringList
pamDiagnosis( const QString& root, const QString& user, const QString& password, QStringList& failing )
{
    struct Probe
    {
        const char* key;
        const char* name;
        const char* service;  ///< an existing service, or nullptr to write @p lines as a test service
        const char* lines;
    };
    static const Probe probes[] = {
        { "unix", "password (pam_unix)", nullptr, "auth required pam_unix.so\n" },
        { "shells", "shell listed in /etc/shells (pam_shells)", nullptr, "auth required pam_shells.so\n" },
        { "nologin", "no nologin file (pam_nologin)", nullptr, "auth required pam_nologin.so\n" },
        { "faillock", "not locked out (pam_faillock)", nullptr, "auth required pam_faillock.so preauth\n" },
        { "system-auth", "system-auth stack", "system-auth", nullptr },
        { "system-login", "system-login stack", "system-login", nullptr },
    };
    const QString probeService = QStringLiteral( "claudeinstall-probe" );
    const QString probeFile = root + QStringLiteral( "/etc/pam.d/" ) + probeService;
    QStringList report;
    for ( const Probe& p : probes )
    {
        QString service;
        if ( p.service )
        {
            service = QLatin1String( p.service );
            if ( !QFileInfo::exists( root + QStringLiteral( "/etc/pam.d/" ) + service ) )
            {
                report << QStringLiteral( "%1: not present" ).arg( QLatin1String( p.name ) );
                continue;
            }
        }
        else
        {
            QFile f( probeFile );
            if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) )
            {
                continue;
            }
            f.write( QByteArray( "#%PAM-1.0\n" ) + p.lines );
            f.close();
            service = probeService;
        }
        bool ran = false;
        const QString result = pamLogin( root, service, user, password, ran, true );
        QFile::remove( probeFile );
        if ( !ran )
        {
            return {};
        }
        report << ( result.isEmpty() ? QStringLiteral( "%1: OK" ).arg( QLatin1String( p.name ) )
                                     : QStringLiteral( "%1: FAILS (%2)" ).arg( QLatin1String( p.name ), result ) );
        if ( !result.isEmpty() )
        {
            failing << QLatin1String( p.key );
        }
    }
    // pam_unix and the stacks are cleared of the test's failed tries
    quietInTarget( root, { "faillock", "--user", user, "--reset" } );
    return report;
}

/// A SHA-512 crypt hash of @p password made here on the live system: every libxcrypt and glibc reads it
QString
sha512Hash( const QString& password )
{
    char salt[ CRYPT_GENSALT_OUTPUT_SIZE ];
    if ( !crypt_gensalt_rn( "$6$", 0, nullptr, 0, salt, int( sizeof( salt ) ) ) )
    {
        return QString();
    }
    auto data = std::make_unique< crypt_data >();
    std::memset( data.get(), 0, sizeof( crypt_data ) );
    const char* hash = crypt_r( password.toUtf8().constData(), salt, data.get() );
    return ( hash && hash[ 0 ] == '$' ) ? QString::fromLatin1( hash ) : QString();
}

/// The shells listed in the target's /etc/shells
QStringList
listedShells( const QString& root )
{
    QFile f( root + QStringLiteral( "/etc/shells" ) );
    QStringList shells;
    if ( f.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        for ( const QString& raw : QString::fromUtf8( f.readAll() ).split( QLatin1Char( '\n' ) ) )
        {
            const QString line = raw.trimmed();
            if ( !line.isEmpty() && !line.startsWith( QLatin1Char( '#' ) ) )
            {
                shells << line;
            }
        }
    }
    return shells;
}

/// What Arch's system-login PAM stack refuses even with the right password:
/// pam_shells (the shell must be listed in /etc/shells exactly) and pam_nologin
QStringList
loginStackProblems( const QString& root, const QString& shellPath )
{
    QStringList problems;
    if ( !shellPath.isEmpty() && !listedShells( root ).contains( shellPath ) )
    {
        problems << QStringLiteral( "the login shell %1 is not listed in /etc/shells, so pam_shells refuses the login" ).arg( shellPath );
    }
    for ( const char* file : { "/etc/nologin", "/run/nologin", "/var/run/nologin" } )
    {
        if ( QFileInfo::exists( root + QLatin1String( file ) ) )
        {
            problems << QStringLiteral( "%1 exists, so pam_nologin refuses every login but root's" ).arg( QLatin1String( file ) );
        }
    }
    return problems;
}

/// What stops the login screen from starting a desktop: no display manager, or no session whose program is installed
QStringList
desktopProblems( const QString& root, const QString& dm )
{
    if ( dm.isEmpty() )
    {
        // some profiles have no login screen on purpose (DM: ""): the console login was tested through PAM "login"
        cWarning() << "No display manager is enabled: the user logs in on the console";
        return {};
    }
    static const QRegularExpression space( QStringLiteral( "\\s+" ) );
    QStringList good, broken;
    for ( const char* dir : { "/usr/share/wayland-sessions", "/usr/share/xsessions" } )
    {
        const QDir d( root + QLatin1String( dir ) );
        for ( const QString& name : d.entryList( { QStringLiteral( "*.desktop" ) }, QDir::Files ) )
        {
            QFile f( d.filePath( name ) );
            if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
            {
                continue;
            }
            QString exec;
            while ( !f.atEnd() && exec.isEmpty() )
            {
                const QString line = QString::fromUtf8( f.readLine() ).trimmed();
                if ( line.startsWith( QLatin1String( "Exec=" ) ) )
                {
                    exec = line.mid( 5 );
                }
            }
            // the program and every absolute path it is handed (wrappers such as plasma-dbus-run-session-if-needed)
            QStringList missing;
            const QStringList words = exec.split( space, Qt::SkipEmptyParts );
            for ( int i = 0; i < words.size(); ++i )
            {
                QString w = words.at( i );
                w.remove( QLatin1Char( '"' ) );
                if ( i > 0 && !w.startsWith( QLatin1Char( '/' ) ) )
                {
                    continue;
                }
                const QString path = w.startsWith( QLatin1Char( '/' ) ) ? root + w : root + QStringLiteral( "/usr/bin/" ) + w;
                if ( !QFileInfo( path ).isExecutable() )
                {
                    missing << w;
                }
            }
            if ( exec.isEmpty() || !missing.isEmpty() )
            {
                broken << QStringLiteral( "%1 (%2 missing)" ).arg( name, missing.isEmpty() ? QStringLiteral( "Exec" ) : missing.join( ", " ) );
            }
            else
            {
                good << name;
            }
        }
    }
    if ( good.isEmpty() )
    {
        return { broken.isEmpty() ? QStringLiteral( "no desktop session is installed (wayland-sessions / xsessions are empty)" )
                                  : QStringLiteral( "no desktop session can start: %1" ).arg( broken.join( "; " ) ) };
    }
    if ( !broken.isEmpty() )
    {
        cWarning() << "Desktop sessions that cannot start:" << broken;
    }
    return {};
}

/// What would stop @p user from logging in with @p password on the new system; empty when all is well.
/// A real login through the new system's PAM stack (the display manager's service), the login shell
/// started as the user, the home directory, and a desktop session the login screen can start.
QStringList
verifyUserAccount( const QString& root, const QString& user, const QString& password, const QString& home )
{
    QStringList problems;

    // 1. The real login: the new system's PAM decides (password, lock, expiry, forced change, faillock)
    const QString dm = displayManager( root );
    const QString service = !dm.isEmpty() && QFileInfo::exists( root + QStringLiteral( "/etc/pam.d/" ) + dm ) ? dm : QStringLiteral( "login" );
    bool pamRan = false;
    const QString pam = pamLogin( root, service, user, password, pamRan );
    if ( !pam.isEmpty() )
    {
        problems << QStringLiteral( "login through PAM service \"%1\" fails: %2" ).arg( service, pam );
    }
    if ( !pamRan )
    {
        cWarning() << "claudeinstall-logincheck is not installed: the login is checked against /etc/shadow only";
    }
    else
    {
        cDebug() << "Login test through PAM service" << service << ( pam.isEmpty() ? "passed" : "failed" );
    }

    // 2. The password against the hash, so the report says which part is wrong
    QString shadow;
    if ( quietInTarget( root, { "getent", "shadow", user }, QString(), &shadow ) != 0 )
    {
        return { QStringLiteral( "no shadow entry" ) };
    }
    const QString hash = shadow.trimmed().section( ':', 1, 1 );
    if ( hash.isEmpty() || hash.startsWith( '!' ) || hash.startsWith( '*' ) )
    {
        problems << QStringLiteral( "account is locked or has no password" );
    }
    else
    {
        auto data = std::make_unique< crypt_data >();
        std::memset( data.get(), 0, sizeof( crypt_data ) );
        const char* computed = crypt_r( password.toUtf8().constData(), hash.toUtf8().constData(), data.get() );
        if ( !computed || hash != QString::fromUtf8( computed ) )
        {
            problems << QStringLiteral( "the password does not match" );
        }
    }

    QString passwd;
    quietInTarget( root, { "getent", "passwd", user }, QString(), &passwd );
    const QStringList entry = passwd.trimmed().split( ':' );
    const QString uid = entry.value( 2 ), gid = entry.value( 3 ), shellPath = entry.value( 6 );
    if ( shellPath.isEmpty() || !QFileInfo( root + shellPath ).isExecutable() || shellPath.endsWith( QLatin1String( "nologin" ) )
         || shellPath.endsWith( QLatin1String( "/false" ) ) )
    {
        problems << QStringLiteral( "login shell %1 is not usable" ).arg( shellPath );
    }
    else if ( asUserInTarget( root, uid, gid, user, home, shellPath, { shellPath, "-l", "-c", "true" } ) != 0 )
    {
        // the desktop session is started through the login shell: if it cannot start, the login screen comes straight back
        problems << QStringLiteral( "the login shell %1 does not start for the user" ).arg( shellPath );
    }
    problems += loginStackProblems( root, shellPath );

    QString owner;
    quietInTarget( root, { "stat", "-c", "%U", home }, QString(), &owner );
    if ( owner.trimmed() != user )
    {
        problems << QStringLiteral( "home directory is owned by %1" ).arg( owner.trimmed() );
    }
    QString rootOwned;
    quietInTarget( root, { "find", home, "-user", "root", "-print", "-quit" }, QString(), &rootOwned );
    if ( !rootOwned.trimmed().isEmpty() )
    {
        problems << QStringLiteral( "%1 is owned by root" ).arg( rootOwned.trimmed() );
    }
    if ( asUserInTarget( root, uid, gid, user, home, shellPath, { "/usr/bin/test", "-w", home } ) != 0 )
    {
        problems << QStringLiteral( "the user cannot write to the home directory" );
    }

    // 3. Something for the login screen to start
    problems += desktopProblems( root, dm );
    return problems;
}

/// Clears what blocks an account that has the right password: a login shell pam_shells does not
/// accept, nologin files, failed-login lock (faillock), a locked password, expiry and a forced password change
void
unblockAccount( const QString& root, const QString& user )
{
    QString passwd;
    quietInTarget( root, { "getent", "passwd", user }, QString(), &passwd );
    const QString shell = passwd.trimmed().section( ':', 6, 6 );
    const QStringList shells = listedShells( root );
    if ( !shell.isEmpty() && !shells.contains( shell ) )
    {
        // The same program under the name /etc/shells uses (/usr/sbin/fish -> /usr/bin/fish), else list it,
        // else (the shell is not installed at all) bash
        QString use;
        for ( const QString& s : shells )
        {
            if ( QFileInfo( s ).fileName() == QFileInfo( shell ).fileName() && QFileInfo( root + s ).isExecutable() )
            {
                use = s;
                break;
            }
        }
        if ( use.isEmpty() && QFileInfo( root + shell ).isExecutable() && !shell.endsWith( QLatin1String( "nologin" ) ) )
        {
            QFile f( root + QStringLiteral( "/etc/shells" ) );
            if ( f.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
            {
                f.write( ( shell + QLatin1Char( '\n' ) ).toUtf8() );
            }
            cWarning() << "Login shell" << shell << "was not in /etc/shells; added it";
        }
        else
        {
            if ( use.isEmpty() )
            {
                use = QStringLiteral( "/bin/bash" );
            }
            quietInTarget( root, { "usermod", "-s", use, user } );
            cWarning() << "Login shell" << shell << "is not accepted by pam_shells; changed to" << use;
        }
    }
    for ( const char* file : { "/etc/nologin", "/run/nologin", "/var/run/nologin" } )
    {
        QFile::remove( root + QLatin1String( file ) );
    }
    quietInTarget( root, { "faillock", "--user", user, "--reset" } );
    quietInTarget( root, { "usermod", "-U", user } );
    quietInTarget( root, { "chage", "-E", "-1", "-I", "-1", "-m", "0", "-M", "99999", "-d",
                           QDate::currentDate().toString( Qt::ISODate ), user } );
}

/// Last step before unmounting: delete the user inside the new system, create it again and
/// set the password with chpasswd, then check that the login works (nothing is logged).
JobResult
resetUserAccount( const QString& root, const QString& user, const QString& password )
{
    const auto bad = []( const QString& what ) {
        return JobResult::error( QCoreApplication::translate( "DistroProfileJob", "Cannot reset the user account" ), what );
    };

    // Remember what the users module set up, so the new account matches it
    QString passwd;
    if ( quietInTarget( root, { "getent", "passwd", user }, QString(), &passwd ) != 0 )
    {
        return bad( QCoreApplication::translate( "DistroProfileJob", "The user does not exist in the new system." ) );
    }
    const QStringList f = passwd.trimmed().split( ':' );
    if ( f.size() < 7 )
    {
        return bad( QCoreApplication::translate( "DistroProfileJob", "Unreadable account entry." ) );
    }
    const QString uid = f.at( 2 ), comment = f.at( 4 ), home = f.at( 5 ), shell = f.at( 6 );

    QString groupList;
    quietInTarget( root, { "id", "-nG", user }, QString(), &groupList );
    const QStringList oldGroups = groupList.trimmed().split( ' ', Qt::SkipEmptyParts );

    if ( quietInTarget( root, { "userdel", user } ) != 0 )
    {
        return bad( QCoreApplication::translate( "DistroProfileJob", "userdel failed." ) );
    }

    // The user's own group went with the user; the others still exist
    QStringList groups;
    for ( const QString& g : oldGroups )
    {
        if ( g != user && quietInTarget( root, { "getent", "group", g } ) == 0 )
        {
            groups << g;
        }
    }

    QStringList add { "useradd", "-M", "-u", uid, "-d", home, "-s", shell, "-c", comment };
    if ( !groups.isEmpty() )
    {
        add << "-G" << groups.join( ',' );
    }
    add << user;
    if ( quietInTarget( root, add ) != 0 )
    {
        return bad( QCoreApplication::translate( "DistroProfileJob", "useradd failed." ) );
    }

    // The password goes in exactly as it is typed by hand: chpasswd <<< 'user:password'
    // (single quotes in the password are escaped for bash)
    QString quoted = user + QLatin1Char( ':' ) + password;
    quoted.replace( QLatin1Char( '\'' ), QStringLiteral( "'\\''" ) );
    const QStringList setPassword { "bash", "-c", QStringLiteral( "chpasswd <<< '%1'" ).arg( quoted ) };
    if ( quietInTarget( root, setPassword ) != 0 )
    {
        return bad( QCoreApplication::translate( "DistroProfileJob", "chpasswd failed." ) );
    }

    // The home directory survived userdel (and may hold files made as root by earlier steps):
    // it and everything in it belongs to the user and the user's login group
    const auto fixHome = [ & ]() {
        quietInTarget( root, { "mkdir", "-p", home } );
        quietInTarget( root, { "chown", "-R", user + QLatin1Char( ':' ), home } );
        quietInTarget( root, { "chmod", "700", home } );
    };
    fixHome();

    // Check phase: log in to the new account the way the login screen will
    unblockAccount( root, user );
    const QStringList problems = verifyUserAccount( root, user, password, home );
    if ( !problems.isEmpty() )
    {
        cWarning() << "Login test failed:" << problems << "- testing every part of the login, repairing, testing again";
        // Find the part of the login that refuses, then repair it
        QStringList failing;
        const QStringList before = pamDiagnosis( root, user, password, failing );
        cWarning() << "Login parts:" << before;

        // Every part passes on its own but the login screen's service refuses: its PAM file is broken
        const QString sddmPam = root + QStringLiteral( "/etc/pam.d/sddm" );
        if ( !before.isEmpty() && failing.isEmpty() && displayManager( root ) == QLatin1String( "sddm" ) )
        {
            QFile::remove( sddmPam + QStringLiteral( ".claudeinstall-orig" ) );
            QFile::copy( sddmPam, sddmPam + QStringLiteral( ".claudeinstall-orig" ) );
            QFile f( sddmPam );
            if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) )
            {
                // Arch's own sddm PAM file
                f.write( "#%PAM-1.0\n\n"
                         "auth        include     system-login\n"
                         "-auth       optional    pam_gnome_keyring.so\n"
                         "-auth       optional    pam_kwallet5.so\n\n"
                         "account     include     system-login\n\n"
                         "password    include     system-login\n"
                         "-password   optional    pam_gnome_keyring.so use_authtok\n\n"
                         "session     optional    pam_keyinit.so force revoke\n"
                         "session     include     system-login\n"
                         "-session    optional    pam_gnome_keyring.so auto_start\n"
                         "-session    optional    pam_kwallet5.so auto_start\n" );
            }
            cWarning() << "/etc/pam.d/sddm refused a login that every part allows; replaced with Arch's (old one kept as .claudeinstall-orig)";
        }

        quietInTarget( root, setPassword );
        if ( failing.contains( QStringLiteral( "unix" ) ) )
        {
            // the new system's pam_unix does not accept the hash chpasswd made: store a SHA-512 one made here
            const QString hash = sha512Hash( password );
            if ( !hash.isEmpty() )
            {
                quietInTarget( root, { "usermod", "-p", hash, user } );
            }
        }
        unblockAccount( root, user );
        if ( failing.contains( QStringLiteral( "shells" ) ) )
        {
            // still not accepted: bash, listed in /etc/shells
            if ( !listedShells( root ).contains( QStringLiteral( "/bin/bash" ) ) )
            {
                QFile f( root + QStringLiteral( "/etc/shells" ) );
                if ( f.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
                {
                    f.write( "/bin/bash\n" );
                }
            }
            quietInTarget( root, { "usermod", "-s", "/bin/bash", user } );
        }
        fixHome();
        const QStringList again = verifyUserAccount( root, user, password, home );
        // the failed tries must not count against the user at the first real login
        quietInTarget( root, { "faillock", "--user", user, "--reset" } );
        if ( !again.isEmpty() )
        {
            // The error dialog itself says which part refuses, before and after the repair
            QStringList after;
            const QStringList now = pamDiagnosis( root, user, password, after );
            QString entry;
            quietInTarget( root, { "getent", "passwd", user }, QString(), &entry );
            return bad( QCoreApplication::translate( "DistroProfileJob",
                                                     "The new system would not let %1 log in:\n%2\n\n"
                                                     "Each part of the login tested on its own (after repairing):\n%3\n\n"
                                                     "Before repairing:\n%4\n\nAccount: %5" )
                            .arg( user,
                                  again.join( QLatin1Char( '\n' ) ),
                                  now.isEmpty() ? QStringLiteral( "(test helper missing)" ) : now.join( QLatin1Char( '\n' ) ),
                                  before.isEmpty() ? QStringLiteral( "(test helper missing)" ) : before.join( QLatin1Char( '\n' ) ),
                                  entry.trimmed().section( ':', 0, 0 ) + QStringLiteral( ", shell " )
                                      + entry.trimmed().section( ':', 6, 6 ) ) );
        }
        cDebug() << "Login repaired; parts before the repair:" << before;
    }
    cDebug() << "Login test passed for" << user;
    return JobResult::ok();
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

/// "12.3M" / "850k" / "4096" as printed by curl's progress meter, in bytes
double
parseCurlSize( const QString& text, bool& ok )
{
    static const QRegularExpression re( QStringLiteral( "^([0-9]+(?:\\.[0-9]+)?)([kMGTP]?)$" ) );
    const auto m = re.match( text );
    ok = m.hasMatch();
    if ( !ok )
    {
        return 0.0;
    }
    double value = m.captured( 1 ).toDouble();
    const int power = m.captured( 2 ).isEmpty() ? 0 : QStringLiteral( "kMGTP" ).indexOf( m.captured( 2 ) ) + 1;
    for ( int i = 0; i < power; ++i )
    {
        value *= 1024.0;
    }
    return value;
}

QString
humanSize( double bytes )
{
    static const char* const units[] = { "B", "KiB", "MiB", "GiB", "TiB" };
    int i = 0;
    while ( bytes >= 1024.0 && i < 4 )
    {
        bytes /= 1024.0;
        ++i;
    }
    return QString::number( bytes, 'f', i == 0 ? 0 : 1 ) + QLatin1Char( ' ' ) + QLatin1String( units[ i ] );
}

/**
 * One row of curl's progress meter turned into words: percentage, size done / total,
 * speed, size left and time left. Rows that are not the meter come back unchanged;
 * the meter's header row comes back empty.
 */
QString
curlProgress( const QString& line )
{
    static const QRegularExpression space( QStringLiteral( "\\s+" ) );
    const QStringList t = line.split( space, Qt::SkipEmptyParts );
    if ( t.size() == 12 && t.first() == QLatin1String( "%" ) )
    {
        return QString();
    }
    if ( t.size() < 12 )
    {
        return line;
    }
    bool isNumber = false;
    t.first().toInt( &isNumber );
    if ( !isNumber )
    {
        return line;
    }

    bool okTotal = false, okGot = false, okSpeed = false;
    const double total = parseCurlSize( t.at( 1 ), okTotal );
    const double got = parseCurlSize( t.at( 3 ), okGot );
    const double speed = parseCurlSize( t.at( 11 ), okSpeed );
    if ( !okTotal || !okGot )
    {
        return line;
    }
    const QString rate = okSpeed ? humanSize( speed ) + QStringLiteral( "/s" ) : QStringLiteral( "-" );
    const QString eta = t.at( 10 ).contains( QLatin1Char( ':' ) ) && !t.at( 10 ).contains( QLatin1Char( '-' ) )
        ? QStringLiteral( ", %1 remaining" ).arg( t.at( 10 ) )
        : QString();
    if ( total <= 0.0 )
    {
        return QStringLiteral( "%1 received, %2" ).arg( humanSize( got ), rate );
    }
    return QStringLiteral( "%1% - %2 of %3, %4, %5 left%6" )
        .arg( t.first(), humanSize( got ), humanSize( total ), rate, humanSize( total - got ), eta );
}

/// Bytes received and sent by every network interface except loopback, from /proc/net/dev
bool
readNetCounters( qint64& rx, qint64& tx )
{
    QFile f( QStringLiteral( "/proc/net/dev" ) );
    if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        return false;
    }
    static const QRegularExpression space( QStringLiteral( "\\s+" ) );
    rx = tx = 0;
    const QStringList lines = QString::fromLatin1( f.readAll() ).split( QLatin1Char( '\n' ) );
    for ( const QString& line : lines )
    {
        const int colon = line.indexOf( QLatin1Char( ':' ) );
        if ( colon < 0 || line.left( colon ).trimmed() == QLatin1String( "lo" ) )
        {
            continue;
        }
        const QStringList v = line.mid( colon + 1 ).split( space, Qt::SkipEmptyParts );
        if ( v.size() >= 9 )
        {
            rx += v.at( 0 ).toLongLong();
            tx += v.at( 8 ).toLongLong();
        }
    }
    return true;
}

/// "https://host/arch/$repo/os/$arch" -> "host/arch/": a mirror the same way the mirror status names it
QString
mirrorKey( const QString& server )
{
    QString s = server;
    s.remove( QRegularExpression( QStringLiteral( "^[a-z]+://" ) ) );
    const int repo = s.indexOf( QLatin1String( "$repo" ) );
    if ( repo >= 0 )
    {
        s.truncate( repo );
    }
    return s.endsWith( QLatin1Char( '/' ) ) ? s : s + QLatin1Char( '/' );
}

/// Hosts that pacman says it could not download from ("failed retrieving file 'x' from host : ...")
QStringList
failedMirrors( const QString& output )
{
    static const QRegularExpression re( QStringLiteral( "failed retrieving file '[^']*' from (\\S+)" ) );
    QStringList hosts;
    auto it = re.globalMatch( output );
    while ( it.hasNext() )
    {
        hosts << it.next().captured( 1 );
    }
    hosts.removeDuplicates();
    return hosts;
}

/// "libfoo.so=1-64" -> "libfoo.so"
QString
withoutVersion( const QString& dependency )
{
    const int cut = dependency.indexOf( QRegularExpression( QStringLiteral( "[=<>]" ) ) );
    return cut < 0 ? dependency : dependency.left( cut );
}

int
editDistance( const QString& a, const QString& b )
{
    QVector< int > prev( b.size() + 1 ), cur( b.size() + 1 );
    for ( int j = 0; j <= b.size(); ++j )
    {
        prev[ j ] = j;
    }
    for ( int i = 1; i <= a.size(); ++i )
    {
        cur[ 0 ] = i;
        for ( int j = 1; j <= b.size(); ++j )
        {
            cur[ j ] = std::min( { prev[ j ] + 1, cur[ j - 1 ] + 1, prev[ j - 1 ] + ( a.at( i - 1 ) == b.at( j - 1 ) ? 0 : 1 ) } );
        }
        std::swap( prev, cur );
    }
    return prev[ b.size() ];
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
    if ( m_phase == QLatin1String( "finalize" ) )
    {
        return tr( "Finish the user account", "@status" );
    }
    return m_phase == QLatin1String( "customize" ) ? tr( "Apply the distribution's customisation", "@status" )
                                                   : tr( "Install the distribution", "@status" );
}

QString
DistroProfileJob::prettyStatusMessage() const
{
    QMutexLocker lock( &m_statusMutex );
    // While something downloads, the live download / upload speed is shown with the status
    return m_netRates.isEmpty() ? m_status : m_status + QStringLiteral( "   |   " ) + m_netRates;
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

QString
DistroProfileJob::statusText() const
{
    QMutexLocker lock( &m_statusMutex );
    return m_status;
}

void
DistroProfileJob::sampleNetwork()
{
    if ( !m_clock.isValid() )
    {
        return;
    }
    const qint64 now = m_clock.elapsed();
    if ( m_rxBytes >= 0 && now - m_netSampleMs < 1000 )
    {
        return;
    }
    qint64 rx = 0, tx = 0;
    if ( !readNetCounters( rx, tx ) )
    {
        return;
    }
    if ( m_rxBytes >= 0 && now > m_netSampleMs )
    {
        const double seconds = double( now - m_netSampleMs ) / 1000.0;
        const QString rates = QStringLiteral( "↓ %1   ↑ %2" )
                                  .arg( tr( "Download %1/s" ).arg( humanSize( double( qMax< qint64 >( 0, rx - m_rxBytes ) ) / seconds ) ),
                                        tr( "Upload %1/s" ).arg( humanSize( double( qMax< qint64 >( 0, tx - m_txBytes ) ) / seconds ) ) );
        {
            QMutexLocker lock( &m_statusMutex );
            m_netRates = rates;
        }
        emit progress( m_lastProgress );
    }
    m_rxBytes = rx;
    m_txBytes = tx;
    m_netSampleMs = now;
}

void
DistroProfileJob::stopNetworkMeter()
{
    QMutexLocker lock( &m_statusMutex );
    m_netRates.clear();
    m_rxBytes = -1;
}

int
DistroProfileJob::runStreaming( const QStringList& cmd, QString& output, int stallSeconds )
{
    QProcess proc;
    proc.setProcessChannelMode( QProcess::MergedChannels );
    proc.start( cmd.first(), cmd.mid( 1 ) );
    if ( !proc.waitForStarted( 30000 ) )
    {
        output = tr( "Cannot start %1." ).arg( cmd.first() );
        return -1;
    }

    // Lines with a speed in them ("1.2 MiB/s") are download progress; anything else
    // (installing, hooks) shows that the command is getting on with its work
    static const QRegularExpression downloadLine( QStringLiteral( "[0-9.]+\\s*[KMGT]?i?B/s" ) );
    bool worked = false;

    const QString prefix = statusText();
    const bool isCurl = cmd.first() == QLatin1String( "curl" );
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
            const QString raw = parts.at( i ).trimmed();
            const QString line = ( isCurl && !raw.isEmpty() ) ? curlProgress( raw ) : raw;
            if ( !line.isEmpty() )
            {
                setStatus( prefix + QStringLiteral( ": " ) + line.left( 130 ) );
                if ( !downloadLine.match( raw ).hasMatch() && !raw.startsWith( QLatin1String( "error:" ) )
                     && !raw.startsWith( QLatin1String( "warning:" ) ) )
                {
                    worked = true;
                }
            }
        }
    };

    // Watchdog: every stallSeconds the command must have downloaded at least m_stallMinKiBps
    // on average, or done other work. If it did neither it is stuck on a dead or crawling server.
    QElapsedTimer window;
    window.start();
    qint64 rxStart = 0, txStart = 0;
    readNetCounters( rxStart, txStart );
    while ( !proc.waitForFinished( 300 ) )
    {
        consume( proc.readAll() );
        sampleNetwork();
        if ( stallSeconds <= 0 || window.elapsed() < qint64( stallSeconds ) * 1000 )
        {
            continue;
        }
        qint64 rx = 0, tx = 0;
        readNetCounters( rx, tx );
        const qint64 received = rx - rxStart;
        const qint64 needed = qint64( m_stallMinKiBps ) * 1024 * stallSeconds;
        if ( worked || received >= needed )
        {
            cDebug() << "Watchdog:" << cmd.first() << "is making progress, received" << humanSize( double( received ) ) << "in the last"
                     << stallSeconds << "s";
            worked = false;
            rxStart = rx;
            window.restart();
            continue;
        }
        cWarning() << "Watchdog:" << cmd.first() << "received only" << humanSize( double( received ) ) << "in" << stallSeconds
                   << "s and did nothing else; stopping it";
        output += tr( "\nNo download progress for %1 s (%2 received), stopped." ).arg( stallSeconds ).arg( humanSize( double( received ) ) );
        proc.terminate();
        if ( !proc.waitForFinished( 5000 ) )
        {
            proc.kill();
            proc.waitForFinished( 5000 );
        }
        stopNetworkMeter();
        return -2;
    }
    consume( proc.readAll() );
    stopNetworkMeter();
    return proc.exitStatus() == QProcess::NormalExit ? proc.exitCode() : -1;
}

QStringList
DistroProfileJob::runParallel( const QList< QStringList >& cmds, int timeoutMs )
{
    std::vector< std::unique_ptr< QProcess > > procs;
    for ( const QStringList& cmd : cmds )
    {
        auto p = std::make_unique< QProcess >();
        p->start( cmd.first(), cmd.mid( 1 ) );
        procs.push_back( std::move( p ) );
    }
    QElapsedTimer t;
    t.start();
    for ( ;; )
    {
        bool done = true;
        for ( auto& p : procs )
        {
            if ( p->state() != QProcess::NotRunning )
            {
                done = false;
                p->waitForFinished( 50 );
            }
        }
        sampleNetwork();
        if ( done || t.elapsed() > timeoutMs )
        {
            break;
        }
    }
    QStringList out;
    for ( auto& p : procs )
    {
        if ( p->state() != QProcess::NotRunning )
        {
            p->kill();
            p->waitForFinished( 1000 );
        }
        out << QString::fromUtf8( p->readAllStandardOutput() ).trimmed();
    }
    return out;
}

JobResult
DistroProfileJob::checkNetwork()
{
    static const QRegularExpression rttRe( QStringLiteral( "= [0-9.]+/([0-9.]+)/" ) );
    static const QRegularExpression lossRe( QStringLiteral( "([0-9.]+)% packet loss" ) );
    QElapsedTimer waited;
    waited.start();
    for ( int round = 1;; ++round )
    {
        setStatus( tr( "Checking the internet connection (ping)", "@status" ) );
        QString target;
        double rtt = -1.0, loss = 100.0;
        for ( const char* t : { "1.1.1.1", "8.8.8.8", "archlinux.org" } )
        {
            const auto r = host( { "ping", "-c", "4", "-i", "0.25", "-W", "2", t } );
            const auto m = rttRe.match( r.getOutput() );
            if ( m.hasMatch() )
            {
                target = QString::fromLatin1( t );
                rtt = m.captured( 1 ).toDouble();
                const auto l = lossRe.match( r.getOutput() );
                loss = l.hasMatch() ? l.captured( 1 ).toDouble() : 0.0;
                break;
            }
        }
        const bool dns = host( { "getent", "hosts", "archlinux.org" } ).getExitCode() == 0;
        // Some networks block ping: an https answer is just as good
        const bool https = rtt < 0 && dns
            && host( { "curl", "-sS", "-o", "/dev/null", "-I", "--connect-timeout", "5", "--max-time", "10", "https://archlinux.org/" } ).getExitCode() == 0;

        if ( dns && ( rtt >= 0 || https ) )
        {
            const QString how = rtt >= 0 ? tr( "ping %1 ms to %2, %3% packet loss" ).arg( QString::number( rtt, 'f', 0 ), target, QString::number( loss ) )
                                         : tr( "ping is blocked, https works" );
            setStatus( tr( "Internet connection OK: %1", "@status" ).arg( how ) );
            cDebug() << "Network check:" << how;
            if ( loss >= 25.0 )
            {
                cWarning() << "Heavy packet loss (" << loss << "%): downloads may be slow";
            }
            return JobResult::ok();
        }

        const QString why = dns ? tr( "no server answers" ) : tr( "names cannot be looked up (DNS)" );
        if ( waited.elapsed() > 120000 )
        {
            return JobResult::error( tr( "No internet connection" ),
                                     tr( "The packages are downloaded during the installation, but %1 since two minutes.\n"
                                         "Check the cable or Wi-Fi connection and start the installation again." )
                                         .arg( why ) );
        }
        cWarning() << "Network check" << round << "failed:" << why;
        setStatus( tr( "No internet connection (%1); checking again in 5 s", "@status" ).arg( why ) );
        QThread::sleep( 5 );
    }
}

QMap< QString, QString >
DistroProfileJob::mirrorRepos() const
{
    // [repo] sections of pacman.conf whose "Include =" names a mirror list
    QFile f( QStringLiteral( "/etc/pacman.conf" ) );
    if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        return {};
    }
    static const QRegularExpression sectionRe( QStringLiteral( "^\\s*\\[([^\\]]+)\\]" ) );
    static const QRegularExpression includeRe( QStringLiteral( "^\\s*Include\\s*=\\s*(\\S+)" ) );
    QMap< QString, QStringList > byFile;
    QString section;
    while ( !f.atEnd() )
    {
        const QString line = QString::fromUtf8( f.readLine() );
        const auto s = sectionRe.match( line );
        if ( s.hasMatch() )
        {
            section = s.captured( 1 ).trimmed();
            continue;
        }
        const auto i = includeRe.match( line );
        if ( i.hasMatch() && !section.isEmpty() && section != QLatin1String( "options" ) )
        {
            byFile[ QDir::cleanPath( i.captured( 1 ) ) ] << section;
        }
    }
    // The biggest database makes the best speed test: prefer an "extra" repository
    QMap< QString, QString > out;
    for ( auto it = byFile.constBegin(); it != byFile.constEnd(); ++it )
    {
        QString repo = it.value().first();
        for ( const QString& r : it.value() )
        {
            if ( r.contains( QLatin1String( "extra" ) ) )
            {
                repo = r;
                break;
            }
        }
        out.insert( it.key(), repo );
    }
    return out;
}

void
DistroProfileJob::archMirrorsFromStatus( QStringList& good, QSet< QString >& stale )
{
    // archlinux.org checks every mirror all the time: only mirrors that are fully synced and
    // less than an hour behind are used, the ones it reports as behind are dropped
    setStatus( tr( "Getting the up-to-date mirrors from archlinux.org/mirrors/status", "@status" ) );
    const QString json = runParallel( { { "curl", "-fsSL", "--connect-timeout", "10", "--max-time", "30",
                                          "https://archlinux.org/mirrors/status/json/" } },
                                      35000 )
                             .value( 0 );
    const QJsonArray urls = QJsonDocument::fromJson( json.toUtf8() ).object().value( QStringLiteral( "urls" ) ).toArray();
    if ( urls.isEmpty() )
    {
        cWarning() << "The Arch Linux mirror status could not be read; testing the servers already in the list";
        return;
    }
    QVector< QPair< double, QString > > scored;
    QSet< QString > freshKeys, staleKeys;
    for ( const QJsonValue& v : urls )
    {
        const QJsonObject o = v.toObject();
        const QString url = o.value( QStringLiteral( "url" ) ).toString();
        if ( url.isEmpty() )
        {
            continue;
        }
        const QJsonValue delay = o.value( QStringLiteral( "delay" ) );
        const QJsonValue score = o.value( QStringLiteral( "score" ) );
        const bool fresh = o.value( QStringLiteral( "active" ) ).toBool( true )
            && o.value( QStringLiteral( "completion_pct" ) ).toDouble() >= 0.99 && delay.isDouble() && delay.toDouble() <= 3600.0
            && score.isDouble();
        if ( !fresh )
        {
            staleKeys.insert( mirrorKey( url ) );
            continue;
        }
        freshKeys.insert( mirrorKey( url ) );
        if ( o.value( QStringLiteral( "protocol" ) ).toString() == QLatin1String( "https" ) )
        {
            scored.append( { score.toDouble(), url } );
        }
    }
    std::sort( scored.begin(), scored.end(), []( const auto& a, const auto& b ) { return a.first < b.first; } );
    for ( const auto& s : std::as_const( scored ) )
    {
        good << s.second + ( s.second.endsWith( '/' ) ? QString() : QStringLiteral( "/" ) ) + QStringLiteral( "$repo/os/$arch" );
    }
    stale = staleKeys - freshKeys;
    cDebug() << "Mirror status:" << good.size() << "up-to-date https mirrors," << stale.size() << "out of date";
}

void
DistroProfileJob::rankMirrors()
{
    if ( !m_rankMirrors )
    {
        return;
    }
    // "Never time out" is what leaves a download hanging on a dead server
    host( { "sed", "-i", "/^[[:space:]]*DisableDownloadTimeout/d", "/etc/pacman.conf" } );

    m_topHosts.clear();
    const QMap< QString, QString > repos = mirrorRepos();
    const QDir dir( QStringLiteral( "/etc/pacman.d" ) );
    const QStringList lists = dir.entryList( { QStringLiteral( "*mirrorlist" ) }, QDir::Files );
    for ( const QString& name : lists )
    {
        const QString path = QDir::cleanPath( dir.filePath( name ) );
        if ( !repos.isEmpty() && !repos.contains( path ) )
        {
            cDebug() << path << "is not used by pacman.conf; not tested";
            continue;
        }
        QString repo = repos.value( path );
        if ( repo.isEmpty() )
        {
            repo = name == QLatin1String( "mirrorlist" ) ? QStringLiteral( "extra" )
                                                         : name.chopped( int( QStringLiteral( "-mirrorlist" ).size() ) );
        }
        QStringList official;
        QSet< QString > stale;
        if ( name == QLatin1String( "mirrorlist" ) && m_archMirrorStatus )
        {
            archMirrorsFromStatus( official, stale );
        }
        rankMirrorFile( path, repo, official.mid( 0, 40 ), stale );
    }
    stopNetworkMeter();
}

void
DistroProfileJob::rankMirrorFile( const QString& path, const QString& repo, const QStringList& official, const QSet< QString >& stale )
{
    QFile in( path );
    if ( !in.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        return;
    }
    static const QRegularExpression serverRe( QStringLiteral( "^\\s*Server\\s*=\\s*(\\S+)" ) );
    QStringList servers;
    int dropped = 0;
    while ( !in.atEnd() )
    {
        const auto m = serverRe.match( QString::fromUtf8( in.readLine() ) );
        if ( !m.hasMatch() || servers.contains( m.captured( 1 ) ) )
        {
            continue;
        }
        if ( stale.contains( mirrorKey( m.captured( 1 ) ) ) )
        {
            ++dropped;  // the mirror status says it is behind: it would hand out old or missing files
            continue;
        }
        servers << m.captured( 1 );
    }
    in.close();
    // The list's own servers first (they are usually the local ones), then the best official ones
    for ( const QString& s : official )
    {
        if ( !servers.contains( s ) )
        {
            servers << s;
        }
    }
    // Servers that already failed this installation go only if something else is left
    QStringList usable;
    for ( const QString& s : std::as_const( servers ) )
    {
        if ( !m_badHosts.contains( QUrl( s ).host() ) )
        {
            usable << s;
        }
    }
    if ( !usable.isEmpty() )
    {
        servers = usable;
    }
    servers = servers.mid( 0, 80 );
    if ( servers.size() < 2 )
    {
        return;
    }
    const QString file = QFileInfo( path ).fileName();
    auto databaseUrl = [ & ]( const QString& server )
    {
        QString url = server;
        url.replace( QLatin1String( "$repo" ), repo ).replace( QLatin1String( "$arch" ), QStringLiteral( "x86_64" ) );
        return url + '/' + repo + QStringLiteral( ".db" );
    };
    struct Result
    {
        QString server;
        double pingMs = -1.0;
        double speed = -1.0;  ///< bytes per second, -1 when not tested
    };

    // 1. Ping: the TCP connect time of every server, 16 at a time. A server must answer with
    //    the repository database itself (HTTP 200/206), so dead and broken links are dropped.
    QVector< Result > alive;
    for ( int first = 0; first < servers.size(); first += 16 )
    {
        const QStringList batch = servers.mid( first, 16 );
        setStatus( tr( "%1: ping test of servers %2-%3 of %4", "@status" )
                       .arg( file )
                       .arg( first + 1 )
                       .arg( first + batch.size() )
                       .arg( servers.size() ) );
        QList< QStringList > cmds;
        for ( const QString& s : batch )
        {
            cmds << QStringList { "curl", "-s", "-L", "--head", "-o", "/dev/null", "--connect-timeout", "3", "--max-time", "6",
                                  "-w", "%{http_code} %{exitcode} %{time_namelookup} %{time_connect}", databaseUrl( s ) };
        }
        const QStringList outs = runParallel( cmds, 9000 );
        for ( int i = 0; i < batch.size(); ++i )
        {
            const QStringList f = outs.value( i ).split( ' ' );
            if ( f.size() != 4 || ( f.at( 0 ) != QLatin1String( "200" ) && f.at( 0 ) != QLatin1String( "206" ) )
                 || f.at( 1 ) != QLatin1String( "0" ) )
            {
                continue;
            }
            const double connect = f.at( 3 ).toDouble(), lookup = f.at( 2 ).toDouble();
            alive.append( { batch.at( i ), ( connect > lookup ? connect - lookup : connect ) * 1000.0 } );
        }
    }
    if ( alive.isEmpty() )
    {
        cWarning() << "No server in" << path << "answered the ping test; leaving it as it is";
        return;
    }
    std::sort( alive.begin(), alive.end(), []( const Result& a, const Result& b ) { return a.pingMs < b.pingMs; } );

    // 2. Speed: download the database for up to 5 s from the closest servers, one at a time so
    //    they do not share the line. What counts is the real download rate.
    const int tests = qMin( m_speedTests, int( alive.size() ) );
    QVector< Result > ranked, broken;
    for ( int i = 0; i < alive.size(); ++i )
    {
        Result r = alive.at( i );
        if ( i >= tests )
        {
            ranked.append( r );
            continue;
        }
        const QString hostName = QUrl( r.server ).host();
        setStatus( tr( "%1: speed test %2 of %3, %4 (ping %5 ms)", "@status" )
                       .arg( file )
                       .arg( i + 1 )
                       .arg( tests )
                       .arg( hostName, QString::number( r.pingMs, 'f', 0 ) ) );
        const QStringList f = runParallel( { { "curl", "-s", "-L", "-o", "/dev/null", "--connect-timeout", "3", "--max-time", "5", "-w",
                                               "%{http_code} %{exitcode} %{speed_download} %{size_download}", databaseUrl( r.server ) } },
                                           8000 )
                                  .value( 0 )
                                  .split( ' ' );
        const bool ok = f.size() == 4 && f.at( 0 ) == QLatin1String( "200" )
            && ( f.at( 1 ) == QLatin1String( "0" ) || f.at( 1 ) == QLatin1String( "28" ) ) && f.at( 3 ).toDouble() >= 1024.0;
        if ( !ok )
        {
            cWarning() << "Speed test of" << r.server << "failed:" << f.join( ' ' );
            broken.append( r );
            continue;
        }
        r.speed = f.at( 2 ).toDouble();
        cDebug() << "Mirror" << hostName << "ping" << r.pingMs << "ms," << humanSize( r.speed ) << "/s";
        setStatus( tr( "%1: %2 - ping %3 ms, %4/s", "@status" )
                       .arg( file, hostName, QString::number( r.pingMs, 'f', 0 ), humanSize( r.speed ) ) );
        ranked.append( r );
    }
    // Measured servers by speed, then the untested ones by ping (failed speed tests are left out)
    std::stable_sort( ranked.begin(),
                      ranked.end(),
                      []( const Result& a, const Result& b )
                      {
                          if ( ( a.speed >= 0 ) != ( b.speed >= 0 ) )
                          {
                              return a.speed >= 0;
                          }
                          return a.speed >= 0 ? a.speed > b.speed : a.pingMs < b.pingMs;
                      } );
    if ( ranked.isEmpty() )
    {
        ranked = broken;  // better than nothing
    }

    if ( !QFileInfo::exists( path + QStringLiteral( ".claudeinstall-orig" ) ) )
    {
        QFile::copy( path, path + QStringLiteral( ".claudeinstall-orig" ) );
    }
    QFile out( path );
    if ( !out.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) )
    {
        return;
    }
    QTextStream ts( &out );
    ts << "# Ranked by ClaudeInstall on " << QDateTime::currentDateTimeUtc().toString( Qt::ISODate ) << '\n'
       << "# Ping test of " << servers.size() << " servers (" << alive.size() << " answered with the " << repo
       << " database), speed test of the " << tests << " closest; fastest first.\n";
    if ( dropped > 0 )
    {
        ts << "# " << dropped << " servers dropped: out of date according to archlinux.org/mirrors/status\n";
    }
    for ( int i = 0; i < ranked.size() && i < m_mirrorKeep; ++i )
    {
        const Result& r = ranked.at( i );
        ts << "# ping " << QString::number( r.pingMs, 'f', 0 ) << " ms"
           << ( r.speed >= 0 ? QStringLiteral( ", %1/s" ).arg( humanSize( r.speed ) ) : QString() ) << '\n'
           << "Server = " << r.server << '\n';
    }
    const Result& best = ranked.first();
    m_topHosts << QUrl( best.server ).host();
    cDebug() << "Fastest server for" << repo << ":" << best.server << "ping" << best.pingMs << "ms,"
             << ( best.speed >= 0 ? humanSize( best.speed ) + QStringLiteral( "/s" ) : QStringLiteral( "speed not measured" ) );
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

void
DistroProfileJob::loadRenameTable()
{
    // profiles/package-renames.conf is plain data (renames: { old: new }, removed: [ names ]),
    // so a renamed package is fixed by editing it, without a rebuild. The module's own
    // packageRenames setting wins over the file.
    bool ok = false;
    const QVariantMap file = ClaudeInstall::YAML::load( m_profilesDir + QStringLiteral( "/package-renames.conf" ), &ok );
    QVariantMap renames = ok ? file.value( "renames" ).toMap() : QVariantMap();
    const QVariantMap own = m_configuration.value( "packageRenames" ).toMap();
    for ( auto it = own.constBegin(); it != own.constEnd(); ++it )
    {
        renames.insert( it.key(), it.value() );
    }
    for ( auto it = renames.constBegin(); it != renames.constEnd(); ++it )
    {
        m_renames.insert( it.key(), it.value().toString() );
    }
    if ( ok )
    {
        const QStringList removed = file.value( "removed" ).toStringList();
        m_removed = QSet< QString >( removed.begin(), removed.end() );
    }
}

bool
DistroProfileJob::loadRepoIndex( RepoIndex& index ) const
{
    // Every synced database is a tar of <name>-<version>/desc files: read them all in one go
    QProcess proc;
    proc.start( QStringLiteral( "sh" ),
                { QStringLiteral( "-c" ),
                  QStringLiteral( "for f in /var/lib/pacman/sync/*.db; do "
                                  "bsdtar -xOf \"$f\" --include '*/desc' 2>/dev/null || tar -xOf \"$f\" --wildcards '*/desc' 2>/dev/null; "
                                  "echo; done" ) } );
    if ( !proc.waitForFinished( 120000 ) )
    {
        proc.kill();
        return false;
    }
    const QStringList lines = QString::fromUtf8( proc.readAllStandardOutput() ).split( QLatin1Char( '\n' ) );

    QString key, current;
    for ( const QString& raw : lines )
    {
        const QString line = raw.trimmed();
        if ( line.isEmpty() )
        {
            key.clear();
        }
        else if ( line.startsWith( QLatin1Char( '%' ) ) && line.endsWith( QLatin1Char( '%' ) ) && line.size() > 2 )
        {
            key = line.mid( 1, line.size() - 2 );
        }
        else if ( key == QLatin1String( "NAME" ) )
        {
            current = line;
            index.names.insert( line );
        }
        else if ( key == QLatin1String( "GROUPS" ) )
        {
            index.groups.insert( line );
        }
        else if ( key == QLatin1String( "PROVIDES" ) )
        {
            index.providedBy[ withoutVersion( line ) ] << current;
        }
        else if ( key == QLatin1String( "REPLACES" ) )
        {
            index.replacedBy[ withoutVersion( line ) ] << current;
        }
    }
    return !index.names.isEmpty();
}

QString
DistroProfileJob::findReplacement( const RepoIndex& index, const QString& name, const QStringList& wanted, QString& how ) const
{
    // Several candidates: one the profile asks for anyway, else the one that looks most like the old name
    auto pick = [ & ]( QStringList candidates ) -> QString
    {
        candidates.removeDuplicates();
        std::sort( candidates.begin(), candidates.end() );
        for ( const QString& c : candidates )
        {
            if ( wanted.contains( c ) )
            {
                return c;
            }
        }
        QString best;
        int bestLength = -1;
        for ( const QString& c : candidates )
        {
            int n = 0;
            while ( n < c.size() && n < name.size() && c.at( n ) == name.at( n ) )
            {
                ++n;
            }
            if ( n > bestLength )
            {
                best = c;
                bestLength = n;
            }
        }
        return best;
    };

    // 1. The rename table (may chain: a -> b -> c)
    QString target = name;
    for ( int hops = 0; hops < 4 && m_renames.contains( target ); ++hops )
    {
        target = m_renames.value( target );
        if ( index.names.contains( target ) || index.groups.contains( target ) )
        {
            how = tr( "rename table" );
            return target;
        }
    }
    // 2. A package that declares "replaces" for the old name (how pacman itself follows renames)
    if ( index.replacedBy.contains( name ) )
    {
        how = tr( "replaced by" );
        return pick( index.replacedBy.value( name ) );
    }
    // 3. A package that provides the name
    if ( index.providedBy.contains( name ) )
    {
        how = tr( "provided by" );
        return pick( index.providedBy.value( name ) );
    }
    // 4. The same package without a packaging suffix (-git, -bin ...)
    static const QRegularExpression suffix( QStringLiteral( "-(git|bin|svn|hg|nightly|stable|beta|dev)$" ) );
    QString stripped = name;
    stripped.remove( suffix );
    if ( stripped != name && index.names.contains( stripped ) )
    {
        how = tr( "without the suffix" );
        return stripped;
    }
    // 5. One-character typo or spelling change, only when it is unambiguous
    if ( name.size() >= 6 )
    {
        QString found;
        int count = 0;
        for ( const QString& candidate : index.names )
        {
            if ( std::abs( candidate.size() - name.size() ) <= 1 && editDistance( candidate, name ) == 1 )
            {
                found = candidate;
                ++count;
            }
        }
        if ( count == 1 )
        {
            how = tr( "similar name" );
            return found;
        }
    }
    return QString();
}

JobResult
DistroProfileJob::resolvePackages( QStringList& packages )
{
    setStatus( tr( "Checking the package names against the repositories", "@status" ) );
    RepoIndex index;
    if ( !loadRepoIndex( index ) )
    {
        // Cannot check ahead (no bsdtar / empty databases): pacstrap's own errors are handled in bootstrap()
        cWarning() << "Could not read the package databases; names are not checked ahead.";
        return JobResult::ok();
    }

    QStringList result, notes, unresolved;
    for ( const QString& p : std::as_const( packages ) )
    {
        const QString base = p.section( QLatin1Char( '/' ), -1 );  // "extra/foo" -> "foo"
        if ( m_removed.contains( base ) )
        {
            cWarning() << "Package" << p << "is listed as removed for good; dropped.";
            notes << tr( "%1 dropped (listed as removed)" ).arg( p );
            continue;
        }
        if ( index.names.contains( base ) || index.groups.contains( base ) )
        {
            result << p;
            continue;
        }
        QString how;
        const QString to = findReplacement( index, base, packages, how );
        if ( !to.isEmpty() )
        {
            cWarning() << "Package" << p << "is not in the repositories any more; using" << to << "(" << how << ")";
            notes << tr( "%1 -> %2 (%3)" ).arg( p, to, how );
            result << to;
        }
        else
        {
            unresolved << p;
        }
    }
    result.removeDuplicates();
    ClaudeInstall::JobQueue::instance()->globalStorage()->insert( "distroprofileSubstitutions", notes );

    if ( !unresolved.isEmpty() )
    {
        QStringList report;
        for ( const QString& p : std::as_const( unresolved ) )
        {
            const QString base = p.section( QLatin1Char( '/' ), -1 );
            QStringList close;
            for ( const QString& n : std::as_const( index.names ) )
            {
                if ( n.size() >= 5 && ( n.contains( base ) || base.contains( n ) ) )
                {
                    close << n;
                }
            }
            std::sort( close.begin(), close.end() );
            report << ( close.isEmpty() ? p : p + QStringLiteral( "  (maybe: " ) + close.mid( 0, 5 ).join( QStringLiteral( ", " ) ) + ')' );
        }
        cWarning() << "No replacement found for:" << report;
        if ( m_unresolvedIsFatal )
        {
            return JobResult::error( tr( "Packages not found" ),
                                     tr( "These packages are not in the repositories and nothing replaces them:\n%1\n"
                                         "Add the new name to package-renames.conf (renames:) or fix the profile." )
                                         .arg( report.join( QLatin1Char( '\n' ) ) ) );
        }
        // unresolvedPackages: warn in the configuration keeps the old behaviour on purpose
        for ( const QString& p : std::as_const( unresolved ) )
        {
            notes << tr( "%1 dropped (not found)" ).arg( p );
        }
    }
    packages = result;
    return JobResult::ok();
}

JobResult
DistroProfileJob::bootstrap( const QString& root, QStringList packages )
{
    {
        auto online = checkNetwork();
        if ( !online )
        {
            return online;
        }
    }
    rankMirrors();
    refreshHostPackageLists();
    loadRenameTable();
    {
        auto resolved = resolvePackages( packages );
        if ( !resolved )
        {
            return resolved;
        }
    }

    for ( int attempt = 0;; ++attempt )
    {
        if ( packages.isEmpty() )
        {
            return JobResult::error( tr( "Nothing to install" ), tr( "The profile has no packages left to install." ) );
        }
        const QStringList cmd = QStringList { "pacstrap", "-K", root } + packages;
        setStatus( tr( "Installing %1 packages with pacstrap (attempt %2)", "@status" ).arg( packages.size() ).arg( attempt + 1 ) );
        QString out;
        const int code = runStreaming( cmd, out, m_stallSeconds );
        if ( code == 0 )
        {
            break;
        }

        // A name pacstrap cannot find although the check passed: the lists changed meanwhile.
        // Refresh and resolve again; never leave it out silently.
        const QStringList missing = missingPackages( out );
        if ( !missing.isEmpty() )
        {
            cWarning() << "pacstrap cannot find" << missing;
            if ( attempt >= m_retries )
            {
                return JobResult::error( tr( "Packages not found" ), tr( "pacstrap cannot find: %1" ).arg( missing.join( QStringLiteral( ", " ) ) ) );
            }
            refreshHostPackageLists();
            auto again = resolvePackages( packages );
            if ( !again )
            {
                return again;
            }
            continue;
        }
        if ( attempt >= m_retries )
        {
            cWarning() << "Command" << cmd.join( ' ' ) << "failed with exit code" << code;
            cDebug() << out;
            return JobResult::error( tr( "Setup step failed" ),
                                     tr( "The command <pre>%1</pre> returned error code %2." ).arg( cmd.join( ' ' ) ).arg( code ) );
        }
        // Servers that failed (or the ones in use when the download stalled) are not picked again
        QStringList bad = failedMirrors( out );
        if ( code == -2 )
        {
            bad += m_topHosts;
        }
        for ( const QString& h : std::as_const( bad ) )
        {
            m_badHosts.insert( h );
        }
        const QString why = code == -2 ? tr( "no download progress for %1 min" ).arg( qMax( 1, m_stallSeconds / 60 ) )
                                       : tr( "pacstrap failed (error %1)" ).arg( code );
        setStatus( tr( "%1; testing the mirrors again and retrying with the fastest working one", "@status" ).arg( why ) );
        cWarning() << why << "- servers not used again:" << bad;
        {
            auto online = checkNetwork();
            if ( !online )
            {
                return online;
            }
        }
        rankMirrors();
        refreshHostPackageLists();
        QThread::sleep( 1 );
    }

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

    setStatus( tr( "Downloading the files bundle", "@status" ) );
    const QStringList get { "curl", "-fL", "--retry", "5", "--retry-delay", "3", "--connect-timeout", "30", "-o", file, m_assetsUrl };
    QString getOutput;
    if ( runStreaming( get, getOutput ) != 0 )
    {
        return JobResult::error( tr( "Download failed" ), tr( "Could not fetch %1:\n%2" ).arg( m_assetsUrl, getOutput.right( 400 ) ) );
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
        // curl's progress meter is shown as percentage, size done / total, speed and size left
        setStatus( tr( "Downloading %1", "@status" ).arg( QFileInfo( QUrl( url ).path() ).fileName() ) );
        const QStringList get { "curl", "-fL", "--retry", "5", "--retry-delay", "3", "--connect-timeout", "30", "-o", file, url };
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

    if ( m_phase == QLatin1String( "finalize" ) )
    {
        const QString encoded = gs->value( "password" ).toString();
        if ( ctx.user.isEmpty() || encoded.isEmpty() )
        {
            return JobResult::error( tr( "No user" ), tr( "The final account reset needs the user and password from the users module." ) );
        }
        return resetUserAccount( ctx.root, ctx.user, ClaudeInstall::String::obscure( encoded ) );
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
    m_retries = ClaudeInstall::getInteger( configurationMap, "retries", 5 );
    m_rankMirrors = ClaudeInstall::getBool( configurationMap, "rankMirrors", true );
    m_mirrorKeep = qMax( 1, int( ClaudeInstall::getInteger( configurationMap, "mirrorKeep", 10 ) ) );
    m_speedTests = qMax( 1, int( ClaudeInstall::getInteger( configurationMap, "mirrorSpeedTests", 6 ) ) );
    m_archMirrorStatus = ClaudeInstall::getBool( configurationMap, "archMirrorStatus", true );
    m_stallSeconds = int( ClaudeInstall::getInteger( configurationMap, "stallSeconds", 300 ) );
    m_stallMinKiBps = qMax( 0, int( ClaudeInstall::getInteger( configurationMap, "stallMinKiBps", 20 ) ) );
    m_unresolvedIsFatal = ClaudeInstall::getString( configurationMap, "unresolvedPackages", QStringLiteral( "fail" ) ) != QLatin1String( "warn" );
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( DistroProfileJobFactory, registerPlugin< DistroProfileJob >(); )
