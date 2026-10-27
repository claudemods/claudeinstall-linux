/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "TomlFile.h"

#include "compat/Variant.h"

#include <QFile>
#include <QRegularExpression>

namespace DisplayManagers
{
namespace Toml
{
namespace
{
struct ParseError
{
    QString message;
};

[[noreturn]] void
fail( const QString& message )
{
    throw ParseError { message };
}

bool
isBareKeyChar( QChar c )
{
    return ( c.isLetterOrNumber() && c.unicode() < 128 ) || c == '_' || c == '-';
}

/** @brief A recursive-descent parser over a string (a logical document) */
class Parser
{
public:
    explicit Parser( const QString& text )
        : m_text( text )
    {
    }

    QVariantMap parseDocument()
    {
        QVariantMap root;
        QStringList currentPath;

        skipBlank();
        while ( !atEnd() )
        {
            if ( peek() == '[' )
            {
                if ( m_text.mid( m_pos, 2 ) == "[[" )
                {
                    fail( QStringLiteral( "arrays of tables are not supported" ) );
                }
                advance();
                currentPath = parseKeyPath( ']' );
                expect( ']' );
                ensureTable( root, currentPath );
            }
            else
            {
                const QStringList keyPath = parseKeyPath( '=' );
                expect( '=' );
                skipSpaces();
                const QVariant value = parseValue();
                assign( root, currentPath + keyPath, value );
            }
            skipSpaces();
            if ( !atEnd() && peek() != '\n' )
            {
                fail( QStringLiteral( "unexpected text after a value" ) );
            }
            skipBlank();
        }
        return root;
    }

private:
    bool atEnd() const { return m_pos >= m_text.size(); }
    QChar peek() const { return atEnd() ? QChar() : m_text.at( m_pos ); }
    void advance() { m_pos++; }

    void expect( QChar c )
    {
        skipSpaces();
        if ( peek() != c )
        {
            fail( QStringLiteral( "expected '%1'" ).arg( c ) );
        }
        advance();
    }

    /// Skips spaces and tabs, and a comment up to the end of the line
    void skipSpaces()
    {
        while ( !atEnd() && ( peek() == ' ' || peek() == '\t' || peek() == '\r' ) )
        {
            advance();
        }
        if ( peek() == '#' )
        {
            while ( !atEnd() && peek() != '\n' )
            {
                advance();
            }
        }
    }

    /// Skips everything that is blank: spaces, newlines and comments
    void skipBlank()
    {
        for ( ;; )
        {
            skipSpaces();
            if ( peek() == '\n' )
            {
                advance();
            }
            else
            {
                return;
            }
        }
    }

    QString parseKeyPart()
    {
        skipSpaces();
        if ( peek() == '"' )
        {
            return parseBasicString();
        }
        if ( peek() == '\'' )
        {
            return parseLiteralString();
        }
        QString key;
        while ( !atEnd() && isBareKeyChar( peek() ) )
        {
            key += peek();
            advance();
        }
        if ( key.isEmpty() )
        {
            fail( QStringLiteral( "expected a key" ) );
        }
        return key;
    }

    QStringList parseKeyPath( QChar terminator )
    {
        QStringList path;
        for ( ;; )
        {
            path << parseKeyPart();
            skipSpaces();
            if ( peek() == '.' )
            {
                advance();
            }
            else if ( peek() == terminator )
            {
                return path;
            }
            else
            {
                fail( QStringLiteral( "malformed key" ) );
            }
        }
    }

    QString parseBasicString()
    {
        if ( m_text.mid( m_pos, 3 ) == "\"\"\"" )
        {
            fail( QStringLiteral( "multi-line strings are not supported" ) );
        }
        advance();  // opening "
        QString s;
        for ( ;; )
        {
            if ( atEnd() || peek() == '\n' )
            {
                fail( QStringLiteral( "unterminated string" ) );
            }
            const QChar c = peek();
            advance();
            if ( c == '"' )
            {
                return s;
            }
            if ( c != '\\' )
            {
                s += c;
                continue;
            }
            const QChar e = peek();
            advance();
            switch ( e.unicode() )
            {
            case 'b':
                s += '\b';
                break;
            case 't':
                s += '\t';
                break;
            case 'n':
                s += '\n';
                break;
            case 'f':
                s += '\f';
                break;
            case 'r':
                s += '\r';
                break;
            case '"':
                s += '"';
                break;
            case '\\':
                s += '\\';
                break;
            case 'u':
            case 'U':
            {
                const int digits = e == 'u' ? 4 : 8;
                bool ok = false;
                const uint code = m_text.mid( m_pos, digits ).toUInt( &ok, 16 );
                if ( !ok || m_text.mid( m_pos, digits ).size() != digits )
                {
                    fail( QStringLiteral( "bad unicode escape" ) );
                }
                m_pos += digits;
                s += QString::fromUcs4( reinterpret_cast< const char32_t* >( &code ), 1 );
                break;
            }
            default:
                fail( QStringLiteral( "bad escape sequence" ) );
            }
        }
    }

