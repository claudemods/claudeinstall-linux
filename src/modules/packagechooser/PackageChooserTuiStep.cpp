/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "PackageChooserTuiStep.h"

#include "TuiWidgets.h"

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <algorithm>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( PackageChooserTuiStepFactory, registerPlugin< PackageChooserTuiStep >(); )

PackageChooserTuiStep::PackageChooserTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
}

PackageChooserTuiStep::~PackageChooserTuiStep() {}

QString
PackageChooserTuiStep::prettyName() const
{
    return m_config->prettyName();
}

QObject*
PackageChooserTuiStep::config()
{
    return m_config;
}

bool
PackageChooserTuiStep::multiple() const
{
    return m_config->mode() == PackageChooserMode::OptionalMultiple
        || m_config->mode() == PackageChooserMode::RequiredMultiple;
}

bool
PackageChooserTuiStep::hasSelection() const
{
    for ( char c : m_chosen )
    {
        if ( c )
        {
            return true;
        }
    }
    return false;
}

QStringList
PackageChooserTuiStep::selectedIds() const
{
    QStringList ids;
    const auto* model = m_config->model();
    for ( int i = 0; model && i < model->packageCount() && size_t( i ) < m_chosen.size(); ++i )
    {
        const QString id = model->packageData( i ).id;
        if ( m_chosen[ size_t( i ) ] && !id.isEmpty() )  // the "none" package has no id
        {
            ids.append( id );
        }
    }
    return ids;
}

bool
PackageChooserTuiStep::isNextEnabled() const
{
    switch ( m_config->mode() )
    {
    case PackageChooserMode::Optional:
    case PackageChooserMode::OptionalMultiple:
        return true;
    case PackageChooserMode::Required:
    case PackageChooserMode::RequiredMultiple:
        return hasSelection();
    }
    return true;
}

bool
PackageChooserTuiStep::isBackEnabled() const
{
    return true;
}

bool
PackageChooserTuiStep::isAtBeginning() const
{
    return true;
}

bool
PackageChooserTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
PackageChooserTuiStep::jobs() const
{
    return {};
}

void
PackageChooserTuiStep::onActivate()
{
    // Nothing chosen yet: take the default of the configuration
    const auto* model = m_config->model();
    if ( !m_defaultApplied && !hasSelection() && model )
    {
        const auto index = m_config->defaultSelectionIndex();
        if ( index.isValid() && size_t( index.row() ) < m_chosen.size() )
        {
            m_chosen[ size_t( index.row() ) ] = 1;
            m_cursor = index.row();
        }
    }
    // A required choice with no default: start on the first entry so Next is never stuck disabled
    if ( !m_defaultApplied && !hasSelection() && model && model->packageCount() > 0 && !multiple()
         && m_config->mode() == PackageChooserMode::Required && !m_chosen.empty() )
    {
        m_chosen[ 0 ] = 1;
        m_cursor = 0;
    }
    m_defaultApplied = true;
    emit nextStatusChanged( isNextEnabled() );
}

void
PackageChooserTuiStep::onLeave()
{
    m_config->updateGlobalStorage( selectedIds() );
}

void
PackageChooserTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setDefaultId( moduleInstanceKey() );
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

void
PackageChooserTuiStep::toggle( int row )
{
    if ( row < 0 || size_t( row ) >= m_chosen.size() )
    {
        return;
    }
    const bool was = m_chosen[ size_t( row ) ];
    if ( !multiple() )
    {
        // Exactly one, or at most one: choosing another replaces
        std::fill( m_chosen.begin(), m_chosen.end(), 0 );
        // Optional single choice can be unchosen by choosing again
        m_chosen[ size_t( row ) ] = ( was && m_config->mode() == PackageChooserMode::Optional ) ? 0 : 1;
    }
    else
    {
        m_chosen[ size_t( row ) ] = was ? 0 : 1;
    }
    emit nextStatusChanged( isNextEnabled() );
}

ftxui::Component
PackageChooserTuiStep::createComponent()
{
    using namespace ftxui;

    const auto* model = m_config->model();
    m_chosen.assign( model ? size_t( model->packageCount() ) : 0, 0 );

    Component view = Renderer(
        [ this ]
        {
            const auto* packages = m_config->model();
            Elements rows;
            for ( int i = 0; packages && i < packages->packageCount(); ++i )
            {
                const bool chosen = m_chosen[ size_t( i ) ];
                const std::string mark = multiple() ? ( chosen ? "[x] " : "[ ] " ) : ( chosen ? "(*) " : "( ) " );
                Element row = text( mark + packages->packageData( i ).name.get().toStdString() );
                if ( i == m_cursor )
                {
                    row = row | inverted | focus;
                }
                rows.push_back( row );
            }

            if ( rows.empty() )
            {
                rows.push_back( text( tr( "No choices were loaded for this page. See the log file for the reason.", "@label" ).toStdString() )
                                | color( Color::Red ) );
            }

            // The description: of the item under the cursor, or of the module as a whole
            const PackageItem* shown = &m_config->introductionPackage();
            if ( packages && m_cursor >= 0 && m_cursor < packages->packageCount() )
            {
                shown = &packages->packageData( m_cursor );
            }
            return ClaudeInstall::Tui::titled(
                m_config->prettyName(),
                vbox( { vbox( rows ) | vscroll_indicator | yframe | flex,
                        separator(),
                        text( shown->name.get().toStdString() ) | bold,
                        ClaudeInstall::Tui::paragraphOf( shown->description.get() ),
                        text( ( tr( "Up and down move; space chooses; Enter chooses and goes on.", "@label" ) ).toStdString() ) | dim } ) );
        } );

    return CatchEvent( view,
                       [ this ]( Event event )
                       {
                           const int count = int( m_chosen.size() );
                           if ( event == Event::ArrowUp )
                           {
                               m_cursor = qMax( 0, m_cursor - 1 );
                           }
                           else if ( event == Event::ArrowDown )
                           {
                               m_cursor = qMin( qMax( 0, count - 1 ), m_cursor + 1 );
                           }
                           else if ( event == Event::Character( ' ' ) )
                           {
                               toggle( m_cursor );
                           }
                           else if ( event == Event::Return )
                           {
                               // Enter picks the highlighted entry and goes to the next page (as the welcome page does)
                               if ( !multiple() && m_cursor >= 0 && size_t( m_cursor ) < m_chosen.size() && !m_chosen[ size_t( m_cursor ) ] )
                               {
                                   toggle( m_cursor );
                               }
                               else if ( multiple() )
                               {
                                   toggle( m_cursor );
                                   return true;
                               }
                               if ( isNextEnabled() )
                               {
                                   if ( auto* screen = ScreenInteractive::Active() )
                                   {
                                       screen->PostEvent( Event::Special( "\x0E" ) );  // Ctrl-N, see TuiApplication
                                   }
                               }
                           }
                           else
                           {
                               return false;
                           }
                           return true;
                       } );
}
