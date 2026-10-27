/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CLAUDEINSTALL_MODULEFACTORY_H
#define CLAUDEINSTALL_MODULEFACTORY_H

#include "DllMacro.h"

#include "modulesystem/Descriptor.h"
#include "modulesystem/Module.h"

#include <QString>

#include <functional>

namespace ClaudeInstall
{

/** @brief How a frontend creates the Module for a view module
 *
 * Job modules look the same in every frontend, but a view module is
 * shown in a frontend-specific way. The frontend registers a function
 * that creates its kind of view module (a new object, not yet initialized).
 */
using ViewModuleCreator = std::function< Module*() >;
STEPDLLEXPORT void setViewModuleCreator( const ViewModuleCreator& creator );

/**
 * @brief fromDescriptor creates a new Module object of the correct type.
 * @param moduleDescriptor a module descriptor, already parsed into a variant map.
 * @param instanceId the instance id of the new module instance.
 * @param configFileName the name of the configuration file to read.
 * @param moduleDirectory the path to the directory with this module's files.
 * @return a pointer to an object of a subtype of Module.
 */
STEPDLLEXPORT Module* moduleFromDescriptor( const ModuleSystem::Descriptor& moduleDescriptor,
                                            const QString& instanceId,
                                            const QString& configFileName,
                                            const QString& moduleDirectory );
}  // namespace ClaudeInstall

#endif  // CLAUDEINSTALL_MODULEFACTORY_H
