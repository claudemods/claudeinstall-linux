/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "WelcomeTuiStep.h"

#include "BrandingInfo.h"
#include "ModuleLoadState.h"
#include "TuiWidgets.h"
#include "modulesystem/RequirementsModel.h"

#include <algorithm>

#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( WelcomeTuiStepFactory, registerPlugin< WelcomeTuiStep >(); )

WelcomeTuiStep::WelcomeTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
    // The loader says when the requirements have been checked
    connect( ClaudeInstall::ModuleLoadState::instance(),
             &ClaudeInstall::ModuleLoadState::requirementsComplete,
             this,
             &WelcomeTuiStep::nextStatusChanged );
}

WelcomeTuiStep::~WelcomeTuiStep() {}

QString
WelcomeTuiStep::prettyName() const
{
    return tr( "Welcome", "@title" );
}

QObject*
WelcomeTuiStep::config()
{
    return m_config;
}

bool
WelcomeTuiStep::isNextEnabled() const
{
    const auto* requirements = m_config->requirementsModel();
    return requirements && requirements->satisfiedMandatory();
}

bool
WelcomeTuiStep::isBackEnabled() const
{
    return false;
}

bool
WelcomeTuiStep::isAtBeginning() const
{
    return true;
}

bool
WelcomeTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
WelcomeTuiStep::jobs() const
{
    return ClaudeInstall::JobList();
}

void
WelcomeTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

ClaudeInstall::RequirementsList
WelcomeTuiStep::checkRequirements()
{
    return m_config->checkRequirements();
}

ftxui::Component
WelcomeTuiStep::createComponent()
{
    // The menu of languages; its entries are the names the Config knows.
    // On the Linux console only the names its font can draw (Latin scripts) are listed.
    auto* languages = m_config->languagesModel();
    m_languageNames.clear();
    m_languageRows.clear();
    for ( int i = 0; i < languages->rowCount( QModelIndex() ); ++i )
    {
        const QString label
            = languages->data( languages->index( i ), ClaudeInstall::Locale::TranslationsModel::LabelRole ).toString();
        if ( ClaudeInstall::Tui::isConsole() && i != m_config->localeIndex() && !ClaudeInstall::Tui::drawableOnConsole( label ) )
        {
            continue;
        }
        m_languageNames.push_back( label.toStdString() );
        m_languageRows.push_back( i );
    }
    auto positionOf = [ this ]( int row )
    {
        const auto it = std::find( m_languageRows.begin(), m_languageRows.end(), row );
        return it == m_languageRows.end() ? 0 : int( it - m_languageRows.begin() );
    };
    m_languageSelected = positionOf( m_config->localeIndex() );

    ftxui::MenuOption option = ftxui::MenuOption::Vertical();
    option.on_change = [ this ]
    {
        if ( m_languageSelected >= 0 && m_languageSelected < int( m_languageRows.size() ) )
        {
            m_config->setLocaleIndex( m_languageRows[ m_languageSelected ] );
        }
    };
    // Enter (or space) chooses the highlighted language and goes to the next page
    option.on_enter = []
    {
        if ( auto* screen = ftxui::ScreenInteractive::Active() )
        {
            screen->PostEvent( ftxui::Event::Special( "\x0E" ) );  // Ctrl-N, see TuiApplication
        }
    };
    auto menu = ClaudeInstall::Tui::acceptsSpace( ftxui::Menu( &m_languageNames, &m_languageSelected, option ) );

    return ftxui::Renderer(
        menu,
        [ this, menu, positionOf ]
        {
            using namespace ftxui;

            // The language may be changed from elsewhere (e.g. by a location lookup)
            const int wanted = positionOf( m_config->localeIndex() );
            if ( m_languageSelected < 0 || m_languageSelected >= int( m_languageRows.size() )
                 || m_languageRows[ m_languageSelected ] != m_config->localeIndex() )
            {
                m_languageSelected = wanted;
            }

            Elements problems;
            const auto* requirements = m_config->requirementsModel();
            if ( requirements )
            {
                for ( int i = 0; i < requirements->rowCount( QModelIndex() ); ++i )
                {
                    const auto index = requirements->index( i );
                    if ( requirements->data( index, ClaudeInstall::RequirementsModel::Satisfied ).toBool() )
                    {
                        continue;
                    }
                    const bool mandatory = requirements->data( index, ClaudeInstall::RequirementsModel::Mandatory ).toBool();
                    const std::string message
                        = requirements->data( index, ClaudeInstall::RequirementsModel::NegatedText ).toString().toStdString();
                    Element line = hbox( { text( ClaudeInstall::Tui::symbol( false, !mandatory ) ), paragraph( message ) } );
                    problems.push_back( mandatory ? ( line | color( Color::Red ) ) : ( line | color( Color::Yellow ) ) );
                }
            }
            if ( problems.empty() )
            {
                const bool checked = requirements && requirements->satisfiedRequirements();
                problems.push_back( text( checked ? ClaudeInstall::Tui::symbol( true )
                                                        + tr( "This computer satisfies the requirements.", "@label" ).toStdString()
                                                  : " " + tr( "Checking the requirements...", "@label" ).toStdString() )
                                    | dim );
            }

            // The message is a template with HTML ("<h1>Welcome to the %1 installer</h1>")
            const QString product = ClaudeInstall::BrandingInfo::instance().versionedName();
            const QString welcome
                = ClaudeInstall::Tui::stripMarkup( m_config->genericWelcomeMessage().arg( product ) ).trimmed();

            // One column, top to bottom: the greeting, what is wrong (if anything), and then
            // the language list using the whole width and the rest of the height.
            // The list is the only thing to operate here, so it always has the keyboard focus.
            // Enter / Next do nothing while a required check fails: say so instead of staying silent
            Elements blocked;
            if ( !isNextEnabled() && requirements && requirements->satisfiedRequirements() == false
                 && !requirements->satisfiedMandatory() )
            {
                blocked.push_back( paragraph( tr( "Next is disabled until the red item above is fixed.", "@label" ).toStdString() )
                                   | color( Color::Red ) | bold );
            }

            return vbox( { paragraph( welcome.toStdString() ) | bold,
                           separator(),
                           vbox( problems ),
                           vbox( blocked ),
                           separator(),
                           text( " " + tr( "Choose your language: Up / Down, then Enter", "@title" ).toStdString() )
                               | bold,
                           menu->Render() | vscroll_indicator | frame | flex } );
        } );
}
