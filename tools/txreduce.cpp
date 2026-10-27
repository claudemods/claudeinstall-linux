/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: BSD-2-Clause
 *
 */

/*
 * Reduce a translation file -- generally, a Timezone translation -- by
 * dropping untranslated strings. An untranslated string is one that
 * has an empty translation **and** is marked unfinished.
 *
 * This is mostly useful to cut down the size of the source file:
 * far and away most of the zones are not translated, and it's just a
 * handful of places that get special treatment.
 *
 * This is the C++ replacement of ci/txreduce.py.
 *
 *     txreduce <file.ts>
 *
 * The reduced file is written to standard output. If the file has a
 * translation with an unknown type, a warning is printed and nothing else
 * is (the file is left for a human to look at).
 *
 * Unlike the script it replaces, a translation that is unfinished **and has
 * text** is kept: the script dropped it, which contradicts what it says it does.
 */

#include <QByteArray>
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <cstdio>
#include <vector>

namespace
{

struct Analysis
{
    std::vector< bool > drop;  ///< for each <message>, in order: is it to be dropped?
    bool valid = true;  ///< false if there was a translation of unknown type
};

Analysis
analyse( const QByteArray& data )
{
    Analysis result;
    QXmlStreamReader xml( data );

    bool inMessage = false;
    bool dropThis = false;
    while ( !xml.atEnd() )
    {
        xml.readNext();
        if ( xml.isStartElement() && xml.name() == QLatin1String( "message" ) )
        {
            inMessage = true;
            dropThis = false;
        }
        else if ( inMessage && xml.isStartElement() && xml.name() == QLatin1String( "translation" ) )
        {
            const auto attributes = xml.attributes();
            if ( attributes.hasAttribute( "type" ) )
            {
                const QString type = attributes.value( "type" ).toString();
                if ( type == QLatin1String( "unfinished" ) )
                {
                    // Drop it if there is no text in it at all (also not in plural forms)
                    QString text;
                    int depth = 1;
                    while ( depth > 0 && !xml.atEnd() )
                    {
                        xml.readNext();
                        if ( xml.isStartElement() )
                        {
                            ++depth;
                        }
                        else if ( xml.isEndElement() )
                        {
                            --depth;
                        }
                        else if ( xml.isCharacters() )
                        {
                            text += xml.text();
                        }
                    }
                    dropThis = text.isEmpty();
                }
                else
                {
                    std::printf( "WARNING ''%s'' unknown type\n", type.toStdString().c_str() );
                    result.valid = false;
                }
            }
        }
        else if ( inMessage && xml.isEndElement() && xml.name() == QLatin1String( "message" ) )
        {
            result.drop.push_back( dropThis );
            inMessage = false;
        }
    }
    return result;
}

}  // namespace

int
main( int argc, char* argv[] )
{
    if ( argc != 2 )
    {
        std::fprintf( stderr, "Usage: txreduce <file.ts>\n" );
        return 2;
    }

    QFile file( argv[ 1 ] );
    if ( !file.open( QFile::ReadOnly ) )
    {
        std::fprintf( stderr, "Cannot read '%s'\n", argv[ 1 ] );
        return 3;
    }
    const QByteArray data = file.readAll();

    const Analysis analysis = analyse( data );
    if ( !analysis.valid )
    {
        return 0;
    }

    QByteArray out;
    QXmlStreamWriter writer( &out );
    writer.setAutoFormatting( false );
    QXmlStreamReader xml( data );
    size_t message = 0;
    while ( !xml.atEnd() )
    {
        xml.readNext();
        if ( xml.hasError() )
        {
            std::fprintf( stderr, "XML error in '%s': %s\n", argv[ 1 ], xml.errorString().toStdString().c_str() );
            return 4;
        }
        if ( xml.isStartElement() && xml.name() == QLatin1String( "message" ) )
        {
            const bool drop = message < analysis.drop.size() && analysis.drop[ message ];
            ++message;
            if ( drop )
            {
                xml.skipCurrentElement();
                continue;
            }
        }
        writer.writeCurrentToken( xml );
    }

    // Dropping a message leaves its line blank
    for ( const QByteArray& line : out.split( '\n' ) )
    {
        if ( !line.trimmed().isEmpty() )
        {
            std::fwrite( line.constData(), 1, size_t( line.size() ), stdout );
            std::fputc( '\n', stdout );
        }
    }
    return 0;
}
