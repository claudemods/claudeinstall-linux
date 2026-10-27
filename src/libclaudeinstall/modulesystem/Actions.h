/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 *
 */

#ifndef MODULESYSTEM_ACTIONS_H
#define MODULESYSTEM_ACTIONS_H

namespace ClaudeInstall
{
namespace ModuleSystem
{

enum class Action : char
{
    Show,
    Exec
};

}  // namespace ModuleSystem
}  // namespace ClaudeInstall

#endif
