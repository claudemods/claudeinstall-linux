/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ExecutionViewStep.h"

#include "Slideshow.h"

#include "Branding.h"
#include "ClaudeInstallConfig.h"
#include "Job.h"
#include "JobQueue.h"
#include "Settings.h"
#include "ViewManager.h"
#include "modulesystem/Module.h"
#include "modulesystem/ModuleManager.h"
#include "utils/Dirs.h"
#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"
#include "widgets/LogWidget.h"

#include <QAction>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

static ClaudeInstall::Slideshow*
makeSlideshow( QWidget* parent )
{
    const int api = ClaudeInstall::Branding::instance()->slideshowAPI();
    switch ( api )
    {
    case -1:
        return new ClaudeInstall::SlideshowPictures( parent );
#ifdef WITH_QML
    case 1:
        [[fallthrough]];
    case 2:
        return new ClaudeInstall::SlideshowQML( parent );
#endif
    default:
        cWarning() << "Unknown Branding slideshow API" << api;
        return new ClaudeInstall::SlideshowPictures( parent );
    }
}

namespace ClaudeInstall
{

ExecutionViewStep::ExecutionViewStep( QObject* parent )
    : ViewStep( parent )
    , m_widget( new QWidget )
    , m_progressBar( new QProgressBar )
    , m_label( new QLabel )
    , m_slideshow( makeSlideshow( m_widget ) )
    , m_tab_widget( new QTabWidget )
    , m_log_widget( new LogWidget )
    , m_model( new ExecutionModel( this ) )
{
    m_widget->setObjectName( "slideshow" );
    m_progressBar->setObjectName( "exec-progress" );
    CLAUDEINSTALL_RETRANSLATE( m_progressBar->setFormat(
        tr( "%p%", "Progress percentage indicator: %p is where the number 0..100 is placed" ) ); );
    m_label->setObjectName( "exec-message" );

    QVBoxLayout* layout = new QVBoxLayout( m_widget );
    QVBoxLayout* bottomLayout = new QVBoxLayout;
    QHBoxLayout* barLayout = new QHBoxLayout;

    m_progressBar->setMaximum( 10000 );

    m_tab_widget->addTab( m_slideshow->widget(), "Slideshow" );
    m_tab_widget->addTab( m_log_widget, "Log" );
    m_tab_widget->tabBar()->hide();

    layout->addWidget( m_tab_widget );
    ClaudeInstall::unmarginLayout( layout );
    layout->addLayout( bottomLayout );

    bottomLayout->addSpacing( ClaudeInstall::defaultFontHeight() / 2 );
    bottomLayout->addLayout( barLayout );
    bottomLayout->addWidget( m_label );

    QToolBar* toolBar = new QToolBar;
    const auto logButtonIcon = QIcon::fromTheme( "utilities-terminal" );
    auto toggleLogAction = toolBar->addAction(
        Branding::instance()->image(
            { "utilities-log-viewer", "utilities-terminal", "text-x-log", "text-x-changelog", "preferences-log" },
            QSize( 32, 32 ) ),
        "Toggle log" );
    auto toggleLogButton = dynamic_cast< QToolButton* >( toolBar->widgetForAction( toggleLogAction ) );
    connect( toggleLogButton, &QToolButton::clicked, this, &ExecutionViewStep::toggleLog );

    barLayout->addWidget( m_progressBar );
    barLayout->addWidget( toolBar );

    connect( m_model, &ExecutionModel::updated, this, &ExecutionViewStep::updateFromJobQueue );
}

QString
ExecutionViewStep::prettyName() const
{
    return ClaudeInstall::Settings::instance()->isSetupMode() ? tr( "Set Up", "@label" ) : tr( "Install", "@label" );
}

QWidget*
ExecutionViewStep::widget()
{
    return m_widget;
}

void
ExecutionViewStep::next()
{
}

void
ExecutionViewStep::back()
{
}

bool
ExecutionViewStep::isNextEnabled() const
{
    return false;
}

bool
ExecutionViewStep::isBackEnabled() const
{
    return false;
}

bool
ExecutionViewStep::isAtBeginning() const
{
    return true;
}

bool
ExecutionViewStep::isAtEnd() const
{
    return !m_model->running();
}

void
ExecutionViewStep::onActivate()
{
    m_slideshow->changeSlideShowState( Slideshow::Start );

    const auto instanceDescriptors = ClaudeInstall::Settings::instance()->moduleInstances();

    QList< ExecutionModel::WeightedJobs > groups;
    for ( const auto& instanceKey : m_jobInstanceKeys )
    {
        const auto& moduleDescriptor = ClaudeInstall::ModuleManager::instance()->moduleDescriptor( instanceKey );
        ClaudeInstall::Module* module = ClaudeInstall::ModuleManager::instance()->moduleInstance( instanceKey );

        const auto instanceDescriptor
            = std::find_if( instanceDescriptors.constBegin(),
                            instanceDescriptors.constEnd(),
                            [ = ]( const ClaudeInstall::InstanceDescription& d ) { return d.key() == instanceKey; } );
        int weight = moduleDescriptor.weight();
        if ( instanceDescriptor != instanceDescriptors.constEnd() && instanceDescriptor->explicitWeight() )
        {
            weight = instanceDescriptor->weight();
        }
        weight = qBound( 1, weight, 100 );
        if ( module )
        {
            auto jl = module->jobs();
            if ( module->isEmergency() )
            {
                for ( auto& j : jl )
                {
                    j->setEmergency( true );
                }
            }
            groups.append( { weight, jl } );
        }
    }

    m_model->start( groups );
}

JobList
ExecutionViewStep::jobs() const
{
    return JobList();
}

void
ExecutionViewStep::appendJobModuleInstanceKey( const ModuleSystem::InstanceKey& instanceKey )
{
    m_jobInstanceKeys.append( instanceKey );
}

void
ExecutionViewStep::updateFromJobQueue( qreal percent, const QString& message )
{
    m_progressBar->setValue( int( percent * m_progressBar->maximum() ) );
    if ( !message.isEmpty() )
    {
        m_label->setText( message );
    }
}

void
ExecutionViewStep::toggleLog()
{
    const bool logBecomesVisible = m_tab_widget->currentIndex() == 0;  // ie. is not visible right now
    if ( logBecomesVisible )
    {
        m_log_widget->start();
    }
    else
    {
        m_log_widget->stop();
    }
    m_tab_widget->setCurrentIndex( logBecomesVisible ? 1 : 0 );
}

void
ExecutionViewStep::onLeave()
{
    m_log_widget->stop();
    m_slideshow->changeSlideShowState( Slideshow::Stop );
}

}  // namespace ClaudeInstall
