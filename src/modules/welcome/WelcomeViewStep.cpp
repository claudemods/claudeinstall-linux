/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "WelcomeViewStep.h"

#include "Config.h"
#include "WelcomePage.h"

#include "Branding.h"
#include "checker/GeneralRequirements.h"
#include "checker/ScreenCheck.h"
#include "modulesystem/ModuleManager.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( WelcomeViewStepFactory, registerPlugin< WelcomeViewStep >(); )

WelcomeViewStep::WelcomeViewStep( QObject* parent )
    : ClaudeInstall::ViewStep( parent )
    , m_conf( new Config( this ) )
    , m_widget( new WelcomePage( m_conf ) )
{
    // Requirements are checked later: the screen is something only this frontend can look at
    GeneralRequirements::setScreenCheck( &screenIsLargeEnough );

    connect( ClaudeInstall::ModuleManager::instance(),
             &ClaudeInstall::ModuleManager::requirementsComplete,
             this,
             &WelcomeViewStep::nextStatusChanged );
    connect( m_conf, &Config::localeIndexChanged, m_widget, &WelcomePage::externallySelectedLanguage );
}

WelcomeViewStep::~WelcomeViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}

QString
WelcomeViewStep::prettyName() const
{
    return tr( "Welcome", "@title" );
}


QWidget*
WelcomeViewStep::widget()
{
    return m_widget;
}

QObject*
WelcomeViewStep::config()
{
    return m_conf;
}


bool
WelcomeViewStep::isNextEnabled() const
{
    return m_widget->verdict();
}


bool
WelcomeViewStep::isBackEnabled() const
{
    return false;
}


bool
WelcomeViewStep::isAtBeginning() const
{
    return true;
}


bool
WelcomeViewStep::isAtEnd() const
{
    return true;
}


ClaudeInstall::JobList
WelcomeViewStep::jobs() const
{
    return ClaudeInstall::JobList();
}


void
WelcomeViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_conf->setConfigurationMap( configurationMap );

    //here init the qml or qwidgets needed bits
    m_widget->init();
}

ClaudeInstall::RequirementsList
WelcomeViewStep::checkRequirements()
{
    return m_conf->checkRequirements();
}
