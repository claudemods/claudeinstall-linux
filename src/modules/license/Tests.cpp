/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Tests.h"

#include "Config.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

QTEST_GUILESS_MAIN( LicenseTests )

LicenseTests::LicenseTests() {}

LicenseTests::~LicenseTests() {}

static bool
licenseAgree()
{
    return ClaudeInstall::JobQueue::instance()->globalStorage()->value( "licenseAgree" ).toBool();
}

static QVariantMap
entry( const QString& id, const QString& name, const QString& url, bool required )
{
    QVariantMap m;
    m[ "id" ] = id;
    m[ "name" ] = name;
    m[ "url" ] = url;
    m[ "required" ] = required;
    return m;
}

void
LicenseTests::initTestCase()
{
    Logger::setupLogLevel( Logger::LOGDEBUG );
    (void)new ClaudeInstall::JobQueue();
}

void
LicenseTests::testEntries()
{
    QVariantMap local = entry( "c", "C", "file:///tmp/c.txt", false );
    local[ "type" ] = "codec";
    local[ "vendor" ] = "Vendor";
    local[ "expand" ] = true;

    QVariantMap incomplete;  // no name, no url: not valid
    incomplete[ "id" ] = "bad";

    QVariantMap configuration;
    configuration[ "entries" ]
        = QVariantList { entry( "a", "A", "https://example.org/a", true ), incomplete, QString( "not a map" ), local };

    Config config;
    config.setConfigurationMap( configuration );

    const auto entries = config.entries();
    QCOMPARE( entries.size(), 2 );

    QCOMPARE( entries.at( 0 ).m_id, QString( "a" ) );
    QCOMPARE( entries.at( 0 ).m_prettyName, QString( "A" ) );
    QVERIFY( entries.at( 0 ).isRequired() );
    QVERIFY( !entries.at( 0 ).isLocal() );
    QVERIFY( !entries.at( 0 ).expandByDefault() );
    QVERIFY( entries.at( 0 ).m_type == LicenseEntry::Type::Software );  // the default

    QCOMPARE( entries.at( 1 ).m_id, QString( "c" ) );
    QCOMPARE( entries.at( 1 ).m_prettyVendor, QString( "Vendor" ) );
    QVERIFY( !entries.at( 1 ).isRequired() );
    QVERIFY( entries.at( 1 ).isLocal() );
    QVERIFY( entries.at( 1 ).expandByDefault() );
    QVERIFY( entries.at( 1 ).m_type == LicenseEntry::Type::Codec );

    // Without entries there is nothing
    Config empty;
    empty.setConfigurationMap( QVariantMap() );
    QVERIFY( empty.entries().isEmpty() );
}

void
LicenseTests::testAcceptance()
{
    QVariantMap configuration;
    configuration[ "entries" ] = QVariantList { entry( "a", "A", "https://example.org/a", true ) };

    Config config;
    config.setConfigurationMap( configuration );

    // Something is required, nothing is accepted yet
    QVERIFY( !config.allLicensesOptional() );
    QVERIFY( !config.accepted() );
    QVERIFY( !config.isNextEnabled() );
    QVERIFY( !licenseAgree() );

    QSignalSpy accepted( &config, &Config::acceptedChanged );
    QSignalSpy next( &config, &Config::isNextEnabledChanged );

    config.setAccepted( true );
    QVERIFY( config.accepted() );
    QVERIFY( config.isNextEnabled() );
    QVERIFY( licenseAgree() );
    QCOMPARE( accepted.count(), 1 );
    QCOMPARE( next.count(), 1 );
    QCOMPARE( next.last().at( 0 ).toBool(), true );

    // Accepting again does not change the acceptance, but the global storage is kept up to date
    config.setAccepted( true );
    QCOMPARE( accepted.count(), 1 );
    QVERIFY( licenseAgree() );

    config.setAccepted( false );
    QVERIFY( !config.accepted() );
    QVERIFY( !config.isNextEnabled() );
    QVERIFY( !licenseAgree() );
    QCOMPARE( accepted.count(), 2 );

    // Loading the configuration again starts from not-accepted
    config.setAccepted( true );
    config.setConfigurationMap( configuration );
    QVERIFY( !config.accepted() );
    QVERIFY( !config.isNextEnabled() );
}

void
LicenseTests::testOptional()
{
    QVariantMap configuration;
    configuration[ "entries" ] = QVariantList { entry( "a", "A", "https://example.org/a", false ),
                                                entry( "b", "B", "https://example.org/b", false ) };

    Config config;
    config.setConfigurationMap( configuration );

    // Nothing is required: the user can go on without accepting
    QVERIFY( config.allLicensesOptional() );
    QVERIFY( !config.accepted() );
    QVERIFY( config.isNextEnabled() );
    QVERIFY( !licenseAgree() );  // but the terms were not agreed to

    // No licenses at all is also optional
    Config none;
    none.setConfigurationMap( QVariantMap() );
    QVERIFY( none.allLicensesOptional() );
    QVERIFY( none.isNextEnabled() );
}
