/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef DELETEPARTITIONJOB_H
#define DELETEPARTITIONJOB_H

#include "PartitionJob.h"

class Device;
class Partition;
class FileSystem;

/**
 * Deletes an existing partition.
 *
 * This is only used for partitions which already existed before the installer
 * was started: partitions created within the installer and then removed are
 * simply forgotten.
 */
class DeletePartitionJob : public PartitionJob
{
    Q_OBJECT
public:
    DeletePartitionJob( Device* device, Partition* partition );
    QString prettyName() const override;
    QString prettyDescription() const override;
    QString prettyStatusMessage() const override;
    ClaudeInstall::JobResult exec() override;

    void updatePreview();
    Device* device() const { return m_device; }

private:
    Device* m_device;
};

#endif /* DELETEPARTITIONJOB_H */
