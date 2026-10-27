/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef SERVICESSYSTEMDJOB_H
#define SERVICESSYSTEMDJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class PLUGINDLLEXPORT ServicesSystemdJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit ServicesSystemdJob( QObject* parent = nullptr );
    ~ServicesSystemdJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    QVariantList m_units;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( ServicesSystemdJobFactory )

#endif  // SERVICESSYSTEMDJOB_H
