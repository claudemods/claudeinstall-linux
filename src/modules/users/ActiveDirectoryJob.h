/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 Simon Quigley <tsimonq2@ubuntu.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef ACTIVEDIRECTORYJOB_H
#define ACTIVEDIRECTORYJOB_H

#include "Job.h"

class ActiveDirectoryJob : public ClaudeInstall::Job
{
    Q_OBJECT
public:
    ActiveDirectoryJob( const QString& adminLogin,
                        const QString& adminPassword,
                        const QString& domain,
                        const QString& ip );
    QString prettyName() const override;
    QString prettyStatusMessage() const override;
    ClaudeInstall::JobResult exec() override;

private:
    QString m_adminLogin;  // Admin credentials to do the enrollment
    QString m_adminPassword;
    QString m_domain;
    QString m_ip;
};

#endif /* ACTIVEDIRECTORYJOB_H */
