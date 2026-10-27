/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef LUKSBOOTKEYFILEJOB_H
#define LUKSBOOTKEYFILEJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantMap>

/** @brief Creates the LUKS boot key file and adds it to the cryptsetup.
 *
 * This job has no configuration, because it takes everything
 * from the global storage settings set by others.
 */
class PLUGINDLLEXPORT LuksBootKeyFileJob : public ClaudeInstall::CppJob
{
    Q_OBJECT
public:
    explicit LuksBootKeyFileJob( QObject* parent = nullptr );
    ~LuksBootKeyFileJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    QString m_luks2Hash;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( LuksBootKeyFileJobFactory )

#endif  // LUKSBOOTKEYFILEJOB_H
