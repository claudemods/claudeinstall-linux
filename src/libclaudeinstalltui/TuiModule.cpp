/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "TuiModule.h"

#include "PlaceholderStep.h"
#include "TuiStep.h"
#include "TuiWizard.h"

#include "utils/Logger.h"
#include "utils/PluginFactory.h"

#include <QDir>
#include <QLibrary>
#include <QPluginLoader>

namespace ClaudeInstall
{

TuiModule::TuiModule()
    : Module()
{
}

TuiModule::~TuiModule()
{
    delete m_loader;
}

Module::Type
TuiModule::type() const
{
    return Module::Type::View;
}

Module::Interface
TuiModule::interface() const
{
    return Module::Interface::QtPlugin;
}

void
TuiModule::initFrom( const ModuleSystem::Descriptor& moduleDescriptor )
{
    if ( !moduleDescriptor.hasTui() )
    {
        return;  // loadSelf() will use a placeholder
    }

    QDir directory( location() );
    QString load = directory.absoluteFilePath( moduleDescriptor.tuiLoad() );
    if ( !QLibrary::isLibrary( load ) )
    {
        cWarning() << "The text-mode plugin" << load << "of module" << moduleDescriptor.name() << "is not a library.";
        return;
    }
    m_loader = new QPluginLoader( load );
}

void
TuiModule::loadSelf()
{
    if ( m_loader )
    {
        ClaudeInstallPluginFactory* pf = qobject_cast< ClaudeInstallPluginFactory* >( m_loader->instance() );
        if ( !pf )
        {
            cWarning() << "No factory:" << m_loader->errorString();
        }
        else
        {
            m_step = pf->create< TuiStep >();
            if ( !m_step )
            {
                cWarning() << "create() failed" << m_loader->errorString();
            }
        }
    }

    if ( !m_step )
    {
        // Either there is no text-mode plugin, or it did not load; in both
        // cases the sequence of steps stays the same as in the GUI.
        cDebug() << "Module" << instanceKey() << "has no usable text-mode step, using a placeholder.";
        m_step = new PlaceholderStep( name() );
    }

    m_step->setModuleInstanceKey( instanceKey() );
    m_step->setConfigurationMap( m_configurationMap );
    TuiWizard::instance()->addTuiStep( m_step );
    m_loaded = true;
    cDebug() << "TuiModule" << instanceKey() << "loading complete.";
}

JobList
TuiModule::jobs() const
{
    return m_step ? m_step->jobs() : JobList();
}

RequirementsList
TuiModule::checkRequirements()
{
    return m_step ? m_step->checkRequirements() : RequirementsList();
}

}  // namespace ClaudeInstall
