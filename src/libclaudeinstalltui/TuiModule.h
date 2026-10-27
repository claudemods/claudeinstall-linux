/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TUIMODULE_H
#define TUIMODULE_H

#include "DllMacro.h"
#include "modulesystem/Module.h"

class QPluginLoader;

namespace ClaudeInstall
{

class TuiStep;

/**
 * @brief A view module, in the text-mode frontend
 *
 * It loads the module's *tuiLoad* plugin and adds the TuiStep that the
 * plugin makes to the TuiWizard. If the module has no such plugin, a
 * PlaceholderStep is added instead, so that the wizard still has the
 * same sequence of steps as the GUI.
 */
class TUIDLLEXPORT TuiModule : public Module
{
public:
    /// @brief Creates a (not yet initialized) module; for ViewModuleCreator
    static Module* create() { return new TuiModule(); }

    Type type() const override;
    Interface interface() const override;

    void loadSelf() override;
    JobList jobs() const override;

    RequirementsList checkRequirements() override;

    /// @brief The step of this module (after loadSelf())
    TuiStep* step() const { return m_step; }

protected:
    void initFrom( const ModuleSystem::Descriptor& moduleDescriptor ) override;

private:
    explicit TuiModule();
    ~TuiModule() override;

    QPluginLoader* m_loader = nullptr;
    TuiStep* m_step = nullptr;
};

}  // namespace ClaudeInstall

#endif
