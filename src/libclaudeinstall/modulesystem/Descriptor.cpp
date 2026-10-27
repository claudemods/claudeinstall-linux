/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "Descriptor.h"

#include "utils/Logger.h"
#include "utils/Variant.h"

namespace ClaudeInstall
{
namespace ModuleSystem
{

const NamedEnumTable< Type >&
typeNames()
{
    // *INDENT-OFF*
    // clang-format off
    static const NamedEnumTable< Type > table{
        { QStringLiteral( "job" ), Type::Job },
        { QStringLiteral( "view" ), Type::View },
        { QStringLiteral( "viewmodule" ), Type::View },
        { QStringLiteral( "jobmodule" ), Type::Job }
    };
    // *INDENT-ON*
    // clang-format on
    return table;
}

const NamedEnumTable< Interface >&
interfaceNames()
{
    // *INDENT-OFF*
    // clang-format off
    static const NamedEnumTable< Interface > table {
        { QStringLiteral("process"), Interface::Process },
        { QStringLiteral("qtplugin"), Interface::QtPlugin },
    };
    // *INDENT-ON*
    // clang-format on
    return table;
}

Descriptor::Descriptor() {}

Descriptor
Descriptor::fromDescriptorData( const QVariantMap& moduleDesc, const QString& descriptorPath )
{
    Descriptor d;
    Logger::Once o;

    {
        bool typeOk = false;
        QString typeValue = moduleDesc.value( "type" ).toString();
        Type t = typeNames().find( typeValue, typeOk );
        if ( !typeOk )
        {
            if ( o )
            {
                cWarning() << o << "Descriptor file" << descriptorPath;
            }
            cWarning() << o << "Module descriptor contains invalid *type*" << typeValue;
        }

        bool interfaceOk = false;
        QString interfaceValue = moduleDesc.value( "interface" ).toString();
        Interface i = interfaceNames().find( interfaceValue, interfaceOk );
        if ( !interfaceOk )
        {
            if ( o )
            {
                cWarning() << o << "Descriptor file" << descriptorPath;
            }
            cWarning() << o << "Module descriptor contains invalid *interface*" << interfaceValue;
        }

        d.m_name = moduleDesc.value( "name" ).toString();
        if ( typeOk && interfaceOk && !d.m_name.isEmpty() )
        {
            d.m_type = t;
            d.m_interface = i;
            d.m_isValid = true;
        }
    }
    if ( !d.m_isValid )
    {
        return d;
    }

    d.m_isEmergeny = ClaudeInstall::getBool( moduleDesc, "emergency", false );
    d.m_hasConfig = !ClaudeInstall::getBool( moduleDesc, "noconfig", false );  // Inverted logic during load
    d.m_requiredModules = ClaudeInstall::getStringList( moduleDesc, "requiredModules" );
    d.m_weight = int( ClaudeInstall::getInteger( moduleDesc, "weight", -1 ) );

    QStringList consumedKeys { "type", "interface", "name", "emergency", "noconfig", "requiredModules", "weight" };

    switch ( d.interface() )
    {
    case Interface::QtPlugin:
        d.m_script = ClaudeInstall::getString( moduleDesc, "load" );
        d.m_tuiScript = ClaudeInstall::getString( moduleDesc, "tuiLoad" );
        consumedKeys << "load"
                     << "tuiLoad";
        break;
    case Interface::Process:
        d.m_script = ClaudeInstall::getString( moduleDesc, "command" );
        d.m_processTimeout = int( ClaudeInstall::getInteger( moduleDesc, "timeout", 30 ) );
        d.m_processChroot = ClaudeInstall::getBool( moduleDesc, "chroot", false );
        if ( d.m_processTimeout < 0 )
        {
            d.m_processTimeout = 0;
        }
        if ( d.m_script.isEmpty() )
        {
            if ( o )
            {
                cWarning() << o << "Descriptor file" << descriptorPath;
            }
            cWarning() << o << "Module descriptor contains no *script*" << d.name();
            d.m_isValid = false;
        }
        consumedKeys << "command"
                     << "timeout"
                     << "chroot";
        break;
    }

    if ( !d.m_isValid )
    {
        return d;
    }

    QStringList superfluousKeys;
    for ( auto kv = moduleDesc.keyBegin(); kv != moduleDesc.keyEnd(); ++kv )
    {
        if ( !consumedKeys.contains( *kv ) )
        {
            superfluousKeys << *kv;
        }
    }
    if ( !superfluousKeys.isEmpty() )
    {
        if ( o )
        {
            cWarning() << o << "Descriptor file" << descriptorPath;
        }
        cWarning() << o << "Module descriptor contains extra keys:" << Logger::DebugList( superfluousKeys );
        d.m_isValid = false;
    }

    return d;
}

}  // namespace ModuleSystem
}  // namespace ClaudeInstall
