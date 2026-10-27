/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2016 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "CppJob.h"

namespace ClaudeInstall
{

CppJob::CppJob( QObject* parent )
    : Job( parent )
{
}


CppJob::~CppJob() {}


void
CppJob::setModuleInstanceKey( const ClaudeInstall::ModuleSystem::InstanceKey& instanceKey )
{
    m_instanceKey = instanceKey;
}


void
CppJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    Q_UNUSED( configurationMap )
}

}  // namespace ClaudeInstall
