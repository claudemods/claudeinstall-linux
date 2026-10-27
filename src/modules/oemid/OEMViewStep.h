/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef OEMVIEWSTEP_H
#define OEMVIEWSTEP_H

#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

#include "DllMacro.h"

#include <QVariantMap>

class Config;
class OEMPage;

class PLUGINDLLEXPORT OEMViewStep : public ClaudeInstall::ViewStep
{
    Q_OBJECT

public:
    explicit OEMViewStep( QObject* parent = nullptr );
    ~OEMViewStep() override;

    QString prettyName() const override;
    QString prettyStatus() const override;

    QWidget* widget() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    void onActivate() override;
    void onLeave() override;

    ClaudeInstall::JobList jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    Config* m_config;
    OEMPage* m_widget;
    bool m_visited;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( OEMViewStepFactory )

#endif
