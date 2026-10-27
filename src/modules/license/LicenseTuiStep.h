/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LICENSETUISTEP_H
#define LICENSETUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

/// @brief The licenses in the terminal: what they are, where to read them, and one box to accept all
class PLUGINDLLEXPORT LicenseTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit LicenseTuiStep( QObject* parent = nullptr );
    ~LicenseTuiStep() override;

    QString prettyName() const override;

    ftxui::Component createComponent() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;
    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    Config* m_config;
    bool m_accepted = false;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( LicenseTuiStepFactory )

#endif
