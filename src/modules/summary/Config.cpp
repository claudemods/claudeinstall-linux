/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2020, Camilo Higuita <milo.h@aol.com>
 *   SPDX-FileCopyrightText: 2021 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Config.h"

#include "SummaryModel.h"

#include "Settings.h"
#include "Wizard.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"

Config::Config( QObject* parent )
    : QObject( parent )
    , m_summary( new SummaryModel( this ) )

{
    CLAUDEINSTALL_RETRANSLATE_SLOT( &Config::retranslate );
    retranslate();
}

void
Config::retranslate()
{
    m_title = tr( "Summary", "@label" );

    if ( ClaudeInstall::Settings::instance()->isSetupMode() )
    {
        m_message = tr( "This is an overview of what will happen once you start "
                        "the setup procedure." );
    }
    else
    {
        m_message = tr( "This is an overview of what will happen once you start "
                        "the install procedure." );
    }
    Q_EMIT titleChanged( m_title );
    Q_EMIT messageChanged( m_message );
}

ClaudeInstall::StepList
Config::stepsForSummary( const ClaudeInstall::Step* upToHere )
{
    ClaudeInstall::StepList steps;
    const auto* wizard = ClaudeInstall::Wizard::active();
    if ( !wizard )
    {
        return steps;
    }

    for ( ClaudeInstall::Step* step : wizard->steps() )
    {
        // *Assume* that if there's an exec step in the sequence,
        // we don't need a summary for steps before it. This works in
        // practice if there's a summary step before each exec --
        // and in practice, there's only one of each.
        if ( step->isExecutionStep() )
        {
            steps.clear();
            continue;
        }

        // Having reached the parent view-step of the Config object,
        // we know we're providing a summary of steps up until this
        // view step, so we now have steps since the previous exec, up
        // to this summary.
        if ( upToHere == step )
        {
            break;
        }

        steps.append( step );
    }
    return steps;
}


void
Config::collectSummaries( const ClaudeInstall::Step* upToHere, Widgets withWidgets )
{
    m_summary->setSummaryList( stepsForSummary( upToHere ), withWidgets == Widgets::Enabled );
}

void
Config::clearSummaries()
{
    m_summary->setSummaryList( {}, false );
}
