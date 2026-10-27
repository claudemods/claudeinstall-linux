/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef FORMATPARTITIONJOB_H
#define FORMATPARTITIONJOB_H

#include "PartitionJob.h"

class Device;
class Partition;
class FileSystem;

/**
 * This job formats an existing partition.
 *
 * It is only used for existing partitions: newly created partitions are
 * formatted by the CreatePartitionJob.
 */
class FormatPartitionJob : public PartitionJob
{
    Q_OBJECT
public:
    FormatPartitionJob( Device* device, Partition* partition );
    QString prettyName() const override;
    QString prettyDescription() const override;
    QString prettyStatusMessage() const override;
    ClaudeInstall::JobResult exec() override;

    Device* device() const { return m_device; }

private:
    Device* m_device;
};

#endif /* FORMATPARTITIONJOB_H */
