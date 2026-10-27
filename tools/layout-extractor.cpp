/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: BSD-2-Clause
 *
 */

/*
 * Scrapes the X keyboard layout file and produces translations. This is the
 * C++ replacement of src/modules/keyboard/layout-extractor.py.
 *
 *     layout-extractor [path-to-base.lst]
 *
 * By default it reads /usr/local/share/X11/xkb/rules/base.lst (this is usual
 * on FreeBSD), or else /usr/share/X11/xkb/rules/base.lst (Linux). It writes tables of
 * keyboard model, layout, variant and group names for use in translations to
 * KeyboardData_p.cxxtr in the current directory.
 */

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>

namespace
{

using Set = std::set< std::string >;

void
scrapeFile( std::istream& file, Set& models, Set& layouts, Set& variants, Set& groups )
{
    // These regular expressions match what is in keyboardglobal.cpp
    const std::regex modelRe( "^\\s+(\\S+)\\s+(\\w.*)$" );
    const std::regex layoutRe( "^\\s+(\\S+)\\s+(\\w.*)$" );
    const std::regex variantRe( "^\\s+(\\S+)\\s+(\\S+): (\\w.*)$" );
    const std::regex groupRe( "^\\s+grp:(\\S+)\\s+(\\w.*)$" );

    enum class State
    {
        None,
        Model,
        Layout,
        Variant,
        Group
    };
    State state = State::None;

    std::string line;
    while ( std::getline( file, line ) )
    {
        if ( !line.empty() && line.back() == '\r' )
        {
            line.pop_back();
        }

        // Handle changes in section
        if ( line.rfind( "! model", 0 ) == 0 )
        {
            state = State::Model;
            continue;
        }
        if ( line.rfind( "! layout", 0 ) == 0 )
        {
            state = State::Layout;
            continue;
        }
        if ( line.rfind( "! variant", 0 ) == 0 )
        {
            state = State::Variant;
            continue;
        }
        if ( line.rfind( "! option", 0 ) == 0 )
        {
            state = State::Group;
            continue;
        }
        if ( line.find_first_not_of( " \t" ) == std::string::npos )
        {
            state = State::None;
        }
        // Unchanged from the last blank
        if ( state == State::None )
        {
            continue;
        }

        std::smatch m;
        switch ( state )
        {
        case State::Model:
            if ( std::regex_match( line, m, modelRe ) )
            {
                models.insert( m[ 2 ] );
            }
            break;
        case State::Layout:
            if ( std::regex_match( line, m, layoutRe ) )
            {
                layouts.insert( m[ 2 ] );
            }
            break;
        case State::Variant:
            if ( std::regex_match( line, m, variantRe ) )
            {
                variants.insert( m[ 3 ] );
            }
            break;
        case State::Group:
            // Only the lines about groups; other options are not of interest
            if ( std::regex_match( line, m, groupRe ) )
            {
                groups.insert( m[ 2 ] );
            }
            break;
        case State::None:
            break;
        }
    }
}

/// The name as the contents of a C++ string
std::string
escaped( const std::string& s )
{
    std::string r;
    for ( char c : s )
    {
        if ( c == '"' || c == '\\' )
        {
            r += '\\';
        }
        r += c;
    }
    return r;
}

void
writeSet( std::ostream& file, const std::string& label, const Set& set )
{
    file << "/* This returns a reference to local, which is a terrible idea.\n * Good thing it's not meant to be compiled.\n */\n";
    file << "class " << label << " : public QObject {\nQ_OBJECT\npublic:\n";
    file << "\tconst QStringList& table()\n\t{\n\treturn QStringList {\n";
    for ( const auto& x : set )  // a std::set is sorted
    {
        file << "\t\ttr(\"" << escaped( x ) << "\", \"" << label << "\"),\n";
    }
    file << "\t\tQString()\n\t};\n}\n}\n\n";
}

const char cppHeaderComment[] = R"(/*   GENERATED FILE DO NOT EDIT
*
*  === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
*
* SPDX-FileCopyrightText: no
* SPDX-License-Identifier: CC0-1.0
*
* This file is derived from base.lst in the Xorg distribution
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
    std::string input;
    if ( argc > 1 )
    {
        input = argv[ 1 ];
    }
    else if ( std::filesystem::exists( "/usr/local/share/X11/xkb/rules/base.lst" ) )
    {
        input = "/usr/local/share/X11/xkb/rules/base.lst";
    }
    else
    {
        input = "/usr/share/X11/xkb/rules/base.lst";
    }

    std::ifstream in( input );
    if ( !in )
    {
        std::fprintf( stderr, "Cannot read %s\n", input.c_str() );
        return 1;
    }

    Set models, layouts, variants, groups;
    variants.insert( "Default" );
    groups.insert( "None" );
    scrapeFile( in, models, layouts, variants, groups );

    std::ofstream out( "KeyboardData_p.cxxtr", std::ios::binary );
    if ( !out )
    {
        std::fprintf( stderr, "Cannot write KeyboardData_p.cxxtr\n" );
        return 1;
    }
    out << cppHeaderComment;
    writeSet( out, "kb_models", models );
    writeSet( out, "kb_layouts", layouts );
    writeSet( out, "kb_variants", variants );
    writeSet( out, "kb_groups", groups );
    return 0;
}
