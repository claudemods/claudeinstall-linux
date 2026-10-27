/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: BSD-2-Clause
 *
 */

/*
 * Uses the Transifex API to get a list of enabled languages,
 * and outputs CMake settings for inclusion into CMakeLists.txt.
 *
 * This is the C++ replacement of ci/txstats.py.
 *
 *     txstats [-v] [-n] [-e]
 *
 *  -v, --verbose   also output the actual translation percentages
 *  -n, --bogus     use made-up data, do not query Transifex
 *  -e, --edit      edit CMakeLists.txt (in the current directory) in-place
 *
 * Run it from the top of the source tree. The API token is read from
 * ~/.transifexrc (section [https://app.transifex.com], key password).
 */

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextStream>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct TXError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

/// language name -> fraction translated (0.0 to 1.0)
using Languages = std::map< std::string, double >;

/// The API token out of the user's .transifexrc (this is supposed to be secure).
QString
transifexToken()
{
    QFile f( QDir::home().absoluteFilePath( ".transifexrc" ) );
    if ( !f.open( QFile::ReadOnly | QFile::Text ) )
    {
        return QString();
    }

    // A very small INI reader: QSettings would take the "/" in the section name as a nesting
    bool inSection = false;
    QTextStream in( &f );
    while ( !in.atEnd() )
    {
        const QString line = in.readLine().trimmed();
        if ( line.startsWith( '[' ) && line.endsWith( ']' ) )
        {
            inSection = line.mid( 1, line.length() - 2 ) == QStringLiteral( "https://app.transifex.com" );
        }
        else if ( inSection )
        {
            const int sep = line.indexOf( QRegularExpression( "[=:]" ) );
            if ( sep > 0 && line.left( sep ).trimmed() == QStringLiteral( "password" ) )
            {
                return line.mid( sep + 1 ).trimmed();
            }
        }
    }
    return QString();
}

Languages
getFromTransifex()
{
    const QString token = transifexToken();
    if ( token.isEmpty() )
    {
        throw TXError( "Could not get Transifex API token" );
    }

    const QByteArray base = "https://rest.api.transifex.com/resource_language_stats";
    // ':' and the brackets are percent-encoded, as requests would do
    const QByteArray projectFilter = "filter%5Bproject%5D=o%3Aclaudeinstall%3Ap%3Aclaudeinstall";
    const QByteArray resourceFilter = "filter%5Bresource%5D=o%3Aclaudeinstall%3Ap%3Aclaudeinstall%3Ar%3Aclaudeinstall";
    QNetworkRequest request( QUrl::fromEncoded( base + "?" + projectFilter + "&" + resourceFilter ) );
    request.setRawHeader( "accept", "application/vnd.api+json" );
    request.setRawHeader( "authorization", "Bearer " + token.toUtf8() );
    request.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );

    QNetworkAccessManager manager;
    QNetworkReply* reply = manager.get( request );
    QEventLoop loop;
    QObject::connect( reply, &QNetworkReply::finished, &loop, &QEventLoop::quit );
    loop.exec();

    const int status = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    if ( status != 200 )
    {
        throw TXError( "Could not get Transifex data from API" );
    }

    const auto json = QJsonDocument::fromJson( body );
    Languages languages;
    for ( const QJsonValue& v : json.object().value( "data" ).toArray() )
    {
        const auto d = v.toObject();
        const auto attributes = d.value( "attributes" ).toObject();
        const double translated = attributes.value( "translated_strings" ).toDouble();
        const double total = attributes.value( "total_strings" ).toDouble();
        QString key = d.value( "relationships" ).toObject().value( "language" ).toObject().value( "data" ).toObject().value( "id" ).toString();
        if ( !key.startsWith( "l:" ) || total <= 0 )
        {
            throw TXError( "Unexpected data from Transifex" );
        }
        languages[ key.mid( 2 ).toStdString() ] = translated / total;
    }
    return languages;
}

/// Fake language data, for testing without hitting the Transifex servers all the time
Languages
getBogus()
{
    return { { "sq", 1.00 }, { "ar", 0.44 }, { "as", 0.28 }, { "de", 0.15 }, { "da", 0.04 }, { "ts", 0.82 } };
}

/// Where the output goes
class Outputter
{
public:
    virtual ~Outputter() = default;
    virtual void print( const std::string& s ) = 0;
    /// Called when everything went well
    virtual void commit() {}
};

