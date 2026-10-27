/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "WelcomeQmlViewStep.h"

#include "checker/GeneralRequirements.h"
#include "checker/ScreenCheck.h"

#include "locale/TranslationsModel.h"
#include "utils/Dirs.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

#include "Branding.h"
#include "modulesystem/ModuleManager.h"
#include "utils/Yaml.h"

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( WelcomeQmlViewStepFactory, registerPlugin< WelcomeQmlViewStep >(); )

WelcomeQmlViewStep::WelcomeQmlViewStep( QObject* parent )
    : ClaudeInstall::QmlViewStep( parent )
    , m_config( new Config( this ) )
{
    // Requirements are checked later: the screen is something only this frontend can look at
    GeneralRequirements::setScreenCheck( &screenIsLargeEnough );

    connect( ClaudeInstall::ModuleManager::instance(),
             &ClaudeInstall::ModuleManager::requirementsComplete,
             this,
             &WelcomeQmlViewStep::nextStatusChanged );
}


QString
WelcomeQmlViewStep::prettyName() const
{
    return tr( "Welcome", "@title" );
}

bool
WelcomeQmlViewStep::isNextEnabled() const
{
    return m_config->requirementsModel()->satisfiedMandatory();
}

bool
WelcomeQmlViewStep::isBackEnabled() const
{
    // TODO: should return true (it's weird that you are not allowed to have welcome *after* anything
    return false;
}


bool
WelcomeQmlViewStep::isAtBeginning() const
{
    return true;
}


bool
WelcomeQmlViewStep::isAtEnd() const
{
    return true;
}


ClaudeInstall::JobList
WelcomeQmlViewStep::jobs() const
{
    return ClaudeInstall::JobList();
}

void
WelcomeQmlViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::QmlViewStep::setConfigurationMap( configurationMap );  // call parent implementation last
}

ClaudeInstall::RequirementsList
WelcomeQmlViewStep::checkRequirements()
{
    return m_config->checkRequirements();
}

QObject*
WelcomeQmlViewStep::getConfig()
{
    return m_config;
}
