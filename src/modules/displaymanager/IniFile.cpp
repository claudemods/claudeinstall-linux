/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "IniFile.h"

#include <QFile>
#include <QRegularExpression>

namespace DisplayManagers
{

IniFile::Section*
IniFile::findSection( const QString& name )
{
    for ( auto& s : m_sections )
    {
        if ( s.first == name )
        {
            return &s;
        }
    }
    return nullptr;
}

const IniFile::Section*
IniFile::findSection( const QString& name ) const
{
    for ( const auto& s : m_sections )
    {
        if ( s.first == name )
        {
            return &s;
        }
    }
    return nullptr;
}

bool
IniFile::hasSection( const QString& section ) const
{
    return findSection( section ) != nullptr;
}

void
IniFile::addSection( const QString& section )
{
    if ( !hasSection( section ) )
    {
        m_sections.append( { section, Options() } );
    }
}

bool
IniFile::hasOption( const QString& section, const QString& key ) const
{
    const Section* s = findSection( section );
    if ( !s )
    {
        return false;
    }
    for ( const auto& o : s->second )
    {
        if ( o.first == key )
        {
            return true;
        }
    }
    return false;
}

QString
IniFile::value( const QString& section, const QString& key ) const
{
    const Section* s = findSection( section );
    if ( s )
    {
        for ( const auto& o : s->second )
        {
            if ( o.first == key )
            {
                return o.second;
            }
        }
    }
    return QString();
}

void
IniFile::set( const QString& section, const QString& key, const QString& value )
{
    Section* s = findSection( section );
    if ( !s )
    {
        return;
    }
    for ( auto& o : s->second )
    {
        if ( o.first == key )
        {
            o.second = value;  // keeps its position, like a dict
            return;
        }
    }
    s->second.append( { key, value } );
}

void
IniFile::removeOption( const QString& section, const QString& key )
{
    Section* s = findSection( section );
    if ( !s )
    {
        return;
    }
    for ( int i = 0; i < s->second.size(); ++i )
    {
        if ( s->second.at( i ).first == key )
        {
            s->second.removeAt( i );
            return;
        }
    }
}

bool
IniFile::parse( const QString& text, QString* error )
{
    Section* current = nullptr;
    QString optionName;  // The option that continuation lines belong to
    int indentLevel = 0;
    int lineNumber = 0;

    auto fail = [ & ]( const QString& what )
    {
        if ( error )
        {
            *error = QStringLiteral( "line %1: %2" ).arg( lineNumber ).arg( what );
        }
        return false;
    };

    for ( const QString& rawLine : text.split( '\n' ) )
    {
        lineNumber++;
        const QString stripped = rawLine.trimmed();

        // Blank lines and full-line comments
        if ( stripped.isEmpty() || stripped.startsWith( '#' ) || stripped.startsWith( ';' ) )
        {
            continue;
        }

        int currentIndent = 0;
        while ( currentIndent < rawLine.size() && rawLine.at( currentIndent ).isSpace() )
        {
            currentIndent++;
        }

        // Continuation line of a value
        if ( current && !optionName.isEmpty() && currentIndent > indentLevel )
        {
            for ( auto& o : current->second )
            {
                if ( o.first == optionName )
                {
                    o.second += '\n' + stripped;
                }
            }
            continue;
        }

        indentLevel = currentIndent;
        if ( stripped.startsWith( '[' ) && stripped.lastIndexOf( ']' ) > 1 )
        {
            const QString header = stripped.mid( 1, stripped.lastIndexOf( ']' ) - 1 );
            addSection( header );
            current = findSection( header );
            optionName.clear();
        }
        else if ( !current )
        {
            return fail( QStringLiteral( "File contains no section headers" ) );
        }
        else
        {
            const int delimiter = stripped.indexOf( QRegularExpression( "[=:]" ) );
            if ( delimiter <= 0 )
            {
                return fail( QStringLiteral( "Not a section or option: %1" ).arg( stripped ) );
            }
            optionName = stripped.left( delimiter ).trimmed();
            if ( optionName.isEmpty() )
            {
                return fail( QStringLiteral( "Option without a name" ) );
            }
            set( current->first, optionName, stripped.mid( delimiter + 1 ).trimmed() );
        }
    }
    return true;
}

bool
IniFile::read( const QString& path, QString* error )
{
    QFile f( path );
    if ( !f.open( QIODevice::ReadOnly ) )
    {
        if ( error )
        {
            *error = f.errorString();
        }
        return false;
    }
    return parse( QString::fromUtf8( f.readAll() ), error );
}

QString
IniFile::toString() const
{
    QString out;
    for ( const auto& section : m_sections )
    {
        out += '[' + section.first + "]\n";
        for ( const auto& o : section.second )
        {
            QString value = o.second;
            out += o.first + '=' + value.replace( '\n', QStringLiteral( "\n\t" ) ) + '\n';
        }
        out += '\n';
    }
    return out;
}

bool
IniFile::write( const QString& path ) const
{
    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
    {
        return false;
    }
    const QByteArray data = toString().toUtf8();
    return f.write( data ) == data.size();
}

}  // namespace DisplayManagers
