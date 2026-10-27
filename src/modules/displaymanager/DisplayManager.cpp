/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Benjamin Vaudour <benjamin.vaudour@yahoo.fr>
 *   SPDX-FileCopyrightText: 2014-2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2014, 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2018 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2019 Dominic Hayes <ferenosdev@outlook.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "DisplayManager.h"

#include "IniFile.h"
#include "TomlFile.h"

#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <utility>

namespace DisplayManagers
{

namespace
{
QString
translate( const char* text )
{
    return QCoreApplication::translate( "DisplayManagers", text );
}

int
targetCall( const QStringList& command )
{
    return ClaudeInstall::System::instance()->targetEnvCall( command );
}

/// Path of @p relative inside the target (os.path.join semantics)
QString
inTarget( const QString& rootMountPoint, const QString& relative )
{
    return QDir( rootMountPoint ).filePath( relative );
}

/** @brief Reads the lines of a text file, **including** their line endings */
bool
readLines( const QString& path, QStringList& lines )
{
    QFile f( path );
    if ( !f.open( QIODevice::ReadOnly ) )
    {
        return false;
    }
    const QString text = QString::fromUtf8( f.readAll() );
    lines.clear();
    int start = 0;
    while ( start < text.size() )
    {
        const int nl = text.indexOf( '\n', start );
        if ( nl < 0 )
        {
            lines << text.mid( start );
            break;
        }
        lines << text.mid( start, nl - start + 1 );
        start = nl + 1;
    }
    return true;
}

bool
writeText( const QString& path, const QString& text )
{
    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
    {
        return false;
    }
    const QByteArray data = text.toUtf8();
    return f.write( data ) == data.size();
}

/** @brief Replaces each line of the file at @p path that matches @p re by @p replacement
 *
 * This is what `sed -i -e "s/^.*foo=.*/foo=bar/"` does. A missing file is ignored.
 */
void
replaceMatchingLines( const QString& path, const QRegularExpression& re, const QString& replacement )
{
    QFile f( path );
    if ( !f.open( QIODevice::ReadOnly ) )
    {
        return;
    }
    QStringList lines = QString::fromUtf8( f.readAll() ).split( '\n' );
    f.close();
    for ( QString& line : lines )
    {
        if ( re.match( line ).hasMatch() )
        {
            line = replacement;
        }
    }
    writeText( path, lines.join( '\n' ) );
}

/// The failure to write one of the DM's configuration files
Result
cannotWrite( const QString& title, const QString& path )
{
    return Message { title, translate( "Could not write %1" ).arg( path ) };
}

/** @brief Edits the [daemon] section of a MDM or GDM configuration file
 *
 * Existing automatic-login lines are removed, and the section header gets
 * the new settings. A missing file is created.
 */
Result
editDaemonConf( const QString& path, const QString& username, bool doAutologin, const QString& newFileComment )
{
    const QString daemon = doAutologin
        ? QStringLiteral( "[daemon]\nAutomaticLogin=%1\nAutomaticLoginEnable=True\n" ).arg( username )
        : QStringLiteral( "[daemon]\nAutomaticLoginEnable=False\n" );

    QString out;
    if ( QFileInfo::exists( path ) )
    {
        QStringList lines;
        readLines( path, lines );
        for ( QString line : lines )
        {
            if ( line.contains( "AutomaticLogin=" ) )
            {
                line.clear();
            }
            if ( line.contains( "AutomaticLoginEnable=True" ) )
            {
                line.clear();
            }
            if ( line.contains( "[daemon]" ) )
            {
                line = daemon;
            }
            out += line;
        }
    }
    else
    {
        out = newFileComment + daemon;
    }

    if ( !writeText( path, out ) )
    {
        return cannotWrite( translate( "Cannot write configuration file" ), path );
    }
    return std::nullopt;
}

/** @brief Adds a user and group for a DM if needed, with the same locking and ownership steps */
void
ensureGroup( const QString& group, const QStringList& groupaddArguments )
{
    if ( targetCall( { "getent", "group", group } ) != 0 )
    {
        targetCall( QStringList { "groupadd" } + groupaddArguments );
    }
}

void
ensureUser( const QString& user, const QStringList& useraddArguments )
{
    if ( targetCall( { "getent", "passwd", user } ) != 0 )
    {
        targetCall( QStringList { "useradd" } + useraddArguments );
    }
}

class DMmdm : public DisplayManager
{
public:
    explicit DMmdm( const QString& root )
        : DisplayManager( root )
    {
        m_executable = QStringLiteral( "mdm" );
    }
    QString name() const override { return QStringLiteral( "mdm" ); }

