/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Tests.h"

#include "DisplayManager.h"
#include "IniFile.h"
#include "TomlFile.h"

#include "utils/Logger.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace DisplayManagers;

QTEST_GUILESS_MAIN( DisplayManagerTests )

DisplayManagerTests::DisplayManagerTests() {}

DisplayManagerTests::~DisplayManagerTests() {}

static void
writeFile( const QString& path, const QByteArray& contents )
{
    QDir().mkpath( QFileInfo( path ).path() );
    QFile f( path );
    QVERIFY( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) );
    f.write( contents );
}

static QString
readFile( const QString& path )
{
    QFile f( path );
    return f.open( QIODevice::ReadOnly ) ? QString::fromUtf8( f.readAll() ) : QString();
}

void
DisplayManagerTests::initTestCase()
{
    Logger::setupLogLevel( Logger::LOGDEBUG );
}

void
DisplayManagerTests::testIniFile()
{
    IniFile ini;
    QString error;
    QVERIFY( ini.parse( "# a comment\n"
                        "[General]\n"
                        "Numlock=none\n"
                        "Mixed : colon\n"
                        "Dup=first\n"
                        "Dup=second\n"
                        "Long=one\n"
                        "   two\n"
                        "\n"
                        "; another comment\n"
                        "[Autologin]\n"
                        "User = alice \n",
                        &error ) );
    QVERIFY( error.isEmpty() );

    QVERIFY( ini.hasSection( "General" ) );
    QVERIFY( ini.hasSection( "Autologin" ) );
    QVERIFY( !ini.hasSection( "Theme" ) );
    QCOMPARE( ini.value( "General", "Numlock" ), QString( "none" ) );
    QCOMPARE( ini.value( "General", "Mixed" ), QString( "colon" ) );
    QCOMPARE( ini.value( "General", "Dup" ), QString( "second" ) );  // last one wins
    QCOMPARE( ini.value( "General", "Long" ), QString( "one\ntwo" ) );
    QCOMPARE( ini.value( "Autologin", "User" ), QString( "alice" ) );

    ini.set( "Autologin", "Session", "plasma" );
    ini.removeOption( "General", "Numlock" );
    QVERIFY( !ini.hasOption( "General", "Numlock" ) );
    QVERIFY( ini.hasOption( "Autologin", "Session" ) );

    // Written without spaces around the delimiter, no comments, blank line after each section
    QCOMPARE( ini.toString(),
              QString( "[General]\n"
                       "Mixed=colon\n"
                       "Dup=second\n"
                       "Long=one\n\ttwo\n"
                       "\n"
                       "[Autologin]\n"
                       "User=alice\n"
                       "Session=plasma\n"
                       "\n" ) );

    // And it reads back the same
    IniFile again;
    QVERIFY( again.parse( ini.toString() ) );
    QCOMPARE( again.toString(), ini.toString() );
}

void
DisplayManagerTests::testIniFileErrors()
{
    QString error;
    IniFile noSection;
    QVERIFY( !noSection.parse( "User=alice\n", &error ) );
    QVERIFY( !error.isEmpty() );

    IniFile garbage;
    QVERIFY( !garbage.parse( "[Section]\nthis is not an option\n", &error ) );

    IniFile empty;
    QVERIFY( empty.parse( "" ) );
    QCOMPARE( empty.toString(), QString() );
}

