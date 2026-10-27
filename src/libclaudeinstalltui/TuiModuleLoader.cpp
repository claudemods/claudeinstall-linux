/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "TuiModuleLoader.h"

#include "TuiExecutionStep.h"
#include "TuiModule.h"
#include "TuiWizard.h"

#include "ModuleFactory.h"
#include "ModuleLoadState.h"
#include "Settings.h"
#include "modulesystem/RequirementsChecker.h"
#include "modulesystem/RequirementsModel.h"
#include "utils/Logger.h"
#include "utils/Yaml.h"

#include <QDir>
#include <QFileInfo>
#include <QTimer>

namespace ClaudeInstall
{

TuiModuleLoader* TuiModuleLoader::s_instance = nullptr;

TuiModuleLoader*
TuiModuleLoader::instance()
{
    return s_instance;
}

TuiModuleLoader::TuiModuleLoader( const QStringList& paths, QObject* parent )
    : QObject( parent )
    , m_paths( paths )
    , m_requirementsModel( new RequirementsModel( this ) )
{
    Q_ASSERT( !s_instance );
    s_instance = this;

    ModuleLoadState::instance()->setRequirementsModel( m_requirementsModel );
    connect( this, &TuiModuleLoader::requirementsComplete, ModuleLoadState::instance(), &ModuleLoadState::requirementsComplete );
    connect( this, &TuiModuleLoader::modulesLoaded, ModuleLoadState::instance(), &ModuleLoadState::notifyModulesLoaded );

    // View modules are shown in the terminal in this frontend
    setViewModuleCreator( &TuiModule::create );
}

TuiModuleLoader::~TuiModuleLoader()
{
    qDeleteAll( m_loaded );
    if ( s_instance == this )
    {
        s_instance = nullptr;
    }
}

int
TuiModuleLoader::scan()
{
    Logger::Once deb;
    for ( const QString& path : m_paths )
    {
        QDir currentDir( path );
        if ( !currentDir.exists() || !currentDir.isReadable() )
        {
            cDebug() << deb << "Module search path does not exist:" << path;
            continue;
        }

        const QStringList subdirs = currentDir.entryList( QDir::AllDirs | QDir::NoDotAndDotDot );
        for ( const QString& subdir : subdirs )
        {
            currentDir.setPath( path );
            if ( !currentDir.cd( subdir ) )
            {
                cWarning() << "Module directory is not accessible:" << path << "/" << subdir;
                continue;
            }

            QFileInfo descriptorFileInfo( currentDir.absoluteFilePath( QLatin1String( "module.desc" ) ) );
            if ( !descriptorFileInfo.exists() || !descriptorFileInfo.isReadable() )
            {
                cDebug() << deb << "Potential module descriptor is bad" << descriptorFileInfo.absoluteFilePath();
                continue;
            }

            bool ok = false;
            const QVariantMap moduleDescriptorMap = ClaudeInstall::YAML::load( descriptorFileInfo, &ok );
            const QString moduleName = ok ? moduleDescriptorMap.value( "name" ).toString() : QString();

            if ( ok && !moduleName.isEmpty() && ( moduleName == currentDir.dirName() )
                 && !m_descriptors.contains( moduleName ) )
            {
                auto descriptor = ModuleSystem::Descriptor::fromDescriptorData( moduleDescriptorMap,
                                                                                 descriptorFileInfo.absoluteFilePath() );
                descriptor.setDirectory( descriptorFileInfo.absoluteDir().absolutePath() );
                m_descriptors.insert( moduleName, descriptor );
            }
            else if ( !m_descriptors.contains( moduleName ) )
            {
                // Duplicates are fine; other problems are reported
                cWarning() << deb << "Module descriptor" << descriptorFileInfo.absoluteFilePath() << "has bad name"
                           << moduleName;
            }
        }
    }
    cDebug() << deb << "Found" << m_descriptors.count() << "modules";
    return m_descriptors.count();
}

ModuleSystem::Descriptor
TuiModuleLoader::moduleDescriptor( const QString& name ) const
{
    return m_descriptors.value( name );
}

size_t
TuiModuleLoader::removeModulesWithMissingDependencies()
{
    size_t removed = 0;
    bool again = true;
    while ( again )
    {
        again = false;
        for ( auto it = m_descriptors.begin(); it != m_descriptors.end(); ++it )
        {
            QStringList unmet;
            for ( const QString& dep : it->requiredModules() )
            {
                if ( !m_descriptors.contains( dep ) )
                {
                    unmet << dep;
                }
            }
            if ( !unmet.isEmpty() )
            {
                cWarning() << "Module" << it->name() << "requires missing modules" << Logger::DebugList( unmet );
                m_descriptors.erase( it );
                ++removed;
                again = true;
                break;
            }
        }
    }
    return removed;
}

bool
TuiModuleLoader::addModule( Module* module )
{
    if ( !module || !module->instanceKey().isValid() )
    {
        return false;
    }

    // Each module needs the modules it requires to come before it in the sequence
    const auto required = m_descriptors.value( module->name() ).requiredModules();
    for ( const QString& r : required )
    {
        bool found = false;
        for ( const Module* m : m_loaded )
        {
            found = found || ( m->name() == r );
        }
        if ( !found )
        {
            cError() << "Module" << module->name() << "requires" << r << "before it in sequence.";
            return false;
        }
    }

    if ( !module->isLoaded() )
    {
        module->loadSelf();
    }
    // Even a module that failed is kept, so that a second attempt is recognised
    m_loaded.insert( module->instanceKey(), module );
    if ( !module->isLoaded() )
    {
        cError() << "Module" << module->instanceKey().toString() << "loading FAILED.";
        return false;
    }
    return true;
}

bool
TuiModuleLoader::loadModules()
{
    if ( removeModulesWithMissingDependencies() )
    {
        cWarning() << "Some installed modules have unmet dependencies.";
    }

    const auto* settings = Settings::instance();
    const auto customInstances = settings->moduleInstances();

    QStringList failedModules;
    TuiExecutionStep* execStep = nullptr;
    for ( const auto& modulePhase : settings->modulesSequence() )
    {
        const ModuleSystem::Action action = modulePhase.first;
        if ( action == ModuleSystem::Action::Show )
        {
            execStep = nullptr;  // jobs after a show-phase belong to a new exec-step
        }

        for ( const auto& instanceKey : modulePhase.second )
        {
            if ( !instanceKey.isValid() )
            {
                cError() << "Wrong module entry format for module" << instanceKey;
                failedModules.append( instanceKey.toString() );
                continue;
            }

            const auto descriptor = m_descriptors.value( instanceKey.module() );
            if ( !descriptor.isValid() )
            {
                cError() << "Module" << instanceKey.toString() << "not found in module search paths."
                         << Logger::DebugList( m_paths );
                failedModules.append( instanceKey.toString() );
                continue;
            }

            QString configFileName;
            if ( descriptor.hasConfig() )
            {
                for ( const auto& d : customInstances )
                {
                    if ( d.key() == instanceKey )
                    {
                        configFileName = d.configFileName();
                    }
                }
            }

            Module* thisModule = m_loaded.value( instanceKey, nullptr );
            if ( thisModule )
            {
                if ( !thisModule->isLoaded() )
                {
                    cError() << "Module" << instanceKey.toString() << "exists but not loaded.";
                    failedModules.append( instanceKey.toString() );
                    continue;
                }
                cDebug() << "Module" << instanceKey.toString() << "already loaded.";
            }
            else
            {
                thisModule = moduleFromDescriptor( descriptor, instanceKey.id(), configFileName, descriptor.directory() );
                if ( !thisModule )
                {
                    cError() << "Module" << instanceKey.toString() << "cannot be created from descriptor"
                             << configFileName;
                    failedModules.append( instanceKey.toString() );
                    continue;
                }
                if ( !addModule( thisModule ) )
                {
                    failedModules.append( instanceKey.toString() );
                    continue;
                }
            }

            if ( action == ModuleSystem::Action::Exec )
            {
                auto* wizard = TuiWizard::instance();
                if ( !execStep )
                {
                    // Reuse the last step if it already is the exec step
                    execStep = qobject_cast< TuiExecutionStep* >( wizard->tuiSteps().isEmpty() ? nullptr
                                                                                              : wizard->tuiSteps().last() );
                    if ( !execStep )
                    {
                        execStep = new TuiExecutionStep( wizard );
                        wizard->addTuiStep( execStep );
                    }
                }
                execStep->appendModule( thisModule, descriptor.weight() );
            }
        }
    }

    if ( !failedModules.isEmpty() )
    {
        emit modulesFailed( failedModules );
        return false;
    }
    // After the caller has had a chance to connect to everything
    QTimer::singleShot( 0, this, &TuiModuleLoader::modulesLoaded );
    return true;
}

void
TuiModuleLoader::checkRequirements()
{
    cDebug() << "Checking module requirements ..";

    QVector< Module* > modules;
    for ( Module* module : m_loaded )
    {
        modules.append( module );
    }

    auto* rq = new RequirementsChecker( modules, m_requirementsModel, this );
    connect( rq, &RequirementsChecker::done, rq, &RequirementsChecker::deleteLater );
    connect( rq,
             &RequirementsChecker::done,
             this,
             [ this ]()
             {
                 if ( m_requirementsModel->satisfiedMandatory() )
                 {
                     emit requirementsComplete( true );
                 }
                 else
                 {
                     emit requirementsComplete( false );
                     QTimer::singleShot( std::chrono::seconds( 5 ), this, &TuiModuleLoader::checkRequirements );
                 }
             } );

    QTimer::singleShot( 0, rq, &RequirementsChecker::run );
}

}  // namespace ClaudeInstall
