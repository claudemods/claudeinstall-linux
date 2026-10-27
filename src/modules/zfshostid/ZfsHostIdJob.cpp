/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2022 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ZfsHostIdJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"

#include <QFile>

ZfsHostIdJob::ZfsHostIdJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

ZfsHostIdJob::~ZfsHostIdJob() {}

QString
ZfsHostIdJob::prettyName() const
{
    return tr( "Copying zfs generated hostid…", "@status" );
}

ClaudeInstall::JobResult
ZfsHostIdJob::exec()
{
    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();

    // The Python version tested truthiness: absent, null or empty list all mean "no zfs".
    if ( gs->value( "zfsDatasets" ).toList().isEmpty() )
    {
        return ClaudeInstall::JobResult::ok();
    }

    const QString destination = gs->value( "rootMountPoint" ).toString() + QStringLiteral( "/etc/hostid" );
    QFile::remove( destination );  // QFile::copy() refuses to overwrite, shutil.copy2() did not
    if ( !QFile::copy( QStringLiteral( "/etc/hostid" ), destination ) )
    {
        cWarning() << "Could not copy hostid";
    }
    return ClaudeInstall::JobResult::ok();
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( ZfsHostIdJobFactory, registerPlugin< ZfsHostIdJob >(); )
