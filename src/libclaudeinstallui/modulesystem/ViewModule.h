/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CLAUDEINSTALL_VIEWMODULE_H
#define CLAUDEINSTALL_VIEWMODULE_H

#include "DllMacro.h"
#include "modulesystem/Module.h"

class QPluginLoader;

namespace ClaudeInstall
{

class ViewStep;

class UIDLLEXPORT ViewModule : public Module
{
public:
    /// @brief Creates a (not yet initialized) view module; for ViewModuleCreator
    static Module* create() { return new ViewModule(); }

    Type type() const override;
    Interface interface() const override;

    void loadSelf() override;
    JobList jobs() const override;

    RequirementsList checkRequirements() override;

protected:
    void initFrom( const ModuleSystem::Descriptor& moduleDescriptor ) override;

private:
    explicit ViewModule();
    ~ViewModule() override;

    QPluginLoader* m_loader;
    ViewStep* m_viewStep = nullptr;

    friend Module* ClaudeInstall::moduleFromDescriptor( const ModuleSystem::Descriptor& moduleDescriptor,
                                                    const QString& instanceId,
                                                    const QString& configFileName,
                                                    const QString& moduleDirectory );
};

}  // namespace ClaudeInstall

#endif  // CLAUDEINSTALL_VIEWMODULE_H
