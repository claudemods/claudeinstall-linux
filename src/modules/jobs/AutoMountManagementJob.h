/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PARTITION_AUTOMOUNTMANAGEMENTJOB_H
#define PARTITION_AUTOMOUNTMANAGEMENTJOB_H

#include "Job.h"

#include "partition/AutoMount.h"

/**
 * This job sets automounting to a specific value, and when run a
 * second time, **re**sets to the original value. See the documentation
 * for ClaudeInstall::Partition::automountDisable() for details.
 * Use @c true to **disable** automounting.
 *
 * Effectively: queue the **same** job twice; the first time it runs
 * it will set the automount behavior, and the second time it
 * restores the original.
 *
 */
class AutoMountManagementJob : public ClaudeInstall::Job
{
    Q_OBJECT
public:
    AutoMountManagementJob( bool disable = true );

    QString prettyName() const override;
    ClaudeInstall::JobResult exec() override;

private:
    bool m_disable;
    decltype( ClaudeInstall::Partition::automountDisable( true ) ) m_stored;
};

#endif /* PARTITION_AUTOMOUNTMANAGEMENTJOB_H */
