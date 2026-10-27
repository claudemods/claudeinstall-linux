/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Tests.h"

#include "PlaceholderStep.h"
#include "TuiModuleLoader.h"
#include "TuiStep.h"
#include "TuiWidgets.h"
#include "TuiWizard.h"

#include "JobQueue.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStringListModel>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClaudeInstall;

QTEST_GUILESS_MAIN( TuiTests )

namespace
{
/// A one-page step that counts how often its component is built
class CountingStep : public TuiStep
{
public:
    QString prettyName() const override { return QStringLiteral( "counting" ); }
    ftxui::Component createComponent() override
    {
        ++built;
        return ftxui::Renderer( [] { return ftxui::text( "x" ); } );
    }
    bool isNextEnabled() const override { return true; }
    bool isBackEnabled() const override { return true; }
    bool isAtBeginning() const override { return true; }
    bool isAtEnd() const override { return true; }
    JobList jobs() const override { return JobList(); }

    int built = 0;
};

class ExecStandIn : public CountingStep
{
public:
    QString prettyName() const override { return QStringLiteral( "exec" ); }
    bool isExecutionStep() const override { return true; }
};
}  // namespace

TuiTests::TuiTests() {}

TuiTests::~TuiTests() {}

void
TuiTests::initTestCase()
{
    Logger::setupLogLevel( Logger::LOGDEBUG );

    (void)new ClaudeInstall::JobQueue();
    (void)new ClaudeInstall::Settings( true );
}

void
TuiTests::testPlaceholderStep()
{
    PlaceholderStep step( QStringLiteral( "partition" ) );
    QCOMPARE( step.prettyName(), QStringLiteral( "partition" ) );
    QVERIFY( !step.prettyStatus().isEmpty() );
    QVERIFY( step.jobs().isEmpty() );
    // Never in the way of the user
    QVERIFY( step.isNextEnabled() );
    QVERIFY( step.isBackEnabled() );
    QVERIFY( step.isAtBeginning() );
    QVERIFY( step.isAtEnd() );
    QVERIFY( step.component() );
}

void
TuiTests::testComponentIsBuiltOnce()
{
    CountingStep step;
    QCOMPARE( step.built, 0 );
    auto c1 = step.component();
    auto c2 = step.component();
    QCOMPARE( step.built, 1 );
    QVERIFY( c1 == c2 );
}

void
TuiTests::testCancelIsAskedTwice()
{
    auto* wizard = TuiWizard::instance();
    // The wizard is a singleton that outlives the test function, so it owns its steps
    auto* a = new CountingStep;
    auto* b = new CountingStep;
    a->setParent( wizard );
    b->setParent( wizard );
    wizard->addTuiStep( a );
    wizard->addTuiStep( b );
    QCOMPARE( wizard->tuiSteps().size(), 2 );
    wizard->onInitComplete();
    QVERIFY( wizard->currentStep() == a );

    QSignalSpy question( wizard, &TuiWizard::cancelQuestion );
    QSignalSpy exited( wizard, &TuiWizard::exitRequested );

    // The first time, the answer is "not yet" and the application is asked to ask
    wizard->quit();
    QCOMPARE( question.count(), 1 );
    QCOMPARE( exited.count(), 0 );

    // The user says no: nothing happens, and asking again asks again
    wizard->quit();
    QCOMPARE( question.count(), 2 );
    QCOMPARE( exited.count(), 0 );

    // The user says yes
    wizard->confirmCancelAndQuit();
    QCOMPARE( exited.count(), 1 );
    QCOMPARE( exited.first().first().toInt(), 0 );
    QCOMPARE( question.count(), 2 );  // not asked again

    // .. and the yes was used up
    wizard->quit();
    QCOMPARE( question.count(), 3 );
}

void
TuiTests::testExecutionIsAskedTwice()
{
    // Continues with the wizard of the previous test: it is a singleton, at its first step.
    // Put an exec step after the second one, so that going to it needs the confirmation.
    auto* wizard = TuiWizard::instance();
    auto* exec = new ExecStandIn;
    exec->setParent( wizard );
    wizard->addTuiStep( exec );

    // Move to the second step (the last view step)
    wizard->next();
    QCOMPARE( wizard->currentStepIndex(), 1 );

    const bool asks = Settings::instance()->showPromptBeforeExecution();
    QSignalSpy question( wizard, &TuiWizard::executionQuestion );
    wizard->next();
    if ( asks )
    {
        QCOMPARE( question.count(), 1 );
        QCOMPARE( wizard->currentStepIndex(), 1 );  // still waiting for the answer
        wizard->confirmExecutionAndNext();
    }
    QCOMPARE( wizard->currentStepIndex(), 2 );
    QVERIFY( wizard->currentStep() == exec );
}

void
TuiTests::testFieldEditsValue()
{
    std::string value;
    int changes = 0;
    auto f = Tui::field( QStringLiteral( "Name" ), &value, false, [ & ] { ++changes; }, [] { return QStringLiteral( "bad" ); } );

    f->OnEvent( ftxui::Event::Character( 'a' ) );
    f->OnEvent( ftxui::Event::Character( 'b' ) );
    QCOMPARE( value, std::string( "ab" ) );
    QCOMPARE( changes, 2 );

    // The label and the status message are drawn
    ftxui::Screen screen( 40, 3 );
    ftxui::Render( screen, f->Render() );
    const std::string drawn = screen.ToString();
    QVERIFY( drawn.find( "Name" ) != std::string::npos );
    QVERIFY( drawn.find( "bad" ) != std::string::npos );
}

