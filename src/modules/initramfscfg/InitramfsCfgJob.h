/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Rohan Garg <rohan@kde.org>
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef INITRAMFSCFGJOB_H
#define INITRAMFSCFGJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>

/** @brief Installs the initramfs-tools encrypt hook into the target. */
class PLUGINDLLEXPORT InitramfsCfgJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit InitramfsCfgJob( QObject* parent = nullptr );
    ~InitramfsCfgJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( InitramfsCfgJobFactory )

#endif  // INITRAMFSCFGJOB_H
