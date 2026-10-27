/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "TuiWizard.h"

namespace ClaudeInstall
{

TuiWizard* TuiWizard::s_instance = nullptr;

TuiWizard*
TuiWizard::instance()
{
    if ( !s_instance )
    {
        s_instance = new TuiWizard();
    }
    return s_instance;
}

TuiWizard::TuiWizard( QObject* parent )
    : Wizard( parent )
{
}

TuiWizard::~TuiWizard()
{
    if ( s_instance == this )
    {
        s_instance = nullptr;
    }
}

void
TuiWizard::addTuiStep( TuiStep* step )
{
    addStep( step );
}

TuiStepList
TuiWizard::tuiSteps() const
{
    TuiStepList l;
    for ( Step* s : m_steps )
    {
        if ( auto* t = qobject_cast< TuiStep* >( s ) )
        {
            l.append( t );
        }
    }
    return l;
}

TuiStep*
TuiWizard::currentStep() const
{
    return qobject_cast< TuiStep* >( currentStepBase() );
}

bool
TuiWizard::confirmExecution()
{
    if ( m_executionConfirmed )
    {
        m_executionConfirmed = false;
        return true;
    }
    emit executionQuestion();
    return false;
}

bool
TuiWizard::confirmCancel()
{
    if ( m_cancelConfirmed )
    {
        m_cancelConfirmed = false;
        return true;
    }
    emit cancelQuestion();
    return false;
}

void
TuiWizard::confirmCancelAndQuit()
{
    m_cancelConfirmed = true;
    quit();
    m_cancelConfirmed = false;  // quit() may not have asked (e.g. at the end)
}

void
TuiWizard::confirmExecutionAndNext()
{
    m_executionConfirmed = true;
    next();
    m_executionConfirmed = false;
}

void
TuiWizard::showInstallationFailure( const QString& message, const QString& details )
{
    Q_UNUSED( message )
    Q_UNUSED( details )
}

void
TuiWizard::exitApplication( int exitCode )
{
    emit exitRequested( exitCode );
    Wizard::exitApplication( exitCode );
}

}  // namespace ClaudeInstall
