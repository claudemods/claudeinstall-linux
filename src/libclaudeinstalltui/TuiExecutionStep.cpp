/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "TuiExecutionStep.h"

#include "Settings.h"
#include "modulesystem/Module.h"

#include <ftxui/dom/elements.hpp>

#include <string>

namespace ClaudeInstall
{

TuiExecutionStep::TuiExecutionStep( QObject* parent )
    : TuiStep( parent )
    , m_model( new ExecutionModel( this ) )
{
}

TuiExecutionStep::~TuiExecutionStep() {}

QString
TuiExecutionStep::prettyName() const
{
    return Settings::instance() && Settings::instance()->isSetupMode() ? tr( "Set Up", "@label" )
                                                                       : tr( "Install", "@label" );
}

ftxui::Component
TuiExecutionStep::createComponent()
{
    return ftxui::Renderer(
        [ this ]
        {
            using namespace ftxui;
            const float progress = float( m_model->progress() );
            const std::string message = m_model->message().toStdString();
            return vbox( { text( prettyName().toStdString() ) | bold,
                           separator(),
                           hbox( { gauge( progress ) | flex, text( " " + std::to_string( int( progress * 100 ) ) + "%" ) } ),
                           paragraph( message ) } );
        } );
}

void
TuiExecutionStep::onActivate()
{
    const auto instanceDescriptors = Settings::instance()->moduleInstances();

    QList< ExecutionModel::WeightedJobs > groups;
    for ( const auto& entry : m_entries )
    {
        Module* module = entry.module;
        if ( !module )
        {
            continue;
        }

        int weight = entry.weight;
        for ( const auto& d : instanceDescriptors )
        {
            if ( d.key() == module->instanceKey() && d.explicitWeight() )
            {
                weight = d.weight();
            }
        }
        weight = qBound( 1, weight, 100 );

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

    m_model->start( groups );
}

void
TuiExecutionStep::appendModule( Module* module, int weight )
{
    m_entries.append( { module, weight } );
}

}  // namespace ClaudeInstall
