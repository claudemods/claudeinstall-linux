/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2016 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CLAUDEINSTALL_CPPJOBMODULE_H
#define CLAUDEINSTALL_CPPJOBMODULE_H

#include "DllMacro.h"
#include "modulesystem/Module.h"

class QPluginLoader;

namespace ClaudeInstall
{

class STEPDLLEXPORT CppJobModule : public Module
{
public:
    Type type() const override;
    Interface interface() const override;

    void loadSelf() override;
    JobList jobs() const override;

protected:
    void initFrom( const ModuleSystem::Descriptor& moduleDescriptor ) override;

private:
    explicit CppJobModule();
    ~CppJobModule() override;

    QPluginLoader* m_loader;
    job_ptr m_job;

    friend Module* ClaudeInstall::moduleFromDescriptor( const ModuleSystem::Descriptor& moduleDescriptor,
                                                    const QString& instanceId,
                                                    const QString& configFileName,
                                                    const QString& moduleDirectory );
};

}  // namespace ClaudeInstall

#endif  // CLAUDEINSTALL_CPPJOBMODULE_H
