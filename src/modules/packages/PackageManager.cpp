/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 *   SPDX-FileCopyrightText: 2015 Rohan Garg <rohan@garg.io>
 *   SPDX-FileCopyrightText: 2016-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017 2019, Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2017 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2017 Ben Green <Bezzy1999@hotmail.com>
 *   SPDX-FileCopyrightText: 2021 Anubhav Choudhary <ac.10edu@gmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "PackageManager.h"

#include "compat/Variant.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include <QRegularExpression>
#include <QThread>

#include <functional>

namespace Packages
{

namespace
{
bool
isString( const QVariant& v )
{
    return ClaudeInstall::typeOf( v ) == ClaudeInstall::StringVariantType;
}

/// Runs @p command in the target; throws CommandFailed if it doesn't succeed
void
checkCall( const QStringList& command )
{
    const auto r = ClaudeInstall::System::instance()->targetEnvCommand( command );
    if ( r.getExitCode() != 0 )
    {
        throw CommandFailed { command, r.getExitCode(), r.getOutput() };
    }
}

/// Runs @p command in the target, ignoring the result
void
uncheckedCall( const QStringList& command )
{
    ClaudeInstall::System::instance()->targetEnvCommand( command );
}

/** @brief Runs @p command; on failure refreshes the package lists and tries again
 *
 * Repositories change under a running install: the lists on the target go
 * stale and a package (or its old version) is "not found" or answers 404.
 * @p refresh runs between attempts (its failure is ignored). The last
 * failure is thrown when all @p retries are used up.
 */
void
checkCallRefreshing( const QStringList& command, const std::function< void() >& refresh, int retries = 2 )
{
    for ( int attempt = 0;; ++attempt )
    {
        try
        {
            checkCall( command );
            return;
        }
        catch ( const CommandFailed& e )
        {
            if ( attempt >= retries )
            {
                throw;
            }
            cWarning() << "Command" << command.join( ' ' ) << "failed with exit code" << e.exitCode
                       << "- refreshing the package lists and retrying";
            refresh();
            QThread::sleep( 2 );
        }
    }
}

/// The package names that a package manager's @p output says it cannot find
QStringList
missingPackages( const QString& output )
{
    // pacman, apt (two forms), dnf/dnf5/yum, zypper, apk, xbps
    static const QRegularExpression re(
        QStringLiteral( "target not found: (\\S+)"
                        "|Unable to locate package (\\S+)"
                        "|Package '?([^\\s']+)'? has no installation candidate"
                        "|No match for argument: (\\S+)"
                        "|Package '([^']+)' not found"
                        "|(\\S+) \\(no such package\\)" ) );
    QStringList names;
    auto it = re.globalMatch( output );
    while ( it.hasNext() )
    {
        const auto m = it.next();
        for ( int i = 1; i <= m.lastCapturedIndex(); ++i )
        {
            if ( !m.captured( i ).isEmpty() )
            {
                names << m.captured( i );
            }
        }
    }
    names.removeDuplicates();
    return names;
}

/** @brief Installs @p pkgs, surviving package lists that change over time
 *
 * Package lists are snapshots: later, packages are renamed, split or dropped.
 * First an install failure refreshes the package lists and retries. If
 * packages are still reported as not existing, they are left out (and
 * logged) and the rest is installed, instead of one stale name failing the
 * whole list. Other failures are thrown.
 *
 * @p command builds the install command for a list of packages.
 */
void
installTolerant( QStringList pkgs,
                 const std::function< QStringList( const QStringList& ) >& command,
                 const std::function< void() >& refresh )
{
    for ( int round = 0; !pkgs.isEmpty(); ++round )
    {
        try
        {
            checkCallRefreshing( command( pkgs ), refresh );
            return;
        }
        catch ( const CommandFailed& e )
        {
            const QStringList missing = missingPackages( e.output );
            QStringList kept;
            for ( const QString& p : pkgs )
            {
                if ( !missing.contains( p ) )
                {
                    kept << p;
                }
            }
            if ( kept.size() == pkgs.size() || round >= 5 )
            {
                throw;  // not (only) a missing package
            }
            cWarning() << "Skipping packages that are no longer in the repositories:" << missing;
            pkgs = kept;
        }
    }
}

QString
packageName( const QVariant& packageData )
{
    return isString( packageData ) ? packageData.toString() : packageData.toMap().value( "package" ).toString();
}

class PMApk : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "apk" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        installTolerant(
            pkgs,
            []( const QStringList& p ) { return QStringList { "apk", "add" } + p; },
            [] { uncheckedCall( { "apk", "update" } ); } );
    }
    void remove( const QStringList& pkgs ) override
    {
        for ( const QString& pkg : pkgs )
        {
            checkCall( { "apk", "del", pkg } );
        }
    }
    void updateDb() override { checkCall( { "apk", "update" } ); }
    void updateSystem() override { checkCall( { "apk", "upgrade", "--available" } ); }
};

