/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TUIWIZARD_H
#define TUIWIZARD_H

#include "DllMacro.h"
#include "TuiStep.h"
#include "Wizard.h"

namespace ClaudeInstall
{

/**
 * @brief The text-mode frontend of the Wizard
 *
 * A terminal cannot block while it asks a question: the screen is
 * drawn by a loop that the wizard is called from. So the questions
 * of the Wizard ("really cancel?", "start the installation?") are asked
 * in two rounds. The first time the wizard asks, the answer is "no" and
 * the wizard emits cancelQuestion() or executionQuestion(); the application
 * shows the question and, if the user agrees, calls confirmCancelAndQuit() or
 * confirmExecutionAndNext(), which repeat the action with the answer "yes".
 */
class TUIDLLEXPORT TuiWizard : public Wizard
{
    Q_OBJECT
public:
    /// @brief The (single) wizard of the text-mode application
    static TuiWizard* instance();

    /// @brief Appends a step to the roster
    void addTuiStep( TuiStep* step );

    /// @brief The steps, as TuiStep
    TuiStepList tuiSteps() const;
    /// @brief The current step, or @c nullptr
    TuiStep* currentStep() const;

public slots:
    /// @brief The user said *yes* to the cancel question: quit
    void confirmCancelAndQuit();
    /// @brief The user said *yes* to the question before the exec phase: go on
    void confirmExecutionAndNext();

signals:
    void cancelQuestion();
    void executionQuestion();
    /// @brief The wizard ended; the application should stop its loop
    void exitRequested( int exitCode );

protected:
    bool confirmExecution() override;
    bool confirmCancel() override;
    void exitApplication( int exitCode ) override;
    /// @brief Does nothing: the application shows the failure (installationFailed()) and exits when told
    void showInstallationFailure( const QString& message, const QString& details ) override;

private:
    explicit TuiWizard( QObject* parent = nullptr );
    ~TuiWizard() override;

    static TuiWizard* s_instance;

    bool m_executionConfirmed = false;
    bool m_cancelConfirmed = false;
};

}  // namespace ClaudeInstall

#endif
