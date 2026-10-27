/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "KeyboardViewStep.h"

#include "Config.h"
#include "KeyboardPage.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include <QApplication>
#include <QGuiApplication>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( KeyboardViewStepFactory, registerPlugin< KeyboardViewStep >(); )

KeyboardViewStep::KeyboardViewStep( QObject* parent )
    : ClaudeInstall::ViewStep( parent )
    , m_config( new Config( this ) )
    , m_widget( new KeyboardPage( m_config ) )
{
    // The Config is headless; this frontend tells it about its platform
    m_config->setApplyDelay( QApplication::keyboardInputInterval() );
    m_config->setPlatformIsX11( QGuiApplication::platformName() == "xcb" );
    emit nextStatusChanged( true );
}


KeyboardViewStep::~KeyboardViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}


QString
KeyboardViewStep::prettyName() const
{
    return tr( "Keyboard", "@label" );
}


QString
KeyboardViewStep::prettyStatus() const
{
    return m_config->prettyStatus();
}


QWidget*
KeyboardViewStep::widget()
{
    return m_widget;
}


bool
KeyboardViewStep::isNextEnabled() const
{
    return true;
}


bool
KeyboardViewStep::isBackEnabled() const
{
    return true;
}


bool
KeyboardViewStep::isAtBeginning() const
{
    return true;
}


bool
KeyboardViewStep::isAtEnd() const
{
    return true;
}


QList< ClaudeInstall::job_ptr >
KeyboardViewStep::jobs() const
{
    return m_config->createJobs();
}


void
KeyboardViewStep::onActivate()
{
    m_config->guessLocaleKeyboardLayout();
}


void
KeyboardViewStep::onLeave()
{
    m_config->finalize();
}

void
KeyboardViewStep::onCancel()
{
    m_config->cancel();
}

void
KeyboardViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    m_config->detectCurrentKeyboardLayout();
}

QObject*
KeyboardViewStep::config()
{
    return m_config;
}
