/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: BSD-2-Clause
 *
 */

/*
 * Validates a ClaudeInstall config file -- YAML syntax -- against a schema.
 *
 * The schema is also written in YAML syntax, but the schema itself
 * is JSON-schema. This is possible because all JSON is YAML, and most
 * YAML is JSON. The limited subset of YAML that ClaudeInstall uses is
 * JSON-representable, anyway.
 *
 * This is the C++ replacement of ci/configvalidator.py. It needs only yaml-cpp,
 * and implements the part of JSON-schema that the schemata in src/modules/ use:
 *
 *   type, enum, const, properties, patternProperties, additionalProperties,
 *   required, items, minItems, maxItems, uniqueItems, minLength, maxLength,
 *   pattern, minimum, maximum, exclusiveMinimum, exclusiveMaximum,
 *   anyOf, oneOf, allOf, not, if / then / else, $ref (to "#/..." pointers and
 *   to "definitions/..." names), definitions.
 *
 * Other keywords (description, default, $schema, $id, and the nonstandard
 * ones such as "unique") are ignored, as jsonschema does.
 *
 * Usage:
 *     configvalidator <schema> <file> ...
 *     configvalidator -m <module>
 *     configvalidator -x
 *
 * Exits with value 0 on success, otherwise:
 *     2 on invalid command-line arguments
 *     3 on missing files
 *     4 if files have invalid syntax
 *     5 if files fail to validate
 * Use -x as only command-line argument to check that the tool runs (it always does).
 *
 * Use -m <module> as shorthand for standard paths in src/modules/<module>/
 */

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{

const char usage[] = R"(
Validates a ClaudeInstall config file -- YAML syntax -- against a schema.

Usage:
    configvalidator <schema> <file> ...
    configvalidator -m <module>
    configvalidator -x

Exits with value 0 on success, otherwise:
    2 on invalid command-line arguments
    3 on missing files
    4 if files have invalid syntax
    5 if files fail to validate
Use -x as only command-line argument to check that the tool runs.

Use -m <module> as shorthand for standard paths in src/modules/<module>/
)";

constexpr int ERR_USAGE = 2;
constexpr int ERR_FILE_NOT_FOUND = 3;
constexpr int ERR_SYNTAX = 4;
constexpr int ERR_INVALID = 5;

using Node = YAML::Node;
using Errors = std::vector< std::string >;

/// The JSON types of a YAML node
enum class JsonType
{
    Null,
    Boolean,
    Integer,
    Number,
    String,
    Array,
    Object
};

bool
looksInteger( const std::string& s )
{
    static const std::regex re( "^[-+]?[0-9]+$" );
    return std::regex_match( s, re );
}

bool
looksFloat( const std::string& s )
{
    static const std::regex re( "^[-+]?([0-9]+\\.[0-9]*|\\.[0-9]+|[0-9]+)([eE][-+]?[0-9]+)?$" );
    return std::regex_match( s, re );
}

JsonType
typeOf( const Node& n )
{
    switch ( n.Type() )
    {
    case YAML::NodeType::Null:
    case YAML::NodeType::Undefined:
        return JsonType::Null;
    case YAML::NodeType::Sequence:
        return JsonType::Array;
    case YAML::NodeType::Map:
        return JsonType::Object;
    case YAML::NodeType::Scalar:
        break;
    }

    const std::string& s = n.Scalar();
    // A quoted scalar ("false") is a string whatever it looks like; yaml-cpp
    // marks those with the non-specific tag "!".
    if ( n.Tag() == "!" )
    {
        return JsonType::String;
    }
    static const std::set< std::string > bools { "true", "True", "TRUE", "false", "False", "FALSE" };
    if ( bools.count( s ) )
    {
        return JsonType::Boolean;
    }
    if ( looksInteger( s ) )
    {
        return JsonType::Integer;
    }
    if ( looksFloat( s ) )
    {
        return JsonType::Number;
    }
    return JsonType::String;
}

bool
asBool( const Node& n )
{
    return n.Scalar().front() == 't' || n.Scalar().front() == 'T';
}

