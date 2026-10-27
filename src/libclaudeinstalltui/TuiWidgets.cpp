/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "TuiWidgets.h"

#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace ClaudeInstall
{
namespace Tui
{

bool
isConsole()
{
    static const bool console = []
    {
        const char* term = std::getenv( "TERM" );
        if ( term && std::strcmp( term, "linux" ) == 0 )
        {
            return true;
        }
        const char* vars[] = { "LC_ALL", "LC_CTYPE", "LANG" };
        for ( const char* name : vars )
        {
            const char* value = std::getenv( name );
            if ( value && *value )
            {
                const QString v = QString::fromLocal8Bit( value ).toLower();
                return !( v.contains( QStringLiteral( "utf-8" ) ) || v.contains( QStringLiteral( "utf8" ) ) );
            }
        }
        return false;
    }();
    return console;
}

std::string
symbol( bool ok, bool warning )
{
    if ( isConsole() )
    {
        return ok ? " [ok] " : ( warning ? " [!] " : " [X] " );
    }
    return ok ? " ✓ " : ( warning ? " ! " : " ✗ " );
}

bool
drawableOnConsole( const QString& s )
{
    for ( const QChar c : s )
    {
        if ( c.unicode() > 0x024F )
        {
            return false;
        }
    }
    return true;
}

QString
stripMarkup( const QString& s )
{
    QString t = s;
    t.replace( QRegularExpression( QStringLiteral( "<br\\s*/?>|</li>|</p>|</h[1-6]>" ), QRegularExpression::CaseInsensitiveOption ),
               QStringLiteral( "\n" ) );
    t.replace( QRegularExpression( QStringLiteral( "<li>" ), QRegularExpression::CaseInsensitiveOption ), QStringLiteral( "- " ) );
    t.remove( QRegularExpression( QStringLiteral( "<[^>]*>" ) ) );
    t.replace( QStringLiteral( "&lt;" ), QStringLiteral( "<" ) );
    t.replace( QStringLiteral( "&gt;" ), QStringLiteral( ">" ) );
    t.replace( QStringLiteral( "&quot;" ), QStringLiteral( "\"" ) );
    t.replace( QStringLiteral( "&nbsp;" ), QStringLiteral( " " ) );
    t.replace( QStringLiteral( "&amp;" ), QStringLiteral( "&" ) );
    // No more than one empty line in a row
    t.replace( QRegularExpression( QStringLiteral( "\\n{3,}" ) ), QStringLiteral( "\n\n" ) );
    return t.trimmed();
}

ftxui::Element
paragraphOf( const QString& s )
{
    using namespace ftxui;
    const QStringList lines = s.split( QLatin1Char( '\n' ) );
    if ( lines.size() == 1 )
    {
        return paragraph( str( s ) );
    }
    Elements parts;
    for ( const QString& line : lines )
    {
        parts.push_back( line.isEmpty() ? text( "" ) : paragraph( str( line ) ) );
    }
    return vbox( std::move( parts ) );
}

ftxui::Element
titled( const QString& title, ftxui::Element body )
{
    using namespace ftxui;
    return vbox( { text( str( title ) ) | bold, separator(), std::move( body ) } );
}

ftxui::Element
problem( const QString& message, bool warning )
{
    using namespace ftxui;
    if ( message.isEmpty() )
    {
        return emptyElement();
    }
    return paragraph( str( message ) ) | color( warning ? Color::Yellow : Color::Red );
}

ftxui::Component
field( const QString& label,
       std::string* value,
       bool password,
       const std::function< void() >& onChange,
       const std::function< QString() >& status )
{
    using namespace ftxui;

    InputOption option = InputOption::Default();
    option.password = password;
    option.multiline = false;
    option.on_change = onChange;
    Component input = Input( value, option );

    const std::string name = str( label );
    return Renderer( input,
                     [ input, name, status ]
                     {
                         Element line = hbox( { text( name + ": " ) | size( WIDTH, GREATER_THAN, 18 ),
                                                input->Render() | flex } );
                         if ( status )
                         {
                             const QString message = status();
                             if ( !message.isEmpty() )
                             {
                                 return vbox( { line, hbox( { text( "  " ), problem( message ) } ) } );
                             }
                         }
                         return line;
                     } );
}

ftxui::Component
acceptsSpace( ftxui::Component component )
{
    using namespace ftxui;
    // A menu only reacts to Enter; the space bar chooses too
    return CatchEvent( component,
                       [ component ]( Event event )
                       { return event == Event::Character( ' ' ) && component->OnEvent( Event::Return ); } );
}

ftxui::Component
check( const QString& label, bool* value, const std::function< void() >& onChange )
{
    ftxui::CheckboxOption option = ftxui::CheckboxOption::Simple();
    option.on_change = onChange;
    return ftxui::Checkbox( str( label ), value, option );
}

ftxui::Component
picker( const QString& title,
        std::vector< std::string >* entries,
        int* selected,
        const std::function< void() >& onChange,
        const std::function< void() >& onEnter )
{
    using namespace ftxui;

    MenuOption option = MenuOption::Vertical();
    option.on_change = onChange;
    // Enter chooses the highlighted entry and, unless told otherwise, moves on to the next field
    option.on_enter = [ onEnter ]
    {
        if ( onEnter )
        {
            onEnter();
        }
        else if ( auto* screen = ScreenInteractive::Active() )
        {
            screen->PostEvent( Event::Tab );
        }
    };
    Component menu = Menu( entries, selected, option );
    menu = acceptsSpace( menu );

    const std::string heading = str( title );
    return Renderer( menu,
                     [ menu, heading ]
                     {
                         const bool focused = menu->Focused();
                         Element head = text( heading ) | bold;
                         Element list = menu->Render() | vscroll_indicator | frame | flex;
                         Element box = vbox( { head, separator(), list } ) | border;
                         return focused ? ( box | color( Color::Cyan ) ) : box;
                     } );
}

bool
fillEntries( std::vector< std::string >* entries, const QAbstractItemModel* model, int role )
{
    std::vector< std::string > fresh;
    const int count = model->rowCount( QModelIndex() );
    fresh.reserve( size_t( count ) );
    for ( int i = 0; i < count; ++i )
    {
        fresh.push_back( str( model->data( model->index( i, 0 ), role ).toString() ) );
    }
    if ( fresh == *entries )
    {
        return false;
    }
    *entries = std::move( fresh );
    return true;
}

ftxui::Component
scrolled( const std::function< ftxui::Element() >& content )
{
    using namespace ftxui;
    auto position = std::make_shared< float >( 0.0f );

    Component view = Renderer( [ content, position ]
                               { return content() | focusPositionRelative( 0.0f, *position ) | yframe | flex; } );
    return CatchEvent( view,
                       [ position ]( Event event )
                       {
                           float delta = 0.0f;
                           if ( event == Event::ArrowUp )
                           {
                               delta = -0.05f;
                           }
                           else if ( event == Event::ArrowDown )
                           {
                               delta = 0.05f;
                           }
                           else if ( event == Event::PageUp )
                           {
                               delta = -0.4f;
                           }
                           else if ( event == Event::PageDown )
                           {
                               delta = 0.4f;
                           }
                           else
                           {
                               return false;
                           }
                           *position = std::clamp( *position + delta, 0.0f, 1.0f );
                           return true;
                       } );
}

}  // namespace Tui
}  // namespace ClaudeInstall
