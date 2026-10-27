/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef UMOUNTJOB_H
#define UMOUNTJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>

/** @brief Write 'random' data: machine id, entropy, UUIDs
 *
 */
class PLUGINDLLEXPORT UmountJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit UmountJob( QObject* parent = nullptr );
    ~UmountJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( UmountJobFactory )

#endif  // UMOUNTJOB_H
