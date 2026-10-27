/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "TuiApplication.h"

#include "TuiModuleLoader.h"
#include "TuiStep.h"
#include "TuiWizard.h"

#include "BrandingInfo.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <QCoreApplication>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <fcntl.h>
#include <unistd.h>

namespace ClaudeInstall
{

/// @brief Turns a label with a mnemonic ("&Next") into plain text
static std::string
plain( QString s )
{
    return s.remove( QLatin1Char( '&' ) ).toStdString();
}

TuiApplication::TuiApplication( QObject* parent )
    : QObject( parent )
    , m_screen( ftxui::ScreenInteractive::Fullscreen() )
{
}

TuiApplication::~TuiApplication()
{
    m_timer.stop();
    // The loop gives the terminal back
    m_loop.reset();
}

bool
TuiApplication::start( bool redirectStderr )
{
    m_wizard = TuiWizard::instance();
    m_loader = new TuiModuleLoader( Settings::instance()->modulesSearchPaths(), this );
    m_loader->scan();
    if ( !m_loader->loadModules() )
    {
        m_error = tr( "Some modules could not be loaded; the log file has the details.", "@error" );
        return false;
    }

    m_pages = ftxui::Container::Tab( {}, &m_pageIndex );
    const auto steps = m_wizard->tuiSteps();
    for ( TuiStep* step : steps )
    {
        m_pages->Add( step->component() );
        ++m_stepCount;
    }
    if ( m_stepCount == 0 )
    {
        m_error = tr( "There are no steps to show.", "@error" );
        return false;
    }

    connect( m_wizard,
             &TuiWizard::cancelQuestion,
             this,
             [ this ]
             {
                 askQuestion( tr( "Cancel the installation? The system will not be changed.", "@label" ),
                              [ this ] { m_wizard->confirmCancelAndQuit(); } );
             } );
    connect( m_wizard,
             &TuiWizard::executionQuestion,
             this,
             [ this ]
             {
                 askQuestion( tr( "Start the installation now? The changes can not be undone.", "@label" ),
                              [ this ] { m_wizard->confirmExecutionAndNext(); } );
             } );
    connect( m_wizard,
             &TuiWizard::installationFailed,
             this,
             [ this ]( const QString& message, const QString& details )
             { m_failure = details.isEmpty() ? message : message + QStringLiteral( "\n\n" ) + details; } );

    m_wizard->onInitComplete();
    m_loader->checkRequirements();

    if ( redirectStderr )
    {
        // The logger writes to the log file as well; this only stops it scribbling on the screen
        const int devnull = ::open( "/dev/null", O_WRONLY );
        if ( devnull >= 0 )
        {
            ::dup2( devnull, STDERR_FILENO );
            ::close( devnull );
        }
    }

    m_root = ftxui::CatchEvent( ftxui::Renderer( m_pages, [ this ] { return render(); } ),
                                [ this ]( ftxui::Event event ) { return handleEvent( event ); } );
    m_loop = std::make_unique< ftxui::Loop >( &m_screen, m_root );

    m_timer.setInterval( 30 );
    connect( &m_timer, &QTimer::timeout, this, &TuiApplication::tick );
    m_timer.start();
    return true;
}

void
TuiApplication::tick()
{
    if ( !m_loop )
    {
        return;
    }
    if ( m_loop->HasQuitted() )
    {
        // The screen was told to exit (Ctrl-C); that is the user giving up
        m_timer.stop();
        QCoreApplication::exit( 130 );
        return;
    }

    const int current = m_wizard->currentStepIndex();
    m_pageIndex = qBound( 0, current, qMax( 0, m_stepCount - 1 ) );

    // Qt signals do not make FTXUI redraw, so ask for it a few times a second
    if ( ++m_ticks % 4 == 0 )
    {
        m_screen.PostEvent( ftxui::Event::Custom );
    }
    m_loop->RunOnce();
}

void
TuiApplication::askQuestion( const QString& question, const std::function< void() >& onYes )
{
    m_question = question;
    m_questionYes = onYes;
}

bool
TuiApplication::handleEvent( const ftxui::Event& event )
{
    using ftxui::Event;

    if ( !m_failure.isEmpty() )
    {
        if ( event == Event::Return || event == Event::Escape )
        {
            QCoreApplication::exit( EXIT_FAILURE );
        }
        return true;
    }

    if ( !m_question.isEmpty() )
    {
        if ( event == Event::Character( 'y' ) || event == Event::Character( 'Y' ) || event == Event::Return )
        {
            const auto yes = m_questionYes;
            m_question.clear();
            m_questionYes = nullptr;
            if ( yes )
            {
                yes();
            }
        }
        else if ( event == Event::Character( 'n' ) || event == Event::Character( 'N' ) || event == Event::Escape )
        {
            m_question.clear();
            m_questionYes = nullptr;
        }
        return true;  // the question takes all keys
    }

    // The Linux console has no mouse, and its function keys send sequences that are
    // easy to lose (F1-F5 are ESC [ [ A..E), so every action also has a plain key:
    // Ctrl-B back, Ctrl-N next, Esc cancel.
    if ( event == Event::F1 || event == Event::Special( "\x02" ) || event == Event::Special( "\x1B[[A" ) )
    {
        if ( m_wizard->backAndNextVisible() && m_wizard->backEnabled() )
        {
            m_wizard->back();
        }
        return true;
    }
    if ( event == Event::F2 || event == Event::Special( "\x0E" ) || event == Event::Special( "\x1B[[B" ) )
    {
        if ( m_wizard->backAndNextVisible() && m_wizard->nextEnabled() )
        {
            m_wizard->next();
        }
        return true;
    }
    if ( event == Event::F10 || event == Event::Escape )
    {
        if ( m_wizard->quitEnabled() )
        {
            m_wizard->quit();
        }
        return true;
    }
    return false;
}

ftxui::Element
TuiApplication::render()
{
    using namespace ftxui;

    const int current = m_wizard->currentStepIndex();

    Elements sidebar;
    const auto steps = m_wizard->tuiSteps();
    for ( int i = 0; i < steps.count(); ++i )
    {
        Element e = text( " " + plain( steps.at( i )->prettyName() ) + " " );
        if ( i == current )
        {
            e = e | inverted | bold;
        }
        else if ( i > current )
        {
            e = e | dim;
        }
        sidebar.push_back( e );
    }

    auto key = [ & ]( const std::string& k, const QString& label, bool enabled )
    {
        Element e = hbox( { text( k ) | bold, text( " " + plain( label ) + "  " ) } );
        return enabled ? e : ( e | dim );
    };
    Elements footer;
    if ( m_wizard->backAndNextVisible() )
    {
        footer.push_back( key( "F1/Ctrl-B", m_wizard->backLabel(), m_wizard->backEnabled() ) );
        footer.push_back( key( "F2/Ctrl-N", m_wizard->nextLabel(), m_wizard->nextEnabled() ) );
    }
    if ( m_wizard->quitVisible() )
    {
        footer.push_back( key( "F10/Esc", m_wizard->quitLabel(), m_wizard->quitEnabled() ) );
    }

    Element page = m_stepCount > 0 ? m_pages->Render() : text( "" );
    Element whole = vbox( { text( " " + BrandingInfo::instance().productName().toStdString() ) | bold,
                           separator(),
                           hbox( { vbox( sidebar ) | size( WIDTH, EQUAL, 26 ),
                                   separator(),
                                   page | flex | border } )
                               | flex,
                           separator(),
                           hbox( footer ) } );

    const QString message = !m_failure.isEmpty() ? m_failure : m_question;
    if ( !message.isEmpty() )
    {
        const std::string hint = !m_failure.isEmpty() ? tr( "[Enter] Close", "@label" ).toStdString()
                                                      : tr( "[Y]es   [N]o", "@label" ).toStdString();
        Element box = vbox( { paragraph( message.toStdString() ), separator(), text( hint ) | center } ) | border
            | size( WIDTH, LESS_THAN, 70 ) | clear_under | center;
        return dbox( { whole, box } );
    }
    return whole;
}

}  // namespace ClaudeInstall