class PMApt : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "apt" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        installTolerant(
            pkgs,
            []( const QStringList& p ) { return QStringList { "apt-get", "-q", "-y", "--fix-missing", "install" } + p; },
            []
            {
                uncheckedCall( { "dpkg", "--configure", "-a" } );
                uncheckedCall( { "apt-get", "update" } );
            } );
    }
    void remove( const QStringList& pkgs ) override
    {
        checkCall( QStringList { "apt-get", "--purge", "-q", "-y", "remove" } + pkgs );
        checkCall( { "apt-get", "--purge", "-q", "-y", "autoremove" } );
    }
    void updateDb() override { checkCall( { "apt-get", "update" } ); }
    void updateSystem() override { /* Doesn't need to update the system explicitly */ }
};

/** @brief "Legacy" DNF, called DNF-4 even though the executable is dnf-3 in modern Fedora */
class PMDnf : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "dnf" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        installTolerant(
            pkgs,
            []( const QStringList& p ) { return QStringList { "dnf-3", "-y", "install" } + p; },
            [] { uncheckedCall( { "dnf-3", "-y", "makecache", "--refresh" } ); } );
    }
    void remove( const QStringList& pkgs ) override
    {
        // ignore the error code for now because dnf thinks removing a nonexistent package is an error
        uncheckedCall( QStringList { "dnf-3", "--disablerepo=*", "-C", "-y", "remove" } + pkgs );
    }
    void updateDb() override { /* Doesn't need updates */ }
    void updateSystem() override { checkCall( { "dnf-3", "-y", "upgrade" } ); }
};

/** @brief "Modern" DNF, DNF-5, for Fedora 41 and later */
class PMDnf5 : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "dnf5" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        installTolerant(
            pkgs,
            []( const QStringList& p ) { return QStringList { "dnf5", "-y", "install" } + p; },
            [] { uncheckedCall( { "dnf5", "-y", "makecache", "--refresh" } ); } );
    }
    void remove( const QStringList& pkgs ) override
    {
        // ignore the error code for now because dnf thinks removing a nonexistent package is an error
        uncheckedCall( QStringList { "dnf5", "--disablerepo=*", "-C", "-y", "remove" } + pkgs );
    }
    void updateDb() override { /* Doesn't need updates */ }
    void updateSystem() override { checkCall( { "dnf5", "-y", "upgrade" } ); }
};

class PMDummy : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "dummy" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        cDebug() << "Dummy backend: Installing" << pkgs;
        QThread::sleep( 3 );
    }
    void remove( const QStringList& pkgs ) override
    {
        cDebug() << "Dummy backend: Removing" << pkgs;
        QThread::sleep( 3 );
    }
    void updateDb() override { cDebug() << "Dummy backend: Updating DB"; }
    void updateSystem() override { cDebug() << "Dummy backend: Updating System"; }
    void run( const QString& script ) override { cDebug() << "Dummy backend: Running script" << script; }
};

class PMEntropy : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "entropy" ); }
    void install( const QStringList& pkgs, bool ) override { checkCall( QStringList { "equo", "i" } + pkgs ); }
    void remove( const QStringList& pkgs ) override { checkCall( QStringList { "equo", "rm" } + pkgs ); }
    void updateDb() override { checkCall( { "equo", "update" } ); }
    void updateSystem() override { /* Doesn't need to update the system explicitly */ }
};

class PMFlatpak : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "flatpak" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        checkCall( QStringList { "flatpak", "install", "--assumeyes" } + pkgs );
    }
    void remove( const QStringList& pkgs ) override
    {
        checkCall( QStringList { "flatpak", "uninstall", "--noninteractive" } + pkgs );
    }
    void updateDb() override {}
    void updateSystem() override { /* Doesn't need to update the system explicitly */ }
};

class PMLuet : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "luet" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        checkCall( QStringList { "luet", "install", "-y" } + pkgs );
    }
    void remove( const QStringList& pkgs ) override
    {
        checkCall( QStringList { "luet", "uninstall", "-y" } + pkgs );
    }
    void updateDb() override { /* Luet checks for DB update every time it is run */ }
    void updateSystem() override { checkCall( { "luet", "upgrade", "-y" } ); }
};

