/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2016, Lisa Vitolo <shainer@chakraos.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CHANGEFILESYSTEMLABELJOB_H
#define CHANGEFILESYSTEMLABELJOB_H

#include "PartitionJob.h"

#include <kpmcore/core/partitiontable.h>

class Device;
class Partition;

/**
 * This job changes the flags on an existing partition.
 */
class ChangeFilesystemLabelJob : public PartitionJob
{
    Q_OBJECT
public:
    ChangeFilesystemLabelJob( Device* device, Partition* partition, const QString& newLabel );
    QString prettyName() const override;
    QString prettyDescription() const override;
    QString prettyStatusMessage() const override;
    ClaudeInstall::JobResult exec() override;

    Device* device() const;

private:
    Device* m_device;
    QString m_label;
};

#endif  // CHANGEFILESYSTEMLABELJOB_H
