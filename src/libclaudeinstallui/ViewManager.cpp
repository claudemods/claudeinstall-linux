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

#include "ViewManager.h"

#include "Branding.h"
#include "JobQueue.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Paste.h"
#include "utils/Retranslator.h"
#include "utils/String.h"
#include "viewpages/BlankViewStep.h"
#include "viewpages/ExecutionViewStep.h"
#include "viewpages/ViewStep.h"
#include "widgets/ErrorDialog.h"
#include "widgets/TranslationFix.h"

#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFile>
#include <QMessageBox>
#include <QMetaObject>

namespace ClaudeInstall
{

ViewManager* ViewManager::s_instance = nullptr;

ViewManager*
ViewManager::instance()
{
    return s_instance;
}

ViewManager*
ViewManager::instance( QObject* parent )
{
    Q_ASSERT( !s_instance );
    s_instance = new ViewManager( parent );
    return s_instance;
}

ViewManager::ViewManager( QObject* parent )
    : Wizard( parent )
    , m_widget( new QWidget() )
    , m_panelSides( Qt::Horizontal | Qt::Vertical )
{
    Q_ASSERT( !s_instance );

    QBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins( 0, 0, 0, 0 );
    m_widget->setObjectName( "viewManager" );
    m_widget->setLayout( mainLayout );

    m_stack = new QStackedWidget( m_widget );
    m_stack->setObjectName( "viewManagerStack" );
    m_stack->setContentsMargins( 0, 0, 0, 0 );
    mainLayout->addWidget( m_stack );

#ifdef PRESERVE_FOR_TRANSLATION_PURPOSES
    tr( "&Yes" );
    tr( "&No" );
    tr( "&Close" );
#endif
}

ViewManager::~ViewManager()
{
    m_widget->deleteLater();
    s_instance = nullptr;
}

QWidget*
ViewManager::centralWidget()
{
    return m_widget;
}

void
ViewManager::addViewStep( ViewStep* step )
{
    addStep( step );
}

void
ViewManager::stepInserted( int index, Step* step )
{
    auto* viewStep = qobject_cast< ViewStep* >( step );
    if ( !viewStep )
    {
        cError() << "Step" << step->moduleInstanceKey() << "is not a ViewStep.";
        return;
    }

    connect( viewStep, &ViewStep::ensureSize, this, &ViewManager::ensureSize );

    if ( !viewStep->widget() )
    {
        cError() << "ViewStep" << viewStep->moduleInstanceKey() << "has no widget.";
    }
    else
    {
        QLayout* layout = viewStep->widget()->layout();
        if ( layout )
        {
            const auto margins = viewStep->widgetMargins( m_panelSides );
            layout->setContentsMargins( margins.width(), margins.height(), margins.width(), margins.height() );
        }

        m_stack->insertWidget( index, viewStep->widget() );
        m_stack->setCurrentIndex( 0 );
        viewStep->widget()->setFocus();
    }
}

void
ViewManager::stepShown( int index )
{
    m_stack->setCurrentIndex( index );  // Does nothing if out of range
}

void
ViewManager::showInstallationFailure( const QString& message, const QString& details )
{
    QString heading = ClaudeInstall::Settings::instance()->isSetupMode() ? tr( "Setup Failed", "@title" )
                                                                     : tr( "Installation Failed", "@title" );

    ErrorDialog* errorDialog = new ErrorDialog();
    errorDialog->setWindowTitle( tr( "Error", "@title" ) );
    errorDialog->setHeading( "<strong>" + heading + "</strong>" );
    errorDialog->setInformativeText( message );
    errorDialog->setShouldOfferWebPaste( ClaudeInstall::Branding::instance()->uploadServer() );
    errorDialog->setDetails( details );
    errorDialog->show();

    cDebug() << "ClaudeInstall will quit when the dialog closes.";
    connect( errorDialog,
             &QDialog::finished,
             [ errorDialog ]( int result )
             {
                 if ( result == QDialog::Accepted && errorDialog->shouldOfferWebPaste() )
                 {
                     ClaudeInstall::Paste::doLogUploadUI( errorDialog );
                 }
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
                 QApplication::quit();
#else
                 QApplication::exit( EXIT_SUCCESS );
#endif
             } );
}

void
ViewManager::onInitFailed( const QStringList& modules )
{
    // Because this means the installer / setup program is broken by the distributor,
    // don't bother being precise about installer / setup wording.
    QString title( tr( "ClaudeInstall Initialization Failed", "@title" ) );
    QString description( tr( "%1 can not be installed. ClaudeInstall was unable to load all of the configured modules. "
                             "This is a problem with the way ClaudeInstall is being used by the distribution.",
                             "@info" ) );
    QString detailString;

    if ( modules.count() > 0 )
    {
        description.append( tr( "<br/>The following modules could not be loaded:", "@info" ) );
        QStringList details;
        details << QLatin1String( "<ul>" );
        for ( const auto& m : modules )
        {
            details << QLatin1String( "<li>" ) << m << QLatin1String( "</li>" );
        }
        details << QLatin1String( "</ul>" );
        detailString = details.join( QString() );
    }

    insertStep(
        0,
        new BlankViewStep( title, description.arg( ClaudeInstall::Branding::instance()->productName() ), detailString ) );
}

ViewStepList
ViewManager::viewSteps() const
{
    ViewStepList viewSteps;
    for ( Step* step : m_steps )
    {
        if ( auto* viewStep = qobject_cast< ViewStep* >( step ) )
        {
            viewSteps.append( viewStep );
        }
    }
    return viewSteps;
}

ViewStep*
ViewManager::currentStep() const
{
    return qobject_cast< ViewStep* >( currentStepBase() );
}

static int
questionBox( QWidget* parent,
             const QString& title,
             const QString& question,
             const QString& button0,
             const QString& button1 )
{

#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
    return QMessageBox::question( parent,
                                  title,
                                  question,
                                  button0,
                                  button1,
                                  QString(),
                                  0 /* default first button, i.e. confirm */,
                                  1 /* escape is second button, i.e. cancel */ );
#else
    QMessageBox mb( QMessageBox::Question, title, question, QMessageBox::StandardButton::NoButton, parent );
    const auto* const okButton = mb.addButton( button0, QMessageBox::AcceptRole );
    mb.addButton( button1, QMessageBox::RejectRole );
    mb.exec();
    if ( mb.clickedButton() == okButton )
    {
        return 0;
    }
    return 1;  // Cancel
#endif
}

bool
ViewManager::confirmExecution()
{
    const auto* const settings = ClaudeInstall::Settings::instance();

    QString title = settings->isSetupMode() ? tr( "Continue with Setup?", "@title" )
                                            : tr( "Continue with Installation?", "@title" );
    QString question = settings->isSetupMode()
        ? tr( "The %1 setup program is about to make changes to your "
              "disk in order to set up %2.<br/><strong>You will not be able "
              "to undo these changes.</strong>",
              "%1 is short product name, %2 is short product name with version" )
        : tr( "The %1 installer is about to make changes to your "
              "disk in order to install %2.<br/><strong>You will not be able "
              "to undo these changes.</strong>",
              "%1 is short product name, %2 is short product name with version" );
    QString confirm = settings->isSetupMode() ? tr( "&Set Up Now", "@button" ) : tr( "&Install Now", "@button" );

    const auto* branding = ClaudeInstall::Branding::instance();
    int reply = questionBox( m_widget,
                             title,
                             question.arg( branding->shortProductName(), branding->shortVersionedName() ),
                             confirm,
                             tr( "Go &Back", "@button" ) );
    return reply != 1;
}

bool
ViewManager::confirmCancel()
{
    const auto* const settings = ClaudeInstall::Settings::instance();

    QString title = settings->isSetupMode() ? tr( "Cancel Setup?", "@title" ) : tr( "Cancel Installation?", "@title" );
    QString question = settings->isSetupMode() ? tr( "Do you really want to cancel the current setup process?\n"
                                                     "The setup program will quit and all changes will be lost." )
                                               : tr( "Do you really want to cancel the current install process?\n"
                                                     "The installer will quit and all changes will be lost." );
    QMessageBox mb( QMessageBox::Question, title, question, QMessageBox::Yes | QMessageBox::No, m_widget );
    mb.setDefaultButton( QMessageBox::No );
    ClaudeInstall::fixButtonLabels( &mb );
    int response = mb.exec();
    return response == QMessageBox::Yes;
}

void
ViewManager::exitApplication( int exitCode )
{
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
    Q_UNUSED( exitCode )
    QApplication::quit();
#else
    QApplication::exit( exitCode );
#endif
}

}  // namespace ClaudeInstall
