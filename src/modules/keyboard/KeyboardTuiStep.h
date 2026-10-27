/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef KEYBOARDTUISTEP_H
#define KEYBOARDTUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

#include <string>
#include <vector>

/**
 * @brief The keyboard page in the terminal
 *
 * Four lists: layout, variant, physical model and the key combination for
 * switching between layouts. There is no keyboard preview. The lists show the
 * selection that the Config's models keep; changing a list changes the model.
 */
class PLUGINDLLEXPORT KeyboardTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit KeyboardTuiStep( QObject* parent = nullptr );
    ~KeyboardTuiStep() override;

    QString prettyName() const override;
    QString prettyStatus() const override;

    ftxui::Component createComponent() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;
    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

    void onActivate() override;
    void onLeave() override;
    void onCancel() override;
    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    /// @brief Makes the lists show the models
    void sync();

    Config* m_config;

    std::vector< std::string > m_layouts;
    std::vector< std::string > m_variants;
    std::vector< std::string > m_models;
    std::vector< std::string > m_groups;
    int m_layout = 0;
    int m_variant = 0;
    int m_model = 0;
    int m_group = 0;

    bool m_syncing = false;  ///< ignore the lists' change notices while they are being set
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( KeyboardTuiStepFactory )

#endif
