/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Tests.h"

#include "BootloaderHelpers.h"

#include "utils/Logger.h"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace Bootloader;

QTEST_GUILESS_MAIN( BootloaderTests )

BootloaderTests::BootloaderTests() {}

BootloaderTests::~BootloaderTests() {}

static void
touch( const QString& path )
{
    QDir().mkpath( QFileInfo( path ).path() );
    QFile f( path );
    QVERIFY( f.open( QIODevice::WriteOnly ) );
}

void
BootloaderTests::initTestCase()
{
    Logger::setupLogLevel( Logger::LOGDEBUG );
}

void
BootloaderTests::testRenderInBase()
{
    const QStringList decimal { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
    const QStringList hex { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F" };
    const QStringList words { "Sun", "Moon", "Mars" };

    QCOMPARE( renderInBase( 0, decimal ), QString( "0" ) );
    QCOMPARE( renderInBase( 1234, decimal ), QString( "1234" ) );
    QCOMPARE( renderInBase( 255, hex ), QString( "FF" ) );
    QCOMPARE( renderInBase( 255, hex, 4 ), QString( "00FF" ) );
    QCOMPARE( renderInBase( 0, hex, 4 ), QString( "0000" ) );
    QCOMPARE( renderInBase( 65535, hex, 2 ), QString( "FFFF" ) );  // length is a minimum
    QCOMPARE( renderInBase( 4, words ), QString( "MoonMoon" ) );  // 11 in base 3
    QCOMPARE( renderInBase( 0, words ), QString( "Sun" ) );

    try
    {
        renderInBase( 5, QStringList { "0" } );
        QFAIL( "Rendering with one digit should throw" );
    }
    catch ( const ValueError& )
    {
    }
}

void
BootloaderTests::testSerialSuffix()
{
    auto g = efiSuffixGenerator( "derp${SERIAL}" );
    QVERIFY( g );
    QCOMPARE( g->next(), QString( "derp" ) );  // First time, no suffix
    for ( int n = 0; n < 8; ++n )
    {
        g->next();
    }
    // We called next() 10 times in total, starting from 0
    QCOMPARE( g->next(), QString( "derp10" ) );
}

void
BootloaderTests::testRandomAndPhraseSuffix()
{
    // Random, so check the shape
    auto random = efiSuffixGenerator( "derp${RANDOM}" );
    QVERIFY( random );
    QCOMPARE( random->next(), QString( "derp" ) );
    const QRegularExpression hex( "^derp[0-9A-F]{4}$" );
    for ( int n = 0; n < 10; ++n )
    {
        const QString s = random->next();
        QVERIFY2( hex.match( s ).hasMatch(), qPrintable( s ) );
    }

    auto phrase = efiSuffixGenerator( "derp${PHRASE}" );
    QVERIFY( phrase );
    QCOMPARE( phrase->next(), QString( "derp" ) );
    const QRegularExpression words( "^derp(Sun|Moon|Mars|Soyuz|Falcon|Kuaizhou|Gaganyaan)+$" );
    for ( int n = 0; n < 10; ++n )
    {
        const QString s = phrase->next();
        QVERIFY2( words.match( s ).hasMatch(), qPrintable( s ) );
    }
}

void
BootloaderTests::testInvalidSuffix_data()
{
    QTest::addColumn< QString >( "name" );

    QTest::newRow( "no indicator" ) << "derp";
    QTest::newRow( "unknown indicator" ) << "derp${HEX}";
    QTest::newRow( "trailing garbage" ) << "derp${SERIAL}x";
    QTest::newRow( "multiple indicators" ) << "derp${SERIAL}${RANDOM}";
}

void
BootloaderTests::testInvalidSuffix()
{
    QFETCH( QString, name );
    try
    {
        efiSuffixGenerator( name );
        QFAIL( "Shouldn't get a generator" );
    }
    catch ( const ValueError& e )
    {
        QVERIFY( !e.message.isEmpty() );
    }
}

void
BootloaderTests::testChangeEfiSuffix()
{
    QTemporaryDir dir;
    QVERIFY( dir.isValid() );
    const QString d = dir.path();

    QCOMPARE( changeEfiSuffix( d, "claudeinstall-single" ), QString( "claudeinstall-single" ) );
    QCOMPARE( changeEfiSuffix( d, "claudeinstall-serial${SERIAL}" ), QString( "claudeinstall-serial" ) );

    QVERIFY( QDir().mkpath( d + "/claudeinstall-serial" ) );
    QCOMPARE( changeEfiSuffix( d, "claudeinstall-serial${SERIAL}" ), QString( "claudeinstall-serial1" ) );

    QVERIFY( QDir().mkpath( d + "/claudeinstall-serial1" ) );
    QCOMPARE( changeEfiSuffix( d, "claudeinstall-serial${SERIAL}" ), QString( "claudeinstall-serial2" ) );

    // If all attempts are taken, the id is returned unchanged
    for ( int i = 2; i < 10; ++i )
    {
        QVERIFY( QDir().mkpath( d + "/claudeinstall-serial" + QString::number( i ) ) );
    }
    QCOMPARE( changeEfiSuffix( d, "claudeinstall-serial${SERIAL}" ), QString( "claudeinstall-serial${SERIAL}" ) );

    // Bad suffix markers are an error
    try
    {
        changeEfiSuffix( d, "claudeinstall-${HEX}" );
        QFAIL( "An unknown generator should throw" );
    }
    catch ( const ValueError& )
    {
    }
}

void
BootloaderTests::testSanitize()
{
    QCOMPARE( sanitizeFileName( "ClaudeInstall Linux (GRUB)/x" ), QString( "ClaudeInstall_Linux__GRUB_-x" ) );
    QCOMPARE( sanitizeFileName( "plain" ), QString( "plain" ) );
    QCOMPARE( sanitizeFileName( QString() ), QString() );
}

void
BootloaderTests::testFindKernels()
{
    QTemporaryDir root;
    QVERIFY( root.isValid() );
    const QString r = root.path();

    QVERIFY( findKernels( r, "/usr/lib/modules", "^vmlinuz.*" ).isEmpty() );  // nothing there at all

    touch( r + "/usr/lib/modules/6.1.0-arch1/vmlinuz" );
    touch( r + "/usr/lib/modules/6.1.0-arch1/modules.dep" );
    touch( r + "/usr/lib/modules/5.15.0-lts/vmlinuz-lts" );
    touch( r + "/usr/lib/modules/5.15.0-lts/kernel/drivers/vmlinuz-not-a-driver" );
    touch( r + "/boot/vmlinuz" );  // outside of the search path

    const auto kernels = findKernels( r, "/usr/lib/modules", "^vmlinuz.*" );
    QCOMPARE( kernels.size(), 3 );
    // sorted by path
    QCOMPARE( kernels.at( 0 ).path, QString( "usr/lib/modules/5.15.0-lts/kernel/drivers/vmlinuz-not-a-driver" ) );
    QCOMPARE( kernels.at( 0 ).version, QString( "drivers" ) );
    QCOMPARE( kernels.at( 1 ).path, QString( "usr/lib/modules/5.15.0-lts/vmlinuz-lts" ) );
    QCOMPARE( kernels.at( 1 ).version, QString( "5.15.0-lts" ) );
    QCOMPARE( kernels.at( 1 ).type, QString( "default" ) );
    QCOMPARE( kernels.at( 2 ).path, QString( "usr/lib/modules/6.1.0-arch1/vmlinuz" ) );
    QCOMPARE( kernels.at( 2 ).version, QString( "6.1.0-arch1" ) );

    // The default pattern is not anchored, a different search path is possible
    QCOMPARE( findKernels( r, "/boot", "vmlinuz" ).size(), 1 );
    QCOMPARE( findKernels( r, "boot", "vmlinuz" ).size(), 1 );
    QCOMPARE( findKernels( r, "/boot", "^nothing" ).size(), 0 );
}

void
BootloaderTests::testVfatCase()
{
    QTemporaryDir dir;
    QVERIFY( dir.isValid() );
    const QString d = dir.path();

    // Nothing there: where it would go
    QCOMPARE( vfatCorrectCase( d, "EFI" ), d + "/EFI" );

    QVERIFY( QDir().mkpath( d + "/Efi/BOOT" ) );
    QCOMPARE( vfatCorrectCase( d, "EFI" ), d + "/Efi" );
    QCOMPARE( vfatCorrectCase( d + "/Efi", "boot" ), d + "/Efi/BOOT" );
    QCOMPARE( vfatCorrectCase( d + "/Efi", "other" ), d + "/Efi/other" );
}
