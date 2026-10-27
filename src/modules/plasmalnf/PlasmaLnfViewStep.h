/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PLASMALNFVIEWSTEP_H
#define PLASMALNFVIEWSTEP_H

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

class Config;
class PlasmaLnfPage;

class PLUGINDLLEXPORT PlasmaLnfViewStep : public ClaudeInstall::ViewStep
{
    Q_OBJECT

public:
    explicit PlasmaLnfViewStep( QObject* parent = nullptr );
    ~PlasmaLnfViewStep() override;

    QString prettyName() const override;

    QWidget* widget() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    void onLeave() override;

    ClaudeInstall::JobList jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    Config* m_config;
    PlasmaLnfPage* m_widget;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( PlasmaLnfViewStepFactory )

#endif  // PLASMALNFVIEWSTEP_H
