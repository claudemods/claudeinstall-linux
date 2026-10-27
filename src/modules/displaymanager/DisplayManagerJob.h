/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Benjamin Vaudour <benjamin.vaudour@yahoo.fr>
 *   SPDX-FileCopyrightText: 2014-2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2014, 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef DISPLAYMANAGERJOB_H
#define DISPLAYMANAGERJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantMap>

/** @brief Configures display managers (and autologin) in the target
 *
 * This is "best effort": all the display managers that are listed and
 * installed are set up, in order, and the problems found are reported together.
 */
class PLUGINDLLEXPORT DisplayManagerJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit DisplayManagerJob( QObject* parent = nullptr );
    ~DisplayManagerJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    QVariantMap m_configuration;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( DisplayManagerJobFactory )

#endif  // DISPLAYMANAGERJOB_H