    QString parseLiteralString()
    {
        if ( m_text.mid( m_pos, 3 ) == "'''" )
        {
            fail( QStringLiteral( "multi-line strings are not supported" ) );
        }
        advance();  // opening '
        QString s;
        while ( !atEnd() && peek() != '\'' && peek() != '\n' )
        {
            s += peek();
            advance();
        }
        if ( peek() != '\'' )
        {
            fail( QStringLiteral( "unterminated string" ) );
        }
        advance();
        return s;
    }

    QVariant parseValue()
    {
        const QChar c = peek();
        if ( c == '"' )
        {
            return parseBasicString();
        }
        if ( c == '\'' )
        {
            return parseLiteralString();
        }
        if ( c == '[' )
        {
            return parseArray();
        }
        if ( c == '{' )
        {
            return parseInlineTable();
        }

        // Bare value: boolean or number
        QString token;
        while ( !atEnd() && !QString( " \t\r\n,]}#" ).contains( peek() ) )
        {
            token += peek();
            advance();
        }
        if ( token == "true" )
        {
            return true;
        }
        if ( token == "false" )
        {
            return false;
        }
        QString number = token;
        number.remove( '_' );
        bool ok = false;
        if ( number.startsWith( "0x" ) )
        {
            const qlonglong n = number.mid( 2 ).toLongLong( &ok, 16 );
            if ( ok )
            {
                return n;
            }
        }
        const qlonglong n = number.toLongLong( &ok );
        if ( ok )
        {
            return n;
        }
        // Dates and times contain ':' or '-' in the middle, which the double parse rejects too
        const double d = number.toDouble( &ok );
        if ( ok )
        {
            return d;
        }
        fail( QStringLiteral( "unsupported value '%1'" ).arg( token ) );
    }

    QVariantList parseArray()
    {
        advance();  // [
        QVariantList list;
        for ( ;; )
        {
            skipBlank();
            if ( peek() == ']' )
            {
                advance();
                return list;
            }
            list.append( parseValue() );
            skipBlank();
            if ( peek() == ',' )
            {
                advance();
            }
            else if ( peek() != ']' )
            {
                fail( QStringLiteral( "malformed array" ) );
            }
        }
    }

    QVariantMap parseInlineTable()
    {
        advance();  // {
        QVariantMap table;
        skipSpaces();
        if ( peek() == '}' )
        {
            advance();
            return table;
        }
        for ( ;; )
        {
            const QStringList keyPath = parseKeyPath( '=' );
            expect( '=' );
            skipSpaces();
            assign( table, keyPath, parseValue() );
            skipSpaces();
            if ( peek() == ',' )
            {
                advance();
            }
            else if ( peek() == '}' )
            {
                advance();
                return table;
            }
            else
            {
                fail( QStringLiteral( "malformed inline table" ) );
            }
        }
    }

    /// Makes sure that the nested table at @p path exists
    static void ensureTable( QVariantMap& root, const QStringList& path )
    {
        assignTable( root, path, 0 );
    }

    static void assignTable( QVariantMap& table, const QStringList& path, int index )
    {
        if ( index >= path.size() )
        {
            return;
        }
        const QString& key = path.at( index );
        if ( table.contains( key ) && ClaudeInstall::typeOf( table.value( key ) ) != ClaudeInstall::MapVariantType )
        {
            fail( QStringLiteral( "'%1' is not a table" ).arg( key ) );
        }
        QVariantMap sub = table.value( key ).toMap();
        assignTable( sub, path, index + 1 );
        table.insert( key, sub );
    }

    static void assign( QVariantMap& table, const QStringList& path, const QVariant& value )
    {
        assignValue( table, path, 0, value );
    }

    static void assignValue( QVariantMap& table, const QStringList& path, int index, const QVariant& value )
    {
        const QString& key = path.at( index );
        if ( index == path.size() - 1 )
        {
            if ( table.contains( key ) )
            {
                fail( QStringLiteral( "duplicate key '%1'" ).arg( key ) );
            }
            table.insert( key, value );
            return;
        }
        if ( table.contains( key ) && ClaudeInstall::typeOf( table.value( key ) ) != ClaudeInstall::MapVariantType )
        {
            fail( QStringLiteral( "'%1' is not a table" ).arg( key ) );
        }
        QVariantMap sub = table.value( key ).toMap();
        assignValue( sub, path, index + 1, value );
        table.insert( key, sub );
    }

