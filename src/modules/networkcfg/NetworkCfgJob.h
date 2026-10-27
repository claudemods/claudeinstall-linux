/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef NETWORKCFGJOB_H
#define NETWORKCFGJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>

/** @brief Copies the live system's network configuration to the target. */
class PLUGINDLLEXPORT NetworkCfgJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit NetworkCfgJob( QObject* parent = nullptr );
    ~NetworkCfgJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( NetworkCfgJobFactory )

#endif  // NETWORKCFGJOB_H