class PMPackageKit : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "packagekit" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        for ( const QString& pkg : pkgs )
        {
            checkCall( { "pkcon", "-py", "install", pkg } );
        }
    }
    void remove( const QStringList& pkgs ) override
    {
        for ( const QString& pkg : pkgs )
        {
            checkCall( { "pkcon", "-py", "remove", pkg } );
        }
    }
    void updateDb() override { checkCall( { "pkcon", "refresh" } ); }
    void updateSystem() override { checkCall( { "pkcon", "-py", "update" } ); }
};

class PMPacman : public PackageManager
{
public:
    explicit PMPacman( const PacmanConfig& config )
        : m_config( config )
    {
    }

    QString backend() const override { return QStringLiteral( "pacman" ); }

    void install( const QStringList& pkgs, bool fromLocal ) override
    {
        QStringList command { "pacman", fromLocal ? "-U" : "-S" };

        // Don't ask for user intervention, take the default action
        command << "--noconfirm";
        // Don't report download progress for each file
        command << "--noprogressbar";
        if ( m_config.neededOnly )
        {
            command << "--needed";
        }
        if ( m_config.disableDownloadTimeout )
        {
            command << "--disable-download-timeout";
        }
        // Local files (-U) do not depend on the package lists
        if ( fromLocal )
        {
            command += pkgs;
            runPacman( command );
            return;
        }

        installTolerant(
            pkgs,
            [ command ]( const QStringList& p ) { return command + p; },
            []
            {
                uncheckedCall( { "rm", "-f", "/var/lib/pacman/db.lck" } );
                uncheckedCall( { "pacman", "-Syy", "--noconfirm" } );
            } );
    }
    void remove( const QStringList& pkgs ) override
    {
        runPacman( QStringList { "pacman", "-Rs", "--noconfirm" } + pkgs );
    }
    void updateDb() override { runPacman( { "pacman", "-Sy" } ); }
    void updateSystem() override
    {
        QStringList command { "pacman", "-Su", "--noconfirm" };
        if ( m_config.disableDownloadTimeout )
        {
            command << "--disable-download-timeout";
        }
        runPacman( command );
    }

private:
    /// Calls pacman in a loop until it is successful or the number of retries is exceeded
    void runPacman( const QStringList& command )
    {
        int count = 0;
        for ( ;; )
        {
            count++;
            try
            {
                checkCall( command );
                return;
            }
            catch ( const CommandFailed& )
            {
                if ( count > m_config.numRetries )
                {
                    throw;
                }
            }
        }
    }

    PacmanConfig m_config;
};

class PMPamac : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "pamac" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        delDbLock();
        checkCall( QStringList { "pamac", "install", "--no-confirm" } + pkgs );
    }
    void remove( const QStringList& pkgs ) override
    {
        delDbLock();
        checkCall( QStringList { "pamac", "remove", "--no-confirm" } + pkgs );
    }
    void updateDb() override
    {
        delDbLock();
        checkCall( { "pamac", "update", "--no-confirm" } );
    }
    void updateSystem() override
    {
        delDbLock();
        checkCall( { "pamac", "upgrade", "--no-confirm" } );
    }

private:
    /// In case of some error or crash, the database will be locked, resulting in remaining packages not being installed.
    void delDbLock() { checkCall( { "rm", "-f", "/var/lib/pacman/db.lck" } ); }
};

// NOTE: the commands of xbps and zypp below are carried over unchanged
//       from the Python implementation, including some doubtful ones.
class PMPisi : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "pisi" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        // The Python version had a missing comma here, making "install-y" one argument.
        checkCall( QStringList { "pisi", "install", "-y" } + pkgs );
    }
    void remove( const QStringList& pkgs ) override { checkCall( QStringList { "pisi", "remove", "-y" } + pkgs ); }
    void updateDb() override { checkCall( { "pisi", "update-repo" } ); }
    void updateSystem() override { /* Doesn't need to update the system explicitly */ }
};

class PMPortage : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "portage" ); }
    void install( const QStringList& pkgs, bool ) override { checkCall( QStringList { "emerge", "-v" } + pkgs ); }
    void remove( const QStringList& pkgs ) override
    {
        checkCall( QStringList { "emerge", "-C" } + pkgs );
        checkCall( { "emerge", "--depclean", "-q" } );
    }
    void updateDb() override { checkCall( { "emerge", "--sync" } ); }
    void updateSystem() override { /* Doesn't need to update the system explicitly */ }
};

class PMXbps : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "xbps" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        installTolerant(
            pkgs,
            []( const QStringList& p ) { return QStringList { "xbps-install", "-Sy" } + p; },
            [] { uncheckedCall( { "xbps-install", "-S" } ); } );
    }
    void remove( const QStringList& pkgs ) override { runXbps( QStringList { "xbps-remove", "-Ry" } + pkgs ); }
    void updateDb() override { runXbps( { "xbps-install", "-S" } ); }
    void updateSystem() override { runXbps( { "xbps-install", "-Suy" } ); }