double
asDouble( const Node& n )
{
    return std::stod( n.Scalar() );
}

const char*
typeName( JsonType t )
{
    switch ( t )
    {
    case JsonType::Null:
        return "null";
    case JsonType::Boolean:
        return "boolean";
    case JsonType::Integer:
        return "integer";
    case JsonType::Number:
        return "number";
    case JsonType::String:
        return "string";
    case JsonType::Array:
        return "array";
    case JsonType::Object:
        return "object";
    }
    return "?";
}

/// Does the instance type match the schema's type name?
bool
typeMatches( JsonType t, const std::string& name )
{
    if ( name == "null" )
    {
        return t == JsonType::Null;
    }
    if ( name == "boolean" )
    {
        return t == JsonType::Boolean;
    }
    if ( name == "integer" )
    {
        return t == JsonType::Integer;
    }
    if ( name == "number" )
    {
        return t == JsonType::Integer || t == JsonType::Number;
    }
    if ( name == "string" )
    {
        return t == JsonType::String;
    }
    if ( name == "array" )
    {
        return t == JsonType::Array;
    }
    if ( name == "object" )
    {
        return t == JsonType::Object;
    }
    return false;
}

/// A text for a node that is the same for equal JSON values (used for enum, const, uniqueItems)
std::string
canonical( const Node& n )
{
    switch ( typeOf( n ) )
    {
    case JsonType::Null:
        return "null";
    case JsonType::Boolean:
        return asBool( n ) ? "true" : "false";
    case JsonType::Integer:
    case JsonType::Number:
    {
        std::ostringstream o;
        o << "num:" << asDouble( n );
        return o.str();
    }
    case JsonType::String:
        return "str:" + n.Scalar();
    case JsonType::Array:
    {
        std::string s = "[";
        for ( const auto& c : n )
        {
            s += canonical( c ) + ",";
        }
        return s + "]";
    }
    case JsonType::Object:
    {
        std::vector< std::string > items;
        for ( const auto& kv : n )
        {
            items.push_back( kv.first.Scalar() + "=" + canonical( kv.second ) );
        }
        std::sort( items.begin(), items.end() );
        std::string s = "{";
        for ( const auto& i : items )
        {
            s += i + ",";
        }
        return s + "}";
    }
    }
    return {};
}

std::string
show( const Node& n )
{
    YAML::Emitter e;
    e.SetOutputCharset( YAML::EscapeNonAscii );
    e << YAML::Flow << n;
    return e.c_str() ? e.c_str() : "";
}

class Validator
{
public:
    explicit Validator( const Node& root )
        : m_root( root )
    {
    }

