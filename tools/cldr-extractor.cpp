/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: BSD-2-Clause
 *
 */

/*
 * Scrapes some data out of ICU CLDR supplemental data. This is the C++
 * replacement of src/libclaudeinstall/locale/cldr-extractor.py.
 *
 * To use this tool, you must have downloaded the CLDR data, e.g.
 * http://unicode.org/Public/cldr/35.1/, and extracted the zip file.
 * Run the tool from **inside** the common/ directory that is created
 * (it reads supplemental/likelySubtags.xml, and writes CountryData_p.cpp
 * to the current directory).
 *
 * The tool prints C++ code that compiles.
 *
 * To avoid having to cross-reference multiple XML files, the tool
 * cheats: it reads the comments as well to get names. So it looks for
 * pairs of lines like this:
 *
 *    <likelySubtag from="und_BQ" to="pap_Latn_BQ"/>
 *    <!--{ ?; ?; Caribbean Netherlands } => { Papiamento; Latin; Caribbean Netherlands }-->
 *
 * It extracts the 2-character country code "BQ" from the sub-tag, and
 * parses the comment to get a language and country name (instead of looking up
 * "pap" and "BQ" in other tables). This may be considered a hack.
 *
 * A large collection of exceptions can be found in the two mapper tables,
 * which massage the CLDR names to Qt enum values.
 */

#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace
{

// These are languages listed in CLDR that don't match
// the enum-values in QLocale::Language.
const std::map< std::string, std::string > languageMapper {
    { "?", "AnyLanguage" },
    { "Bangla", "Bengali" },
    { "Kalaallisut", "Greenlandic" },
    { "Haitian Creole", "Haitian" },
    { "Kyrgyz", "Kirghiz" },
    { "Norwegian Bokmål", "NorwegianBokmal" },
    { "Tokelau", "TokelauLanguage" },
    { "Tuvalu", "TuvaluLanguage" },
};

const std::map< std::string, std::string > countryMapper {
    { "Åland Islands", "AlandIslands" },
    { "St. Barthélemy", "SaintBarthelemy" },
    { "Côte d’Ivoire", "IvoryCoast" },
    { "Curaçao", "CuraSao" },
    { "Réunion", "Reunion" },
    { "São Tomé & Príncipe", "SaoTomeAndPrincipe" },
    { "Bosnia & Herzegovina", "BosniaAndHerzegowina" },
    { "Czechia", "CzechRepublic" },
    { "St. Pierre & Miquelon", "SaintPierreAndMiquelon" },
    { "Vatican City", "VaticanCityState" },
    { "South Georgia & South Sandwich Islands", "SouthGeorgiaAndTheSouthSandwichIslands" },
    { "Timor-Leste", "EastTimor" },
    { "Wallis & Futuna", "WallisAndFutunaIslands" },
    { "Myanmar (Burma)", "Myanmar" },
    { "Svalbard & Jan Mayen", "SvalbardAndJanMayenIslands" },
    { "St. Martin", "SaintMartin" },
    { "North Macedonia", "Macedonia" },
    { "Hong Kong SAR China", "HongKong" },
    { "Macao SAR China", "Macau" },
    { "Eurozone", "AnyCountry" },  // Not likely for GeoIP
    { "Caribbean Netherlands", "Bonaire" },  // Bonaire, Saba, St.Eustatius
};

struct CountryData
{
    std::string code;  // two letters; empty for the terminating entry
    std::string language = "AnyLanguage";
    std::string country = "AnyCountry";

    /// Must match the output format of the declaration below
    std::string str() const
    {
        const std::string c0 = code.empty() ? "0" : std::string( "'" ) + code[ 0 ] + "'";
        const std::string c1 = code.empty() ? "0" : std::string( "'" ) + code[ 1 ] + "'";
        return "{ QLocale::Language::" + language + ", QLocale::Country::" + country + ", " + c0 + ", " + c1 + " },";
    }
};

const char cppClassName[] = "CountryData";  // Must match the type name in the declaration
const char cppDeclaration[] = R"(
struct CountryData
{
    QLocale::Language l;
    QLocale::Country c;
    char cc1;
    char cc2;
};
)";

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

void
replaceAll( std::string& s, const std::string& from, const std::string& to )
{
    for ( size_t p = s.find( from ); p != std::string::npos; p = s.find( from, p + to.size() ) )
    {
        s.replace( p, from.size(), to );
    }
}

/**
 * Given two lines @p l1 and @p l2 which are the <likelySubtag> element-line
 * and the comment-line underneath it, return a CountryData for them,
 * or nothing if the two lines are not relevant (e.g. not the right subtag from,
 * or 3-letter country codes).
 */
