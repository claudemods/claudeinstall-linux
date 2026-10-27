/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2021 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef FINISHEDQMLVIEWSTEP_H
#define FINISHEDQMLVIEWSTEP_H

#include <QObject>

#include "Config.h"

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/QmlViewStep.h"

#include <QVariant>

class Config;

class PLUGINDLLEXPORT FinishedQmlViewStep : public ClaudeInstall::QmlViewStep
{
    Q_OBJECT

public:
    explicit FinishedQmlViewStep( QObject* parent = nullptr );

    QString prettyName() const override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    void onActivate() override;

    ClaudeInstall::JobList jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;
    QObject* getConfig() override;

private:
    Config* m_config;

    bool m_installFailed;  // Track if onInstallationFailed() was called
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( FinishedQmlViewStepFactory )

#endif
