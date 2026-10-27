/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "LocaleTuiStep.h"

#include "TuiWidgets.h"

#include "utils/RAII.h"

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( LocaleTuiStepFactory, registerPlugin< LocaleTuiStep >(); )

using namespace ClaudeInstall::Locale;

LocaleTuiStep::LocaleTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
}

LocaleTuiStep::~LocaleTuiStep() {}

QString
LocaleTuiStep::prettyName() const
{
    return tr( "Location", "@label" );
}

QString
LocaleTuiStep::prettyStatus() const
{
    return m_config->prettyStatus();
}

QObject*
LocaleTuiStep::config()
{
    return m_config;
}

bool
LocaleTuiStep::isNextEnabled() const
{
    return true;
}

bool
LocaleTuiStep::isBackEnabled() const
{
    return true;
}

bool
LocaleTuiStep::isAtBeginning() const
{
    return true;
}

bool
LocaleTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
LocaleTuiStep::jobs() const
{
    return m_config->createJobs();
}

void
LocaleTuiStep::onActivate()
{
    m_config->setCurrentLocation();  // Finalize the location
    if ( m_regionList )
    {
        sync( true );
    }
}

void
LocaleTuiStep::onLeave()
{
    m_config->finalizeGlobalStorage();
}

void
LocaleTuiStep::onCancel()
{
    m_config->cancel();
}

void
LocaleTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

void
LocaleTuiStep::fillZones()
{
    auto* zones = m_config->regionalZonesModel();
    ClaudeInstall::Tui::fillEntries( &m_zones, zones, ZonesModel::NameRole );
    m_zoneKeys.clear();
    for ( int i = 0; i < zones->rowCount(); ++i )
    {
        m_zoneKeys.append( zones->data( zones->index( i, 0 ), ZonesModel::KeyRole ).toString() );
    }
}

/// @brief The index of the entry for @p code in @p list: the exact one, else one that starts with it
static int
indexOfCode( const QStringList& list, const QString& code )
{
    int i = list.indexOf( code );
    if ( i < 0 && !code.isEmpty() )
    {
        for ( int j = 0; j < list.count() && i < 0; ++j )
        {
            if ( list.at( j ).startsWith( code ) )
            {
                i = j;
            }
        }
    }
    return i;
}

void
LocaleTuiStep::sync( bool force )
{
    cScopedAssignment syncing( &m_syncing, true, false );

    // The time zone
    if ( force || !( m_regionList->Focused() || m_zoneList->Focused() ) )
    {
        if ( const auto* location = m_config->currentLocation() )
        {
            const int r = m_regionKeys.indexOf( location->region() );
            if ( r >= 0 )
            {
                m_region = r;
                auto* zones = m_config->regionalZonesModel();
                if ( zones->region() != location->region() || m_zoneKeys.isEmpty() )
                {
                    zones->setRegion( location->region() );
                    fillZones();
                }
                const int z = m_zoneKeys.indexOf( location->zone() );
                m_zone = z >= 0 ? z : 0;
            }
        }
    }

    // Language and formats (the lists of codes are the Config's)
    const QStringList codes = m_config->supportedLocales();
    if ( force || !m_languageList->Focused() )
    {
        const int i = indexOfCode( codes, m_config->currentLanguageCode() );
        if ( i >= 0 )
        {
            m_language = i;
        }
    }
    if ( force || !m_formatList->Focused() )
    {
        const int i = indexOfCode( codes, m_config->currentLCCode() );
        if ( i >= 0 )
        {
            m_format = i;
        }
    }
}

void
LocaleTuiStep::regionChanged()
{
    if ( m_syncing || m_region < 0 || m_region >= m_regionKeys.count() )
    {
        return;
    }
    m_config->regionalZonesModel()->setRegion( m_regionKeys.at( m_region ) );
    fillZones();
    m_zone = 0;
    zoneChanged();
}

void
LocaleTuiStep::zoneChanged()
{
    if ( m_syncing || m_region < 0 || m_region >= m_regionKeys.count() || m_zone < 0 || m_zone >= m_zoneKeys.count() )
    {
        return;
    }
    m_config->setCurrentLocation( m_regionKeys.at( m_region ), m_zoneKeys.at( m_zone ) );
}

ftxui::Component
LocaleTuiStep::createComponent()
{
    using namespace ftxui;
    namespace Tui = ClaudeInstall::Tui;

    m_config->setCurrentLocation();

    auto* regions = m_config->regionModel();
    Tui::fillEntries( &m_regions, regions, RegionsModel::NameRole );
    for ( int i = 0; i < regions->rowCount( QModelIndex() ); ++i )
    {
        m_regionKeys.append( regions->data( regions->index( i, 0 ), RegionsModel::KeyRole ).toString() );
    }
    const QStringList codes = m_config->supportedLocales();
    for ( const QString& code : codes )
    {
        m_languages.push_back( code.toStdString() );
    }
    m_formats = m_languages;

    // One list: the region. Enter chooses it and goes to the next page (Ctrl-N, see TuiApplication).
    // The zone follows from the region (the detected one if it is in that region).
    m_regionList = Tui::picker( tr( "Region", "@label" ),
                                &m_regions,
                                &m_region,
                                [ this ] { regionChanged(); },
                                []
                                {
                                    if ( auto* screen = ftxui::ScreenInteractive::Active() )
                                    {
                                        screen->PostEvent( ftxui::Event::Special( "\x0E" ) );
                                    }
                                } );
    m_zoneList = Tui::picker( tr( "Zone", "@label" ), &m_zones, &m_zone, [ this ] { zoneChanged(); } );
    m_languageList = Tui::picker( tr( "System language", "@label" ),
                                  &m_languages,
                                  &m_language,
                                  [ this ]
                                  {
                                      if ( !m_syncing && m_language >= 0 && size_t( m_language ) < m_languages.size() )
                                      {
                                          m_config->setLanguageExplicitly(
                                              QString::fromStdString( m_languages[ size_t( m_language ) ] ) );
                                      }
                                  } );
    m_formatList = Tui::picker( tr( "Numbers and dates", "@label" ),
                                &m_formats,
                                &m_format,
                                [ this ]
                                {
                                    if ( !m_syncing && m_format >= 0 && size_t( m_format ) < m_formats.size() )
                                    {
                                        m_config->setLCLocaleExplicitly(
                                            QString::fromStdString( m_formats[ size_t( m_format ) ] ) );
                                    }
                                } );
    sync( true );

    // Only the region is asked. The zone, the system language and the format of numbers and
    // dates follow from it and from the language chosen on the first page.
    return Renderer( m_regionList,
                     [ this ]
                     {
                         sync( false );
                         std::string zone;
                         if ( const auto* location = m_config->currentLocation() )
                         {
                             zone = ( location->region() + QLatin1Char( '/' ) + location->zone() ).toStdString();
                         }
                         return vbox( { m_regionList->Render() | flex,
                                        text( " " + tr( "Time zone:", "@label" ).toStdString() + " " + zone ) | dim } );
                     } );
}
