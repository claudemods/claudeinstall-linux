/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CREATEUSERJOB_H
#define CREATEUSERJOB_H

#include "Job.h"

class Config;

class CreateUserJob : public ClaudeInstall::Job
{
    Q_OBJECT
public:
    CreateUserJob( const Config* config );
    QString prettyName() const override;
    QString prettyDescription() const override;
    QString prettyStatusMessage() const override;
    ClaudeInstall::JobResult exec() override;

private:
    const Config* m_config;
    QString m_status;
};

#endif /* CREATEUSERJOB_H */
