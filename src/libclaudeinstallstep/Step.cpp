/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Step.h"

namespace ClaudeInstall
{

Step::Step( QObject* parent )
    : QObject( parent )
{
}

Step::~Step() {}

QString
Step::prettyStatus() const
{
    return QString();
}

QString
Step::summaryText() const
{
    return prettyStatus();
}

QObject*
Step::createSummaryWidget() const
{
    return nullptr;
}

QObject*
Step::config()
{
    return nullptr;
}

void
Step::onActivate()
{
}

void
Step::onLeave()
{
}

void
Step::onCancel()
{
}

void
Step::next()
{
}

void
Step::back()
{
}

void
Step::setModuleInstanceKey( const ClaudeInstall::ModuleSystem::InstanceKey& instanceKey )
{
    m_instanceKey = instanceKey;
}

void
Step::setConfigurationMap( const QVariantMap& configurationMap )
{
    Q_UNUSED( configurationMap )
}

RequirementsList
Step::checkRequirements()
{
    return RequirementsList();
}

bool
Step::isExecutionStep() const
{
    return false;
}

}  // namespace ClaudeInstall