    Result setAutologin( const QString& username, bool doAutologin, const DesktopEnvironment* ) override
    {
        return editDaemonConf( inTarget( m_rootMountPoint, "etc/mdm/custom.conf" ),
                               username,
                               doAutologin,
                               QStringLiteral( "# ClaudeInstall - Configure automatic login for user\n" ) );
    }

    Result basicSetup() override
    {
        ensureGroup( "mdm", { "-g", "128", "mdm" } );
        ensureUser( "mdm",
                    { "-c",
                      "\"Linux Mint Display Manager\"",
                      "-u",
                      "128",
                      "-g",
                      "mdm",
                      "-d",
                      "/var/lib/mdm",
                      "-s",
                      "/usr/bin/nologin",
                      "mdm" } );

        targetCall( { "passwd", "-l", "mdm" } );
        targetCall( { "chown", "root:mdm", "/var/lib/mdm" } );
        targetCall( { "chmod", "1770", "/var/lib/mdm" } );
        return std::nullopt;
    }

    Result desktopEnvironmentSetup( const DesktopEnvironment& de ) override
    {
        // sed -i "s|default.desktop|<de>.desktop|g" etc/mdm/custom.conf
        const QString path = inTarget( m_rootMountPoint, "etc/mdm/custom.conf" );
        QStringList lines;
        if ( readLines( path, lines ) )
        {
            static const QRegularExpression re( QStringLiteral( "default.desktop" ) );
            QString out;
            for ( QString line : lines )
            {
                out += line.replace( re, de.desktopFile + QStringLiteral( ".desktop" ) );
            }
            writeText( path, out );
        }
        return std::nullopt;
    }

    Result greeterSetup() override { return std::nullopt; }
};

class DMgdm : public DisplayManager
{
public:
    explicit DMgdm( const QString& root )
        : DisplayManager( root )
    {
        m_executable = QStringLiteral( "gdm" );
    }
    QString name() const override { return QStringLiteral( "gdm" ); }

    /** @brief GDM exists with different executable names, so search for one of them */
    bool haveDm() override
    {
        // Pairs of executable and its configuration file
        static const QList< QPair< QString, QString > > candidates {
            { "gdm", "etc/gdm/custom.conf" },
            { "gdm3", "etc/gdm3/daemon.conf" },
            { "gdm3", "etc/gdm3/custom.conf" },
        };

        auto haveExecutable = [ this ]( const QString& executable )
        {
            return QFileInfo::exists( m_rootMountPoint + "/usr/bin/" + executable )
                || QFileInfo::exists( m_rootMountPoint + "/usr/sbin/" + executable );
        };
        auto haveConfig = [ this ]( const QString& config ) { return QFileInfo::exists( m_rootMountPoint + '/' + config ); };

        // Look for an existing configuration file as a hint, then keep
        // the found-executable name and config around for later.
        for ( const auto& c : candidates )
        {
            if ( haveConfig( c.second ) && haveExecutable( c.first ) )
            {
                m_executable = c.first;
                m_config = c.second;
                return true;
            }
        }
        for ( const auto& c : candidates )
        {
            if ( haveExecutable( c.first ) )
            {
                m_executable = c.first;
                m_config = c.second;
                return true;
            }
        }
        return false;
    }

    Result setAutologin( const QString& username, bool doAutologin, const DesktopEnvironment* de ) override
    {
        if ( m_config.isEmpty() )
        {
            return Message { translate( "Cannot write GDM configuration file" ),
                             translate( "No config file for GDM has been set." ) };
        }

        const QString path = inTarget( m_rootMountPoint, m_config );
        if ( const auto r = editDaemonConf(
                 path, username, doAutologin, QStringLiteral( "# ClaudeInstall - Enable automatic login for user\n" ) ) )
        {
            return r;
        }

        if ( doAutologin )
        {
            const QString accountServiceDir = m_rootMountPoint + QStringLiteral( "/var/lib/AccountsService/users" );
            if ( QFileInfo::exists( accountServiceDir ) )
            {
                QString userFile = QStringLiteral( "[User]\n" );
                if ( de )
                {
                    userFile += QStringLiteral( "XSession=%1\n" ).arg( de->desktopFile );
                }
                userFile += QStringLiteral( "Icon=\n" );
                if ( !writeText( accountServiceDir + '/' + username, userFile ) )
                {
                    return cannotWrite( translate( "Cannot write GDM configuration file" ),
                                        accountServiceDir + '/' + username );
                }
            }
        }
        return std::nullopt;
    }

