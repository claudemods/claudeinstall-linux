/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef OPENRCDMCRYPTCFGJOB_H
#define OPENRCDMCRYPTCFGJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantMap>

class PLUGINDLLEXPORT OpenrcDmcryptCfgJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit OpenrcDmcryptCfgJob( QObject* parent = nullptr );
    ~OpenrcDmcryptCfgJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    QString m_configFilePath;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( OpenrcDmcryptCfgJobFactory )

#endif  // OPENRCDMCRYPTCFGJOB_H
