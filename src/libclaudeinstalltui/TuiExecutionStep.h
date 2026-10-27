/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TUIEXECUTIONSTEP_H
#define TUIEXECUTIONSTEP_H

#include "DllMacro.h"
#include "ExecutionModel.h"
#include "TuiStep.h"

#include <QList>

namespace ClaudeInstall
{

class Module;

/**
 * @brief The *exec* phase in the terminal: a progress bar and the status message
 *
 * This is the text-mode ExecutionViewStep. Which modules' jobs run is
 * decided by the loader (the *exec* sequences in settings.conf), which
 * appends the modules here; the step then starts them when it becomes current.
 */
class TUIDLLEXPORT TuiExecutionStep : public TuiStep
{
    Q_OBJECT
public:
    explicit TuiExecutionStep( QObject* parent = nullptr );
    ~TuiExecutionStep() override;

    QString prettyName() const override;
    ftxui::Component createComponent() override;

    bool isNextEnabled() const override { return false; }
    bool isBackEnabled() const override { return false; }
    bool isAtBeginning() const override { return true; }
    bool isAtEnd() const override { return !m_model->running(); }

    void onActivate() override;
    JobList jobs() const override { return JobList(); }
    bool isExecutionStep() const override { return true; }

    /// @brief Adds the jobs of @p module, with its @p weight in the overall progress
    void appendModule( Module* module, int weight );

    ExecutionModel* model() const { return m_model; }

private:
    struct Entry
    {
        Module* module;
        int weight;
    };

    QList< Entry > m_entries;
    ExecutionModel* m_model;
};

}  // namespace ClaudeInstall

#endif
