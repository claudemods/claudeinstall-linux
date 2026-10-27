/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Tests.h"

#include "BrandingInfo.h"
#include "ExecutionModel.h"
#include "Step.h"
#include "Wizard.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "Settings.h"
#include "utils/Logger.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClaudeInstall;

QTEST_GUILESS_MAIN( StepTests )

namespace
{
/// A step with a number of pages that records how it is used
class FakeStep : public Step
{
public:
    FakeStep( const QString& name, int pages = 1, bool ready = true, bool exec = false )
        : m_name( name )
        , m_pages( pages )
        , m_ready( ready )
        , m_exec( exec )
    {
    }

    QString prettyName() const override { return m_name; }
    bool isNextEnabled() const override { return m_ready; }
    bool isBackEnabled() const override { return true; }
    bool isAtBeginning() const override { return m_page == 0; }
    bool isAtEnd() const override { return m_page == m_pages - 1; }
    void next() override { ++m_page; }
    void back() override { --m_page; }
    JobList jobs() const override { return JobList(); }
    void onActivate() override { ++activated; }
    void onLeave() override { ++left; }
    void onCancel() override { cancelOrder = ++s_cancelCounter; }
    bool isExecutionStep() const override { return m_exec; }

    void setReady( bool ready )
    {
        m_ready = ready;
        emit nextStatusChanged( ready );
    }

    int page() const { return m_page; }

    int activated = 0;
    int left = 0;
    int cancelOrder = 0;
    static int s_cancelCounter;

private:
    QString m_name;
    int m_pages;
    int m_page = 0;
    bool m_ready;
    bool m_exec;
};

int FakeStep::s_cancelCounter = 0;

/// A wizard that records what the frontend hooks are asked
class TestWizard : public Wizard
{
public:
    bool allowCancel = false;
    int cancelAsked = 0;
    int exitCode = -1;
    QList< int > shown;

protected:
    void stepShown( int index ) override { shown.append( index ); }
    bool confirmCancel() override
    {
        ++cancelAsked;
        return allowCancel;
    }
    void exitApplication( int code ) override { exitCode = code; }
};
}  // namespace

StepTests::StepTests() {}

StepTests::~StepTests() {}

void
StepTests::initTestCase()
{
    Logger::setupLogLevel( Logger::LOGDEBUG );

    (void)new ClaudeInstall::JobQueue();
    (void)new ClaudeInstall::Settings( true );
}

void
StepTests::testStepDefaults()
{
    FakeStep step( "defaults" );
    QVERIFY( step.config() == nullptr );
    QVERIFY( step.prettyStatus().isEmpty() );
    QVERIFY( step.summaryText().isEmpty() );  // follows prettyStatus()
    QVERIFY( !step.isExecutionStep() );
    QVERIFY( step.checkRequirements().isEmpty() );
}

void
StepTests::testNavigation()
{
    TestWizard wizard;
    FakeStep a( "A" );
    FakeStep b( "B", 2 );
    FakeStep c( "C" );
    wizard.addStep( &a );
    wizard.addStep( &b );
    wizard.addStep( &c );
    QCOMPARE( wizard.steps().size(), 3 );

    QVERIFY( wizard.currentStepBase() == nullptr );  // not started
    QVERIFY( wizard.nextEnabled() );  // from the first step
    wizard.onInitComplete();
    QCOMPARE( wizard.currentStepIndex(), 0 );
    QVERIFY( wizard.currentStepBase() == &a );
    QCOMPARE( a.activated, 1 );

    // A has one page: next switches to B
    wizard.next();
    QCOMPARE( wizard.currentStepIndex(), 1 );
    QCOMPARE( a.left, 1 );
    QCOMPARE( b.activated, 1 );
    QCOMPARE( wizard.nextLabel(), QString( "&Next" ) );
    QCOMPARE( wizard.quitLabel(), QString( "&Cancel" ) );

    // B has two pages: next stays in B and moves to its second page
    wizard.next();
    QCOMPARE( wizard.currentStepIndex(), 1 );
    QCOMPARE( b.page(), 1 );
    QCOMPARE( b.left, 0 );

    // Now at the end of B: next switches to C, which is the very end
    wizard.next();
    QCOMPARE( wizard.currentStepIndex(), 2 );
    QCOMPARE( c.activated, 1 );
    QCOMPARE( wizard.quitLabel(), QString( "&Done" ) );
    QVERIFY( wizard.quitEnabled() );

    // Back from C (at its beginning) goes to B, which is on its second page
    wizard.back();
    QCOMPARE( wizard.currentStepIndex(), 1 );
    QCOMPARE( wizard.quitLabel(), QString( "&Cancel" ) );
    // .. and back inside B goes to its first page, then leaves it
    wizard.back();
    QCOMPARE( wizard.currentStepIndex(), 1 );
    QCOMPARE( b.page(), 0 );
    wizard.back();
    QCOMPARE( wizard.currentStepIndex(), 0 );
    QVERIFY( !wizard.backEnabled() );  // can't go back from the first step

    // The frontend was told which steps to show, in order
    QCOMPARE( wizard.shown, ( QList< int > { 1, 2, 1, 0 } ) );
}

