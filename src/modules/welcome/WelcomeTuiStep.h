/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef WELCOMETUISTEP_H
#define WELCOMETUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

#include <string>
#include <vector>

/**
 * @brief The welcome page in the terminal
 *
 * It shows the welcome message, the language to choose and what the
 * computer lacks (the requirements). All of that comes from the Config,
 * which is the same object the GUI page uses.
 */
class PLUGINDLLEXPORT WelcomeTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit WelcomeTuiStep( QObject* parent = nullptr );
    ~WelcomeTuiStep() override;

    QString prettyName() const override;

    ftxui::Component createComponent() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

    ClaudeInstall::RequirementsList checkRequirements() override;

private:
    Config* m_config;

    std::vector< std::string > m_languageNames;
    std::vector< int > m_languageRows;  ///< model row of each entry of m_languageNames
    int m_languageSelected = 0;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( WelcomeTuiStepFactory )

#endif