void
DisplayManagerTests::testTomlParse()
{
    QVariantMap config;
    QString error;
    QVERIFY( Toml::parse( "# greetd\n"
                          "[terminal]\n"
                          "vt = 1  # a comment\n"
                          "\n"
                          "[default_session]\n"
                          "command = \"agreety --cmd /bin/sh\"\n"
                          "user = 'greeter'\n"
                          "escaped = \"tab\\there \\\"quoted\\\" \\u00e9\"\n"
                          "list = [ 1, 2.5, \"three\",\n"
                          "         true ]\n"
                          "inline = { a = 1, b = \"x\" }\n"
                          "dotted.key = false\n",
                          config,
                          &error ) );
    QVERIFY2( error.isEmpty(), qPrintable( error ) );

    QCOMPARE( config.value( "terminal" ).toMap().value( "vt" ).toLongLong(), 1LL );
    const QVariantMap session = config.value( "default_session" ).toMap();
    QCOMPARE( session.value( "command" ).toString(), QString( "agreety --cmd /bin/sh" ) );
    QCOMPARE( session.value( "user" ).toString(), QString( "greeter" ) );
    QCOMPARE( session.value( "escaped" ).toString(), QString( "tab\there \"quoted\" \u00e9" ) );
    const QVariantList list = session.value( "list" ).toList();
    QCOMPARE( list.size(), 4 );
    QCOMPARE( list.at( 0 ).toLongLong(), 1LL );
    QCOMPARE( list.at( 1 ).toDouble(), 2.5 );
    QCOMPARE( list.at( 2 ).toString(), QString( "three" ) );
    QCOMPARE( list.at( 3 ).toBool(), true );
    QCOMPARE( session.value( "inline" ).toMap().value( "b" ).toString(), QString( "x" ) );
    QVERIFY( session.value( "dotted" ).toMap().contains( "key" ) );
    QCOMPARE( session.value( "dotted" ).toMap().value( "key" ).toBool(), false );
}

void
DisplayManagerTests::testTomlUnsupported()
{
    QVariantMap config;
    QString error;
    // Things that are not valid TOML
    QVERIFY( !Toml::parse( "key = \n", config, &error ) );
    QVERIFY( !Toml::parse( "key = \"unterminated\n", config, &error ) );
    QVERIFY( !Toml::parse( "a = 1\na = 2\n", config, &error ) );
    QVERIFY( !Toml::parse( "= 1\n", config, &error ) );
    // Things that are valid but not supported, which must be refused rather than lost
    QVERIFY( !Toml::parse( "[[servers]]\nname = 'x'\n", config, &error ) );
    QVERIFY( !Toml::parse( "text = \"\"\"multi\nline\"\"\"\n", config, &error ) );
    QVERIFY( !Toml::parse( "when = 1979-05-27\n", config, &error ) );
    QVERIFY( !error.isEmpty() );
}

void
DisplayManagerTests::testTomlRoundTrip()
{
    QVariantMap terminal;
    terminal[ "vt" ] = "next";
    QVariantMap session;
    session[ "command" ] = "tuigreet --cmd \"a b\"";
    session[ "user" ] = "greeter";
    QVariantMap initial;
    initial[ "command" ] = "/usr/bin/startplasma-x11";
    initial[ "user" ] = "d";
    QVariantMap config;
    config[ "terminal" ] = terminal;
    config[ "default_session" ] = session;
    config[ "initial_session" ] = initial;
    config[ "verbose" ] = true;
    config[ "numbers" ] = QVariantList { 1, 2 };

    const QString text = Toml::dump( config );
    // Top-level values come before the first table
    QVERIFY( text.startsWith( "numbers = [1, 2]\nverbose = true\n" ) );
    QVERIFY( text.contains( "[default_session]\ncommand = \"tuigreet --cmd \\\"a b\\\"\"\nuser = \"greeter\"\n" ) );

    QVariantMap again;
    QString error;
    QVERIFY2( Toml::parse( text, again, &error ), qPrintable( error ) );
    QCOMPARE( again.value( "verbose" ).toBool(), true );
    QCOMPARE( again.value( "default_session" ).toMap().value( "command" ).toString(),
              session.value( "command" ).toString() );
    QCOMPARE( again.value( "initial_session" ).toMap().value( "user" ).toString(), QString( "d" ) );
    QCOMPARE( Toml::dump( again ), text );
}