void
StepTests::testModel()
{
    TestWizard wizard;
    FakeStep a( "First" );
    FakeStep b( "Second" );
    wizard.addStep( &a );
    wizard.addStep( &b );

    QCOMPARE( wizard.rowCount(), 2 );
    QCOMPARE( wizard.data( wizard.index( 0 ), Qt::DisplayRole ).toString(), QString( "First" ) );
    QCOMPARE( wizard.data( wizard.index( 1 ), Qt::DisplayRole ).toString(), QString( "Second" ) );
    QVERIFY( !wizard.data( wizard.index( 5 ), Qt::DisplayRole ).isValid() );

    wizard.onInitComplete();
    wizard.next();
    QCOMPARE( wizard.data( wizard.index( 0 ), Wizard::ProgressTreeItemCurrentIndex ).toInt(), 1 );

    // Adding a step is visible to the model's users
    QSignalSpy inserted( &wizard, &QAbstractItemModel::rowsInserted );
    FakeStep c( "Third" );
    wizard.addStep( &c );
    QCOMPARE( inserted.count(), 1 );
    QCOMPARE( wizard.rowCount(), 3 );
}

void
StepTests::testNextStatus()
{
    TestWizard wizard;
    FakeStep a( "A", 1, false );
    FakeStep b( "B" );
    wizard.addStep( &a );
    wizard.addStep( &b );
    wizard.onInitComplete();
    QVERIFY( !wizard.nextEnabled() );  // the first step is not ready

    QSignalSpy spy( &wizard, &Wizard::nextEnabledChanged );
    a.setReady( true );
    QVERIFY( wizard.nextEnabled() );
    QCOMPARE( spy.count(), 1 );

    // Only the current step's status matters
    b.setReady( false );
    QVERIFY( wizard.nextEnabled() );
    QCOMPARE( spy.count(), 1 );
}

void
StepTests::testExecutionStep()
{
    TestWizard wizard;
    FakeStep a( "A" );
    FakeStep b( "B" );
    FakeStep exec( "Install", 1, false, true );
    wizard.addStep( &a );
    wizard.addStep( &b );
    wizard.addStep( &exec );
    wizard.onInitComplete();

    wizard.next();  // to B, just before the exec step
    QCOMPARE( wizard.nextLabel(), QString( "&Install" ) );

    wizard.next();  // into the exec step
    QCOMPARE( wizard.currentStepIndex(), 2 );
    QVERIFY( !wizard.nextEnabled() );
    QVERIFY( !wizard.backEnabled() );
}

void
StepTests::testCancel()
{
    TestWizard wizard;
    FakeStep a( "A" );
    FakeStep b( "B" );
    wizard.addStep( &a );
    wizard.addStep( &b );
    wizard.onInitComplete();
    wizard.next();  // in B, the very end

    // At the end there is no question, just quit
    QCOMPARE( wizard.confirmCancelInstallation(), Wizard::Confirmation::EndOfInstallation );
    wizard.quit();
    QCOMPARE( wizard.cancelAsked, 0 );
    QCOMPARE( wizard.exitCode, 0 );

    // Before the end the user is asked: saying no does nothing
    TestWizard asking;
    FakeStep c( "C" );
    FakeStep d( "D" );
    FakeStep e( "E" );
    asking.addStep( &c );
    asking.addStep( &d );
    asking.addStep( &e );
    asking.onInitComplete();
    asking.next();  // in D
    asking.quit();
    QCOMPARE( asking.cancelAsked, 1 );
    QCOMPARE( asking.exitCode, -1 );

    // .. and saying yes cancels the steps, last first, and exits
    asking.allowCancel = true;
    asking.quit();
    QCOMPARE( asking.cancelAsked, 2 );
    QCOMPARE( asking.exitCode, 0 );
    QVERIFY( d.cancelOrder > 0 );
    QVERIFY( c.cancelOrder > d.cancelOrder );
    QCOMPARE( e.cancelOrder, 0 );  // never reached
}

