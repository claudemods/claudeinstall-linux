/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "OemTuiStep.h"

#include "TuiWidgets.h"

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( OemTuiStepFactory, registerPlugin< OemTuiStep >(); )

OemTuiStep::OemTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
}

OemTuiStep::~OemTuiStep() {}

QString
OemTuiStep::prettyName() const
{
    return tr( "OEM Configuration" );
}

QString
OemTuiStep::prettyStatus() const
{
    return m_config->prettyStatus();
}

QObject*
OemTuiStep::config()
{
    return m_config;
}

bool
OemTuiStep::isNextEnabled() const
{
    return true;
}

bool
OemTuiStep::isBackEnabled() const
{
    return true;
}

bool
OemTuiStep::isAtBeginning() const
{
    return true;
}

bool
OemTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
OemTuiStep::jobs() const
{
    return m_config->createJobs();
}

void
OemTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

ftxui::Component
OemTuiStep::createComponent()
{
    m_identifier = m_config->batchIdentifier().toStdString();
    auto input = ClaudeInstall::Tui::field(
        tr( "Batch identifier", "@label" ),
        &m_identifier,
        false,
        [ this ] { m_config->setBatchIdentifier( QString::fromStdString( m_identifier ) ); } );

    return ftxui::Renderer( input,
                            [ this, input ]
                            {
                                using namespace ftxui;
                                return ClaudeInstall::Tui::titled(
                                    prettyName(),
                                    vbox( { ClaudeInstall::Tui::paragraphOf(
                                                tr( "Enter an identifier for this batch of installations. "
                                                    "It may be left empty.",
                                                    "@label" ) ),
                                            text( "" ),
                                            input->Render() } ) );
                            } );
}
