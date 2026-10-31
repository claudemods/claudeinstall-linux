// SPDX-FileCopyrightText: no
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MainWindow.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
const char* const kModes[] = { "new", "clone", "distro" };
const char* const kInterfaces[] = { "gui", "tui", "both" };

QString
osReleaseValue( const QString& key )
{
    QFile f( QStringLiteral( "/etc/os-release" ) );
    if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        return QString();
    }
    const QString prefix = key + QLatin1Char( '=' );
    QTextStream in( &f );
    while ( !in.atEnd() )
    {
        QString line = in.readLine();
        if ( line.startsWith( prefix ) )
        {
            line = line.mid( prefix.size() );
            line.remove( QLatin1Char( '"' ) ).remove( QLatin1Char( '\'' ) );
            return line;
        }
    }
    return QString();
}

bool
isArchFamily()
{
    static const QStringList archIds { "arch", "manjaro", "endeavouros", "garuda", "cachyos", "kaos" };
    const QStringList ids = ( osReleaseValue( "ID" ) + QLatin1Char( ' ' ) + osReleaseValue( "ID_LIKE" ) )
                                .split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
    for ( const QString& id : ids )
    {
        if ( archIds.contains( id ) )
        {
            return true;
        }
    }
    return false;
}

QLabel*
heading( const QString& title, const QString& subtitle )
{
    auto* label = new QLabel( QStringLiteral( "<h2 style='margin:0'>%1</h2><p style='margin-top:4px'>%2</p>" )
                                  .arg( title.toHtmlEscaped(), subtitle.toHtmlEscaped() ) );
    label->setWordWrap( true );
    label->setTextFormat( Qt::RichText );
    return label;
}

QFrame*
makeCard( QRadioButton* button, const QString& title, const QString& description )
{
    auto* frame = new QFrame;
    frame->setFrameShape( QFrame::StyledPanel );
    auto* layout = new QVBoxLayout( frame );
    button->setText( title );
    QFont f = button->font();
    f.setBold( true );
    button->setFont( f );
    auto* text = new QLabel( description );
    text->setWordWrap( true );
    text->setContentsMargins( 24, 0, 0, 0 );
    layout->addWidget( button );
    layout->addWidget( text );
    return frame;
}
}  // namespace

MainWindow::MainWindow( const QString& projectDir, QWidget* parent )
    : QMainWindow( parent )
    , m_dir( projectDir )
{
    setWindowTitle( tr( "ClaudeInstall Setup Helper" ) );
    setMinimumSize( 860, 600 );
    resize( 960, 660 );

    auto* central = new QWidget;
    auto* root = new QVBoxLayout( central );
    root->setContentsMargins( 16, 16, 16, 12 );
    root->setSpacing( 12 );
    auto* body = new QHBoxLayout;
    body->setSpacing( 20 );

    m_nav = new QListWidget;
    m_nav->setFixedWidth( 210 );
    m_nav->setSpacing( 3 );
    m_nav->setFrameShape( QFrame::NoFrame );
    m_nav->setStyleSheet( QStringLiteral(
        "QListWidget { background: transparent; outline: 0; font-size: 11pt; }"
        "QListWidget::item { padding: 10px 12px; border-radius: 6px; }"
        "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }" ) );
    m_nav->addItems( { tr( "1.  Installer type" ), tr( "2.  Interface" ), tr( "3.  Build options" ), tr( "4.  Review" ) } );

    m_pages = new QStackedWidget;
    m_pages->addWidget( createTypePage() );
    m_pages->addWidget( createInterfacePage() );
    m_pages->addWidget( createOptionsPage() );
    m_pages->addWidget( createReviewPage() );

    body->addWidget( m_nav );
    body->addWidget( m_pages, 1 );
    root->addLayout( body, 1 );

    auto* line = new QFrame;
    line->setFrameShape( QFrame::HLine );
    line->setFrameShadow( QFrame::Sunken );
    root->addWidget( line );

    auto* buttons = new QHBoxLayout;
    auto* reset = new QPushButton( tr( "Reset to defaults" ) );
    auto* back = new QPushButton( tr( "Back" ) );
    auto* next = new QPushButton( tr( "Next" ) );
    auto* save = new QPushButton( tr( "Save" ) );
    auto* build = new QPushButton( tr( "Save && Build" ) );
    build->setDefault( true );
    buttons->addWidget( reset );
    buttons->addStretch( 1 );
    buttons->addWidget( back );
    buttons->addWidget( next );
    buttons->addSpacing( 16 );
    buttons->addWidget( save );
    buttons->addWidget( build );
    root->addLayout( buttons );
    setCentralWidget( central );

    connect( m_nav, &QListWidget::currentRowChanged, this, [this]( int row ) {
        m_pages->setCurrentIndex( row );
        if ( row == 3 )
        {
            refreshReview();
        }
    } );
    connect( back, &QPushButton::clicked, this, [this] { m_nav->setCurrentRow( qMax( 0, m_nav->currentRow() - 1 ) ); } );
    connect( next, &QPushButton::clicked, this, [this] {
        m_nav->setCurrentRow( qMin( m_nav->count() - 1, m_nav->currentRow() + 1 ) );
    } );
    connect( reset, &QPushButton::clicked, this, &MainWindow::resetDefaults );
    connect( save, &QPushButton::clicked, this, [this] { saveConfig(); } );
    connect( build, &QPushButton::clicked, this, &MainWindow::saveAndBuild );

    resetDefaults();
    loadConfig();
    m_nav->setCurrentRow( 0 );
    statusBar()->showMessage( tr( "Settings file: %1" ).arg( configPath() ) );
}

