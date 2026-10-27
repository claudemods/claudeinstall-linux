/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "TrackingTuiStep.h"

#include "TrackingJobs.h"
#include "TuiWidgets.h"

#include "utils/Logger.h"

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( TrackingTuiStepFactory, registerPlugin< TrackingTuiStep >(); )

TrackingTuiStep::TrackingTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
}

TrackingTuiStep::~TrackingTuiStep() {}

QString
TrackingTuiStep::prettyName() const
{
    return tr( "Feedback", "@title" );
}

QObject*
TrackingTuiStep::config()
{
    return m_config;
}

bool
TrackingTuiStep::isNextEnabled() const
{
    return true;
}

bool
TrackingTuiStep::isBackEnabled() const
{
    return true;
}

bool
TrackingTuiStep::isAtBeginning() const
{
    return true;
}

bool
TrackingTuiStep::isAtEnd() const
{
    return true;
}

void
TrackingTuiStep::onLeave()
{
    cDebug() << "Install tracking:" << m_config->installTracking()->isEnabled();
    cDebug() << Logger::SubEntry << "Machine tracking:" << m_config->machineTracking()->isEnabled();
    cDebug() << Logger::SubEntry << "   User tracking:" << m_config->userTracking()->isEnabled();
}

ClaudeInstall::JobList
TrackingTuiStep::jobs() const
{
    ClaudeInstall::JobList l;
    addJob( l, m_config->installTracking() );
    addJob( l, m_config->machineTracking() );
    addJob( l, m_config->userTracking() );
    return l;
}

void
TrackingTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

ftxui::Component
TrackingTuiStep::createComponent()
{
    using namespace ftxui;

    // Only the kinds that the configuration lets the user choose are offered
    Components boxes;
    auto addKind = [ & ]( TrackingStyleConfig* style, bool* value, const QString& label )
    {
        *value = style->isEnabled();
        if ( style->isConfigurable() )
        {
            boxes.push_back( ClaudeInstall::Tui::check( label, value, [ style, value ] { style->setTracking( *value ); } ) );
        }
    };
    addKind( m_config->installTracking(), &m_install, tr( "Send a note that this installation happened", "@label" ) );
    addKind( m_config->machineTracking(), &m_machine, tr( "Let this computer report back from time to time", "@label" ) );
    addKind( m_config->userTracking(), &m_user, tr( "Share information about how the user uses the system", "@label" ) );

    const bool anyKind = !boxes.empty();
    if ( anyKind )
    {
        boxes.push_back( ClaudeInstall::Tui::check( tr( "Do not send any feedback", "@label" ),
                                                    &m_none,
                                                    [ this ]
                                                    {
                                                        m_config->noTracking( m_none );
                                                        if ( m_none )
                                                        {
                                                            m_install = m_machine = m_user = false;
                                                        }
                                                    } ) );
    }

    Component form = boxes.empty() ? Renderer( [] { return emptyElement(); } ) : Container::Vertical( boxes );
    return Renderer( form,
                     [ this, form, anyKind ]
                     {
                         Elements policies;
                         const QString general = m_config->generalPolicy();
                         if ( !general.isEmpty() )
                         {
                             policies.push_back( text( ( tr( "More about feedback: ", "@label" ) + general ).toStdString() )
                                                 | dim );
                         }
                         return ClaudeInstall::Tui::titled(
                             tr( "Feedback", "@title" ),
                             vbox( { ClaudeInstall::Tui::paragraphOf(
                                         anyKind ? tr( "Choose what, if anything, may be reported.", "@label" )
                                                 : tr( "No feedback is configured for this installation.", "@label" ) ),
                                     text( "" ),
                                     form->Render(),
                                     filler(),
                                     vbox( policies ) } ) );
                     } );
}
