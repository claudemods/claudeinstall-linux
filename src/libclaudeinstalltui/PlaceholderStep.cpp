/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "PlaceholderStep.h"

#include <ftxui/dom/elements.hpp>

namespace ClaudeInstall
{

PlaceholderStep::PlaceholderStep( const QString& moduleName, QObject* parent )
    : TuiStep( parent )
    , m_moduleName( moduleName )
{
}

PlaceholderStep::~PlaceholderStep() {}

QString
PlaceholderStep::prettyName() const
{
    return m_moduleName;
}

QString
PlaceholderStep::prettyStatus() const
{
    return tr( "This step is not available in text mode.", "@status" );
}

ftxui::Component
PlaceholderStep::createComponent()
{
    const std::string heading = m_moduleName.toStdString();
    const std::string body = prettyStatus().toStdString();
    return ftxui::Renderer(
        [ heading, body ]
        {
            using namespace ftxui;
            return vbox( { text( heading ) | bold, separator(), paragraph( body ) } );
        } );
}

}  // namespace ClaudeInstall
