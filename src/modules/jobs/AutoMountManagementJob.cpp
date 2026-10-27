/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "AutoMountManagementJob.h"

#include "utils/Logger.h"

AutoMountManagementJob::AutoMountManagementJob( bool disable )
    : m_disable( disable )
{
}

QString
AutoMountManagementJob::prettyName() const
{
    return tr( "Managing auto-mount settings…", "@status" );
}

ClaudeInstall::JobResult
AutoMountManagementJob::exec()
{
    if ( m_stored )
    {
        cDebug() << "Restore automount settings";
        ClaudeInstall::Partition::automountRestore( m_stored );
        m_stored.reset();
    }
    else
    {
        cDebug() << "Set automount to" << ( m_disable ? "disable" : "enable" );
        m_stored = ClaudeInstall::Partition::automountDisable( m_disable );
    }
    return ClaudeInstall::JobResult::ok();
}
