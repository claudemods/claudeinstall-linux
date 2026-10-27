/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */
#include "JobExample.h"

namespace ClaudeInstall
{

QString
NamedJob::prettyName() const
{
    return tr( "Example job (%1)" ).arg( m_name );
}

JobResult
GoodJob::exec()
{
    return JobResult::ok();
}

JobResult
FailJob::exec()
{
    return JobResult::error( tr( "Job failed (%1)" ).arg( m_name ),
                             tr( "Programmed job failure was explicitly requested." ) );
}

}  // namespace ClaudeInstall