    Result basicSetup() override
    {
        ensureGroup( "gdm", { "-g", "120", "gdm" } );
        ensureUser( "gdm",
                    { "-c",
                      "\"Gnome Display Manager\"",
                      "-u",
                      "120",
                      "-g",
                      "gdm",
                      "-d",
                      "/var/lib/gdm",
                      "-s",
                      "/usr/bin/nologin",
                      "gdm" } );

        targetCall( { "passwd", "-l", "gdm" } );
        targetCall( { "chown", "-R", "gdm:gdm", "/var/lib/gdm" } );
        return std::nullopt;
    }

    Result desktopEnvironmentSetup( const DesktopEnvironment& ) override { return std::nullopt; }
    Result greeterSetup() override { return std::nullopt; }

private:
    QString m_config;  // Set by haveDm()
};

class DMlxdm : public DisplayManager
{
public:
    explicit DMlxdm( const QString& root )
        : DisplayManager( root )
    {
        m_executable = QStringLiteral( "lxdm" );
    }
    QString name() const override { return QStringLiteral( "lxdm" ); }

    Result setAutologin( const QString& username, bool doAutologin, const DesktopEnvironment* ) override
    {
        const QString path = inTarget( m_rootMountPoint, "etc/lxdm/lxdm.conf" );
        QStringList lines;
        if ( !readLines( path, lines ) )
        {
            return Message { translate( "Cannot write LXDM configuration file" ),
                             translate( "LXDM config file %1 does not exist" ).arg( path ) };
        }

        QString out;
        for ( QString line : lines )
        {
            if ( line.contains( "autologin=" ) )
            {
                line = doAutologin ? QStringLiteral( "autologin=%1\n" ).arg( username ) : QStringLiteral( "# autologin=\n" );
            }
            out += line;
        }
        if ( !writeText( path, out ) )
        {
            return cannotWrite( translate( "Cannot write LXDM configuration file" ), path );
        }
        return std::nullopt;
    }

    Result basicSetup() override
    {
        ensureGroup( "lxdm", { "--system", "lxdm" } );

        targetCall( { "chgrp", "-R", "lxdm", "/var/lib/lxdm" } );
        targetCall( { "chgrp", "lxdm", "/etc/lxdm/lxdm.conf" } );
        targetCall( { "chmod", "+r", "/etc/lxdm/lxdm.conf" } );
        return std::nullopt;
    }

    Result desktopEnvironmentSetup( const DesktopEnvironment& de ) override
    {
        // sed -i -e "s|^.*session=.*|session=<executable>|" etc/lxdm/lxdm.conf
        static const QRegularExpression re( QStringLiteral( "^.*session=.*" ) );
        replaceMatchingLines(
            inTarget( m_rootMountPoint, "etc/lxdm/lxdm.conf" ), re, QStringLiteral( "session=%1" ).arg( de.executable ) );
        return std::nullopt;
    }

    Result greeterSetup() override { return std::nullopt; }
};

class DMlightdm : public DisplayManager
{
public:
    explicit DMlightdm( const QString& root )
        : DisplayManager( root )
    {
        m_executable = QStringLiteral( "lightdm" );
    }
    QString name() const override { return QStringLiteral( "lightdm" ); }

    void configure( const QVariantMap& settings ) override
    {
        DisplayManager::configure( settings );
        if ( settings.contains( "preferred_greeters" ) )
        {
            m_preferredGreeters = ClaudeInstall::getStringList( settings, "preferred_greeters" );
        }
    }

    Result setAutologin( const QString& username, bool doAutologin, const DesktopEnvironment* ) override
    {
        // Ideally, we should use an INI parser for the conf file,
        // but we just do a simple text replacement for now, as it
        // worksforme(tm)
        const QString path = inTarget( m_rootMountPoint, "etc/lightdm/lightdm.conf" );

        QStringList lines;
        QString out;
        if ( readLines( path, lines ) )
        {
            // Check to make sure [SeatDefaults] or [Seat:*] is in the config,
            // otherwise we'll risk malforming the config. (The Python version
            // compared whole lines including the newline and so always added it.)
            bool haveSeat = false;
            for ( const QString& line : std::as_const( lines ) )
            {
                const QString stripped = line.trimmed();
                haveSeat = haveSeat || stripped == "[SeatDefaults]" || stripped == "[Seat:*]";
            }
            if ( !haveSeat )
            {
                // Prepend Seat line to start of file rather than leaving it without one
                // This keeps the config from being malformed for LightDM
                out = QStringLiteral( "[Seat:*]\n" );
            }

            for ( QString line : lines )
            {
                if ( line.contains( "autologin-user=" ) )
                {
                    line = doAutologin ? QStringLiteral( "autologin-user=%1\n" ).arg( username )
                                       : QStringLiteral( "#autologin-user=\n" );
                }
                out += line;
            }
        }
        else
        {
            // Create a new lightdm.conf file; this is documented to be
            // read last, after everything in lightdm.conf.d/
            out = doAutologin ? QStringLiteral( "[Seat:*]\nautologin-user=%1\n" ).arg( username )
                              : QStringLiteral( "[Seat:*]\n#autologin-user=\n" );
        }

        if ( !writeText( path, out ) )
        {
            return Message { translate( "Cannot write LightDM configuration file" ),
                             translate( "LightDM config file %1 does not exist" ).arg( path ) };
        }
        return std::nullopt;
    }

