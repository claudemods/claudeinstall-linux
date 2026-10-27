/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "SummaryTuiStep.h"

#include "TuiWidgets.h"

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( SummaryTuiStepFactory, registerPlugin< SummaryTuiStep >(); )

SummaryTuiStep::SummaryTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
}

SummaryTuiStep::~SummaryTuiStep() {}

QString
SummaryTuiStep::prettyName() const
{
    return m_config->title();
}

QObject*
SummaryTuiStep::config()
{
    return m_config;
}

bool
SummaryTuiStep::isNextEnabled() const
{
    return true;
}

bool
SummaryTuiStep::isBackEnabled() const
{
    return true;
}

bool
SummaryTuiStep::isAtBeginning() const
{
    return true;
}

bool
SummaryTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
SummaryTuiStep::jobs() const
{
    return {};
}

void
SummaryTuiStep::onActivate()
{
    // No widgets: a terminal cannot show them
    m_config->collectSummaries( this, Config::Widgets::Disabled );
}

void
SummaryTuiStep::onLeave()
{
    m_config->clearSummaries();
}

ftxui::Component
SummaryTuiStep::createComponent()
{
    return ClaudeInstall::Tui::scrolled(
        [ this ]
        {
            using namespace ftxui;
            Elements lines;
            auto* model = m_config->summaryModel();
            for ( int i = 0; i < model->rowCount(); ++i )
            {
                const auto index = model->index( i );
                const QString title = model->data( index, SummaryModel::TitleRole ).toString();
                const QString message = model->data( index, SummaryModel::MessageRole ).toString();
                if ( message.isEmpty() )
                {
                    continue;
                }
                lines.push_back( text( title.toStdString() ) | bold );
                lines.push_back( ClaudeInstall::Tui::paragraphOf( message ) );
                lines.push_back( text( "" ) );
            }
            if ( lines.empty() )
            {
                lines.push_back( ClaudeInstall::Tui::paragraphOf( tr( "Nothing is going to be changed.", "@label" ) ) );
            }
            return ClaudeInstall::Tui::titled( m_config->message(), vbox( lines ) );
        } );
}
