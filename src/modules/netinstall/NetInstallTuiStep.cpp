/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "NetInstallTuiStep.h"

#include "TuiWidgets.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include <ftxui/component/event.hpp>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( NetInstallTuiStepFactory, registerPlugin< NetInstallTuiStep >(); )

NetInstallTuiStep::NetInstallTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
    connect( m_config, &Config::statusReady, this, &NetInstallTuiStep::nextIsReady );
}

NetInstallTuiStep::~NetInstallTuiStep() {}

QString
NetInstallTuiStep::prettyName() const
{
    return m_config->sidebarLabel();
}

QObject*
NetInstallTuiStep::config()
{
    return m_config;
}

bool
NetInstallTuiStep::isNextEnabled() const
{
    return !m_config->required() || m_nextEnabled;
}

bool
NetInstallTuiStep::isBackEnabled() const
{
    return true;
}

bool
NetInstallTuiStep::isAtBeginning() const
{
    return true;
}

bool
NetInstallTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
NetInstallTuiStep::jobs() const
{
    return {};
}

void
NetInstallTuiStep::nextIsReady()
{
    m_nextEnabled = true;
    m_expandedInitially = false;  // there is data now
    emit nextStatusChanged( true );
}

void
NetInstallTuiStep::onActivate()
{
    // Other modules (the package chooser) can select or add things for us
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    const QStringList selectNames = gs->value( "netinstallSelect" ).toStringList();
    if ( !selectNames.isEmpty() )
    {
        m_config->model()->setSelections( selectNames );
    }
    const QVariantList groups = gs->value( "netinstallAdd" ).toList();
    if ( !groups.isEmpty() )
    {
        m_config->model()->appendModelData( groups );
    }
}

void
NetInstallTuiStep::onLeave()
{
    m_config->finalizeGlobalStorage( moduleInstanceKey() );
}

void
NetInstallTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

void
NetInstallTuiStep::collect( const QModelIndex& parent, int depth, std::vector< Row >& into ) const
{
    const auto* model = m_config->model();
    for ( int i = 0; i < model->rowCount( parent ); ++i )
    {
        const QModelIndex index = model->index( i, 0, parent );
        if ( model->data( index, Qt::DisplayRole ).isNull() )
        {
            continue;  // hidden
        }
        const bool children = model->rowCount( index ) > 0;
        into.push_back( { index, depth, children } );
        if ( children && m_expanded.contains( index.internalId() ) )
        {
            collect( index, depth + 1, into );
        }
    }
}

std::vector< NetInstallTuiStep::Row >
NetInstallTuiStep::rows() const
{
    std::vector< Row > r;
    collect( QModelIndex(), 0, r );
    return r;
}

void
NetInstallTuiStep::expandInitial( const QModelIndex& parent )
{
    const auto* model = m_config->model();
    for ( int i = 0; i < model->rowCount( parent ); ++i )
    {
        const QModelIndex index = model->index( i, 0, parent );
        if ( model->data( index, PackageModel::MetaExpandRole ).toBool() )
        {
            m_expanded.insert( index.internalId() );
        }
        expandInitial( index );
    }
}

void
NetInstallTuiStep::toggleChecked( const QModelIndex& index )
{
    auto* model = m_config->model();
    const QVariant state = model->data( index, Qt::CheckStateRole );
    if ( !state.isValid() )
    {
        return;  // not changeable
    }
    model->setData( index, int( state.toInt() == Qt::Checked ? Qt::Unchecked : Qt::Checked ), Qt::CheckStateRole );
}

ftxui::Component
NetInstallTuiStep::createComponent()
{
    using namespace ftxui;

    Component view = Renderer(
        [ this ]
        {
            if ( !m_expandedInitially && m_config->model()->rowCount() > 0 )
            {
                expandInitial( QModelIndex() );
                m_expandedInitially = true;
            }

            const auto list = rows();
            m_cursor = qBound( 0, m_cursor, qMax( 0, int( list.size() ) - 1 ) );

            Elements lines;
            for ( size_t i = 0; i < list.size(); ++i )
            {
                const Row& row = list[ i ];
                const QVariant state = m_config->model()->data( row.index, Qt::CheckStateRole );
                std::string mark = "    ";
                if ( state.isValid() )
                {
                    mark = state.toInt() == Qt::Checked ? "[x] " : state.toInt() == Qt::PartiallyChecked ? "[-] " : "[ ] ";
                }
                const std::string fold = row.hasChildren ? ( m_expanded.contains( row.index.internalId() ) ? "v " : "> " )
                                                          : "  ";
                const QString name = m_config->model()->data( row.index, Qt::DisplayRole ).toString();
                Element line = text( std::string( size_t( row.depth ) * 2, ' ' ) + fold + mark + name.toStdString() );
                if ( int( i ) == m_cursor )
                {
                    line = line | inverted | focus;
                }
                lines.push_back( line );
            }
            if ( lines.empty() )
            {
                lines.push_back( text( m_config->status().toStdString() ) | dim );
            }

            Elements status;
            if ( m_config->statusCode() != Config::Status::Ok )
            {
                status.push_back( ClaudeInstall::Tui::problem( m_config->status() ) );
            }
            return ClaudeInstall::Tui::titled(
                m_config->titleLabel(),
                vbox( { vbox( lines ) | vscroll_indicator | yframe | flex,
                        vbox( status ),
                        text( ( tr( "Up and down move; right and left open and close; space chooses.", "@label" ) )
                                  .toStdString() )
                            | dim } ) );
        } );

    return CatchEvent( view,
                       [ this ]( Event event )
                       {
                           const auto list = rows();
                           const int count = int( list.size() );
                           if ( count == 0 )
                           {
                               return false;
                           }
                           m_cursor = qBound( 0, m_cursor, count - 1 );
                           const Row& row = list[ size_t( m_cursor ) ];

                           if ( event == Event::ArrowUp )
                           {
                               m_cursor = qMax( 0, m_cursor - 1 );
                           }
                           else if ( event == Event::ArrowDown )
                           {
                               m_cursor = qMin( count - 1, m_cursor + 1 );
                           }
                           else if ( event == Event::ArrowRight )
                           {
                               if ( row.hasChildren )
                               {
                                   m_expanded.insert( row.index.internalId() );
                               }
                           }
                           else if ( event == Event::ArrowLeft )
                           {
                               m_expanded.remove( row.index.internalId() );
                           }
                           else if ( event == Event::Character( ' ' ) || event == Event::Return )
                           {
                               toggleChecked( row.index );
                           }
                           else
                           {
                               return false;
                           }
                           return true;
                       } );
}