    /// Validates @p instance against @p schema, adding messages to @p errors; returns @c true if valid
    bool validate( const Node& instance, const Node& schema, const std::string& path, Errors& errors, int depth = 0 )
    {
        const size_t before = errors.size();
        if ( depth > 64 )
        {
            errors.push_back( path + ": schema nesting is too deep (recursive $ref?)" );
            return false;
        }
        if ( schema.IsScalar() )
        {
            // true / false schemata
            if ( typeOf( schema ) == JsonType::Boolean && !asBool( schema ) )
            {
                errors.push_back( path + ": nothing is valid here" );
            }
            return errors.size() == before;
        }
        if ( !schema.IsMap() )
        {
            return true;
        }

        if ( const Node ref = schema[ "$ref" ] )
        {
            const Node target = resolve( ref.Scalar() );
            if ( !target )
            {
                errors.push_back( path + ": cannot resolve $ref '" + ref.Scalar() + "'" );
                return false;
            }
            validate( instance, target, path, errors, depth + 1 );
            // Draft 7: siblings of $ref are ignored
            return errors.size() == before;
        }

        const JsonType t = typeOf( instance );

        if ( const Node type = schema[ "type" ] )
        {
            bool ok = false;
            std::string wanted;
            if ( type.IsSequence() )
            {
                for ( const auto& n : type )
                {
                    ok = ok || typeMatches( t, n.Scalar() );
                    wanted += ( wanted.empty() ? "" : ", " ) + n.Scalar();
                }
            }
            else
            {
                ok = typeMatches( t, type.Scalar() );
                wanted = type.Scalar();
            }
            if ( !ok )
            {
                errors.push_back( path + ": " + show( instance ) + " is of type '" + typeName( t ) + "', not '" + wanted
                                  + "'" );
                // Further checks are about the right type; stop here as jsonschema's first error would
                return false;
            }
        }

        if ( const Node e = schema[ "enum" ] )
        {
            bool found = false;
            for ( const auto& n : e )
            {
                found = found || canonical( n ) == canonical( instance );
            }
            if ( !found )
            {
                errors.push_back( path + ": " + show( instance ) + " is not one of " + show( e ) );
            }
        }
        if ( const Node c = schema[ "const" ] )
        {
            if ( canonical( c ) != canonical( instance ) )
            {
                errors.push_back( path + ": " + show( instance ) + " was expected to be " + show( c ) );
            }
        }

        // Numbers
        if ( t == JsonType::Integer || t == JsonType::Number )
        {
            const double v = asDouble( instance );
            if ( const Node n = schema[ "minimum" ] )
            {
                if ( v < asDouble( n ) )
                {
                    errors.push_back( path + ": " + show( instance ) + " is less than the minimum of " + n.Scalar() );
                }
            }
            if ( const Node n = schema[ "maximum" ] )
            {
                if ( v > asDouble( n ) )
                {
                    errors.push_back( path + ": " + show( instance ) + " is greater than the maximum of " + n.Scalar() );
                }
            }
            if ( const Node n = schema[ "exclusiveMinimum" ] )
            {
                if ( n.IsScalar() && typeOf( n ) != JsonType::Boolean && v <= asDouble( n ) )
                {
                    errors.push_back( path + ": " + show( instance ) + " is not greater than " + n.Scalar() );
                }
            }
            if ( const Node n = schema[ "exclusiveMaximum" ] )
            {
                if ( n.IsScalar() && typeOf( n ) != JsonType::Boolean && v >= asDouble( n ) )
                {
                    errors.push_back( path + ": " + show( instance ) + " is not less than " + n.Scalar() );
                }
            }
        }

        // Strings
        if ( t == JsonType::String )
        {
            const std::string& s = instance.Scalar();
            // Length in characters, not bytes
            const size_t length = std::count_if( s.begin(), s.end(), []( unsigned char c ) { return ( c & 0xC0 ) != 0x80; } );
            if ( const Node n = schema[ "minLength" ] )
            {
                if ( length < n.as< size_t >() )
                {
                    errors.push_back( path + ": " + show( instance ) + " is too short" );
                }
            }
            if ( const Node n = schema[ "maxLength" ] )
            {
                if ( length > n.as< size_t >() )
                {
                    errors.push_back( path + ": " + show( instance ) + " is too long" );
                }
            }
            if ( const Node n = schema[ "pattern" ] )
            {
                try
                {
                    if ( !std::regex_search( s, std::regex( n.Scalar() ) ) )
                    {
                        errors.push_back( path + ": " + show( instance ) + " does not match '" + n.Scalar() + "'" );
                    }
                }
                catch ( const std::regex_error& )
                {
                    errors.push_back( path + ": the pattern '" + n.Scalar() + "' of the schema is not valid" );
                }
            }
        }

        // Arrays
        if ( t == JsonType::Array )
        {
            const size_t count = instance.size();
            if ( const Node n = schema[ "minItems" ] )
            {
                if ( count < n.as< size_t >() )
                {
                    errors.push_back( path + ": " + show( instance ) + " is too short" );
                }
            }
            if ( const Node n = schema[ "maxItems" ] )
            {
                if ( count > n.as< size_t >() )
                {
                    errors.push_back( path + ": " + show( instance ) + " is too long" );
                }
            }
            if ( const Node n = schema[ "uniqueItems" ] )
            {
                if ( typeOf( n ) == JsonType::Boolean && asBool( n ) )
                {
                    std::set< std::string > seen;
                    for ( const auto& item : instance )
                    {
                        if ( !seen.insert( canonical( item ) ).second )
                        {
                            errors.push_back( path + ": " + show( instance ) + " has non-unique elements" );
                            break;
                        }
                    }
                }
            }
            if ( const Node items = schema[ "items" ] )
            {
                size_t i = 0;
                for ( const auto& item : instance )
                {
                    const Node itemSchema = items.IsSequence() ? ( i < items.size() ? items[ i ] : Node() ) : items;
                    if ( itemSchema )
                    {
                        validate( item, itemSchema, path + "[" + std::to_string( i ) + "]", errors, depth + 1 );
                    }
                    ++i;
                }
            }
        }

        // Objects
        if ( t == JsonType::Object )
        {
            if ( const Node req = schema[ "required" ] )
            {
                if ( req.IsSequence() )
                {
                    for ( const auto& r : req )
                    {
                        if ( !instance[ r.Scalar() ] )
                        {
                            errors.push_back( path + ": '" + r.Scalar() + "' is a required property" );
                        }
                    }
                }
            }
            if ( const Node n = schema[ "minProperties" ] )
            {
                if ( instance.size() < n.as< size_t >() )
                {
                    errors.push_back( path + ": " + show( instance ) + " does not have enough properties" );
                }
            }
            if ( const Node n = schema[ "maxProperties" ] )
            {
                if ( instance.size() > n.as< size_t >() )
                {
                    errors.push_back( path + ": " + show( instance ) + " has too many properties" );
                }
            }

            const Node properties = schema[ "properties" ];
            const Node patterns = schema[ "patternProperties" ];
            const Node additional = schema[ "additionalProperties" ];
            std::vector< std::string > extra;
            for ( const auto& kv : instance )
            {
                const std::string key = kv.first.Scalar();
                bool matched = false;
                if ( properties && properties[ key ] )
                {
                    matched = true;
                    validate( kv.second, properties[ key ], path + "." + key, errors, depth + 1 );
                }
                if ( patterns && patterns.IsMap() )
                {
                    for ( const auto& p : patterns )
                    {
                        try
                        {
                            if ( std::regex_search( key, std::regex( p.first.Scalar() ) ) )
                            {
                                matched = true;
                                validate( kv.second, p.second, path + "." + key, errors, depth + 1 );
                            }
                        }
                        catch ( const std::regex_error& )
                        {
                            errors.push_back( path + ": the pattern '" + p.first.Scalar() + "' of the schema is not valid" );
                        }
                    }
                }
                if ( !matched )
                {
                    extra.push_back( key );
                    if ( additional && additional.IsMap() )
                    {
                        validate( kv.second, additional, path + "." + key, errors, depth + 1 );
                    }
                }
            }
            if ( additional && additional.IsScalar() && typeOf( additional ) == JsonType::Boolean && !asBool( additional )
                 && !extra.empty() )
            {
                std::string names;
                for ( const auto& e : extra )
                {
                    names += ( names.empty() ? "'" : ", '" ) + e + "'";
                }
                errors.push_back( path + ": additional properties are not allowed (" + names + " "
                                  + ( extra.size() == 1 ? "was" : "were" ) + " unexpected)" );
            }
        }

        // Combinations
        if ( const Node all = schema[ "allOf" ] )
        {
            for ( const auto& s : all )
            {
                validate( instance, s, path, errors, depth + 1 );
            }
        }
        if ( const Node any = schema[ "anyOf" ] )
        {
            bool ok = false;
            for ( const auto& s : any )
            {
                Errors sub;
                if ( validate( instance, s, path, sub, depth + 1 ) )
                {
                    ok = true;
                    break;
                }
            }
            if ( !ok )
            {
                errors.push_back( path + ": " + show( instance ) + " is not valid under any of the given schemas" );
            }
        }
        if ( const Node one = schema[ "oneOf" ] )
        {
            int valid = 0;
            for ( const auto& s : one )
            {
                Errors sub;
                valid += validate( instance, s, path, sub, depth + 1 ) ? 1 : 0;
            }
            if ( valid == 0 )
            {
                errors.push_back( path + ": " + show( instance ) + " is not valid under any of the given schemas" );
            }
            else if ( valid > 1 )
            {
                errors.push_back( path + ": " + show( instance ) + " is valid under more than one of the given schemas" );
            }
        }
        if ( const Node n = schema[ "not" ] )
        {
            Errors sub;
            if ( validate( instance, n, path, sub, depth + 1 ) )
            {
                errors.push_back( path + ": " + show( instance ) + " should not be valid under " + show( n ) );
            }
        }
        if ( const Node condition = schema[ "if" ] )
        {
            Errors sub;
            const bool holds = validate( instance, condition, path, sub, depth + 1 );
            if ( holds && schema[ "then" ] )
            {
                validate( instance, schema[ "then" ], path, errors, depth + 1 );
            }
            else if ( !holds && schema[ "else" ] )
            {
                validate( instance, schema[ "else" ], path, errors, depth + 1 );
            }
        }

        return errors.size() == before;
    }