void
StepTests::testExecutionModel()
{
    ExecutionModel model;
    QCOMPARE( model.progress(), 0.0 );
    QVERIFY( model.message().isEmpty() );

    // It follows the progress of the job queue
    QSignalSpy spy( &model, &ExecutionModel::updated );
    emit ClaudeInstall::JobQueue::instance()->progress( 0.25, QStringLiteral( "working" ) );
    QCOMPARE( spy.count(), 1 );
    QCOMPARE( model.progress(), 0.25 );
    QCOMPARE( model.message(), QString( "working" ) );

    // An empty message keeps the last one
    emit ClaudeInstall::JobQueue::instance()->progress( 0.5, QString() );
    QCOMPARE( model.progress(), 0.5 );
    QCOMPARE( model.message(), QString( "working" ) );
}

/// Writes a branding descriptor into <root>/<component>/branding.desc, returns its path
static QString
writeBranding( const QTemporaryDir& root, const QString& component, const QByteArray& contents )
{
    QDir( root.path() ).mkpath( component );
    const QString path = QDir( root.path() ).absoluteFilePath( component + QStringLiteral( "/branding.desc" ) );
    QFile f( path );
    if ( !f.open( QFile::WriteOnly ) )
    {
        return QString();
    }
    f.write( contents );
    return path;
}

void
StepTests::testBrandingFromDescriptor()
{
    QTemporaryDir root;
    QVERIFY( root.isValid() );
    const QString path = writeBranding( root,
                                        "mybrand",
                                        "---\n"
                                        "componentName: mybrand\n"
                                        "welcomeStyleClaudeInstall: true\n"
                                        "strings:\n"
                                        "  productName: Test OS\n"
                                        "  version: 1.2\n"
                                        "  shortVersion: 1\n"
                                        "  versionedName: Test OS 1.2\n"
                                        "  shortVersionedName: Test 1\n"
                                        "  shortProductName: Test\n"
                                        "  bootloaderEntryName: Test OS\n"
                                        "  productUrl: https://example.org/\n"
                                        "  supportUrl: https://example.org/support\n"
                                        "  releaseNotesUrl: https://example.org/notes\n" );
    QVERIFY( !path.isEmpty() );

    auto& info = BrandingInfo::instance();
    QVERIFY( info.loadFromDescriptor( path ) );
    QCOMPARE( info.componentName(), QStringLiteral( "mybrand" ) );
    QCOMPARE( info.productName(), QStringLiteral( "Test OS" ) );
    QCOMPARE( info.versionedName(), QStringLiteral( "Test OS 1.2" ) );
    QCOMPARE( info.string( BrandingInfo::SupportUrl ), QStringLiteral( "https://example.org/support" ) );
    QVERIFY( info.string( BrandingInfo::KnownIssuesUrl ).isEmpty() );  // not in the file
    QVERIFY( info.welcomeStyleClaudeInstall() );
    QCOMPARE( info.componentDirectory(), QDir( root.path() ).absoluteFilePath( "mybrand" ) );
    QVERIFY( info.translationsDirectory().endsWith( QStringLiteral( "/lang/claudeinstall-mybrand" ) ) );

    // The globals are the same as the GUI branding makes
    ClaudeInstall::GlobalStorage gs( nullptr );
    info.setGlobals( &gs );
    const auto branding = gs.value( "branding" ).toMap();
    QCOMPARE( branding.value( "productName" ).toString(), QStringLiteral( "Test OS" ) );
    QCOMPARE( branding.value( "shortVersion" ).toString(), QStringLiteral( "1" ) );
    QCOMPARE( branding.count(), 12 );

    QCOMPARE( BrandingInfo::entryKey( BrandingInfo::DonateUrl ), QStringLiteral( "donateUrl" ) );
}

void
StepTests::testBrandingFromBadDescriptor()
{
    auto& info = BrandingInfo::instance();
    const QString before = info.productName();

    QVERIFY( !info.loadFromDescriptor( QStringLiteral( "/does/not/exist/branding.desc" ) ) );

    QTemporaryDir root;
    QVERIFY( root.isValid() );
    // The component name must match the directory
    QVERIFY( !info.loadFromDescriptor(
        writeBranding( root, "dirname", "---\ncomponentName: other\nstrings:\n  productName: Other\n" ) ) );
    // There must be strings
    QVERIFY( !info.loadFromDescriptor( writeBranding( root, "nostrings", "---\ncomponentName: nostrings\n" ) ) );

    QCOMPARE( info.productName(), before );  // a failure leaves things as they were
}