std::optional< CountryData >
extricateSubtags( const std::string& l1, const std::string& l2 )
{
    if ( l1.find( "from=\"und_" ) == std::string::npos )
    {
        return std::nullopt;
    }
    if ( l2.find( "{ ?; ?;" ) == std::string::npos )
    {
        return std::nullopt;
    }

    // This is extremely crude "parsing" which chops up the string
    // by delimiter and then extracts some substring.
    const auto und = l1.find( "und_" );
    const std::string afterUnd = l1.substr( und + 4 );
    const auto quote = afterUnd.find( '"' );
    if ( quote == std::string::npos )
    {
        return std::nullopt;
    }
    const std::string code = afterUnd.substr( 0, quote );
    if ( code.size() != 2 )
    {
        return std::nullopt;
    }

    // The third piece of the line, split at ';'
    size_t start = 0;
    for ( int i = 0; i < 2; ++i )
    {
        start = l2.find( ';', start );
        if ( start == std::string::npos )
        {
            return std::nullopt;
        }
        ++start;
    }
    const auto end = l2.find( ';', start );
    const std::string part = l2.substr( start, end == std::string::npos ? std::string::npos : end - start );

    const auto openBrace = part.find( '{' );
    const auto closeBrace = part.find( '}' );
    if ( openBrace == std::string::npos || closeBrace == std::string::npos || closeBrace < 1 )
    {
        return std::nullopt;
    }
    std::string language = strip( part.substr( openBrace + 1 ) );
    std::string country = strip( part.substr( 0, closeBrace - 1 ) );

    // Handle mapped cases
    if ( auto it = languageMapper.find( language ); it != languageMapper.end() )
    {
        language = it->second;
    }
    replaceAll( language, " ", "" );

    // Handle mapped cases and then do a bunch of standard replacements.
    if ( auto it = countryMapper.find( country ); it != countryMapper.end() )
    {
        country = it->second;
    }
    replaceAll( country, " ", "" );
    replaceAll( country, "-", "" );
    replaceAll( country, ".", "" );
    replaceAll( country, "&", "And" );

    return CountryData { code, language, country };
}

/// Reads the list of CountryData objects from the likelySubtags file.
bool
readSubtagsFile( std::vector< CountryData >& data )
{
    std::ifstream f( "supplemental/likelySubtags.xml" );
    if ( !f )
    {
        std::fprintf( stderr, "Cannot open supplemental/likelySubtags.xml (run this from inside common/)\n" );
        return false;
    }

    std::string l1;
    while ( std::getline( f, l1 ) )
    {
        if ( l1.find( "<likelySubtag from=\"und_" ) == std::string::npos )
        {
            continue;
        }
        std::string l2;
        std::getline( f, l2 );
        if ( l2.find( "<!--" ) == std::string::npos )
        {
            std::fprintf( stderr, "Expected a comment after: %s\n", l1.c_str() );
            return false;
        }
        if ( auto d = extricateSubtags( l1, l2 ) )
        {
            data.push_back( *d );
        }
    }
    data.push_back( CountryData() );  // the terminating entry
    return true;
}

const char cppHeaderComment[] = R"(/*   GENERATED FILE DO NOT EDIT
*
*  === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
*
* SPDX-FileCopyrightText: 1991-2019 Unicode, Inc.
* SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
* SPDX-License-Identifier: CC0
*
* This file is derived from CLDR data from Unicode, Inc. Applicable terms
* are listed at http://unicode.org/copyright.html , of which the most
* important are:
*
* A. Unicode Copyright
*    1. Copyright © 1991-2019 Unicode, Inc. All rights reserved.
* B. Definitions
*    Unicode Data Files ("DATA FILES") include all data files under the directories:
*    https://www.unicode.org/Public/
* C. Terms of Use
*    1. Certain documents and files on this website contain a legend indicating
*       that "Modification is permitted." Any person is hereby authorized,
*       without fee, to modify such documents and files to create derivative
*       works conforming to the Unicode® Standard, subject to Terms and
*       Conditions herein.
*    2. Any person is hereby authorized, without fee, to view, use, reproduce,
*       and distribute all documents and files, subject to the Terms and
*       Conditions herein.
*/

// BEGIN Generated from CLDR data
// *INDENT-OFF*
// clang-format off

)";

const char cppFooterComment[] = R"(
// END Generated from CLDR data
)";

/// Given a class name (e.g. CountryData) return an identifier for the data table for that class (country_data).
std::string
makeIdentifier( const std::string& className )
{
    std::string id( 1, char( std::tolower( static_cast< unsigned char >( className[ 0 ] ) ) ) );
    for ( size_t i = 1; i < className.size(); ++i )
    {
        const char c = className[ i ];
        if ( std::isupper( static_cast< unsigned char >( c ) ) )
        {
            id += '_';
            id += char( std::tolower( static_cast< unsigned char >( c ) ) );
        }
        else
        {
            id += c;
        }
    }
    return id;
}

}  // namespace

int
main()
{
    std::vector< CountryData > data;
    if ( !readSubtagsFile( data ) )
    {
        return 1;
    }

    const std::string cls = cppClassName;
    const std::string identifier = makeIdentifier( cls );
    std::ofstream f( cls + "_p.cpp", std::ios::binary );
    if ( !f )
    {
        std::fprintf( stderr, "Cannot write %s_p.cpp\n", cls.c_str() );
        return 1;
    }
    f << cppHeaderComment << cppDeclaration;
    f << "\nstatic constexpr int const " << identifier << "_size = " << data.size() << ";\n";
    f << "\nstatic const " << cls << " " << identifier << "_table[] = {\n";
    for ( const auto& d : data )
    {
        f << d.str() << "\n";
    }
    f << "};\n\n";
    f << "static_assert( (sizeof(" << identifier << "_table) / sizeof(" << cls << ")) == " << identifier
      << "_size, \"Table size mismatch for " << cls << "\" );\n\n";
    f << cppFooterComment;
    return 0;
}