    /** @brief Checks that the schema itself is sane (what jsonschema's check_schema() catches most often)
     *
     * Returns @c true if it is; otherwise there are messages in @p errors.
     */
    bool checkSchema( const Node& schema, const std::string& path, Errors& errors, int depth = 0 ) const
    {
        const size_t before = errors.size();
        if ( depth > 64 || !schema.IsMap() )
        {
            if ( !schema.IsMap() && !( schema.IsScalar() && typeOf( schema ) == JsonType::Boolean ) )
            {
                errors.push_back( path + ": a schema must be a mapping or a boolean" );
            }
            return errors.size() == before;
        }

        static const std::set< std::string > typeNames { "null", "boolean", "integer", "number", "string", "array", "object" };
        if ( const Node type = schema[ "type" ] )
        {
            auto check = [ & ]( const Node& n )
            {
                if ( !n.IsScalar() || !typeNames.count( n.Scalar() ) )
                {
                    errors.push_back( path + ": " + show( n ) + " is not a valid type name" );
                }
            };
            if ( type.IsSequence() )
            {
                for ( const auto& n : type )
                {
                    check( n );
                }
            }
            else
            {
                check( type );
            }
        }
        if ( const Node req = schema[ "required" ] )
        {
            if ( !req.IsSequence() )
            {
                errors.push_back( path + ": 'required' must be a list of names" );
            }
        }
        if ( const Node props = schema[ "properties" ] )
        {
            if ( !props.IsMap() )
            {
                errors.push_back( path + ": 'properties' must be a mapping" );
            }
            else
            {
                for ( const auto& kv : props )
                {
                    checkSchema( kv.second, path + ".properties." + kv.first.Scalar(), errors, depth + 1 );
                }
            }
        }
        if ( const Node defs = schema[ "definitions" ] )
        {
            if ( defs.IsMap() )
            {
                for ( const auto& kv : defs )
                {
                    checkSchema( kv.second, path + ".definitions." + kv.first.Scalar(), errors, depth + 1 );
                }
            }
        }
        for ( const char* key : { "enum", "anyOf", "oneOf", "allOf" } )
        {
            if ( const Node n = schema[ key ] )
            {
                if ( !n.IsSequence() )
                {
                    errors.push_back( path + ": '" + key + "' must be a list" );
                }
            }
        }
        if ( const Node items = schema[ "items" ] )
        {
            if ( items.IsMap() )
            {
                checkSchema( items, path + ".items", errors, depth + 1 );
            }
        }
        return errors.size() == before;
    }

private:
    /// Finds what a $ref refers to; an undefined node if nothing
    Node resolve( const std::string& ref ) const
    {
        std::string pointer = ref;
        if ( !pointer.empty() && pointer[ 0 ] == '#' )
        {
            pointer.erase( 0, 1 );
        }
        else if ( pointer.rfind( "definitions/", 0 ) == 0 )
        {
            pointer = "/" + pointer;  // a name relative to the document
        }
        else
        {
            return Node();
        }

        Node current = YAML::Clone( m_root );
        std::stringstream parts( pointer );
        std::string part;
        while ( std::getline( parts, part, '/' ) )
        {
            if ( part.empty() )
            {
                continue;
            }
            // JSON pointer escapes
            for ( size_t p = part.find( "~1" ); p != std::string::npos; p = part.find( "~1" ) )
            {
                part.replace( p, 2, "/" );
            }
            for ( size_t p = part.find( "~0" ); p != std::string::npos; p = part.find( "~0" ) )
            {
                part.replace( p, 2, "~" );
            }
            if ( current.IsSequence() && looksInteger( part ) )
            {
                current = current[ static_cast< size_t >( std::stoul( part ) ) ];
            }
            else if ( current.IsMap() )
            {
                current = current[ part ];
            }
            else
            {
                return Node();
            }
            if ( !current )
            {
                return Node();
            }
        }
        return current;
    }

