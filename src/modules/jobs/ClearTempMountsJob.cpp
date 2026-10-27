/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ClearTempMountsJob.h"

#include "partition/Mount.h"
#include "utils/Logger.h"
#include "utils/String.h"

#include <kpmcore/util/report.h>

#include <QFile>
#include <QProcess>
#include <QStringList>

#include <algorithm>

ClearTempMountsJob::ClearTempMountsJob()
    : ClaudeInstall::Job()
{
}


QString
ClearTempMountsJob::prettyName() const
{
    return tr( "Clearing all temporary mounts…", "@status" );
}


QString
ClearTempMountsJob::prettyStatusMessage() const
{
    return tr( "Clearing all temporary mounts…", "@status" );
}


ClaudeInstall::JobResult
ClearTempMountsJob::exec()
{
    Logger::Once o;
    // Fetch a list of current mounts to ClaudeInstall temporary directories.
    using MtabInfo = ClaudeInstall::Partition::MtabInfo;
    auto targetMounts = MtabInfo::fromMtabFilteredByPrefix( QStringLiteral( "/tmp/claudeinstall-" ) );

    if ( targetMounts.isEmpty() )
    {
        return ClaudeInstall::JobResult::ok();
    }
    std::sort( targetMounts.begin(), targetMounts.end(), MtabInfo::mountPointOrder );

    QStringList goodNews;
    for ( const auto& m : std::as_const( targetMounts ) )
    {
        cDebug() << o << "Will try to umount path" << m.mountPoint;
        if ( ClaudeInstall::Partition::unmount( m.mountPoint, { "-lv" } ) == 0 )
        {
            // Returns the program's exit code, so 0 is success
            goodNews.append( QString( "Successfully unmounted %1." ).arg( m.mountPoint ) );
        }
    }

    ClaudeInstall::JobResult ok = ClaudeInstall::JobResult::ok();
    ok.setMessage( tr( "Cleared all temporary mounts." ) );
    ok.setDetails( goodNews.join( "\n" ) );

    cDebug() << o << "ClearTempMountsJob finished. Here's what was done:\n" << Logger::DebugList( goodNews );

    return ok;
}