private:
    void runXbps( const QStringList& command )
    {
        const auto r = ClaudeInstall::System::instance()->targetEnvCommand( command );
        cDebug() << r.getOutput();
        if ( r.getExitCode() != 0 )
        {
            throw CommandFailed { command, r.getExitCode(), r.getOutput() };
        }
    }
};

class PMYum : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "yum" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        installTolerant(
            pkgs,
            []( const QStringList& p ) { return QStringList { "yum", "-y", "install" } + p; },
            [] { uncheckedCall( { "yum", "-y", "clean", "expire-cache" } ); } );
    }
    void remove( const QStringList& pkgs ) override
    {
        checkCall( QStringList { "yum", "--disablerepo=*", "-C", "-y", "remove" } + pkgs );
    }
    void updateDb() override { /* Doesn't need updates */ }
    void updateSystem() override { checkCall( { "yum", "-y", "upgrade" } ); }
};

class PMZypp : public PackageManager
{
public:
    QString backend() const override { return QStringLiteral( "zypp" ); }
    void install( const QStringList& pkgs, bool ) override
    {
        installTolerant(
            pkgs,
            []( const QStringList& p )
            {
                return QStringList { "zypper", "--non-interactive", "--quiet-install", "install", "--auto-agree-with-licenses" }
                    + p;
            },
            [] { uncheckedCall( { "zypper", "--non-interactive", "refresh", "--force" } ); } );
    }
    void remove( const QStringList& pkgs ) override
    {
        checkCall( QStringList { "zypper", "--non-interactive", "remove" } + pkgs );
    }
    void updateDb() override { checkCall( { "zypper", "--non-interactive", "update" } ); }
    void updateSystem() override { /* Doesn't need to update the system explicitly */ }
};

using Creator = std::function< std::unique_ptr< PackageManager >( const QVariantMap& ) >;
template < typename T >
Creator
simple()
{
    return []( const QVariantMap& ) { return std::unique_ptr< PackageManager >( new T ); };
}

/// Keep these alphabetical (by backend name), even the Dummy implementation.
const QList< QPair< QString, Creator > >&
creators()
{
    static const QList< QPair< QString, Creator > > list {
        { "apk", simple< PMApk >() },
        { "apt", simple< PMApt >() },
        { "dnf", simple< PMDnf >() },
        { "dnf5", simple< PMDnf5 >() },
        { "dummy", simple< PMDummy >() },
        { "entropy", simple< PMEntropy >() },
        { "flatpak", simple< PMFlatpak >() },
        { "luet", simple< PMLuet >() },
        { "packagekit", simple< PMPackageKit >() },
        { "pacman",
          []( const QVariantMap& configuration )
          {
              return std::unique_ptr< PackageManager >( new PMPacman( PacmanConfig::fromJobConfiguration( configuration ) ) );
          } },
        { "pamac", simple< PMPamac >() },
        { "pisi", simple< PMPisi >() },
        { "portage", simple< PMPortage >() },
        { "xbps", simple< PMXbps >() },
        { "yum", simple< PMYum >() },
        { "zypp", simple< PMZypp >() },
    };
    return list;
}
}  // namespace

PacmanConfig
PacmanConfig::fromJobConfiguration( const QVariantMap& jobConfiguration )
{
    PacmanConfig config;
    const QVariant pacman = jobConfiguration.value( "pacman" );
    if ( !pacman.isValid() || pacman.isNull() )
    {
        return config;
    }
    if ( ClaudeInstall::typeOf( pacman ) != ClaudeInstall::MapVariantType )
    {
        cWarning() << "Job configuration *pacman* will be ignored.";
        return config;
    }

    const QVariantMap map = pacman.toMap();
    config.numRetries = ClaudeInstall::getInteger( map, "num_retries", 0 );
    config.disableDownloadTimeout = ClaudeInstall::getBool( map, "disable_download_timeout", false );
    config.neededOnly = ClaudeInstall::getBool( map, "needed_only", false );
    return config;
}

PackageManager::~PackageManager() {}

QStringList
PackageManager::backends()
{
    QStringList names;
    for ( const auto& c : creators() )
    {
        names << c.first;
    }
    return names;
}

std::unique_ptr< PackageManager >
PackageManager::create( const QString& backend, const QVariantMap& jobConfiguration )
{
    for ( const auto& c : creators() )
    {
        if ( c.first == backend )
        {
            return c.second( jobConfiguration );
        }
    }
    return nullptr;
}

