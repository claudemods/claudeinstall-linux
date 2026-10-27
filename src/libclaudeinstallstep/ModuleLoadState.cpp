/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ModuleLoadState.h"

namespace ClaudeInstall
{

ModuleLoadState::ModuleLoadState( QObject* parent )
    : QObject( parent )
{
}

ModuleLoadState*
ModuleLoadState::instance()
{
    static ModuleLoadState s_instance;
    return &s_instance;
}

void
ModuleLoadState::notifyModulesLoaded()
{
    m_loaded = true;
    emit modulesLoaded();
}

}  // namespace ClaudeInstall
