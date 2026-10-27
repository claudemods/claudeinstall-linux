/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CLEARTEMPMOUNTSJOB_H
#define CLEARTEMPMOUNTSJOB_H

#include "Job.h"

class Device;

/**
 * This job tries to free all temporary mounts used by ClaudeInstall, so partitioning
 * operations can proceed.
 */
class ClearTempMountsJob : public ClaudeInstall::Job
{
    Q_OBJECT
public:
    explicit ClearTempMountsJob();
    QString prettyName() const override;
    QString prettyStatusMessage() const override;
    ClaudeInstall::JobResult exec() override;
};

#endif  // CLEARTEMPMOUNTSJOB_H
