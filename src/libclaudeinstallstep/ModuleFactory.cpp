/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ModuleFactory.h"

#include "ClaudeInstallConfig.h"
#include "CppJobModule.h"
#include "ProcessJobModule.h"

#include "utils/Dirs.h"
#include "utils/Logger.h"
#include "utils/NamedEnum.h"
#include "utils/Yaml.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

namespace ClaudeInstall
{

static ViewModuleCreator s_viewModuleCreator;

void
setViewModuleCreator( const ViewModuleCreator& creator )
{
    s_viewModuleCreator = creator;
}

Module*
moduleFromDescriptor( const ClaudeInstall::ModuleSystem::Descriptor& moduleDescriptor,
                      const QString& instanceId,
                      const QString& configFileName,
                      const QString& moduleDirectory )
{
    using Type = ClaudeInstall::ModuleSystem::Type;
    using Interface = ClaudeInstall::ModuleSystem::Interface;

    std::unique_ptr< Module > m;

    if ( !moduleDescriptor.isValid() )
    {
        cError() << "Bad module descriptor format" << instanceId;
        return nullptr;
    }
    if ( moduleDescriptor.type() == Type::View )
    {
        if ( moduleDescriptor.interface() == Interface::QtPlugin )
        {
            // How to show a view module depends on the frontend, which says so
            if ( s_viewModuleCreator )
            {
                m.reset( s_viewModuleCreator() );
            }
            else
            {
                cError() << "No frontend can create view modules.";
            }
        }
        else
        {
            cError() << "Bad interface"
                     << ClaudeInstall::ModuleSystem::interfaceNames().find( moduleDescriptor.interface() )
                     << "for module type" << ClaudeInstall::ModuleSystem::typeNames().find( moduleDescriptor.type() );
        }
    }
    else if ( moduleDescriptor.type() == Type::Job )
    {
        if ( moduleDescriptor.interface() == Interface::QtPlugin )
        {
            m.reset( new CppJobModule() );
        }
        else if ( moduleDescriptor.interface() == Interface::Process )
        {
            m.reset( new ProcessJobModule() );
        }
        else
        {
            cError() << "Bad interface"
                     << ClaudeInstall::ModuleSystem::interfaceNames().find( moduleDescriptor.interface() )
                     << "for module type" << ClaudeInstall::ModuleSystem::typeNames().find( moduleDescriptor.type() );
        }
    }
    else
    {
        cError() << "Bad module type" << ClaudeInstall::ModuleSystem::typeNames().find( moduleDescriptor.type() );
    }

    if ( !m )
    {
        cError() << "Bad module type (" << ClaudeInstall::ModuleSystem::typeNames().find( moduleDescriptor.type() )
                 << ") or interface string ("
                 << ClaudeInstall::ModuleSystem::interfaceNames().find( moduleDescriptor.interface() ) << ") for module "
                 << instanceId;
        return nullptr;
    }

    QDir moduleDir( moduleDirectory );
    if ( moduleDir.exists() && moduleDir.isReadable() )
    {
        m->m_directory = moduleDir.absolutePath();
    }
    else
    {
        cError() << "Bad module directory" << moduleDirectory << "for" << instanceId;
        return nullptr;
    }

    m->initFrom( moduleDescriptor, instanceId );
    if ( !m->m_key.isValid() )
    {
        cError() << "Module" << instanceId << "invalid ID";
        return nullptr;
    }

    m->initFrom( moduleDescriptor );
    if ( !configFileName.isEmpty() )
    {
        try
        {
            m->loadConfigurationFile( configFileName );
        }
        catch ( ::YAML::Exception& e )
        {
            cError() << "YAML parser error " << e.what();
            return nullptr;
        }
    }
    return m.release();
}

}  // namespace ClaudeInstall
