/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PLASMALNFJOB_H
#define PLASMALNFJOB_H

#include <QObject>
#include <QVariantMap>

#include "Job.h"

class PlasmaLnfJob : public ClaudeInstall::Job
{
    Q_OBJECT

public:
    explicit PlasmaLnfJob( const QString& lnfPath, const QString& id );
    ~PlasmaLnfJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

private:
    QString m_lnfPath;
    QString m_id;
};

#endif  // PLASMALNFJOB_H
