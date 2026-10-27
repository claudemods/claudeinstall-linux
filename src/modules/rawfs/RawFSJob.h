/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd <arnaud.ferraris@collabora.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef RAWFSJOB_H
#define RAWFSJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

/** @brief Raw copy of filesystems (images or devices) onto partitions */
class PLUGINDLLEXPORT RawFSJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit RawFSJob( QObject* parent = nullptr );
    ~RawFSJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    QVariantList m_targets;
    bool m_bogus = false;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( RawFSJobFactory )

#endif  // RAWFSJOB_H
