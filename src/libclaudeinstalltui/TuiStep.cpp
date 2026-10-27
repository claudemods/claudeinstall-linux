/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "TuiStep.h"

namespace ClaudeInstall
{

TuiStep::TuiStep( QObject* parent )
    : Step( parent )
{
}

TuiStep::~TuiStep() {}

ftxui::Component
TuiStep::component()
{
    if ( !m_component )
    {
        m_component = createComponent();
    }
    return m_component;
}

}  // namespace ClaudeInstall
