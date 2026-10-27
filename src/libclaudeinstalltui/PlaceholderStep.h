/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PLACEHOLDERSTEP_H
#define PLACEHOLDERSTEP_H

#include "DllMacro.h"
#include "TuiStep.h"

namespace ClaudeInstall
{

/**
 * @brief Stands in for a view module that has no text-mode version
 *
 * The step tells the user that the page of the module is not available in
 * the terminal and lets them go on. It has no jobs: whatever the real step
 * would have configured stays at the defaults of its module.
 */
class TUIDLLEXPORT PlaceholderStep : public TuiStep
{
    Q_OBJECT
public:
    explicit PlaceholderStep( const QString& moduleName, QObject* parent = nullptr );
    ~PlaceholderStep() override;

    QString prettyName() const override;
    QString prettyStatus() const override;

    ftxui::Component createComponent() override;

    bool isNextEnabled() const override { return true; }
    bool isBackEnabled() const override { return true; }
    bool isAtBeginning() const override { return true; }
    bool isAtEnd() const override { return true; }

    JobList jobs() const override { return JobList(); }

private:
    QString m_moduleName;
};

}  // namespace ClaudeInstall

#endif
