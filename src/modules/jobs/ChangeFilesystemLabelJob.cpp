/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2016, Lisa Vitolo <shainer@chakraos.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ChangeFilesystemLabelJob.h"

#include "core/KPMHelpers.h"

#include "utils/Logger.h"

#include <kpmcore/backend/corebackend.h>
#include <kpmcore/backend/corebackendmanager.h>
#include <kpmcore/core/device.h>
#include <kpmcore/core/partition.h>
#include <kpmcore/ops/setfilesystemlabeloperation.h>
#include <kpmcore/util/report.h>

ChangeFilesystemLabelJob::ChangeFilesystemLabelJob( Device* device, Partition* partition, const QString& newLabel )
    : PartitionJob( partition )
    , m_device( device )
    , m_label( newLabel )
{
}


QString
ChangeFilesystemLabelJob::prettyName() const
{
    return tr( "Set filesystem label on %1", "@title" ).arg( partition()->partitionPath() );
}


QString
ChangeFilesystemLabelJob::prettyDescription() const
{
    return tr( "Set filesystem label <strong>%1</strong> to partition <strong>%2</strong>", "@info" )
        .arg( m_label )
        .arg( partition()->partitionPath() );
}


QString
ChangeFilesystemLabelJob::prettyStatusMessage() const
{
    return tr( "Setting filesystem label <strong>%1</strong> to partition <strong>%2</strong>…", "@status" )
        .arg( m_label )
        .arg( partition()->partitionPath() );
}


ClaudeInstall::JobResult
ChangeFilesystemLabelJob::exec()
{
    if ( m_label == partition()->fileSystem().label() )
    {
        return ClaudeInstall::JobResult::ok();
    }

    // Check for luks device
    if ( partition()->fileSystem().type() == FileSystem::Luks )
    {
        if ( KPMHelpers::cryptLabel( partition(), m_label ) )
        {
            return ClaudeInstall::JobResult::ok();
        }
        return ClaudeInstall::JobResult::error(
            tr( "The installer failed to update partition table on disk '%1'.", "@info" ).arg( m_device->name() ) );
    }

    Report report( nullptr );
    SetFileSystemLabelOperation op( *partition(), m_label );
    op.setStatus( Operation::StatusRunning );

    if ( op.execute( report ) )
    {
        return ClaudeInstall::JobResult::ok();
    }
    return ClaudeInstall::JobResult::error(
        tr( "The installer failed to update partition table on disk '%1'.", "@info" ).arg( m_device->name() ),
        report.toText() );
}
