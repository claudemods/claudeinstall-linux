/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2014-2019 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2017-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "BootloaderHelpers.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>

#include <algorithm>
#include <utility>

namespace Bootloader
{

QString
renderInBase( quint64 value, const QStringList& baseValues, int length )
{
    if ( baseValues.size() < 2 )
    {
        throw ValueError { QStringLiteral( "Insufficient items for base-N rendering" ) };
    }
    if ( length < 1 )
    {
        length = 1;
    }

    QStringList digits;
    const quint64 base = baseValues.size();
    while ( value > 0 )
    {
        digits.append( baseValues.at( int( value % base ) ) );
        value /= base;
    }
    while ( digits.size() < length )
    {
        digits.append( baseValues.first() );
    }
    std::reverse( digits.begin(), digits.end() );
    return digits.join( QString() );
}

EfiSuffixGenerator::~EfiSuffixGenerator() {}

QString
EfiSuffixGenerator::next()
{
    m_counter++;
    return m_counter > 0 ? m_name + suffix( m_counter ) : m_name;
}

namespace
{
/// Appends a serial number to the given name.
class SerialEfi : public EfiSuffixGenerator
{
public:
    using EfiSuffixGenerator::EfiSuffixGenerator;

protected:
    QString suffix( int counter ) override { return QString::number( counter ); }
};

/// Appends a random 4-digit hex number to the given name.
class RandomEfi : public EfiSuffixGenerator
{
public:
    using EfiSuffixGenerator::EfiSuffixGenerator;

protected:
    QString suffix( int ) override
    {
        const quint64 v = QRandomGenerator::global()->bounded( 65536 );  // 16 bits
        return renderInBase( v,
                             { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F" },
                             4 );
    }
};

/// Appends a random phrase to the given name.
class PhraseEfi : public EfiSuffixGenerator
{
public:
    using EfiSuffixGenerator::EfiSuffixGenerator;

protected:
    QString suffix( int counter ) override
    {
        static const QStringList words { "Sun", "Moon", "Mars", "Soyuz", "Falcon", "Kuaizhou", "Gaganyaan" };

        const int desiredLength = 1 + counter / 5;
        quint64 limit = 1;  // words.size() ** desiredLength
        for ( int i = 0; i < desiredLength; ++i )
        {
            limit *= quint64( words.size() );
        }
        const quint64 v = QRandomGenerator::global()->bounded( limit + 1 );  // inclusive
        return renderInBase( v, words );
    }
};
}  // namespace

std::unique_ptr< EfiSuffixGenerator >
efiSuffixGenerator( const QString& name )
{
    if ( !name.contains( "${" ) )
    {
        throw ValueError { QStringLiteral( "Misplaced call to get_efi_suffix_generator, no ${}" ) };
    }
    if ( !name.endsWith( '}' ) )
    {
        throw ValueError { QStringLiteral( "Misplaced call to get_efi_suffix_generator, no trailing ${}" ) };
    }
    if ( name.count( "${" ) > 1 )
    {
        throw ValueError { QStringLiteral( "EFI ID '%1' contains multiple generators" ).arg( name ) };
    }

    static const QRegularExpression re( QStringLiteral( "^(.*)\\$\\{([^}]*)\\}$" ) );
    const auto m = re.match( name );
    if ( !m.hasMatch() )
    {
        throw ValueError { QStringLiteral( "EFI ID '%1' is malformed" ).arg( name ) };
    }
    const QString prefix = m.captured( 1 );
    const QString generatorName = m.captured( 2 );

    if ( generatorName == "SERIAL" )
    {
        return std::make_unique< SerialEfi >( prefix );
    }
    if ( generatorName == "RANDOM" )
    {
        return std::make_unique< RandomEfi >( prefix );
    }
    if ( generatorName == "PHRASE" )
    {
        return std::make_unique< PhraseEfi >( prefix );
    }
    throw ValueError { QStringLiteral( "EFI suffix '%1' is unknown" ).arg( generatorName ) };
}

QString
changeEfiSuffix( const QString& efiDirectory, const QString& bootloaderId )
{
    if ( !( bootloaderId.endsWith( '}' ) && bootloaderId.contains( "${" ) ) )
    {
        // Just one attempt, which is the id itself whether it is already in use or not
        return bootloaderId;
    }

    // Do 10 attempts with any suffix generator
    auto generator = efiSuffixGenerator( bootloaderId );
    for ( int attempt = 0; attempt < 10; ++attempt )
    {
        const QString candidate = generator->next();
        if ( !QFileInfo::exists( QDir( efiDirectory ).filePath( candidate ) ) )
        {
            return candidate;
        }
    }
    return bootloaderId;
}

QString
sanitizeFileName( const QString& name )
{
    QString s = name;
    s.replace( ' ', '_' ).replace( '/', '-' ).replace( '(', '_' ).replace( ')', '_' );
    return s;
}

QList< Kernel >
findKernels( const QString& installationRootPath, const QString& searchPath, const QString& pattern )
{
    QString relativeSearchPath = searchPath;
    while ( relativeSearchPath.startsWith( '/' ) )
    {
        relativeSearchPath.remove( 0, 1 );
    }
    const QString top = QDir( installationRootPath ).filePath( relativeSearchPath );

    const QRegularExpression re( pattern );
    QStringList matches;
    QDirIterator it( top, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories );
    while ( it.hasNext() )
    {
        const QFileInfo fi( it.next() );
        if ( re.match( fi.fileName() ).hasMatch() )
        {
            matches.append( fi.absoluteFilePath() );
        }
    }
    matches.sort();  // a directory listing has no useful order

    QList< Kernel > kernels;
    const QDir root( installationRootPath );
    for ( const QString& path : std::as_const( matches ) )
    {
        const QFileInfo fi( path );
        kernels.append( { root.relativeFilePath( path ), QStringLiteral( "default" ), fi.dir().dirName() } );
    }
    return kernels;
}

QString
vfatCorrectCase( const QString& parent, const QString& name )
{
    const QDir dir( parent );
    for ( const QString& candidate : dir.entryList( QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System ) )
    {
        if ( name.toLower() == candidate.toLower() )
        {
            return dir.filePath( candidate );
        }
    }
    return dir.filePath( name );
}

}  // namespace Bootloader