class PrintOutputter : public Outputter
{
public:
    void print( const std::string& s ) override { std::puts( s.c_str() ); }
};

/// Edits CMakeLists.txt in-place
class EditingOutputter : public Outputter
{
public:
    EditingOutputter()
    {
        QFile f( "CMakeLists.txt" );
        if ( !f.open( QFile::ReadOnly | QFile::Text ) )
        {
            throw TXError( "Cannot read CMakeLists.txt" );
        }
        QStringList lines;
        {
            QTextStream in( &f );
            while ( !in.atEnd() )
            {
                lines << in.readLine();
            }
        }

        int mark = -1;
        QString markText;
        for ( int i = 0; i < lines.size(); ++i )
        {
            if ( lines[ i ].startsWith( "# Total " ) && lines[ i ].endsWith( " languages" ) )
            {
                mark = i;
                markText = lines[ i ];
                break;
            }
        }
        if ( mark < 0 )
        {
            throw TXError( "No CMakeLists.txt lines for TX stats found" );
        }
        m_pre = lines.mid( 0, mark );

        int nextMark = mark + 1;
        for ( int i = mark + 1; i < lines.size(); ++i )
        {
            ++nextMark;
            if ( lines[ i ].startsWith( markText ) )
            {
                break;
            }
        }
        // Catch runaway marks: there should be four set-lines, and the file
        // is supposed to end with some boilerplate. (A formatter may put
        // one language per line, hence the generous limit.)
        if ( nextMark > mark + 150 || nextMark > lines.size() - 4 )
        {
            throw TXError( "Could not find end of TX settings in CMakeLists.txt" );
        }
        m_post = lines.mid( nextMark );
        std::puts( "# Editing CMakeLists.txt in-place" );
    }

    void print( const std::string& s ) override
    {
        m_mid << QString::fromStdString( s );
        if ( !s.empty() && s[ 0 ] == '#' )
        {
            std::puts( s.c_str() );
        }
    }

    void commit() override
    {
        QFile f( "CMakeLists.txt" );
        if ( !f.open( QFile::WriteOnly | QFile::Text | QFile::Truncate ) )
        {
            throw TXError( "Cannot write CMakeLists.txt" );
        }
        QTextStream out( &f );
        for ( const auto& list : { m_pre, m_mid, m_post } )
        {
            for ( const QString& l : list )
            {
                out << l << "\n";
            }
        }
        std::puts( "# CMakeLists.txt updated" );
    }

private:
    QStringList m_pre, m_mid, m_post;
};

/**
 * Outputs all of the languages in @p all that satisfy the translation-percentage
 * filter @p filter, as a CMake set() command with the @p label as part of the
 * variable name. Performs line-wrapping.
 */
void
outputLangs( const std::vector< std::pair< double, std::string > >& all,
             Outputter& outputter,
             const std::string& label,
             const std::function< bool( double ) >& filter )
{
    std::vector< std::string > these;
    for ( const auto& [ s, l ] : all )
    {
        if ( filter( s ) )
        {
            these.push_back( l );
        }
    }
    std::sort( these.begin(), these.end() );

    std::string out = "set( _tx_" + label;
    for ( const auto& l : these )
    {
        out += " " + l;
    }
    out += " )";

    const size_t width = 68;
    std::string prefix;
    const std::string trailer = "  # " + std::to_string( these.size() ) + " languages";  // comment at the end of the CMake line
    while ( out.size() > width - prefix.size() )
    {
        const size_t space = out.substr( 0, width ).rfind( ' ' );
        if ( space == std::string::npos )
        {
            break;  // an unbreakable word; leave it
        }
        outputter.print( prefix + out.substr( 0, space ) );
        out = out.substr( space + 1 );
        prefix = "    ";
    }
    outputter.print( prefix + out + trailer );
}

