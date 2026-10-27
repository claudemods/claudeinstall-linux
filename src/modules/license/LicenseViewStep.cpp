/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2015 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "LicenseViewStep.h"

#include "Config.h"
#include "LicensePage.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include "compat/Variant.h"
#include "utils/Logger.h"

#include <QVariantMap>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( LicenseViewStepFactory, registerPlugin< LicenseViewStep >(); )

LicenseViewStep::LicenseViewStep( QObject* parent )
    : ClaudeInstall::ViewStep( parent )
    , m_config( new Config( this ) )
    , m_widget( new LicensePage( m_config ) )
{
    emit nextStatusChanged( false );
    connect( m_config, &Config::isNextEnabledChanged, this, &LicenseViewStep::nextStatusChanged );
}


LicenseViewStep::~LicenseViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}


QString
LicenseViewStep::prettyName() const
{
    return tr( "License", "@label" );
}


QWidget*
LicenseViewStep::widget()
{
    return m_widget;
}


bool
LicenseViewStep::isNextEnabled() const
{
    return m_config->isNextEnabled();
}


bool
LicenseViewStep::isBackEnabled() const
{
    return true;
}


bool
LicenseViewStep::isAtBeginning() const
{
    return true;
}


bool
LicenseViewStep::isAtEnd() const
{
    return true;
}


QList< ClaudeInstall::job_ptr >
LicenseViewStep::jobs() const
{
    return QList< ClaudeInstall::job_ptr >();
}

void
LicenseViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    m_widget->loadEntries();
}

QObject*
LicenseViewStep::config()
{
    return m_config;
}