    QString m_text;
    int m_pos = 0;
};

QString
formatString( const QString& s )
{
    QString out = QStringLiteral( "\"" );
    for ( const QChar c : s )
    {
        switch ( c.unicode() )
        {
        case '"':
            out += QStringLiteral( "\\\"" );
            break;
        case '\\':
            out += QStringLiteral( "\\\\" );
            break;
        case '\n':
            out += QStringLiteral( "\\n" );
            break;
        case '\t':
            out += QStringLiteral( "\\t" );
            break;
        case '\r':
            out += QStringLiteral( "\\r" );
            break;
        case '\b':
            out += QStringLiteral( "\\b" );
            break;
        case '\f':
            out += QStringLiteral( "\\f" );
            break;
        default:
            if ( c.unicode() < 0x20 || c.unicode() == 0x7f )
            {
                out += QStringLiteral( "\\u%1" ).arg( c.unicode(), 4, 16, QChar( '0' ) );
            }
            else
            {
                out += c;
            }
        }
    }
    return out + '"';
}

QString
formatKey( const QString& key )
{
    if ( !key.isEmpty() )
    {
        bool bare = true;
        for ( const QChar c : key )
        {
            bare = bare && isBareKeyChar( c );
        }
        if ( bare )
        {
            return key;
        }
    }
    return formatString( key );
}

QString formatValue( const QVariant& v );

QString
formatInlineTable( const QVariantMap& table )
{
    QStringList items;
    for ( auto it = table.constBegin(); it != table.constEnd(); ++it )
    {
        items << formatKey( it.key() ) + " = " + formatValue( it.value() );
    }
    return items.isEmpty() ? QStringLiteral( "{}" ) : '{' + items.join( ", " ) + '}';
}

QString
formatValue( const QVariant& v )
{
    const auto type = ClaudeInstall::typeOf( v );
    if ( type == ClaudeInstall::BoolVariantType )
    {
        return v.toBool() ? QStringLiteral( "true" ) : QStringLiteral( "false" );
    }
    if ( ClaudeInstall::isIntegerVariantType( v ) )
    {
        return QString::number( v.toLongLong() );
    }
    if ( type == ClaudeInstall::DoubleVariantType )
    {
        return QString::number( v.toDouble(), 'g', 17 );
    }
    if ( type == ClaudeInstall::ListVariantType || type == ClaudeInstall::StringListVariantType )
    {
        QStringList items;
        for ( const QVariant& item : v.toList() )
        {
            items << formatValue( item );
        }
        return '[' + items.join( ", " ) + ']';
    }
    if ( type == ClaudeInstall::MapVariantType )
    {
        return formatInlineTable( v.toMap() );
    }
    return formatString( v.toString() );
}

bool
isTable( const QVariant& v )
{
    return ClaudeInstall::typeOf( v ) == ClaudeInstall::MapVariantType;
}

void
dumpTable( const QVariantMap& table, const QString& path, QString& out )
{
    for ( auto it = table.constBegin(); it != table.constEnd(); ++it )
    {
        if ( !isTable( it.value() ) )
        {
            out += formatKey( it.key() ) + " = " + formatValue( it.value() ) + '\n';
        }
    }
    for ( auto it = table.constBegin(); it != table.constEnd(); ++it )
    {
        if ( isTable( it.value() ) )
        {
            const QString subPath = path.isEmpty() ? formatKey( it.key() ) : path + '.' + formatKey( it.key() );
            out += QStringLiteral( "\n[%1]\n" ).arg( subPath );
            dumpTable( it.value().toMap(), subPath, out );
        }
    }
}
}  // namespace

bool
parse( const QString& text, QVariantMap& result, QString* error )
{
    try
    {
        result = Parser( text ).parseDocument();
        return true;
    }
    catch ( const ParseError& e )
    {
        if ( error )
        {
            *error = e.message;
        }
        return false;
    }
}

bool
load( const QString& path, QVariantMap& result, QString* error )
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
    return parse( QString::fromUtf8( f.readAll() ), result, error );
}

QString
dump( const QVariantMap& table )
{
    QString out;
    dumpTable( table, QString(), out );
    // Without top-level values the first table starts with the blank line
    return out.startsWith( '\n' ) ? out.mid( 1 ) : out;
}

bool
save( const QString& path, const QVariantMap& table )
{
    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
    {
        return false;
    }
    const QByteArray data = dump( table ).toUtf8();
    return f.write( data ) == data.size();
}

}  // namespace Toml
}  // namespace DisplayManagers
