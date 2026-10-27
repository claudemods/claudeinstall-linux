/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "KeyboardTuiStep.h"

#include "TuiWidgets.h"

#include "utils/RAII.h"

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( KeyboardTuiStepFactory, registerPlugin< KeyboardTuiStep >(); )

KeyboardTuiStep::KeyboardTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
}

KeyboardTuiStep::~KeyboardTuiStep() {}

QString
KeyboardTuiStep::prettyName() const
{
    return tr( "Keyboard", "@label" );
}

QString
KeyboardTuiStep::prettyStatus() const
{
    return m_config->prettyStatus();
}

QObject*
KeyboardTuiStep::config()
{
    return m_config;
}

bool
KeyboardTuiStep::isNextEnabled() const
{
    return true;
}

bool
KeyboardTuiStep::isBackEnabled() const
{
    return true;
}

bool
KeyboardTuiStep::isAtBeginning() const
{
    return true;
}

bool
KeyboardTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
KeyboardTuiStep::jobs() const
{
    return m_config->createJobs();
}

void
KeyboardTuiStep::onActivate()
{
    m_config->guessLocaleKeyboardLayout();
}

void
KeyboardTuiStep::onLeave()
{
    m_config->finalize();
}

void
KeyboardTuiStep::onCancel()
{
    m_config->cancel();
}

void
KeyboardTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    m_config->detectCurrentKeyboardLayout();
    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

void
KeyboardTuiStep::sync()
{
    namespace Tui = ClaudeInstall::Tui;
    cScopedAssignment syncing( &m_syncing, true, false );

    // The layouts and models do not change; the variants do, with the layout
    auto* layouts = m_config->keyboardLayouts();
    if ( m_layouts.size() != size_t( layouts->rowCount() ) )
    {
        Tui::fillEntries( &m_layouts, layouts, Qt::DisplayRole );
    }
    auto* models = m_config->keyboardModels();
    if ( m_models.size() != size_t( models->rowCount() ) )
    {
        Tui::fillEntries( &m_models, models, XKBListModel::LabelRole );
    }
    Tui::fillEntries( &m_variants, m_config->keyboardVariants(), XKBListModel::LabelRole );
    if ( m_groups.size() != size_t( m_config->keyboardGroupsSwitchers()->rowCount() ) )
    {
        Tui::fillEntries( &m_groups, m_config->keyboardGroupsSwitchers(), XKBListModel::LabelRole );
    }

    m_layout = qMax( 0, layouts->currentIndex() );
    m_variant = qMax( 0, m_config->keyboardVariants()->currentIndex() );
    m_model = qMax( 0, models->currentIndex() );
    m_group = qMax( 0, m_config->keyboardGroupsSwitchers()->currentIndex() );
}

ftxui::Component
KeyboardTuiStep::createComponent()
{
    using namespace ftxui;
    namespace Tui = ClaudeInstall::Tui;

    m_config->keyboardModels()->setCurrentIndex();  // To default PC105, as the GUI does
    sync();

    auto layouts = Tui::picker( tr( "Layout", "@label" ),
                                &m_layouts,
                                &m_layout,
                                [ this ]
                                {
                                    if ( !m_syncing )
                                    {
                                        m_config->keyboardLayouts()->setCurrentIndex( m_layout );
                                    }
                                } );
    auto variants = Tui::picker( tr( "Variant", "@label" ),
                                 &m_variants,
                                 &m_variant,
                                 [ this ]
                                 {
                                     if ( !m_syncing )
                                     {
                                         m_config->keyboardVariants()->setCurrentIndex( m_variant );
                                     }
                                 } );
    auto models = Tui::picker( tr( "Keyboard model", "@label" ),
                               &m_models,
                               &m_model,
                               [ this ]
                               {
                                   if ( !m_syncing )
                                   {
                                       m_config->keyboardModels()->setCurrentIndex( m_model );
                                   }
                               } );
    auto groups = Tui::picker( tr( "Switch layouts with", "@label" ),
                               &m_groups,
                               &m_group,
                               [ this ]
                               {
                                   if ( !m_syncing )
                                   {
                                       m_config->keyboardGroupsSwitchers()->setCurrentIndex( m_group );
                                   }
                               } );

    Component lists = Container::Horizontal( { layouts, variants, models, groups } );
    return Renderer( lists,
                     [ this, layouts, variants, models, groups ]
                     {
                         sync();
                         return vbox( { hbox( { layouts->Render() | flex,
                                                variants->Render() | flex,
                                                models->Render() | size( WIDTH, EQUAL, 28 ),
                                                groups->Render() | size( WIDTH, EQUAL, 28 ) } )
                                            | flex,
                                        text( m_config->prettyStatus().toStdString() ),
                                        text( ( tr( "Left and right change the list, up and down the choice.", "@label" ) )
                                                  .toStdString() )
                                            | dim } );
                     } );
}
