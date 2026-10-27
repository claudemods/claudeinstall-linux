/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */
#include "PlasmaLnfViewStep.h"

#include "Config.h"
#include "PlasmaLnfPage.h"
#include "ThemeInfo.h"

#include "utils/Logger.h"
#include "utils/Variant.h"

#include <QVariantMap>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( PlasmaLnfViewStepFactory, registerPlugin< PlasmaLnfViewStep >(); )

PlasmaLnfViewStep::PlasmaLnfViewStep( QObject* parent )
    : ClaudeInstall::ViewStep( parent )
    , m_config( new Config( this ) )
    , m_widget( new PlasmaLnfPage( m_config ) )
{
    emit nextStatusChanged( false );
}


PlasmaLnfViewStep::~PlasmaLnfViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}


QString
PlasmaLnfViewStep::prettyName() const
{
    return tr( "Look-and-Feel", "@label" );
}


QWidget*
PlasmaLnfViewStep::widget()
{
    return m_widget;
}


bool
PlasmaLnfViewStep::isNextEnabled() const
{
    return true;
}


bool
PlasmaLnfViewStep::isBackEnabled() const
{
    return true;
}


bool
PlasmaLnfViewStep::isAtBeginning() const
{
    return true;
}


bool
PlasmaLnfViewStep::isAtEnd() const
{
    return true;
}


void
PlasmaLnfViewStep::onLeave()
{
}


ClaudeInstall::JobList
PlasmaLnfViewStep::jobs() const
{
    return m_config->createJobs();
}


void
PlasmaLnfViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
}

QObject*
PlasmaLnfViewStep::config()
{
    return m_config;
}
