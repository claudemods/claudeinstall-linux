/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

/**@file Various small jobs
 *
 * This file collects miscellaneous jobs that need to be run to prepare
 * the system for the user-creation job.
 */

#ifndef USERS_MISCJOBS_H
#define USERS_MISCJOBS_H

#include "Config.h"

#include "Job.h"

class SetupSudoJob : public ClaudeInstall::Job
{
    Q_OBJECT
public:
    SetupSudoJob( const QString& group, Config::SudoStyle style );
    QString prettyName() const override;
    ClaudeInstall::JobResult exec() override;

public:
    QString m_sudoGroup;
    Config::SudoStyle m_sudoStyle;
};

class SetupGroupsJob : public ClaudeInstall::Job
{
    Q_OBJECT

public:
    SetupGroupsJob( const Config* config );
    QString prettyName() const override;
    ClaudeInstall::JobResult exec() override;

public:
    const Config* m_config;
};

#endif
