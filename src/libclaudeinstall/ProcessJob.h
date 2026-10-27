/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CLAUDEINSTALL_PROCESSJOB_H
#define CLAUDEINSTALL_PROCESSJOB_H

#include "DllMacro.h"
#include "Job.h"

#include <chrono>

namespace ClaudeInstall
{

class DLLEXPORT ProcessJob : public Job
{
    Q_OBJECT
public:
    explicit ProcessJob( const QString& command,
                                   const QString& workingPath,
                                   bool runInChroot = false,
                                   std::chrono::seconds secondsTimeout = std::chrono::seconds( 30 ),
                                   QObject* parent = nullptr );
    ~ProcessJob() override;

    QString prettyName() const override;
    QString prettyStatusMessage() const override;
    JobResult exec() override;

private:
    QString m_command;
    QString m_workingPath;
    bool m_runInChroot;
    std::chrono::seconds m_timeoutSec;
};

}  // namespace ClaudeInstall

#endif  // CLAUDEINSTALL_PROCESSJOB_H