void
DisplayManagerTests::testDesktopEnvironments()
{
    QTemporaryDir root;
    QVERIFY( root.isValid() );
    const QString r = root.path();

    // Nothing installed
    QVERIFY( !findDesktopEnvironment( r ) );

    // KDE: the .desktop file and the executable
    writeFile( r + "/usr/bin/startkde", "#! /bin/sh\n" );
    QVERIFY( !findDesktopEnvironment( r ) );  // still no .desktop file
    writeFile( r + "/usr/share/xsessions/plasma.desktop", "[Desktop Entry]\nExec=startkde\n" );
    const auto found = findDesktopEnvironment( r );
    QVERIFY( found );
    QCOMPARE( found->desktopFile, QString( "plasma" ) );
    QCOMPARE( found->executable, QString( "/usr/bin/startkde" ) );

    // Wayland sessions are found, and the executable is found through TryExec
    writeFile( r + "/usr/share/wayland-sessions/custom.desktop", "[Desktop Entry]\nTryExec=customwm\n" );
    writeFile( r + "/usr/local/bin/customwm", "#! /bin/sh\n" );
    DesktopEnvironment custom( "/usr/bin/not-installed", "custom" );
    QVERIFY( custom.findExecutable( r ).isEmpty() );
    QVERIFY( !custom.findDesktopFile( r ).isEmpty() );
    QVERIFY( custom.isInstalled( r ) );
    QVERIFY( custom.updateFromDesktopFile( r ) );
    QCOMPARE( custom.executable, QString( "/usr/local/bin/customwm" ) );

    // A DE without .desktop file is not there
    DesktopEnvironment missing( "/usr/bin/startkde", "no-such-session" );
    QVERIFY( !missing.isInstalled( r ) );
    QVERIFY( !missing.updateFromDesktopFile( r ) );
}

void
DisplayManagerTests::testCreateDisplayManagers()
{
    const QStringList names = DisplayManager::names();
    for ( const char* n : { "gdm", "greetd", "lightdm", "lxdm", "mdm", "sddm", "slim", "sysconfig" } )
    {
        QVERIFY2( names.contains( n ), n );
        auto dm = DisplayManager::create( n, "/nonexistent" );
        QVERIFY( dm );
        QCOMPARE( dm->name(), QString( n ) );
        QVERIFY( !dm->haveDm() );  // nothing is installed in this root
    }
    QVERIFY( !DisplayManager::create( "no-such-dm", "/tmp" ) );
}

void
DisplayManagerTests::testSddm()
{
    QTemporaryDir root;
    QVERIFY( root.isValid() );
    const QString conf = root.path() + "/etc/sddm.conf";
    QDir().mkpath( root.path() + "/etc" );

    const DesktopEnvironment de( "startplasma-x11", "kde-plasma" );
    auto dm = DisplayManager::create( "sddm", root.path() );
    QVERIFY( dm );

    // Repeated (this time checks load/save)
    for ( int i = 0; i < 3; ++i )
    {
        QVERIFY( !dm->setAutologin( "d", true, &de ) );
        IniFile ini;
        QVERIFY( ini.read( conf ) );
        QCOMPARE( ini.value( "Autologin", "User" ), QString( "d" ) );
        QCOMPARE( ini.value( "Autologin", "Session" ), QString( "kde-plasma" ) );
    }

    // Switching autologin off removes the user, but keeps other settings
    QVERIFY( !dm->setAutologin( "d", false, nullptr ) );
    IniFile ini;
    QVERIFY( ini.read( conf ) );
    QVERIFY( !ini.hasOption( "Autologin", "User" ) );
    QVERIFY( ini.hasOption( "Autologin", "Session" ) );

    // A different configuration file
    dm->configure( { { "configuration_file", "/etc/sddm.conf.d/claudeinstall.conf" } } );
    QDir().mkpath( root.path() + "/etc/sddm.conf.d" );
    QVERIFY( !dm->setAutologin( "e", true, nullptr ) );
    QVERIFY( readFile( root.path() + "/etc/sddm.conf.d/claudeinstall.conf" ).contains( "User=e\n" ) );

    // A broken file is reported, and left alone
    dm->configure( { { "configuration_file", "/etc/sddm.conf" } } );
    writeFile( conf, "User=oops\n" );
    QVERIFY( dm->setAutologin( "d", true, &de ) );
    QCOMPARE( readFile( conf ), QString( "User=oops\n" ) );
}