void
PackageManager::run( const QString& script )
{
    if ( !script.isEmpty() )
    {
        checkCall( script.split( ' ' ) );
    }
}

void
PackageManager::installPackage( const QVariant& packageData, bool fromLocal )
{
    if ( isString( packageData ) )
    {
        install( { packageData.toString() }, fromLocal );
    }
    else
    {
        const QVariantMap map = packageData.toMap();
        run( map.value( "pre-script" ).toString() );
        install( { map.value( "package" ).toString() }, fromLocal );
        run( map.value( "post-script" ).toString() );
    }
}

void
PackageManager::removePackage( const QVariant& packageData )
{
    if ( isString( packageData ) )
    {
        remove( { packageData.toString() } );
    }
    else
    {
        const QVariantMap map = packageData.toMap();
        run( map.value( "pre-script" ).toString() );
        remove( { map.value( "package" ).toString() } );
        run( map.value( "post-script" ).toString() );
    }
}

/// Are all the entries plain package names?
static bool
allStrings( const QVariantList& packageList )
{
    for ( const auto& p : packageList )
    {
        if ( !isString( p ) )
        {
            return false;
        }
    }
    return true;
}

void
PackageManager::operationInstall( const QVariantList& packageList, bool fromLocal )
{
    if ( allStrings( packageList ) )
    {
        install( QVariant( packageList ).toStringList(), fromLocal );
    }
    else
    {
        for ( const auto& package : packageList )
        {
            installPackage( package, fromLocal );
        }
    }
}

void
PackageManager::operationTryInstall( const QVariantList& packageList )
{
    // we make a separate package manager call for each package so a
    // single failing package won't stop all of them
    for ( const auto& package : packageList )
    {
        try
        {
            installPackage( package );
        }
        catch ( const CommandFailed& )
        {
            cWarning() << "Could not install package" << packageName( package );
        }
    }
}

void
PackageManager::operationRemove( const QVariantList& packageList )
{
    if ( allStrings( packageList ) )
    {
        remove( QVariant( packageList ).toStringList() );
    }
    else
    {
        for ( const auto& package : packageList )
        {
            removePackage( package );
        }
    }
}

void
PackageManager::operationTryRemove( const QVariantList& packageList )
{
    for ( const auto& package : packageList )
    {
        try
        {
            removePackage( package );
        }
        catch ( const CommandFailed& )
        {
            cWarning() << "Could not remove package" << packageName( package );
        }
    }
}

/** @brief Substitutes $LOCALE and ${LOCALE} like Python's string.Template.safe_substitute() */
static QString
substituteLocale( const QString& text, const QString& locale )
{
    static const QRegularExpression placeholder( QStringLiteral( "\\$(?:(\\$)|([_a-zA-Z][_a-zA-Z0-9]*)|\\{([_a-zA-Z][_a-zA-Z0-9]*)\\})" ) );

    QString out;
    int last = 0;
    auto it = placeholder.globalMatch( text );
    while ( it.hasNext() )
    {
        const auto m = it.next();
        out += text.mid( last, m.capturedStart() - last );
        last = m.capturedEnd();

        const QString name = m.captured( 2 ).isEmpty() ? m.captured( 3 ) : m.captured( 2 );
        if ( !m.captured( 1 ).isEmpty() )
        {
            out += '$';  // "$$" is an escaped dollar sign
        }
        else if ( name == "LOCALE" )
        {
            out += locale;
        }
        else
        {
            out += m.captured( 0 );  // unknown placeholders are left alone
        }
    }
    out += text.mid( last );
    return out;
}

QVariantList
substLocale( const QVariantList& packages, const QString& localeIn )
{
    // It is possible to skip the locale-setting entirely. Then pretend it is
    // "en", so that LOCALE-decorated package names are removed from the list.
    const QString locale = localeIn.isEmpty() ? QStringLiteral( "en" ) : localeIn;

    QVariantList ret;
    for ( const QVariant& packageData : packages )
    {
        QString name = packageName( packageData );

        // Update the name: substitute LOCALE, and drop packages
        // if locale is en and LOCALE is in the package name.
        if ( locale != "en" )
        {
            name = substituteLocale( name, locale );
        }
        else if ( name.contains( "LOCALE" ) )
        {
            continue;
        }

        // Put it back in the entry
        if ( isString( packageData ) )
        {
            ret.append( name );
        }
        else
        {
            QVariantMap map = packageData.toMap();
            map[ "package" ] = name;
            ret.append( map );
        }
    }
    return ret;
}

}  // namespace Packages
