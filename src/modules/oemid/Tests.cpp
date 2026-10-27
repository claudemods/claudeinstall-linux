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

#include "utils/Logger.h"

#include <QRegularExpression>
#include <QSignalSpy>
#include <QtTest/QtTest>

QTEST_GUILESS_MAIN( OemIdTests )

OemIdTests::OemIdTests() {}

OemIdTests::~OemIdTests() {}

static QVariantMap
batch( const QString& identifier )
{
    QVariantMap m;
    m[ "batch-identifier" ] = identifier;
    return m;
}

void
OemIdTests::initTestCase()
{
    Logger::setupLogLevel( Logger::LOGDEBUG );
}

void
OemIdTests::testSubstitution()
{
    Config config;
    QVERIFY( config.batchIdentifier().isEmpty() );

    // ${DATE} is replaced by the current date, in ISO format
    config.setConfigurationMap( batch( "neon-${DATE}" ) );
    QVERIFY2( QRegularExpression( "^neon-\\d{4}-\\d{2}-\\d{2}$" ).match( config.batchIdentifier() ).hasMatch(),
              qPrintable( config.batchIdentifier() ) );

    // Plain identifiers are used as they are, an empty one is fine
    config.setConfigurationMap( batch( "plain" ) );
    QCOMPARE( config.batchIdentifier(), QString( "plain" ) );
    config.setConfigurationMap( batch( QString() ) );
    QVERIFY( config.batchIdentifier().isEmpty() );
    config.setConfigurationMap( QVariantMap() );
    QVERIFY( config.batchIdentifier().isEmpty() );
}

void
OemIdTests::testChange()
{
    Config config;
    config.setConfigurationMap( batch( "suggested" ) );

    QSignalSpy spy( &config, &Config::batchIdentifierChanged );
    config.setBatchIdentifier( "chosen" );
    QCOMPARE( config.batchIdentifier(), QString( "chosen" ) );
    QCOMPARE( spy.count(), 1 );
    QCOMPARE( spy.last().at( 0 ).toString(), QString( "chosen" ) );

    // Not a change: no signal
    config.setBatchIdentifier( "chosen" );
    QCOMPARE( spy.count(), 1 );

    QVERIFY( config.prettyStatus().contains( "<code>chosen</code>" ) );
}

void
OemIdTests::testJobs()
{
    Config config;
    config.setConfigurationMap( batch( "batch-1" ) );

    // One job writes the identifier, whatever it is -- even if it is empty
    QCOMPARE( config.createJobs().size(), 1 );
    config.setBatchIdentifier( QString() );
    QCOMPARE( config.createJobs().size(), 1 );
}
