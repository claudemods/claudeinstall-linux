/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015, Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2020, Camilo Higuita <milo.h@aol.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "SummaryQmlViewStep.h"

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( SummaryQmlViewStepFactory, registerPlugin< SummaryQmlViewStep >(); )

SummaryQmlViewStep::SummaryQmlViewStep( QObject* parent )
    : ClaudeInstall::QmlViewStep( parent )
    , m_config( new Config( this ) )
{
    emit nextStatusChanged( true );
}


SummaryQmlViewStep::~SummaryQmlViewStep() {}

QString
SummaryQmlViewStep::prettyName() const
{
    return m_config->title();
}


bool
SummaryQmlViewStep::isNextEnabled() const
{
    return true;
}


bool
SummaryQmlViewStep::isBackEnabled() const
{
    return true;
}


bool
SummaryQmlViewStep::isAtBeginning() const
{
    return true;
}


bool
SummaryQmlViewStep::isAtEnd() const
{
    return true;
}


ClaudeInstall::JobList
SummaryQmlViewStep::jobs() const
{
    return {};
}


void
SummaryQmlViewStep::onActivate()
{
    // Collect the steps before this one: those need to have their
    // summary (text or widget) displayed.
    m_config->collectSummaries( this, Config::Widgets::Disabled );
}
