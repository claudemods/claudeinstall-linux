/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef FINISHEDVIEWSTEP_H
#define FINISHEDVIEWSTEP_H

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

class Config;
class FinishedPage;

class PLUGINDLLEXPORT FinishedViewStep : public ClaudeInstall::ViewStep
{
    Q_OBJECT

public:
    explicit FinishedViewStep( QObject* parent = nullptr );
    ~FinishedViewStep() override;

    QString prettyName() const override;

    QWidget* widget() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    void onActivate() override;

    ClaudeInstall::JobList jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    Config* m_config;
    FinishedPage* m_widget;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( FinishedViewStepFactory )

#endif
