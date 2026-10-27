/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LIBCLAUDEINSTALL_PACKAGES_GLOBALS_H
#define LIBCLAUDEINSTALL_PACKAGES_GLOBALS_H

#include "DllMacro.h"
#include "GlobalStorage.h"
#include "modulesystem/InstanceKey.h"

namespace ClaudeInstall
{
namespace Packages
{
/** @brief Sets the install-packages GS keys for the given module
 *
 * This replaces previously-set install-packages lists for the
 * given module by the two new lists.
 *
 * Returns @c true if anything was changed, @c false otherwise.
 */
DLLEXPORT bool setGSPackageAdditions( ClaudeInstall::GlobalStorage* gs,
                                      const ClaudeInstall::ModuleSystem::InstanceKey& module,
                                      const QVariantList& installPackages,
                                      const QVariantList& tryInstallPackages );
/** @brief Sets the install-packages GS keys for the given module
 *
 * This replaces previously-set install-packages lists. Use this with
 * plain lists of package names. It does not support try-install.
 */
DLLEXPORT bool setGSPackageAdditions( ClaudeInstall::GlobalStorage* gs,
                                      const ClaudeInstall::ModuleSystem::InstanceKey& module,
                                      const QStringList& installPackages );
// void setGSPackageRemovals( const ClaudeInstall::ModuleSystem::InstanceKey& key, const QVariantList& removePackages );
}  // namespace Packages
}  // namespace ClaudeInstall


#endif
