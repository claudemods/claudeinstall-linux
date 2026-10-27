/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2020 Camilo Higuita <milo.h@aol.com>
 *   SPDX-FileCopyrightText: 2020 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "KeyboardQmlViewStep.h"

#include "Config.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include <QApplication>
#include <QGuiApplication>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( KeyboardQmlViewStepFactory, registerPlugin< KeyboardQmlViewStep >(); )

KeyboardQmlViewStep::KeyboardQmlViewStep( QObject* parent )
    : ClaudeInstall::QmlViewStep( parent )
    , m_config( new Config( this ) )
{
    // The Config is headless; this frontend tells it about its platform
    m_config->setApplyDelay( QApplication::keyboardInputInterval() );
    m_config->setPlatformIsX11( QGuiApplication::platformName() == "xcb" );

    m_config->detectCurrentKeyboardLayout();
    emit nextStatusChanged( true );
}

QString
KeyboardQmlViewStep::prettyName() const
{
    return tr( "Keyboard", "@label" );
}

QString
KeyboardQmlViewStep::prettyStatus() const
{
    return m_config->prettyStatus();
}

bool
KeyboardQmlViewStep::isNextEnabled() const
{
    return true;
}

bool
KeyboardQmlViewStep::isBackEnabled() const
{
    return true;
}

bool
KeyboardQmlViewStep::isAtBeginning() const
{
    return true;
}

bool
KeyboardQmlViewStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
KeyboardQmlViewStep::jobs() const
{
    return m_config->createJobs();
}

void
KeyboardQmlViewStep::onActivate()
{
    m_config->guessLocaleKeyboardLayout();
}

void
KeyboardQmlViewStep::onLeave()
{
    m_config->finalize();
}

void
KeyboardQmlViewStep::onCancel()
{
    m_config->cancel();
}

QObject*
KeyboardQmlViewStep::getConfig()
{
    return m_config;
}

void
KeyboardQmlViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::QmlViewStep::setConfigurationMap( configurationMap );
}
