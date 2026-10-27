/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 2019, Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd <arnaud.ferraris@collabora.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "FinishedViewStep.h"

#include "Config.h"
#include "FinishedPage.h"

#include "JobQueue.h"

#include <QApplication>

FinishedViewStep::FinishedViewStep( QObject* parent )
    : ClaudeInstall::ViewStep( parent )
    , m_config( new Config( this ) )
    , m_widget( new FinishedPage( m_config ) )
{
    auto jq = ClaudeInstall::JobQueue::instance();
    connect( jq, &ClaudeInstall::JobQueue::failed, m_config, &Config::onInstallationFailed );
    connect( jq, &ClaudeInstall::JobQueue::failed, m_widget, &FinishedPage::onInstallationFailed );

    emit nextStatusChanged( true );
}


FinishedViewStep::~FinishedViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}


QString
FinishedViewStep::prettyName() const
{
    return tr( "Finish", "@label" );
}


QWidget*
FinishedViewStep::widget()
{
    return m_widget;
}


bool
FinishedViewStep::isNextEnabled() const
{
    return false;
}


bool
FinishedViewStep::isBackEnabled() const
{
    return false;
}


bool
FinishedViewStep::isAtBeginning() const
{
    return true;
}


bool
FinishedViewStep::isAtEnd() const
{
    return true;
}


void
FinishedViewStep::onActivate()
{
    m_config->doNotify();
    connect( qApp, &QApplication::aboutToQuit, m_config, qOverload<>( &Config::doRestart ) );
}


ClaudeInstall::JobList
FinishedViewStep::jobs() const
{
    return ClaudeInstall::JobList();
}

void
FinishedViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( FinishedViewStepFactory, registerPlugin< FinishedViewStep >(); )

QObject*
FinishedViewStep::config()
{
    return m_config;
}
