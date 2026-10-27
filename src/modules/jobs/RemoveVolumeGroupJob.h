/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2018 Caio Jordão Carvalho <caiojcarvalho@gmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef REMOVEVOLUMEGROUPJOB_H
#define REMOVEVOLUMEGROUPJOB_H

#include "Job.h"
#include "partition/KPMManager.h"

class Device;
class LvmDevice;

class RemoveVolumeGroupJob : public ClaudeInstall::Job
{
    Q_OBJECT
public:
    RemoveVolumeGroupJob( Device*, LvmDevice* device );

    QString prettyName() const override;
    QString prettyDescription() const override;
    QString prettyStatusMessage() const override;
    ClaudeInstall::JobResult exec() override;

private:
    ClaudeInstall::Partition::KPMManager m_kpmcore;
    LvmDevice* m_device;
};

#endif  // REMOVEVOLUMEGROUPJOB_H