QWidget*
MainWindow::createTypePage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout( page );
    layout->addWidget( heading( tr( "What is this installer for?" ),
                                tr( "Pick the kind of installer to build. This decides which configuration is installed." ) ) );

    for ( auto& b : m_modeButtons )
    {
        b = new QRadioButton;
    }
    layout->addWidget( makeCard( m_modeButtons[ 0 ],
                                 tr( "New system setup" ),
                                 tr( "Creates users and installs packages on a fresh disk." ) ) );
    layout->addWidget( makeCard( m_modeButtons[ 1 ],
                                 tr( "Cloned system" ),
                                 tr( "Copies this running system to another disk. No new users and no package install." ) ) );
    layout->addWidget( makeCard( m_modeButtons[ 2 ],
                                 tr( "claudemods distributions" ),
                                 tr( "Installs a distribution online with pacstrap and a profile (Spitfire, Apex, CachyOS, "
                                     "Arch ...). Needs an Arch-family system plus arch-install-scripts, curl and unzip." ) ) );

    m_distroWarning = new QLabel;
    m_distroWarning->setWordWrap( true );
    m_distroWarning->setStyleSheet( QStringLiteral( "color:#b36b00;" ) );
    layout->addWidget( m_distroWarning );
    layout->addStretch( 1 );

    for ( auto* b : m_modeButtons )
    {
        connect( b, &QRadioButton::toggled, this, [this] {
            updateWarnings();
            refreshReview();
        } );
    }
    return page;
}

QWidget*
MainWindow::createInterfacePage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout( page );
    layout->addWidget( heading( tr( "Which interface?" ), tr( "The graphical installer is always built with Qt6." ) ) );

    for ( auto& b : m_interfaceButtons )
    {
        b = new QRadioButton;
    }
    layout->addWidget( makeCard( m_interfaceButtons[ 0 ], tr( "GUI (Qt6)" ), tr( "Graphical installer for a desktop live session." ) ) );
    layout->addWidget( makeCard( m_interfaceButtons[ 1 ], tr( "TUI (terminal)" ), tr( "Text-mode installer for servers, TTYs and minimal live images." ) ) );
    layout->addWidget( makeCard( m_interfaceButtons[ 2 ], tr( "Both" ), tr( "Build the GUI and the terminal installer." ) ) );
    layout->addStretch( 1 );

    for ( auto* b : m_interfaceButtons )
    {
        connect( b, &QRadioButton::toggled, this, &MainWindow::refreshReview );
    }
    return page;
}

