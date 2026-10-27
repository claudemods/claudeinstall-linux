/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Dominic Hayes <ferenosdev@outlook.com>
 *   SPDX-FileCopyrightText: 2019 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2021 Anubhav Choudhary <ac.10edu@gmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Wizard.h"

#include "JobQueue.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Retranslator.h"

#include <QCoreApplication>

// The strings of the buttons were translated in the ViewManager before the
// wizard logic moved here; keep that context so existing translations apply.
#define VM_TRANSLATE( ... ) QCoreApplication::translate( "ClaudeInstall::ViewManager", __VA_ARGS__ )

#define UPDATE_BUTTON_PROPERTY( name, value ) \
    do \
    { \
        m_##name = value; \
        emit name##Changed( m_##name ); \
    } while ( false )

namespace ClaudeInstall
{

Wizard* Wizard::s_active = nullptr;

Wizard::Wizard( QObject* parent )
    : QAbstractListModel( parent )
    , m_currentStep( -1 )
{
    s_active = this;
    updateButtonLabels();

    if ( JobQueue::instance() )
    {
        connect( JobQueue::instance(), &JobQueue::failed, this, &Wizard::onInstallationFailed );
        connect( JobQueue::instance(), &JobQueue::finished, this, &Wizard::next );
    }

    CLAUDEINSTALL_RETRANSLATE_SLOT( &Wizard::updateButtonLabels );
}

Wizard::~Wizard()
{
    if ( s_active == this )
    {
        s_active = nullptr;
    }
}

void
Wizard::addStep( Step* step )
{
    insertStep( m_steps.size(), step );
    // If this is the first inserted step, update status of "Next" button
    if ( m_steps.count() == 1 )
    {
        m_nextEnabled = step->isNextEnabled();
        emit nextEnabledChanged( m_nextEnabled );
    }
}

void
Wizard::insertStep( int before, Step* step )
{
    beginInsertRows( QModelIndex(), before, before );
    m_steps.insert( before, step );
    connect( step, &Step::nextStatusChanged, this, &Wizard::updateNextStatus );
    stepInserted( before, step );
    endInsertRows();
}

void
Wizard::stepInserted( int index, Step* step )
{
    Q_UNUSED( index )
    Q_UNUSED( step )
}

void
Wizard::stepShown( int index )
{
    Q_UNUSED( index )
}

bool
Wizard::confirmExecution()
{
    return true;
}

bool
Wizard::confirmCancel()
{
    return true;
}

void
Wizard::exitApplication( int exitCode )
{
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
    Q_UNUSED( exitCode )
    QCoreApplication::quit();
#else
    QCoreApplication::exit( exitCode );
#endif
}

void
Wizard::showInstallationFailure( const QString& message, const QString& details )
{
    Q_UNUSED( message )
    Q_UNUSED( details )
    exitApplication( EXIT_FAILURE );
}

void
Wizard::onInstallationFailed( const QString& message, const QString& details )
{
    cError() << "Installation failed:" << message;
    cDebug() << Logger::SubEntry << "- message:" << message;
    cDebug() << Logger::SubEntry << "- details:" << Logger::NoQuote << details;

    emit installationFailed( message, details );
    showInstallationFailure( message, details );
}

void
Wizard::onInitComplete()
{
    m_currentStep = 0;

    // Tell the first step that it's been shown.
    if ( m_steps.count() > 0 )
    {
        m_steps.first()->onActivate();
    }

    emit currentStepChanged();
}

void
Wizard::updateNextStatus( bool status )
{
    Step* step = qobject_cast< Step* >( sender() );
    if ( step && currentStepValid() )
    {
        if ( step == m_steps.at( m_currentStep ) )
        {
            m_nextEnabled = status;
            emit nextEnabledChanged( m_nextEnabled );
        }
    }
}

Step*
Wizard::currentStepBase() const
{
    return currentStepValid() ? m_steps.value( m_currentStep ) : nullptr;
}

/** @brief Is the given step at @p index an execution step?
 *
 * Returns true if the step is an execution step, false otherwise.
 * Also returns false if the @p index is out of range.
 */
static inline bool
stepIsExecute( const StepList& steps, int index )
{
    return ( 0 <= index ) && ( index < steps.count() ) && steps.at( index )->isExecutionStep();
}

static inline bool
isAtVeryEnd( const StepList& steps, int index )
{
    // If we have an empty list, then there's no point right now
    // in checking if we're at the end.
    if ( steps.count() == 0 )
    {
        return false;
    }
    // .. and if the index is invalid, ignore it too
    if ( !( ( 0 <= index ) && ( index < steps.count() ) ) )
    {
        return false;
    }
    return ( index >= steps.count() ) || ( index == steps.count() - 1 && steps.last()->isAtEnd() );
}

void
Wizard::next()
{
    if ( !currentStepValid() )
    {
        return;
    }

    Step* step = m_steps.at( m_currentStep );
    bool executing = false;
    if ( step->isAtEnd() )
    {
        const auto* const settings = ClaudeInstall::Settings::instance();

        // Special case when the user clicks next on the very last page in a view phase
        // and right before switching to an execution phase.
        // Depending on ClaudeInstall::Settings, we show an "are you sure" prompt or not.
        if ( settings->showPromptBeforeExecution() && stepIsExecute( m_steps, m_currentStep + 1 ) )
        {
            if ( !confirmExecution() )
            {
                return;
            }
        }

        m_currentStep++;

        stepShown( m_currentStep );  // Does nothing if out of range
        step->onLeave();

        if ( m_currentStep < m_steps.count() )
        {
            m_steps.at( m_currentStep )->onActivate();
            executing = m_steps.at( m_currentStep )->isExecutionStep();
            emit currentStepChanged();
        }
        else
        {
            // Reached the end in a weird state (e.g. no finished step after an exec)
            executing = false;
            UPDATE_BUTTON_PROPERTY( nextEnabled, false );
            UPDATE_BUTTON_PROPERTY( backEnabled, false );
        }
        updateCancelEnabled( !settings->disableCancel() && !( executing && settings->disableCancelDuringExec() ) );
        updateBackAndNextVisibility( !( executing && settings->hideBackAndNextDuringExec() ) );
    }
    else
    {
        step->next();
    }

    if ( m_currentStep < m_steps.count() )
    {
        UPDATE_BUTTON_PROPERTY( nextEnabled, !executing && m_steps.at( m_currentStep )->isNextEnabled() );
        UPDATE_BUTTON_PROPERTY( backEnabled, !executing && m_steps.at( m_currentStep )->isBackEnabled() );
    }

    updateButtonLabels();
}

void
Wizard::updateButtonLabels()
{
    const auto* const settings = ClaudeInstall::Settings::instance();
    const bool setupMode = settings ? settings->isSetupMode() : false;

    QString nextIsInstallationStep = setupMode ? VM_TRANSLATE( "&Set Up", "@button" ) : VM_TRANSLATE( "&Install", "@button" );
    QString quitOnCompleteTooltip = setupMode ? VM_TRANSLATE( "Setup is complete. Close the setup program.", "@tooltip" )
                                              : VM_TRANSLATE( "The installation is complete. Close the installer.", "@tooltip" );
    QString cancelBeforeInstallationTooltip
        = setupMode ? VM_TRANSLATE( "Cancel the setup process without changing the system.", "@tooltip" )
                    : VM_TRANSLATE( "Cancel the installation process without changing the system.", "@tooltip" );

    // If we're going into the execution step / install phase, other message
    if ( stepIsExecute( m_steps, m_currentStep + 1 ) )
    {
        UPDATE_BUTTON_PROPERTY( nextLabel, nextIsInstallationStep );
        UPDATE_BUTTON_PROPERTY( nextIcon, "run-install" );
    }
    else
    {
        UPDATE_BUTTON_PROPERTY( nextLabel, VM_TRANSLATE( "&Next", "@button" ) );
        UPDATE_BUTTON_PROPERTY( nextIcon, "go-next" );
    }

    // Going back is always simple
    UPDATE_BUTTON_PROPERTY( backLabel, VM_TRANSLATE( "&Back", "@button" ) );
    UPDATE_BUTTON_PROPERTY( backIcon, "go-previous" );

    // Cancel button changes label at the end
    if ( isAtVeryEnd( m_steps, m_currentStep ) )
    {
        UPDATE_BUTTON_PROPERTY( quitLabel, VM_TRANSLATE( "&Done", "@button" ) );
        UPDATE_BUTTON_PROPERTY( quitTooltip, quitOnCompleteTooltip );
        UPDATE_BUTTON_PROPERTY( quitVisible, true );
        UPDATE_BUTTON_PROPERTY( quitIcon, "dialog-ok-apply" );
        updateCancelEnabled( true );
        if ( settings && settings->quitAtEnd() )
        {
            quit();
        }
    }
    else
    {
        if ( settings && settings->disableCancel() )
        {
            UPDATE_BUTTON_PROPERTY( quitVisible, false );
        }
        updateCancelEnabled( !( settings && settings->disableCancel() )
                             && !( stepIsExecute( m_steps, m_currentStep ) && settings
                                   && settings->disableCancelDuringExec() ) );

        UPDATE_BUTTON_PROPERTY( quitLabel, VM_TRANSLATE( "&Cancel", "@button" ) );
        UPDATE_BUTTON_PROPERTY( quitTooltip, cancelBeforeInstallationTooltip );
        UPDATE_BUTTON_PROPERTY( quitIcon, "dialog-cancel" );
    }
}

void
Wizard::back()
{
    if ( !currentStepValid() )
    {
        return;
    }

    Step* step = m_steps.at( m_currentStep );
    if ( step->isAtBeginning() && m_currentStep > 0 )
    {
        m_currentStep--;
        stepShown( m_currentStep );
        step->onLeave();
        m_steps.at( m_currentStep )->onActivate();
        emit currentStepChanged();
    }
    else if ( !step->isAtBeginning() )
    {
        step->back();
    }
    else
    {
        return;
    }

    UPDATE_BUTTON_PROPERTY( nextEnabled, m_steps.at( m_currentStep )->isNextEnabled() );
    UPDATE_BUTTON_PROPERTY( backEnabled,
                            ( m_currentStep == 0 && m_steps.first()->isAtBeginning() )
                                ? false
                                : m_steps.at( m_currentStep )->isBackEnabled() );

    updateButtonLabels();
}

void
Wizard::quit()
{
    const auto r = confirmCancelInstallation();
    if ( r == Confirmation::Continue )
    {
        return;
    }

    if ( r == Confirmation::CancelInstallation )
    {
        // Cancel steps in reverse
        for ( int i = m_currentStep; i >= 0; --i )
        {
            auto* step = m_steps.at( i );
            cDebug() << "Cancelling view step" << step->moduleInstanceKey();
            step->onCancel();
        }
    }

    exitApplication( EXIT_SUCCESS );
}

Wizard::Confirmation
Wizard::confirmCancelInstallation()
{
    const auto* const settings = ClaudeInstall::Settings::instance();

    // When we're at the very end, then it's always OK to exit.
    if ( isAtVeryEnd( m_steps, m_currentStep ) )
    {
        return Confirmation::EndOfInstallation;
    }

    // Not at the very end, cancel/quit might be disabled
    if ( settings->disableCancel() )
    {
        return Confirmation::Continue;
    }
    if ( settings->disableCancelDuringExec() && stepIsExecute( m_steps, m_currentStep ) )
    {
        return Confirmation::Continue;
    }

    // Otherwise, confirm cancel/quit.
    return confirmCancel() ? Confirmation::CancelInstallation : Confirmation::Continue;
}

void
Wizard::updateCancelEnabled( bool enabled )
{
    UPDATE_BUTTON_PROPERTY( quitEnabled, enabled );
    emit cancelEnabled( enabled );
}

void
Wizard::updateBackAndNextVisibility( bool visible )
{
    UPDATE_BUTTON_PROPERTY( backAndNextVisible, visible );
}

QVariant
Wizard::data( const QModelIndex& index, int role ) const
{
    if ( !index.isValid() )
    {
        return QVariant();
    }

    if ( ( index.row() < 0 ) || ( index.row() >= m_steps.length() ) )
    {
        return QVariant();
    }

    const auto* step = m_steps.at( index.row() );
    if ( !step )
    {
        return QVariant();
    }

    switch ( role )
    {
    case Qt::DisplayRole:
        return step->prettyName();
    case Qt::ToolTipRole:
        if ( ClaudeInstall::Settings::instance()->debugMode() )
        {
            auto key = step->moduleInstanceKey();
            // The tooltip is intentionally not translated:
            // we must be in debug-mode (-d) so presumably it
            // is a distro-developer or ClaudeInstall-developer
            // running it, and we don't need translation for them.
            QString toolTip( "<b>Debug information</b>" );  // Intentionally no translation here
            toolTip.append( "<br/>Type:\tViewStep" );
            toolTip.append( QString( "<br/>Pretty:\t%1" ).arg( step->prettyName() ) );
            toolTip.append( QString( "<br/>Status:\t%1" ).arg( step->prettyStatus() ) );
            toolTip.append(
                QString( "<br/>Source:\t%1" ).arg( key.isValid() ? key.toString() : QStringLiteral( "built-in" ) ) );
            return toolTip;
        }
        else
        {
            return QVariant();
        }
    case ProgressTreeItemCurrentIndex:
        return m_currentStep;
    default:
        return QVariant();
    }
}

int
Wizard::rowCount( const QModelIndex& parent ) const
{
    if ( parent.column() > 0 )
    {
        return 0;
    }
    return m_steps.length();
}

bool
Wizard::isChrootMode() const
{
    const auto* s = Settings::instance();
    return s ? s->doChroot() : true;
}

bool
Wizard::isDebugMode() const
{
    const auto* s = Settings::instance();
    return s ? s->debugMode() : false;
}

bool
Wizard::isSetupMode() const
{
    const auto* s = Settings::instance();
    return s ? s->isSetupMode() : false;
}

QString
Wizard::logFilePath() const
{
    return Logger::logFile();
}

}  // namespace ClaudeInstall