    Result basicSetup() override
    {
        targetCall( { "mkdir", "-p", "/run/lightdm" } );

        ensureGroup( "lightdm", { "-g", "620", "lightdm" } );
        ensureUser( "lightdm",
                    { "-c",
                      "\"LightDM Display Manager\"",
                      "-u",
                      "620",
                      "-g",
                      "lightdm",
                      "-d",
                      "/var/run/lightdm",
                      "-s",
                      "/usr/bin/nologin",
                      "lightdm" } );

        targetCall( { "passwd", "-l", "lightdm" } );
        targetCall( { "chown", "-R", "lightdm:lightdm", "/run/lightdm" } );
        // The Python version had a missing comma: it ran chmod with one argument "+r/etc/lightdm/lightdm.conf"
        targetCall( { "chmod", "+r", "/etc/lightdm/lightdm.conf" } );
        return std::nullopt;
    }

    Result desktopEnvironmentSetup( const DesktopEnvironment& de ) override
    {
        // sed -i -e "s/^.*user-session=.*/user-session=<desktop file>/" etc/lightdm/lightdm.conf
        static const QRegularExpression re( QStringLiteral( "^.*user-session=.*" ) );
        replaceMatchingLines( inTarget( m_rootMountPoint, "etc/lightdm/lightdm.conf" ),
                              re,
                              QStringLiteral( "user-session=%1" ).arg( de.desktopFile ) );
        return std::nullopt;
    }

    /** @brief Finds the preferred greeter
     *
     * On Debian, lightdm-greeter.desktop is typically a symlink managed by
     * update-alternatives pointing to /etc/alternatives/lightdm-greeter which
     * is also a symlink to a real .desktop file back in /usr/share/xgreeters/
     *
     * Returns the name of the preferred greeter -- usually a .desktop file that
     * specifies where the actual executable is, or empty if nothing was found.
     */
    QString findPreferredGreeter() const
    {
        QStringList available = QDir( inTarget( m_rootMountPoint, "usr/share/xgreeters" ) )
                                    .entryList( QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System );
        available.sort();
        for ( const QString& n : m_preferredGreeters )  // Preferred ones
        {
            if ( available.contains( n ) )
            {
                return n;
            }
        }
        for ( const QString& n : std::as_const( available ) )  // .. otherwise any .desktop
        {
            if ( n.endsWith( ".desktop" ) )
            {
                return n;
            }
        }
        return QString();
    }

    Result greeterSetup() override
    {
        const QString greeterName = findPreferredGreeter();
        if ( greeterName.isEmpty() )
        {
            cError() << "No greeter found at all, preferred" << m_preferredGreeters;
            return Message { translate( "Cannot configure LightDM" ), translate( "No LightDM greeter installed." ) };
        }

        QString greeter = QFileInfo( greeterName ).fileName();  // Follow symlinks, hope they are not absolute
        if ( greeter.endsWith( ".desktop" ) )
        {
            greeter.chop( 8 );  // Remove ".desktop" from the end
        }

        cDebug() << "found greeter" << greeter;
        // sed -i -e "s/^.*greeter-session=.*/greeter-session=<greeter>/" etc/lightdm/lightdm.conf
        static const QRegularExpression re( QStringLiteral( "^.*greeter-session=.*" ) );
        replaceMatchingLines(
            inTarget( m_rootMountPoint, "etc/lightdm/lightdm.conf" ), re, QStringLiteral( "greeter-session=%1" ).arg( greeter ) );
        cDebug() << greeter << "configured as greeter.";
        return std::nullopt;
    }

private:
    // Can be overridden in the .conf file. With no value it won't match any
    // desktop file in the xgreeters directory and instead we end up picking
    // the alphabetically first file there.
    QStringList m_preferredGreeters;
};

class DMslim : public DisplayManager
{
public:
    explicit DMslim( const QString& root )
        : DisplayManager( root )
    {
        m_executable = QStringLiteral( "slim" );
    }
    QString name() const override { return QStringLiteral( "slim" ); }