QWidget*
MainWindow::createOptionsPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout( page );
    layout->addWidget( heading( tr( "Build options" ), tr( "What build.sh should do when you run  ./build.sh build" ) ) );

    auto* steps = new QGroupBox( tr( "Steps" ) );
    auto* stepsLayout = new QVBoxLayout( steps );
    m_deps = new QCheckBox( tr( "Install build dependencies first (uses sudo)" ) );
    m_clean = new QCheckBox( tr( "Clean build: remove the build directory first" ) );
    m_install = new QCheckBox( tr( "Install after building (uses sudo)" ) );
    m_package = new QCheckBox( tr( "Also create a distribution package (needs fpm, otherwise a tarball)" ) );
    m_test = new QCheckBox( tr( "Test build: allow a machine whose only disk holds the running system (no partitioning)" ) );
    for ( auto* c : { m_deps, m_clean, m_install, m_package, m_test } )
    {
        stepsLayout->addWidget( c );
        connect( c, &QCheckBox::toggled, this, &MainWindow::refreshReview );
    }
    layout->addWidget( steps );

    auto* advanced = new QGroupBox( tr( "Advanced" ) );
    auto* form = new QFormLayout( advanced );

    m_jobs = new QSpinBox;
    m_jobs->setRange( 0, 256 );
    m_jobs->setSpecialValueText( tr( "Auto (all cores)" ) );
    form->addRow( tr( "Parallel jobs:" ), m_jobs );

    m_buildDir = new QLineEdit;
    m_buildDir->setPlaceholderText( tr( "build/ next to build.sh" ) );
    auto* browseBuild = new QToolButton;
    browseBuild->setText( QStringLiteral( "…" ) );
    auto* buildRow = new QHBoxLayout;
    buildRow->addWidget( m_buildDir, 1 );
    buildRow->addWidget( browseBuild );
    form->addRow( tr( "Build directory:" ), buildRow );

    m_neededFiles = new QLineEdit;
    m_neededFiles->setPlaceholderText( tr( "needed-files/ next to build.sh (distributions mode)" ) );
    auto* browseNeeded = new QToolButton;
    browseNeeded->setText( QStringLiteral( "…" ) );
    auto* neededRow = new QHBoxLayout;
    neededRow->addWidget( m_neededFiles, 1 );
    neededRow->addWidget( browseNeeded );
    form->addRow( tr( "needed-files folder:" ), neededRow );

    m_cmakeArgs = new QLineEdit;
    m_cmakeArgs->setPlaceholderText( tr( "extra CMake arguments, e.g. -DBUILD_TESTING=ON" ) );
    form->addRow( tr( "Extra CMake arguments:" ), m_cmakeArgs );
    layout->addWidget( advanced );
    layout->addStretch( 1 );

    connect( m_jobs, &QSpinBox::valueChanged, this, &MainWindow::refreshReview );
    for ( auto* e : { m_buildDir, m_neededFiles, m_cmakeArgs } )
    {
        connect( e, &QLineEdit::textChanged, this, &MainWindow::refreshReview );
    }
    connect( browseBuild, &QToolButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory( this, tr( "Build directory" ), m_dir );
        if ( !d.isEmpty() )
        {
            m_buildDir->setText( d );
        }
    } );
    connect( browseNeeded, &QToolButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory( this, tr( "needed-files folder" ), m_dir );
        if ( !d.isEmpty() )
        {
            m_neededFiles->setText( d );
        }
    } );
    return page;
}

QWidget*
MainWindow::createReviewPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout( page );
    layout->addWidget( heading( tr( "Review" ),
                                tr( "This is what gets saved. Press “Save & Build” to save and compile, "
                                    "or run  ./build.sh build  yourself later." ) ) );
    m_review = new QPlainTextEdit;
    m_review->setReadOnly( true );
    m_review->setFont( QFontDatabase::systemFont( QFontDatabase::FixedFont ) );
    layout->addWidget( m_review, 1 );
    return page;
}

QString
MainWindow::configPath() const
{
    return QDir( m_dir ).filePath( QStringLiteral( "build.config" ) );
}

