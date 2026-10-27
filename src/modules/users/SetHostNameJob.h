/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Rohan Garg <rohan@kde.org>
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef SETHOSTNAMEJOB_CPP_H
#define SETHOSTNAMEJOB_CPP_H

#include "Config.h"

#include "Job.h"

class SetHostNameJob : public ClaudeInstall::Job
{
    Q_OBJECT
public:
    SetHostNameJob( const Config* c );
    QString prettyName() const override;
    QString prettyDescription() const override;
    QString prettyStatusMessage() const override;
    ClaudeInstall::JobResult exec() override;

private:
    const Config* m_config;
};

#endif  // SETHOSTNAMEJOB_CPP_H