    Result setAutologin( const QString& username, bool doAutologin, const DesktopEnvironment* ) override
    {
        const QString path = inTarget( m_rootMountPoint, "etc/slim.conf" );
        QStringList lines;
        if ( !readLines( path, lines ) )
        {
            return Message { translate( "Cannot write SLIM configuration file" ),
                             translate( "SLIM config file %1 does not exist" ).arg( path ) };
        }

        QString out;
        for ( QString line : lines )
        {
            if ( line.contains( "auto_login" ) )
            {
                line = doAutologin ? QStringLiteral( "auto_login yes\n" ) : QStringLiteral( "auto_login no\n" );
            }
            if ( doAutologin && line.contains( "default_user" ) )
            {
                line = QStringLiteral( "default_user %1\n" ).arg( username );
            }
            out += line;
        }
        if ( !writeText( path, out ) )
        {
            return cannotWrite( translate( "Cannot write SLIM configuration file" ), path );
        }
        return std::nullopt;
    }

    Result basicSetup() override { return std::nullopt; }
    Result desktopEnvironmentSetup( const DesktopEnvironment& ) override { return std::nullopt; }
    Result greeterSetup() override { return std::nullopt; }
};

class DMsddm : public DisplayManager
{
public:
    explicit DMsddm( const QString& root )
        : DisplayManager( root )
    {
        m_executable = QStringLiteral( "sddm" );
    }
    QString name() const override { return QStringLiteral( "sddm" ); }

    void configure( const QVariantMap& settings ) override
    {
        DisplayManager::configure( settings );
        if ( settings.contains( "configuration_file" ) )
        {
            m_configurationFile = ClaudeInstall::getString( settings, "configuration_file" );
        }
    }

    Result setAutologin( const QString& username, bool doAutologin, const DesktopEnvironment* de ) override
    {
        QString relative = m_configurationFile;
        while ( relative.startsWith( '/' ) )
        {
            relative.remove( 0, 1 );
        }
        const QString path = inTarget( m_rootMountPoint, relative );

        IniFile config;
        if ( QFileInfo( path ).isFile() )
        {
            QString error;
            if ( !config.read( path, &error ) )
            {
                return Message { translate( "Cannot read SDDM configuration file" ),
                                 translate( "%1: %2" ).arg( path, error ) };
            }
        }

        if ( !config.hasSection( "Autologin" ) )
        {
            config.addSection( "Autologin" );
        }

        if ( doAutologin )
        {
            config.set( "Autologin", "User", username );
        }
        else if ( config.hasOption( "Autologin", "User" ) )
        {
            config.removeOption( "Autologin", "User" );
        }

        if ( de )
        {
            config.set( "Autologin", "Session", de->desktopFile );
        }

        if ( !config.write( path ) )
        {
            return cannotWrite( translate( "Cannot write SDDM configuration file" ), path );
        }
        return std::nullopt;
    }

    Result basicSetup() override { return std::nullopt; }
    Result desktopEnvironmentSetup( const DesktopEnvironment& ) override { return std::nullopt; }
    Result greeterSetup() override { return std::nullopt; }

private:
    QString m_configurationFile = QStringLiteral( "/etc/sddm.conf" );
};

class DMgreetd : public DisplayManager
{
public:
    explicit DMgreetd( const QString& root )
        : DisplayManager( root )
    {
        m_executable = QStringLiteral( "greetd" );
    }
    QString name() const override { return QStringLiteral( "greetd" ); }

    void configure( const QVariantMap& settings ) override
    {
        DisplayManager::configure( settings );
        if ( settings.contains( "greeter_user" ) )
        {
            m_greeterUser = ClaudeInstall::getString( settings, "greeter_user" );
        }
        if ( settings.contains( "greeter_group" ) )
        {
            m_greeterGroup = ClaudeInstall::getString( settings, "greeter_group" );
        }
        if ( settings.contains( "greeter_css_location" ) )
        {
            m_greeterCssLocation = ClaudeInstall::getString( settings, "greeter_css_location" );
        }
    }

    Result basicSetup() override
    {
        ensureGroup( m_greeterGroup, { m_greeterGroup } );
        ensureUser( m_greeterUser, { "-c", "\"Greeter User\"", "-g", m_greeterGroup, "-s", "/bin/bash", m_greeterUser } );
        return std::nullopt;
    }

