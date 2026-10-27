/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TUISTEP_H
#define TUISTEP_H

#include "DllMacro.h"
#include "Step.h"

#include <ftxui/component/component.hpp>

#include <QString>

namespace ClaudeInstall
{

/**
 * @brief A step of the wizard that is shown in a terminal
 *
 * This is the text-mode counterpart of ViewStep: a Step (navigation, jobs,
 * summary text) plus the FTXUI component that is the step's page. A text-mode
 * module is a plugin whose factory creates a TuiStep, normally one that
 * shares its Config object (and the jobs) with the module's GUI step by using
 * the module's `<module>_config` library.
 *
 * The component is built once, when the application first needs it, and the
 * page is then shown whenever the step is current. The page reads from
 * the Config and writes to it as the user edits; there are no signals
 * to connect, since the application redraws regularly.
 */
class TUIDLLEXPORT TuiStep : public Step
{
    Q_OBJECT
public:
    explicit TuiStep( QObject* parent = nullptr );
    ~TuiStep() override;

    /** @brief Builds the page of this step
     *
     * Called once. The page should be a component that can take keyboard
     * focus (a menu, an input, a container of these); the application
     * passes events to it only while this step is current. For a page that
     * has nothing to interact with, return `ftxui::Renderer()` of the
     * text.
     */
    virtual ftxui::Component createComponent() = 0;

    /** @brief The page of this step; created on first use */
    ftxui::Component component();

private:
    ftxui::Component m_component;
};

using TuiStepList = QList< TuiStep* >;

}  // namespace ClaudeInstall

#endif
