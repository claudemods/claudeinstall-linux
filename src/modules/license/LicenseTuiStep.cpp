/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "LicenseTuiStep.h"

#include "TuiWidgets.h"

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( LicenseTuiStepFactory, registerPlugin< LicenseTuiStep >(); )

LicenseTuiStep::LicenseTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
    connect( m_config, &Config::isNextEnabledChanged, this, &LicenseTuiStep::nextStatusChanged );
}

LicenseTuiStep::~LicenseTuiStep() {}

QString
LicenseTuiStep::prettyName() const
{
    return tr( "License", "@label" );
}

QObject*
LicenseTuiStep::config()
{
    return m_config;
}

bool
LicenseTuiStep::isNextEnabled() const
{
    return m_config->isNextEnabled();
}

bool
LicenseTuiStep::isBackEnabled() const
{
    return true;
}

bool
LicenseTuiStep::isAtBeginning() const
{
    return true;
}

bool
LicenseTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
LicenseTuiStep::jobs() const
{
    return ClaudeInstall::JobList();
}

void
LicenseTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

ftxui::Component
LicenseTuiStep::createComponent()
{
    using namespace ftxui;

    m_accepted = m_config->accepted();
    Component accept = ClaudeInstall::Tui::check( m_config->allLicensesOptional()
                                                      ? tr( "I accept these terms", "@label" )
                                                      : tr( "I accept these terms (required to continue)", "@label" ),
                                                  &m_accepted,
                                                  [ this ] { m_config->setAccepted( m_accepted ); } );

    return Renderer( accept,
                     [ this, accept ]
                     {
                         Elements lines;
                         for ( const LicenseEntry& entry : m_config->entries() )
                         {
                             QString heading = entry.m_prettyName;
                             if ( !entry.m_prettyVendor.isEmpty() )
                             {
                                 heading += QStringLiteral( " — " ) + entry.m_prettyVendor;
                             }
                             if ( entry.isRequired() )
                             {
                                 heading += QStringLiteral( " *" );
                             }
                             lines.push_back( ftxui::text( heading.toStdString() ) | bold );
                             lines.push_back( ftxui::text( "  " + entry.m_url.toString().toStdString() ) | dim );
                             lines.push_back( ftxui::text( "" ) );
                         }
                         if ( lines.empty() )
                         {
                             lines.push_back( ClaudeInstall::Tui::paragraphOf( tr( "There are no licenses to accept.", "@label" ) ) );
                         }
                         return ClaudeInstall::Tui::titled(
                             tr( "License Agreement", "@title" ),
                             vbox( { ClaudeInstall::Tui::paragraphOf(
                                         tr( "Please read the terms at the addresses below; "
                                             "they cannot be shown in the terminal.",
                                             "@label" ) ),
                                     text( "" ),
                                     vbox( lines ) | flex,
                                     accept->Render() } ) );
                     } );
}
