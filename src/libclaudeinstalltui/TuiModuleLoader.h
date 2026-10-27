/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TUIMODULELOADER_H
#define TUIMODULELOADER_H

#include "DllMacro.h"

#include "modulesystem/Descriptor.h"
#include "modulesystem/InstanceKey.h"
#include "modulesystem/Module.h"

#include <QMap>
#include <QObject>
#include <QStringList>

namespace ClaudeInstall
{

class RequirementsModel;

/**
 * @brief Finds and loads the modules for the text-mode frontend
 *
 * This is the headless counterpart of the ModuleManager: it scans the
 * module search paths for module.desc files, creates the modules listed in
 * the *sequence* of settings.conf (view modules as TuiModule, job modules
 * as usual) and checks their requirements. It publishes the requirements and
 * the end of loading on ModuleLoadState, so that Config objects find out the
 * same way they do in the GUI.
 *
 * Unlike the ModuleManager, everything here is synchronous (except the
 * requirements check, which reports through signals); there is no widget
 * to keep responsive while it runs.
 */
class TUIDLLEXPORT TuiModuleLoader : public QObject
{
    Q_OBJECT
public:
    explicit TuiModuleLoader( const QStringList& paths, QObject* parent = nullptr );
    ~TuiModuleLoader() override;

    static TuiModuleLoader* instance();

    /// @brief Reads the descriptors from the search paths; returns the number found
    int scan();

    /** @brief Loads the modules of the settings' sequence
     *
     * The TuiWizard gets a step for each view module, and a TuiExecutionStep at
     * each *exec* phase. Returns @c true if everything loaded; otherwise emits
     * modulesFailed() and returns @c false.
     */
    bool loadModules();

    /// @brief Starts checking the requirements of all the loaded modules
    void checkRequirements();

    ModuleSystem::Descriptor moduleDescriptor( const QString& name ) const;
    ModuleSystem::Descriptor moduleDescriptor( const ModuleSystem::InstanceKey& key ) const
    {
        return moduleDescriptor( key.module() );
    }
    Module* moduleInstance( const ModuleSystem::InstanceKey& key ) const { return m_loaded.value( key ); }

    RequirementsModel* requirementsModel() const { return m_requirementsModel; }

signals:
    void modulesLoaded();
    void modulesFailed( const QStringList& modules );
    void requirementsComplete( bool satisfied );

private:
    bool addModule( Module* module );
    size_t removeModulesWithMissingDependencies();

    QStringList m_paths;
    QMap< QString, ModuleSystem::Descriptor > m_descriptors;
    QMap< ModuleSystem::InstanceKey, Module* > m_loaded;
    RequirementsModel* m_requirementsModel;

    static TuiModuleLoader* s_instance;
};

}  // namespace ClaudeInstall

#endif