    Result desktopEnvironmentSetup( const DesktopEnvironment& de ) override
    {
        const QString path = inTarget( m_rootMountPoint, "etc/greetd/environments" );
        if ( !writeText( path, de.executable + '\n' ) )
        {
            return cannotWrite( translate( "Cannot configure greetd" ), path );
        }
        return std::nullopt;
    }

    Result greeterSetup() override { return std::nullopt; }

    Result setAutologin( const QString& username, bool doAutologin, const DesktopEnvironment* de ) override
    {
        if ( !de )
        {
            return Message { translate( "Cannot configure greetd" ), translate( "No desktop environment was found." ) };
        }

        const QString configPath = inTarget( m_rootMountPoint, "etc/greetd/config.toml" );
        QVariantMap config;
        if ( QFileInfo::exists( configPath ) )
        {
            QString error;
            if ( !Toml::load( configPath, config, &error ) )
            {
                return Message { translate( "Cannot read greetd configuration file" ),
                                 translate( "%1: %2" ).arg( configPath, error ) };
            }
        }

        QVariantMap terminal;
        terminal[ "vt" ] = "next";
        config[ "terminal" ] = terminal;

        QVariantMap defaultSession = config.value( "default_session" ).toMap();
        defaultSession[ "user" ] = m_greeterUser;

        const QString deCommand = de->executable;
        if ( QFileInfo::exists( inTarget( m_rootMountPoint, "usr/bin/gtkgreet" ) )
             && QFileInfo::exists( inTarget( m_rootMountPoint, "usr/bin/cage" ) ) )
        {
            QString command = QStringLiteral( "cage -d -s -- gtkgreet" );
            if ( !m_greeterCssLocation.isEmpty() )
            {
                command += QStringLiteral( " -s %1" ).arg( m_greeterCssLocation );
            }
            defaultSession[ "command" ] = command;
        }
        else if ( QFileInfo::exists( inTarget( m_rootMountPoint, "usr/bin/tuigreet" ) ) )
        {
            defaultSession[ "command" ]
                = QStringLiteral( "tuigreet --remember --time --issue --asterisks --cmd " ) + deCommand;
        }
        else if ( QFileInfo::exists( inTarget( m_rootMountPoint, "usr/bin/ddlm" ) ) )
        {
            defaultSession[ "command" ] = QStringLiteral( "ddlm --target " ) + deCommand;
        }
        else
        {
            defaultSession[ "command" ] = QStringLiteral( "agreety --cmd " ) + deCommand;
        }
        config[ "default_session" ] = defaultSession;

        if ( doAutologin )
        {
            // Log in as user, with given DE
            QVariantMap initial;
            initial[ "command" ] = deCommand;
            initial[ "user" ] = username;
            config[ "initial_session" ] = initial;
        }
        else
        {
            // No autologin, remove any autologin that was copied from the live ISO
            config.remove( "initial_session" );
        }

        if ( !Toml::save( configPath, config ) )
        {
            return cannotWrite( translate( "Cannot configure greetd" ), configPath );
        }
        return std::nullopt;
    }

private:
    QString m_greeterUser = QStringLiteral( "greeter" );
    QString m_greeterGroup = QStringLiteral( "greetd" );
    QString m_greeterCssLocation;
};

/** @brief For openSUSE-derivatives there is only sysconfig to configure
 *
 * There is no special DM configuration. Instead, check that sysconfig is
 * available in the target.
 */
class DMsysconfig : public DisplayManager
{
public:
    explicit DMsysconfig( const QString& root )
        : DisplayManager( root )
    {
    }
    QString name() const override { return QStringLiteral( "sysconfig" ); }

    bool haveDm() override { return QFileInfo::exists( m_rootMountPoint + QStringLiteral( "/etc/sysconfig/displaymanager" ) ); }

    Result setAutologin( const QString& username, bool doAutologin, const DesktopEnvironment* ) override
    {
        // sed -i -e 's|^DISPLAYMANAGER_AUTOLOGIN=.*|DISPLAYMANAGER_AUTOLOGIN="<username>"|' etc/sysconfig/displaymanager
        static const QRegularExpression re( QStringLiteral( "^DISPLAYMANAGER_AUTOLOGIN=.*" ) );
        replaceMatchingLines( m_rootMountPoint + QStringLiteral( "/etc/sysconfig/displaymanager" ),
                              re,
                              QStringLiteral( "DISPLAYMANAGER_AUTOLOGIN=\"%1\"" ).arg( doAutologin ? username : QString() ) );
        return std::nullopt;
    }

