/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TRACKINGTUISTEP_H
#define TRACKINGTUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

/// @brief The feedback (tracking) choices in the terminal: one box per kind that the configuration allows
class PLUGINDLLEXPORT TrackingTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit TrackingTuiStep( QObject* parent = nullptr );
    ~TrackingTuiStep() override;

    QString prettyName() const override;

    ftxui::Component createComponent() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;
    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

    void onLeave() override;
    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    Config* m_config;

    bool m_install = false;
    bool m_machine = false;
    bool m_user = false;
    bool m_none = false;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( TrackingTuiStepFactory )

#endif