int
getTxStats( const Languages& languages, Outputter& outputter, bool verbose )
{
    // Some languages go into the "incomplete" list by definition,
    // regardless of their completion status: this can have various reasons.
    //  - Interlingue still is not supported by the minimum Qt version
    const std::vector< std::string > incompleteLanguages { "ie" };

    std::vector< std::pair< double, std::string > > all;
    const std::string markText = "# Total " + std::to_string( languages.size() ) + " languages";
    outputter.print( markText );
    for ( const auto& [ name, fraction ] : languages )
    {
        double stats = fraction;
        // The by-definition-incomplete languages get a percentage lower than
        // zero: they end up at the bottom of the -v output but you can
        // still see the actual percentage.
        if ( std::find( incompleteLanguages.begin(), incompleteLanguages.end(), name ) != incompleteLanguages.end() )
        {
            stats = -stats;
        }
        all.emplace_back( stats, name );
    }

    if ( verbose )
    {
        auto sorted = all;
        std::sort( sorted.begin(), sorted.end(), std::greater<>() );
        for ( const auto& [ s, l ] : sorted )
        {
            char line[ 128 ];
            std::snprintf( line, sizeof( line ), "#  %16s\t%6.2f", l.c_str(), s * 100.0 );
            outputter.print( line );
        }
    }
    outputLangs( all, outputter, "complete", []( double s ) { return s == 1.0; } );
    outputLangs( all, outputter, "good", []( double s ) { return 1.0 > s && s >= 0.75; } );
    outputLangs( all, outputter, "ok", []( double s ) { return 0.75 > s && s >= 0.05; } );
    outputLangs( all, outputter, "incomplete", []( double s ) { return 0.05 > s; } );
    outputter.print( markText );

    // Audit the languages that are in TX, mapped to the files in git
    for ( const auto& [ name, fraction ] : languages )
    {
        Q_UNUSED( fraction )
        if ( !QFileInfo::exists( QStringLiteral( "lang/claudeinstall_%1.ts" ).arg( QString::fromStdString( name ) ) ) )
        {
            std::printf( "# !! Missing translation file for %s\n", name.c_str() );
        }
    }

    // Audit the files that are in git, mapped to TX
    const std::vector< QString > specialCases { "CMakeLists.txt", "txload.cpp", "claudeinstall_i18n.qrc.in" };
    for ( const QString& fileName : QDir( "lang" ).entryList( QDir::Files | QDir::NoDotAndDotDot ) )
    {
        if ( std::find( specialCases.begin(), specialCases.end(), fileName ) != specialCases.end() )
        {
            continue;
        }
        auto audit = [ & ]( int prefixLength, const char* what )
        {
            const std::string key = fileName.mid( prefixLength, fileName.length() - prefixLength - 3 ).toStdString();
            if ( languages.find( key ) == languages.end() && key != "en" )
            {
                std::printf( "# !! Translation file for %s%s not in TX\n", what, key.c_str() );
            }
        };
        if ( fileName.startsWith( "claudeinstall_" ) && fileName.endsWith( ".ts" ) )
        {
            audit( 14, "" );
        }
        else if ( fileName.startsWith( "tz_" ) && fileName.endsWith( ".ts" ) )
        {
            audit( 3, "TZ " );
        }
        else if ( fileName.startsWith( "kb_" ) && fileName.endsWith( ".ts" ) )
        {
            audit( 3, "KB " );
        }
        else
        {
            std::printf( "# !! Weird translation file %s not in TX\n", fileName.toStdString().c_str() );
        }
    }
    return 0;
}

}  // namespace

int
main( int argc, char* argv[] )
{
    QCoreApplication app( argc, argv );

    QCommandLineParser parser;
    parser.setApplicationDescription( "Update Transifex Statistics" );
    parser.addHelpOption();
    QCommandLineOption verbose( QStringList { "v", "verbose" }, "Show statistics" );
    QCommandLineOption bogus( QStringList { "n", "bogus" }, "Use bogus data (do not query Transifex)" );
    QCommandLineOption edit( QStringList { "e", "edit" }, "Edit CMakeLists.txt in-place" );
    parser.addOption( verbose );
    parser.addOption( bogus );
    parser.addOption( edit );
    parser.process( app );

    try
    {
        const Languages languages = parser.isSet( bogus ) ? getBogus() : getFromTransifex();
        std::unique_ptr< Outputter > outputter;
        if ( parser.isSet( edit ) )
        {
            outputter = std::make_unique< EditingOutputter >();
        }
        else
        {
            outputter = std::make_unique< PrintOutputter >();
        }
        const int r = getTxStats( languages, *outputter, parser.isSet( verbose ) );
        if ( r == 0 )
        {
            outputter->commit();
        }
        return r;
    }
    catch ( const TXError& e )
    {
        std::printf( "! %s\n", e.what() );
        return 1;
    }
}