    Result basicSetup() override { return std::nullopt; }
    Result desktopEnvironmentSetup( const DesktopEnvironment& ) override { return std::nullopt; }
    Result greeterSetup() override { return std::nullopt; }
};

using Creator = std::unique_ptr< DisplayManager > ( * )( const QString& );
template < typename T >
std::unique_ptr< DisplayManager >
make( const QString& root )
{
    return std::unique_ptr< DisplayManager >( new T( root ) );
}

const QList< QPair< QString, Creator > >&
creators()
{
    static const QList< QPair< QString, Creator > > list {
        { "gdm", &make< DMgdm > },
        { "greetd", &make< DMgreetd > },
        { "lightdm", &make< DMlightdm > },
        { "lxdm", &make< DMlxdm > },
        { "mdm", &make< DMmdm > },
        { "sddm", &make< DMsddm > },
        { "slim", &make< DMslim > },
        { "sysconfig", &make< DMsysconfig > },
    };
    return list;
}
}  // namespace

// ---- DesktopEnvironment

DesktopEnvironment::DesktopEnvironment( const QString& executable_, const QString& desktopFile_ )
    : executable( executable_ )
    , desktopFile( desktopFile_ )
{
}

QString
DesktopEnvironment::searchExecutable( const QString& rootMountPoint, const QString& pathname ) const
{
    // If the pathname is absolute, just check there inside the target,
    // otherwise search in a sort-of-sensible $PATH.
    const QStringList path = pathname.startsWith( '/' ) ? QStringList { QString() }
                                                        : QStringList { "/bin/", "/usr/bin/", "/sbin/", "/usr/local/bin/" };
    for ( const QString& p : path )
    {
        const QString absolutePath = rootMountPoint + p + pathname;
        if ( QFileInfo::exists( absolutePath ) )
        {
            return absolutePath;
        }
    }
    return QString();
}

QString
DesktopEnvironment::searchTryExec( const QString& rootMountPoint, const QString& absoluteDesktopFile ) const
{
    // Check the .desktop file for a TryExec line and, if that is found,
    // search for the command (executable pathname) within the target.
    QFile f( absoluteDesktopFile );
    if ( !f.open( QIODevice::ReadOnly ) )
    {
        return QString();
    }
    for ( const QString& line : QString::fromUtf8( f.readAll() ).split( '\n' ) )
    {
        if ( !line.startsWith( "TryExec" ) )
        {
            continue;
        }
        const QStringList keyValue = line.split( '=' );
        if ( keyValue.size() == 2 && keyValue.at( 0 ).trimmed() == "TryExec" )
        {
            return searchExecutable( rootMountPoint, keyValue.at( 1 ).trimmed() );
        }
    }
    return QString();
}

QString
DesktopEnvironment::findExecutable( const QString& rootMountPoint ) const
{
    return searchExecutable( rootMountPoint, executable );
}

QString
DesktopEnvironment::findDesktopFile( const QString& rootMountPoint ) const
{
    // Searches both X11 and Wayland sessions
    const QString x11Sessions = QStringLiteral( "%1/usr/share/xsessions/%2.desktop" ).arg( rootMountPoint, desktopFile );
    const QString waylandSessions
        = QStringLiteral( "%1/usr/share/wayland-sessions/%2.desktop" ).arg( rootMountPoint, desktopFile );
    for ( const QString& candidate : { x11Sessions, waylandSessions } )
    {
        if ( QFileInfo::exists( candidate ) )
        {
            return candidate;
        }
    }
    return QString();
}

bool
DesktopEnvironment::isInstalled( const QString& rootMountPoint ) const
{
    const QString desktop = findDesktopFile( rootMountPoint );
    if ( desktop.isEmpty() )
    {
        return false;
    }
    return !findExecutable( rootMountPoint ).isEmpty() || !searchTryExec( rootMountPoint, desktop ).isEmpty();
}

bool
DesktopEnvironment::updateFromDesktopFile( const QString& rootMountPoint )
{
    const QString desktop = findDesktopFile( rootMountPoint );
    if ( desktop.isEmpty() )
    {
        return false;
    }

    if ( !findExecutable( rootMountPoint ).isEmpty() )
    {
        // .desktop found and executable as well.
        return true;
    }

    QString executableFile = searchTryExec( rootMountPoint, desktop );
    if ( !executableFile.isEmpty() )
    {
        // Found from the .desktop file, so update own executable config
        if ( !rootMountPoint.isEmpty() && executableFile.startsWith( rootMountPoint ) )
        {
            executableFile = executableFile.mid( rootMountPoint.size() );
        }
        if ( executableFile.isEmpty() )
        {
            // Somehow chopped down to nothing
            return false;
        }
        if ( executableFile.at( 0 ) != '/' )
        {
            executableFile.prepend( '/' );
        }
        executable = executableFile;
        return true;
    }
    // This is to double-check
    return isInstalled( rootMountPoint );
}

QList< DesktopEnvironment >
knownDesktopEnvironments()
{
    // Each DE has a sample executable to look for, and a .desktop filename.
    // If the executable exists, the DE is assumed to be installed and to use
    // the given .desktop filename.
    //
    // If the .desktop file exists and contains a TryExec line and that
    // TryExec executable exists (searched in /bin, /usr/bin, /sbin and
    // /usr/local/bin) then the DE is assumed to be installed
    // and to use that .desktop filename.
    return {
        { "/usr/bin/startplasma-x11", "plasma" },  // KDE Plasma 5.17+
        { "/usr/bin/startkde", "plasma" },  // KDE Plasma 5
        { "/usr/bin/startkde", "kde-plasma" },  // KDE Plasma 4
        { "/usr/bin/budgie-desktop", "budgie-desktop" },  // Budgie v10
        { "/usr/bin/budgie-session", "budgie-desktop" },  // Budgie v8
        { "/usr/bin/gnome-session", "gnome" },
        { "/usr/bin/cinnamon-session-cinnamon", "cinnamon" },
        { "/usr/bin/mate-session", "mate" },
        { "/usr/bin/enlightenment_start", "enlightenment" },
        { "/usr/bin/lxsession", "LXDE" },
        { "/usr/bin/startlxde", "LXDE" },
        { "/usr/bin/lxqt-session", "lxqt" },
        { "/usr/bin/pekwm", "pekwm" },
        { "/usr/bin/pantheon-session", "pantheon" },
        { "/usr/bin/startdde", "deepin" },
        { "/usr/bin/startxfce4", "xfce" },
        { "/usr/bin/openbox-session", "openbox" },
        { "/usr/bin/i3", "i3" },
        { "/usr/bin/awesome", "awesome" },
        { "/usr/bin/bspwm", "bspwm" },
        { "/usr/bin/herbstluftwm", "herbstluftwm" },
        { "/usr/bin/qtile", "qtile" },
        { "/usr/bin/xmonad", "xmonad" },
        { "/usr/bin/dwm", "dwm" },
        { "/usr/bin/jwm", "jwm" },
        { "/usr/bin/icewm-session", "icewm-session" },
        { "/usr/bin/fvwm3", "fvwm3" },
        { "/usr/bin/sway", "sway" },
        { "/usr/bin/ukui-session", "ukui" },
        { "/usr/bin/cutefish-session", "cutefish-xsession" },
        { "/usr/bin/river", "river" },
        { "/usr/bin/Hyprland", "hyprland" },
    };
}

std::optional< DesktopEnvironment >
findDesktopEnvironment( const QString& rootMountPoint )
{
    cDebug() << "Using rootMountPoint" << rootMountPoint;
    for ( const DesktopEnvironment& de : knownDesktopEnvironments() )
    {
        if ( de.isInstalled( rootMountPoint ) )
        {
            cDebug() << ".. selected DE" << de.desktopFile;
            return de;
        }
    }
    return std::nullopt;
}

// ---- DisplayManager

DisplayManager::DisplayManager( const QString& rootMountPoint )
    : m_rootMountPoint( rootMountPoint )
{
}

DisplayManager::~DisplayManager() {}

bool
DisplayManager::haveDm()
{
    if ( m_executable.isEmpty() )
    {
        return false;
    }
    return QFileInfo::exists( QStringLiteral( "%1/usr/bin/%2" ).arg( m_rootMountPoint, m_executable ) )
        || QFileInfo::exists( QStringLiteral( "%1/usr/sbin/%2" ).arg( m_rootMountPoint, m_executable ) );
}

void
DisplayManager::configure( const QVariantMap& settings )
{
    if ( settings.contains( "executable" ) )
    {
        m_executable = ClaudeInstall::getString( settings, "executable" );
    }
}

QStringList
DisplayManager::names()
{
    QStringList names;
    for ( const auto& c : creators() )
    {
        names << c.first;
    }
    return names;
}

std::unique_ptr< DisplayManager >
DisplayManager::create( const QString& name, const QString& rootMountPoint )
{
    for ( const auto& c : creators() )
    {
        if ( c.first == name )
        {
            return c.second( rootMountPoint );
        }
    }
    return nullptr;
}

}  // namespace DisplayManagers
