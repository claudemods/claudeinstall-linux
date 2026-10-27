/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2022 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef UNPACKFSC_TARBALLRUNNER_H
#define UNPACKFSC_TARBALLRUNNER_H

#include "Runners.h"

/** @brief Use (GNU) tar for extracting a filesystem
 *
 */
class TarballRunner : public Runner
{
    Q_OBJECT
public:
    using Runner::Runner;

    ClaudeInstall::JobResult run() override;

protected Q_SLOTS:
    void tarballProgress( QString line );

private:
    // Progress reporting
    int m_total = 0;
    int m_processed = 0;
    int m_since = 0;
};

#endif