QMap< QString, QString >
MainWindow::values() const
{
    auto checked = []( const QCheckBox* c ) { return c->isChecked() ? QStringLiteral( "1" ) : QStringLiteral( "0" ); };
    auto flat = []( QString s ) { return s.replace( QLatin1Char( '\n' ), QLatin1Char( ' ' ) ).trimmed(); };

    QMap< QString, QString > v;
    for ( int i = 0; i < 3; ++i )
    {
        if ( m_modeButtons[ i ]->isChecked() )
        {
            v[ "MODE" ] = QString::fromLatin1( kModes[ i ] );
        }
        if ( m_interfaceButtons[ i ]->isChecked() )
        {
            v[ "INTERFACE" ] = QString::fromLatin1( kInterfaces[ i ] );
        }
    }
    v[ "TEST" ] = checked( m_test );
    v[ "DEPS" ] = checked( m_deps );
    v[ "CLEAN" ] = checked( m_clean );
    v[ "INSTALL" ] = checked( m_install );
    v[ "PACKAGE" ] = checked( m_package );
    v[ "JOBS" ] = QString::number( m_jobs->value() );
    v[ "BUILDDIR" ] = flat( m_buildDir->text() );
    v[ "NEEDED_FILES" ] = flat( m_neededFiles->text() );
    v[ "CMAKE_ARGS" ] = flat( m_cmakeArgs->text() );
    return v;
}

QStringList
MainWindow::legacyFlags() const
{
    const auto v = values();
    QStringList f;
    f << ( v[ "MODE" ] == QLatin1String( "distro" ) ? QStringLiteral( "--claudemodsdistrosonline" )
                                                     : QStringLiteral( "--%1" ).arg( v[ "MODE" ] ) );
    f << QStringLiteral( "--%1" ).arg( v[ "INTERFACE" ] );
    if ( v[ "TEST" ] == QLatin1String( "1" ) )
    {
        f << QStringLiteral( "--test" );
    }
    if ( v[ "DEPS" ] != QLatin1String( "1" ) )
    {
        f << QStringLiteral( "--no-deps" );
    }
    if ( v[ "CLEAN" ] == QLatin1String( "1" ) )
    {
        f << QStringLiteral( "--clean" );
    }
    if ( v[ "INSTALL" ] == QLatin1String( "1" ) )
    {
        f << QStringLiteral( "--install" );
    }
    if ( v[ "PACKAGE" ] == QLatin1String( "1" ) )
    {
        f << QStringLiteral( "--package" );
    }
    return f;
}

void
MainWindow::resetDefaults()
{
    m_modeButtons[ 0 ]->setChecked( true );
    m_interfaceButtons[ 0 ]->setChecked( true );
    m_deps->setChecked( true );
    m_clean->setChecked( false );
    m_install->setChecked( false );
    m_package->setChecked( false );
    m_test->setChecked( false );
    m_jobs->setValue( 0 );
    m_buildDir->clear();
    m_neededFiles->clear();
    m_cmakeArgs->clear();
    updateWarnings();
    refreshReview();
}

void
MainWindow::loadConfig()
{
    QFile f( configPath() );
    if ( !f.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        return;
    }
    QMap< QString, QString > v;
    QTextStream in( &f );
    while ( !in.atEnd() )
    {
        const QString line = in.readLine();
        const int eq = line.indexOf( QLatin1Char( '=' ) );
        if ( line.startsWith( QLatin1Char( '#' ) ) || eq <= 0 )
        {
            continue;
        }
        v[ line.left( eq ).trimmed() ] = line.mid( eq + 1 );
    }
    for ( int i = 0; i < 3; ++i )
    {
        if ( v.value( "MODE" ) == QLatin1String( kModes[ i ] ) )
        {
            m_modeButtons[ i ]->setChecked( true );
        }
        if ( v.value( "INTERFACE" ) == QLatin1String( kInterfaces[ i ] ) )
        {
            m_interfaceButtons[ i ]->setChecked( true );
        }
    }
    auto on = [&v]( const char* key, bool def ) {
        return v.contains( key ) ? v.value( key ) == QLatin1String( "1" ) : def;
    };
    m_test->setChecked( on( "TEST", false ) );
    m_deps->setChecked( on( "DEPS", true ) );
    m_clean->setChecked( on( "CLEAN", false ) );
    m_install->setChecked( on( "INSTALL", false ) );
    m_package->setChecked( on( "PACKAGE", false ) );
    m_jobs->setValue( v.value( "JOBS" ).toInt() );
    m_buildDir->setText( v.value( "BUILDDIR" ) );
    m_neededFiles->setText( v.value( "NEEDED_FILES" ) );
    m_cmakeArgs->setText( v.value( "CMAKE_ARGS" ) );
    updateWarnings();
    refreshReview();
}