    const Node& m_root;
};

}  // namespace

static bool
loadYaml( const std::string& fileName, Node& out, std::string& problem )
{
    try
    {
        out = YAML::LoadFile( fileName );
        return true;
    }
    catch ( const YAML::Exception& e )
    {
        std::ostringstream o;
        o << e.msg << " (line " << e.mark.line + 1 << ", column " << e.mark.column + 1 << ")";
        problem = o.str();
        return false;
    }
}

int
main( int argc, char* argv[] )
{
    if ( argc < 3 )
    {
        // Special-case: called with -x to just test that the tool runs
        if ( argc == 2 && std::string( argv[ 1 ] ) == "-x" )
        {
            return 0;
        }
        std::cout << usage;
        return ERR_USAGE;
    }

    std::string schemaFileName;
    std::vector< std::string > configFileNames;
    if ( argc == 3 && std::string( argv[ 1 ] ) == "-m" )
    {
        const std::string module = argv[ 2 ];
        schemaFileName = "src/modules/" + module + "/" + module + ".schema.yaml";
        configFileNames.push_back( "src/modules/" + module + "/" + module + ".conf" );
    }
    else
    {
        schemaFileName = argv[ 1 ];
        for ( int i = 2; i < argc; ++i )
        {
            configFileNames.push_back( argv[ i ] );
        }
    }

    if ( !std::filesystem::exists( schemaFileName ) )
    {
        std::cout << usage << "\nSchema file '" << schemaFileName << "' does not exist.\n";
        return ERR_FILE_NOT_FOUND;
    }
    for ( const auto& f : configFileNames )
    {
        if ( !std::filesystem::exists( f ) )
        {
            std::cout << usage << "\nYAML file '" << f << "' does not exist.\n";
            return ERR_FILE_NOT_FOUND;
        }
    }

    // FILES SYNTAX CHECK
    Node schema;
    std::string problem;
    if ( !loadYaml( schemaFileName, schema, problem ) )
    {
        std::cout << "Schema error: " << problem << ".\n\nSchema file '" << schemaFileName << "' is invalid YAML.\n";
        return ERR_SYNTAX;
    }

    Validator validator( schema );
    {
        Errors errors;
        if ( !validator.checkSchema( schema, "schema", errors ) )
        {
            for ( const auto& e : errors )
            {
                std::cout << e << "\n";
            }
            std::cout << "\nSchema file '" << schemaFileName << "' is invalid JSON-Schema.\n";
            return ERR_INVALID;
        }
    }

    std::vector< Node > configs;
    for ( const auto& f : configFileNames )
    {
        Node config;
        if ( !loadYaml( f, config, problem ) )
        {
            std::cout << "YAML error: " << problem << ".\n\nYAML file '" << f << "' is invalid.\n";
            return ERR_SYNTAX;
        }
        if ( config.IsNull() )
        {
            std::cout << "YAML file '" << f << "' is empty.\n";
        }
        configs.push_back( config );
    }

    // SCHEMA VALIDATION
    for ( size_t i = 0; i < configs.size(); ++i )
    {
        Errors errors;
        if ( !validator.validate( configs[ i ], schema, "config", errors ) )
        {
            for ( const auto& e : errors )
            {
                std::cout << e << "\n";
            }
            std::cout << "\nConfig file '" << configFileNames[ i ] << "' does not validate in schema.\n";
            return ERR_INVALID;
        }
    }
    return 0;
}
