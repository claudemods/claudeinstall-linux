/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Tests.h"

#include "PackageManager.h"

#include "utils/Logger.h"
#include "utils/Yaml.h"

#include <QtTest/QtTest>

using namespace Packages;

QTEST_GUILESS_MAIN( PackagesTests )

PackagesTests::PackagesTests() {}

PackagesTests::~PackagesTests() {}

void
PackagesTests::initTestCase()
{
    Logger::setupLogLevel( Logger::LOGDEBUG );
}

void
PackagesTests::testBackends()
{
    const QStringList names = PackageManager::backends();
    QVERIFY( names.contains( "pacman" ) );
    QVERIFY( names.contains( "apt" ) );
    QVERIFY( names.contains( "dummy" ) );

    const QVariantMap noConfig;
    QVERIFY( PackageManager::create( "pacman", noConfig ) );
    QCOMPARE( PackageManager::create( "dummy", noConfig )->backend(), QString( "dummy" ) );
    QVERIFY( !PackageManager::create( "no-such-backend", noConfig ) );
}

void
PackagesTests::testPacmanDefaults()
{
    const auto config = PacmanConfig::fromJobConfiguration( QVariantMap() );
    QCOMPARE( config.numRetries, 0 );
    QVERIFY( !config.disableDownloadTimeout );
    QVERIFY( !config.neededOnly );
}

void
PackagesTests::testPacmanConfig_data()
{
    QTest::addColumn< QString >( "filename" );
    QTest::addColumn< int >( "retries" );
    QTest::addColumn< bool >( "timeout" );
    QTest::addColumn< bool >( "needed" );

    QTest::newRow( "ops-1" ) << "pm-pacman-1.yaml" << 4 << true << true;
    QTest::newRow( "ops-2" ) << "pm-pacman-2.yaml" << 3 << false << false;
}

void
PackagesTests::testPacmanConfig()
{
    QFETCH( QString, filename );
    QFETCH( int, retries );
    QFETCH( bool, timeout );
    QFETCH( bool, needed );

    bool ok = false;
    const auto configuration = ClaudeInstall::YAML::load( QStringLiteral( BUILD_AS_TEST ) + "/tests/" + filename, &ok );
    QVERIFY( ok );
    QCOMPARE( configuration.value( "backend" ).toString(), QString( "pacman" ) );

    const auto config = PacmanConfig::fromJobConfiguration( configuration );
    QCOMPARE( config.numRetries, retries );
    QCOMPARE( config.disableDownloadTimeout, timeout );
    QCOMPARE( config.neededOnly, needed );
}

void
PackagesTests::testPacmanBadConfig()
{
    // A *pacman* setting that isn't a map is ignored
    QVariantMap configuration;
    configuration[ "pacman" ] = QStringList { "num_retries" };
    const auto config = PacmanConfig::fromJobConfiguration( configuration );
    QCOMPARE( config.numRetries, 0 );
    QVERIFY( !config.neededOnly );
}

void
PackagesTests::testSubstLocale()
{
    QVariantMap withScripts;
    withScripts[ "package" ] = "hunspell-$LOCALE";
    withScripts[ "pre-script" ] = "touch /tmp/foo";
    const QVariantList list { "vi", "kde-l10n-${LOCALE}", "plain-$$LOCALE", QVariant( withScripts ) };

    // With a real locale, placeholders are replaced ($$ is an escaped dollar)
    const auto nl = substLocale( list, "nl" );
    QCOMPARE( nl.size(), 4 );
    QCOMPARE( nl.at( 0 ).toString(), QString( "vi" ) );
    QCOMPARE( nl.at( 1 ).toString(), QString( "kde-l10n-nl" ) );
    QCOMPARE( nl.at( 2 ).toString(), QString( "plain-$LOCALE" ) );
    QCOMPARE( nl.at( 3 ).toMap().value( "package" ).toString(), QString( "hunspell-nl" ) );
    QCOMPARE( nl.at( 3 ).toMap().value( "pre-script" ).toString(), QString( "touch /tmp/foo" ) );

    // For English (or no locale at all) the localized packages are dropped
    for ( const QString& locale : { QString( "en" ), QString() } )
    {
        const auto en = substLocale( list, locale );
        QCOMPARE( en.size(), 1 );
        QCOMPARE( en.at( 0 ).toString(), QString( "vi" ) );
    }
}