void
DisplayManagerTests::testGdm()
{
    QTemporaryDir root;
    QVERIFY( root.isValid() );
    const QString r = root.path();
    writeFile( r + "/usr/bin/gdm3", "#! /bin/sh\n:\n" );
    QDir().mkpath( r + "/etc/gdm3" );

    const DesktopEnvironment de( "startplasma-x11", "kde-plasma" );
    auto dm = DisplayManager::create( "gdm", r );
    QVERIFY( dm );
    QVERIFY( dm->haveDm() );
    QCOMPARE( dm->executable(), QString( "gdm3" ) );

    // No config file yet: it is created. Repeating changes nothing (this checks load/save).
    for ( int i = 0; i < 3; ++i )
    {
        QVERIFY( !dm->setAutologin( "d", true, &de ) );
        const QString text = readFile( r + "/etc/gdm3/daemon.conf" );
        QCOMPARE( text.count( "[daemon]" ), 1 );
        QCOMPARE( text.count( "AutomaticLogin=d\n" ), 1 );
        QCOMPARE( text.count( "AutomaticLoginEnable=True\n" ), 1 );
    }

    // Autologin off
    QVERIFY( !dm->setAutologin( "d", false, nullptr ) );
    const QString off = readFile( r + "/etc/gdm3/daemon.conf" );
    QVERIFY( !off.contains( "AutomaticLogin=d" ) );
    QVERIFY( off.contains( "AutomaticLoginEnable=False\n" ) );

    // With AccountsService, the session is recorded for autologin
    QDir().mkpath( r + "/var/lib/AccountsService/users" );
    QVERIFY( !dm->setAutologin( "d", true, &de ) );
    QCOMPARE( readFile( r + "/var/lib/AccountsService/users/d" ), QString( "[User]\nXSession=kde-plasma\nIcon=\n" ) );
}

void
DisplayManagerTests::testGreetd()
{
    QTemporaryDir root;
    QVERIFY( root.isValid() );
    const QString r = root.path();
    QDir().mkpath( r + "/etc/greetd" );
    const QString conf = r + "/etc/greetd/config.toml";

    const DesktopEnvironment de( "startplasma-x11", "kde-plasma" );
    auto dm = DisplayManager::create( "greetd", r );
    QVERIFY( dm );
    dm->configure( { { "greeter_user", "tom_bombadil" } } );

    // Repeated (this time checks load/save)
    for ( int i = 0; i < 3; ++i )
    {
        QVERIFY( !dm->setAutologin( "d", true, &de ) );
        QVariantMap config;
        QString error;
        QVERIFY2( Toml::load( conf, config, &error ), qPrintable( error ) );
        QCOMPARE( config.value( "terminal" ).toMap().value( "vt" ).toString(), QString( "next" ) );
        const QVariantMap session = config.value( "default_session" ).toMap();
        QCOMPARE( session.value( "user" ).toString(), QString( "tom_bombadil" ) );
        QCOMPARE( session.value( "command" ).toString(), QString( "agreety --cmd startplasma-x11" ) );
        QCOMPARE( config.value( "initial_session" ).toMap().value( "user" ).toString(), QString( "d" ) );
        QCOMPARE( config.value( "initial_session" ).toMap().value( "command" ).toString(),
                  QString( "startplasma-x11" ) );
    }

    // Other greeters change the command
    writeFile( r + "/usr/bin/tuigreet", "#! /bin/sh\n" );
    QVERIFY( !dm->setAutologin( "d", true, &de ) );
    QVariantMap config;
    QVERIFY( Toml::load( conf, config ) );
    QVERIFY( config.value( "default_session" ).toMap().value( "command" ).toString().startsWith( "tuigreet " ) );

    // Without autologin, the initial session goes away
    QVERIFY( !dm->setAutologin( "d", false, &de ) );
    QVERIFY( Toml::load( conf, config ) );
    QVERIFY( !config.contains( "initial_session" ) );

    // The default DE is written for the greeter
    QVERIFY( !dm->desktopEnvironmentSetup( de ) );
    QCOMPARE( readFile( r + "/etc/greetd/environments" ), QString( "startplasma-x11\n" ) );

    // Unsupported TOML in the file is reported and the file is not touched
    writeFile( conf, "[[sessions]]\nname = 'x'\n" );
    QVERIFY( dm->setAutologin( "d", true, &de ) );
    QCOMPARE( readFile( conf ), QString( "[[sessions]]\nname = 'x'\n" ) );

    // There needs to be a desktop environment to log in to
    QVERIFY( dm->setAutologin( "d", true, nullptr ) );
}

