/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef FINISHEDTUISTEP_H
#define FINISHEDTUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

/// @brief The last page in the terminal: the installation is done, and the system may restart
class PLUGINDLLEXPORT FinishedTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit FinishedTuiStep( QObject* parent = nullptr );
    ~FinishedTuiStep() override;

    QString prettyName() const override;

    ftxui::Component createComponent() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;
    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

    void onActivate() override;
    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    Config* m_config;
    bool m_restart = false;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( FinishedTuiStepFactory )

#endif
