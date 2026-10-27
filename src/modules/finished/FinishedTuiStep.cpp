/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "FinishedTuiStep.h"

#include "TuiWidgets.h"

#include "JobQueue.h"

#include <QCoreApplication>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( FinishedTuiStepFactory, registerPlugin< FinishedTuiStep >(); )

FinishedTuiStep::FinishedTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
    if ( auto* jq = ClaudeInstall::JobQueue::instance() )
    {
        connect( jq, &ClaudeInstall::JobQueue::failed, m_config, &Config::onInstallationFailed );
    }
}

FinishedTuiStep::~FinishedTuiStep() {}

QString
FinishedTuiStep::prettyName() const
{
    return tr( "Finish", "@label" );
}

QObject*
FinishedTuiStep::config()
{
    return m_config;
}

bool
FinishedTuiStep::isNextEnabled() const
{
    return false;
}

bool
FinishedTuiStep::isBackEnabled() const
{
    return false;
}

bool
FinishedTuiStep::isAtBeginning() const
{
    return true;
}

bool
FinishedTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
FinishedTuiStep::jobs() const
{
    return ClaudeInstall::JobList();
}

void
FinishedTuiStep::onActivate()
{
    m_config->doNotify();
    // The system restarts when the application ends, if the user wants that
    connect( QCoreApplication::instance(),
             &QCoreApplication::aboutToQuit,
             m_config,
             qOverload<>( &Config::doRestart ),
             Qt::UniqueConnection );
}

void
FinishedTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

ftxui::Component
FinishedTuiStep::createComponent()
{
    using namespace ftxui;

    m_restart = m_config->restartNowWanted();
    const bool askRestart = m_config->restartNowMode() == Config::RestartMode::UserDefaultUnchecked
        || m_config->restartNowMode() == Config::RestartMode::UserDefaultChecked;

    Component restart = askRestart ? ClaudeInstall::Tui::check( tr( "Restart now", "@label" ),
                                                                 &m_restart,
                                                                 [ this ] { m_config->setRestartNowWanted( m_restart ); } )
                                   : Renderer( [] { return emptyElement(); } );

    return Renderer( restart,
                     [ this, restart, askRestart ]
                     {
                         if ( m_config->hasFailed() )
                         {
                             return ClaudeInstall::Tui::titled(
                                 tr( "Failed", "@title" ),
                                 vbox( { ClaudeInstall::Tui::problem( m_config->failureMessage() ),
                                         ClaudeInstall::Tui::paragraphOf( m_config->failureDetails() ) } ) );
                         }
                         Elements body { ClaudeInstall::Tui::paragraphOf( tr( "All done. The installation is complete.", "@label" ) ) };
                         if ( askRestart )
                         {
                             body.push_back( text( "" ) );
                             body.push_back( restart->Render() );
                         }
                         body.push_back( filler() );
                         body.push_back( text( tr( "Press F10 to finish.", "@label" ).toStdString() ) | dim );
                         return ClaudeInstall::Tui::titled( tr( "Done", "@title" ), vbox( body ) );
                     } );
}
