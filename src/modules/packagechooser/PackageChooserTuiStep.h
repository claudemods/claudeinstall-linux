/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PACKAGECHOOSERTUISTEP_H
#define PACKAGECHOOSERTUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

#include <vector>

/**
 * @brief The package chooser in the terminal
 *
 * A list of the packages from the Config's model, with the one the cursor
 * is on described underneath (there are no screenshots). Up and down move,
 * space or enter chooses. In the single-choice modes choosing one unchooses
 * the others, as the GUI list does.
 */
class PLUGINDLLEXPORT PackageChooserTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit PackageChooserTuiStep( QObject* parent = nullptr );
    ~PackageChooserTuiStep() override;

    QString prettyName() const override;

    ftxui::Component createComponent() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;
    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

    void onActivate() override;
    void onLeave() override;
    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    bool multiple() const;
    bool hasSelection() const;
    QStringList selectedIds() const;
    void toggle( int row );

    Config* m_config;
    std::vector< char > m_chosen;  ///< a row is chosen; as char to get its address
    int m_cursor = 0;
    bool m_defaultApplied = false;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( PackageChooserTuiStepFactory )

#endif
