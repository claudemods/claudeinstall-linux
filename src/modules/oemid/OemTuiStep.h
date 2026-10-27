/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef OEMTUISTEP_H
#define OEMTUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

#include <string>

/// @brief The OEM batch identifier, as one text field in the terminal
class PLUGINDLLEXPORT OemTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit OemTuiStep( QObject* parent = nullptr );
    ~OemTuiStep() override;

    QString prettyName() const override;
    QString prettyStatus() const override;

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
    std::string m_identifier;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( OemTuiStepFactory )

#endif
