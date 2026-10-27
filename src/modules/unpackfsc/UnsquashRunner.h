/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef UNPACKFSC_UNSQUASHRUNNER_H
#define UNPACKFSC_UNSQUASHRUNNER_H

#include "Runners.h"

/** @brief Use Unsquash for extracting a filesystem
 *
 */
class UnsquashRunner : public Runner
{
    Q_OBJECT
public:
    using Runner::Runner;

    ClaudeInstall::JobResult run() override;

protected Q_SLOTS:
    void unsquashProgress( QString line );

private:
    int m_inodes = 0;  // Total in the FS

    // Progress reporting
    int m_processed = 0;
    int m_since = 0;
};

#endif
