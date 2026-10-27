/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef SETTIMEZONEJOB_H
#define SETTIMEZONEJOB_H

#include "Job.h"


class SetTimezoneJob : public ClaudeInstall::Job
{
    Q_OBJECT
public:
    SetTimezoneJob( const QString& region, const QString& zone );

    QString prettyName() const override;
    ClaudeInstall::JobResult exec() override;

private:
    QString m_region;
    QString m_zone;
};

#endif /* SETTIMEZONEJOB_H */
