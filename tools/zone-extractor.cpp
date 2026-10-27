/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: BSD-2-Clause
 *
 */

/*
 * Scrapes some data out of zoneinfo/zone.tab. This is the C++ replacement of
 * src/libclaudeinstall/locale/zone-extractor.py.
 *
 *     zone-extractor [path-to-zone.tab]
 *
 * By default it reads /usr/share/zoneinfo/zone.tab (this is usual on FreeBSD
 * and Linux). It writes tables of zone names for use in translations to
 * ZoneData_p.cpp in the current directory.
 */

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{

std::string
strip( const std::string& s )
{
    const char* ws = " \t\r\n\f\v";
    const auto a = s.find_first_not_of( ws );
    if ( a == std::string::npos )
    {
        return {};
    }
    return s.substr( a, s.find_last_not_of( ws ) - a + 1 );
}

/// Reads the regions and zones out of the file; returns @c false if a zone is listed twice
bool
scrapeFile( std::istream& file, std::set< std::string >& regions, std::set< std::string >& zones )
{
    std::string line;
    while ( std::getline( file, line ) )
    {
        if ( !line.empty() && line[ 0 ] == '#' )
        {
            continue;
        }

        // Split at tabs
        std::vector< std::string > parts;
        std::stringstream ss( line );
        std::string part;
        while ( std::getline( ss, part, '\t' ) )
        {
            parts.push_back( part );
        }
        if ( parts.size() < 3 )
        {
            continue;
        }

        const std::string& zoneId = parts[ 2 ];
        const auto slash = zoneId.find( '/' );
        if ( slash == std::string::npos )
        {
            continue;
        }

        const std::string region = zoneId.substr( 0, slash );
        std::string zone = strip( zoneId.substr( slash + 1 ) );
        for ( char& c : zone )
        {
            if ( c == '_' )
            {
                c = ' ';
            }
        }

        regions.insert( region );
        if ( !zones.insert( zone ).second )
        {
            std::fprintf( stderr, "The zone '%s' is listed twice\n", zone.c_str() );
            return false;
        }
    }
    return true;
}

void
writeSet( std::ostream& file, const std::string& label, const std::set< std::string >& set )
{
    file << "/* This returns a reference to local, which is a terrible idea.\n * Good thing it's not meant to be compiled.\n */\n";
    file << "static const QStringList& " << label << "_table()\n{\n\treturn QStringList {\n";
    for ( const auto& x : set )  // a std::set is sorted
    {
        file << "\t\tQObject::tr(\"" << x << "\", \"" << label << "\"),\n";
    }
    file << "\t\tQString()\n\t};\n}\n\n";
}

const char cppHeaderComment[] = R"(/*   GENERATED FILE DO NOT EDIT
*
*  === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
*
* SPDX-FileCopyrightText: 2009 Arthur David Olson
* SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
* SPDX-License-Identifier: CC0-1.0
*
* This file is derived from zone.tab, which has its own copyright statement:
*
* This file is in the public domain, so clarified as of
* 2009-05-17 by Arthur David Olson.
*
* From Paul Eggert (2018-06-27):
* This file is intended as a backward-compatibility aid for older programs.
* New programs should use zone1970.tab.  This file is like zone1970.tab (see
* zone1970.tab's comments), but with the following additional restrictions:
*
* 1.  This file contains only ASCII characters.
* 2.  The first data column contains exactly one country code.
*
*/

/** THIS FILE EXISTS ONLY FOR TRANSLATIONS PURPOSES **/

// *INDENT-OFF*
// clang-format off
)";

}  // namespace

int
main( int argc, char* argv[] )
{
    const std::string input = argc > 1 ? argv[ 1 ] : "/usr/share/zoneinfo/zone.tab";
    std::ifstream in( input );
    if ( !in )
    {
        std::fprintf( stderr, "Cannot read %s\n", input.c_str() );
        return 1;
    }

    std::set< std::string > regions, zones;
    if ( !scrapeFile( in, regions, zones ) )
    {
        return 1;
    }

    std::ofstream out( "ZoneData_p.cpp", std::ios::binary );
    if ( !out )
    {
        std::fprintf( stderr, "Cannot write ZoneData_p.cpp\n" );
        return 1;
    }
    out << cppHeaderComment;
    writeSet( out, "tz_regions", regions );
    writeSet( out, "tz_names", zones );
    return 0;
}
