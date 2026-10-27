/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "WelcomeTuiStep.h"

#include "ModuleLoadState.h"
#include "modulesystem/RequirementsModel.h"

#include <ftxui/component/component_options.hpp>
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
    // The menu of languages; its entries are the names the Config knows
    auto* languages = m_config->languagesModel();
    m_languageNames.clear();
    for ( int i = 0; i < languages->rowCount( QModelIndex() ); ++i )
    {
        m_languageNames.push_back(
            languages->data( languages->index( i ), ClaudeInstall::Locale::TranslationsModel::LabelRole ).toString().toStdString() );
    }
    m_languageSelected = m_config->localeIndex();

    ftxui::MenuOption option = ftxui::MenuOption::Vertical();
    option.on_change = [ this ] { m_config->setLocaleIndex( m_languageSelected ); };
    auto menu = ftxui::Menu( &m_languageNames, &m_languageSelected, option );

    return ftxui::Renderer(
        menu,
        [ this, menu ]
        {
            using namespace ftxui;

            // The language may be changed from elsewhere (e.g. by a location lookup)
            if ( m_config->localeIndex() != m_languageSelected )
            {
                m_languageSelected = m_config->localeIndex();
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
                    Element line = hbox( { text( mandatory ? " ✗ " : " ! " ), paragraph( message ) } );
                    problems.push_back( mandatory ? ( line | color( Color::Red ) ) : ( line | color( Color::Yellow ) ) );
                }
            }
            if ( problems.empty() )
            {
                const bool checked = requirements && requirements->satisfiedRequirements();
                problems.push_back( text( checked ? tr( " ✓ This computer satisfies the requirements.", "@label" ).toStdString()
                                                  : tr( " Checking the requirements…", "@label" ).toStdString() )
                                    | dim );
            }

            return hbox( { vbox( { paragraph( m_config->genericWelcomeMessage().toStdString() ),
                                   separator(),
                                   text( tr( "Requirements", "@title" ).toStdString() ) | bold,
                                   vbox( problems ) } )
                               | flex,
                           separator(),
                           vbox( { text( tr( "Language", "@title" ).toStdString() ) | bold,
                                   menu->Render() | vscroll_indicator | frame | flex } )
                               | size( WIDTH, EQUAL, 30 ) } );
        } );
}