void
TuiTests::testCheckToggles()
{
    bool value = false;
    int changes = 0;
    auto c = Tui::check( QStringLiteral( "Yes" ), &value, [ & ] { ++changes; } );

    c->OnEvent( ftxui::Event::Return );
    QVERIFY( value );
    QCOMPARE( changes, 1 );
    c->OnEvent( ftxui::Event::Return );
    QVERIFY( !value );
    QCOMPARE( changes, 2 );
}

void
TuiTests::testScrolledRenders()
{
    auto page = Tui::scrolled(
        []
        {
            ftxui::Elements lines;
            for ( int i = 0; i < 100; ++i )
            {
                lines.push_back( ftxui::text( "line " + std::to_string( i ) ) );
            }
            return ftxui::vbox( lines );
        } );

    // Not handled: other keys go on to the application
    QVERIFY( !page->OnEvent( ftxui::Event::Character( 'x' ) ) );
    QVERIFY( page->OnEvent( ftxui::Event::PageDown ) );

    ftxui::Screen screen( 20, 5 );
    ftxui::Render( screen, page->Render() );
    QVERIFY( !screen.ToString().empty() );
}

void
TuiTests::testStripMarkup()
{
    QCOMPARE( Tui::stripMarkup( QStringLiteral( "plain" ) ), QStringLiteral( "plain" ) );
    QCOMPARE( Tui::stripMarkup( QStringLiteral( "<strong>Erase</strong> disk <b>/dev/sda</b>" ) ),
              QStringLiteral( "Erase disk /dev/sda" ) );
    QCOMPARE( Tui::stripMarkup( QStringLiteral( "one<br/>two<br>three" ) ), QStringLiteral( "one\ntwo\nthree" ) );
    QCOMPARE( Tui::stripMarkup( QStringLiteral( "<ul><li>a</li><li>b</li></ul>" ) ), QStringLiteral( "- a\n- b" ) );
    QCOMPARE( Tui::stripMarkup( QStringLiteral( "a &amp; b &lt;c&gt;" ) ), QStringLiteral( "a & b <c>" ) );
    // Runs of empty lines are squeezed
    QCOMPARE( Tui::stripMarkup( QStringLiteral( "a<br/><br/><br/><br/>b" ) ), QStringLiteral( "a\n\nb" ) );
}

void
TuiTests::testPickerMoves()
{
    std::vector< std::string > entries { "one", "two", "three" };
    int selected = 0;
    int changes = 0;
    auto p = Tui::picker( QStringLiteral( "Numbers" ), &entries, &selected, [ & ] { ++changes; } );

    p->OnEvent( ftxui::Event::ArrowDown );
    QCOMPARE( selected, 1 );
    QCOMPARE( changes, 1 );
    p->OnEvent( ftxui::Event::ArrowDown );
    QCOMPARE( selected, 2 );

    ftxui::Screen screen( 30, 8 );
    ftxui::Render( screen, p->Render() );
    const std::string drawn = screen.ToString();
    QVERIFY( drawn.find( "Numbers" ) != std::string::npos );
    QVERIFY( drawn.find( "three" ) != std::string::npos );
}

void
TuiTests::testFillEntries()
{
    QStringListModel model( QStringList { "a", "b" } );
    std::vector< std::string > entries;
    QVERIFY( Tui::fillEntries( &entries, &model ) );
    QCOMPARE( entries.size(), size_t( 2 ) );
    QCOMPARE( entries[ 1 ], std::string( "b" ) );
    QVERIFY( !Tui::fillEntries( &entries, &model ) );  // nothing changed

    model.setStringList( QStringList { "a", "c", "d" } );
    QVERIFY( Tui::fillEntries( &entries, &model ) );
    QCOMPARE( entries.size(), size_t( 3 ) );
}

void
TuiTests::testLoaderScan()
{
    QTemporaryDir root;
    QVERIFY( root.isValid() );
    QDir dir( root.path() );

    auto writeDesc = [ & ]( const QString& subdir, const QByteArray& contents )
    {
        QVERIFY( dir.mkpath( subdir ) );
        QFile f( dir.absoluteFilePath( subdir + QStringLiteral( "/module.desc" ) ) );
        QVERIFY( f.open( QFile::WriteOnly ) );
        f.write( contents );
    };
    writeDesc( "good", "---\ntype: job\nname: good\ninterface: process\ncommand: /bin/true\n" );
    writeDesc( "welcome",
               "---\ntype: viewmodule\nname: welcome\ninterface: qtplugin\nload: libw.so\ntuiLoad: libwtui.so\n" );
    writeDesc( "misnamed", "---\ntype: job\nname: something-else\ninterface: process\ncommand: /bin/true\n" );

    TuiModuleLoader loader( { root.path(), QStringLiteral( "/does/not/exist" ) } );
    QCOMPARE( loader.scan(), 2 );  // the misnamed one does not count

    QVERIFY( loader.moduleDescriptor( "good" ).isValid() );
    QVERIFY( !loader.moduleDescriptor( "good" ).hasTui() );
    QVERIFY( loader.moduleDescriptor( "welcome" ).hasTui() );
    QVERIFY( !loader.moduleDescriptor( "misnamed" ).isValid() );
    QVERIFY( loader.requirementsModel() != nullptr );
}
