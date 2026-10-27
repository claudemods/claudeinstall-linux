/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef SUMMARYTUISTEP_H
#define SUMMARYTUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

/**
 * @brief The summary in the terminal
 *
 * Shows the text of what each earlier step is going to do, from the
 * summary model. Steps that only have a summary *widget* (the GUI partition
 * page) show nothing here; they provide their text through
 * Step::summaryText() instead.
 */
class PLUGINDLLEXPORT SummaryTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit SummaryTuiStep( QObject* parent = nullptr );
    ~SummaryTuiStep() override;

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

private:
    Config* m_config;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( SummaryTuiStepFactory )

#endif