void
DisplayManagerTests::testLightdm()
{
    QTemporaryDir root;
    QVERIFY( root.isValid() );
    const QString r = root.path();
    const QString conf = r + "/etc/lightdm/lightdm.conf";
    const DesktopEnvironment de( "startplasma-x11", "kde-plasma" );
    auto dm = DisplayManager::create( "lightdm", r );
    QVERIFY( dm );

    // No file, and no directory for it: a problem
    QVERIFY( dm->setAutologin( "d", true, &de ) );

    QDir().mkpath( r + "/etc/lightdm" );
    QVERIFY( !dm->setAutologin( "d", true, &de ) );
    QCOMPARE( readFile( conf ), QString( "[Seat:*]\nautologin-user=d\n" ) );

    // An existing file keeps its lines, and has the autologin line replaced
    writeFile( conf, "[Seat:*]\n#autologin-user=\nuser-session=default\ngreeter-session=old\n" );
    QVERIFY( !dm->setAutologin( "e", true, &de ) );
    QCOMPARE( readFile( conf ), QString( "[Seat:*]\nautologin-user=e\nuser-session=default\ngreeter-session=old\n" ) );
    QVERIFY( !dm->setAutologin( "e", false, &de ) );
    QVERIFY( readFile( conf ).contains( "#autologin-user=\n" ) );

    // Without a seat section, one is added
    writeFile( conf, "[LightDM]\nsomething=1\n#autologin-user=\n" );
    QVERIFY( !dm->setAutologin( "e", true, &de ) );
    QCOMPARE( readFile( conf ), QString( "[Seat:*]\n[LightDM]\nsomething=1\nautologin-user=e\n" ) );

    // The DE and the greeter are written into the existing lines
    QVERIFY( !dm->desktopEnvironmentSetup( de ) );
    writeFile( conf, "[Seat:*]\n#user-session=default\n" );
    QVERIFY( !dm->desktopEnvironmentSetup( de ) );
    QCOMPARE( readFile( conf ), QString( "[Seat:*]\nuser-session=kde-plasma\n" ) );

    // No greeters at all is a problem; with some, the preferred one is used
    QVERIFY( dm->greeterSetup() );
    writeFile( r + "/usr/share/xgreeters/a-greeter.desktop", "" );
    writeFile( r + "/usr/share/xgreeters/slick-greeter.desktop", "" );
    writeFile( conf, "[Seat:*]\n#greeter-session=example-gtk-gnome\n" );
    dm->configure( { { "preferred_greeters", QStringList { "slick-greeter.desktop", "other.desktop" } } } );
    QVERIFY( !dm->greeterSetup() );
    QCOMPARE( readFile( conf ), QString( "[Seat:*]\ngreeter-session=slick-greeter\n" ) );
    dm->configure( { { "preferred_greeters", QStringList() } } );
    QVERIFY( !dm->greeterSetup() );
    QCOMPARE( readFile( conf ), QString( "[Seat:*]\ngreeter-session=a-greeter\n" ) );
}