bool
MainWindow::saveConfig()
{
    QFile f( configPath() );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) )
    {
        QMessageBox::critical( this, tr( "Cannot save" ), tr( "Could not write %1:\n%2" ).arg( configPath(), f.errorString() ) );
        return false;
    }
    QTextStream out( &f );
    out << "# Written by claudeinstall-helper. Read by \"./build.sh build\". Safe to edit by hand.\n";
    const auto v = values();
    for ( auto it = v.constBegin(); it != v.constEnd(); ++it )
    {
        out << it.key() << '=' << it.value() << '\n';
    }
    out.flush();
    statusBar()->showMessage( tr( "Saved %1" ).arg( configPath() ), 5000 );
    return true;
}

void
MainWindow::saveAndBuild()
{
    if ( !saveConfig() )
    {
        return;
    }

    struct Terminal
    {
        const char* program;
        QStringList prefix;
    };
    const QList< Terminal > terminals {
        { "konsole", { "-e" } },
        { "gnome-terminal", { "--" } },
        { "xfce4-terminal", { "-x" } },
        { "mate-terminal", { "-x" } },
        { "kitty", {} },
        { "alacritty", { "-e" } },
        { "xterm", { "-e" } },
        { "x-terminal-emulator", { "-e" } },
    };
    if ( !QFile::exists( QDir( m_dir ).filePath( QStringLiteral( "build.sh" ) ) ) )
    {
        QMessageBox::critical( this, tr( "build.sh not found" ), tr( "There is no build.sh in %1." ).arg( m_dir ) );
        return;
    }
    // The folder is passed as an argument ($1), not through the terminal's working directory:
    // some terminals (e.g. Konsole attaching to a running instance) ignore the latter.
    const QStringList script { QStringLiteral( "sh" ),
                               QStringLiteral( "-c" ),
                               QStringLiteral( "cd \"$1\" && ./build.sh build; echo; printf 'Finished. Press Enter to close. '; read -r _" ),
                               QStringLiteral( "claudeinstall-build" ),
                               m_dir };

    for ( const auto& t : terminals )
    {
        const QString exe = QStandardPaths::findExecutable( QString::fromLatin1( t.program ) );
        if ( exe.isEmpty() )
        {
            continue;
        }
        if ( QProcess::startDetached( exe, t.prefix + script, m_dir ) )
        {
            statusBar()->showMessage( tr( "Build started in %1" ).arg( QString::fromLatin1( t.program ) ), 8000 );
            return;
        }
    }
    QMessageBox::information( this,
                              tr( "Settings saved" ),
                              tr( "No terminal emulator was found. Run this in a terminal:\n\ncd \"%1\"\n./build.sh build" ).arg( m_dir ) );
}

void
MainWindow::refreshReview()
{
    if ( !m_review )
    {
        return;
    }
    QString text;
    text += tr( "# %1\n" ).arg( configPath() );
    const auto v = values();
    for ( auto it = v.constBegin(); it != v.constEnd(); ++it )
    {
        text += it.key() + QLatin1Char( '=' ) + it.value() + QLatin1Char( '\n' );
    }
    text += tr( "\n# Build with the saved settings:\n./build.sh build\n" );
    text += tr( "\n# Same thing without the settings file:\n./build.sh %1\n" ).arg( legacyFlags().join( QLatin1Char( ' ' ) ) );
    m_review->setPlainText( text );
}

void
MainWindow::updateWarnings()
{
    if ( !m_distroWarning )
    {
        return;
    }
    const bool distro = m_modeButtons[ 2 ]->isChecked();
    if ( distro && !isArchFamily() )
    {
        m_distroWarning->setText( tr( "Warning: this system (%1) is not Arch-family. The distributions installer uses pacstrap "
                                      "and must be built and run on Arch, CachyOS, Manjaro, EndeavourOS ..." )
                                      .arg( osReleaseValue( "PRETTY_NAME" ) ) );
    }
    else
    {
        m_distroWarning->clear();
    }
}
