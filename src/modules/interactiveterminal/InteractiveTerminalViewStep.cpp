/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "InteractiveTerminalViewStep.h"

#include "InteractiveTerminalPage.h"

#include "compat/Variant.h"
#include "utils/Logger.h"

#include <QVariant>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( InteractiveTerminalViewStepFactory,
                                     registerPlugin< InteractiveTerminalViewStep >(); )

InteractiveTerminalViewStep::InteractiveTerminalViewStep( QObject* parent )
    : ClaudeInstall::ViewStep( parent )
    , m_widget( new InteractiveTerminalPage() )
{
    emit nextStatusChanged( true );
}

InteractiveTerminalViewStep::~InteractiveTerminalViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}

QString
InteractiveTerminalViewStep::prettyName() const
{
    return tr( "Script", "@label" );
}

QWidget*
InteractiveTerminalViewStep::widget()
{
    return m_widget;
}

bool
InteractiveTerminalViewStep::isNextEnabled() const
{
    return true;
}

bool
InteractiveTerminalViewStep::isBackEnabled() const
{
    return true;
}

bool
InteractiveTerminalViewStep::isAtBeginning() const
{
    return true;
}

bool
InteractiveTerminalViewStep::isAtEnd() const
{
    return true;
}

QList< ClaudeInstall::job_ptr >
InteractiveTerminalViewStep::jobs() const
{
    cDebug() << "InteractiveTerminal" << prettyName() << "asked for jobs(), this is probably wrong.";
    return QList< ClaudeInstall::job_ptr >();
}

void
InteractiveTerminalViewStep::onActivate()
{
    cDebug() << "InteractiveTerminal" << prettyName() << "activated.";
    m_widget->onActivate();
}

void
InteractiveTerminalViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    if ( configurationMap.contains( "command" )
         && ClaudeInstall::typeOf( configurationMap.value( "command" ) ) == ClaudeInstall::StringVariantType )
    {
        m_widget->setCommand( configurationMap.value( "command" ).toString() );
    }
}
