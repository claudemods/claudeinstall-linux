/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef USERSTUISTEP_H
#define USERSTUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

#include <string>

/**
 * @brief The user-setup page in the terminal
 *
 * Fields for the name, login name, host name and passwords, shown as the
 * Config allows (the root password only if it is written and not the same as
 * the user's). The active-directory options of the GUI page are not offered.
 */
class PLUGINDLLEXPORT UsersTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit UsersTuiStep( QObject* parent = nullptr );
    ~UsersTuiStep() override;

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

    // What the fields edit
    std::string m_fullName;
    std::string m_loginName;
    std::string m_hostName;
    std::string m_password;
    std::string m_passwordAgain;
    std::string m_rootPassword;
    std::string m_rootPasswordAgain;
    bool m_autoLogin = false;
    bool m_reuseForRoot = false;
    bool m_requireStrong = true;
    bool m_showRoot = false;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( UsersTuiStepFactory )

#endif
