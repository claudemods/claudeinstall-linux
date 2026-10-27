/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2016 Artoo <artoo@manjaro.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2018 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "PlymouthCfgJob.h"

#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

PlymouthCfgJob::PlymouthCfgJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

PlymouthCfgJob::~PlymouthCfgJob() {}

QString
PlymouthCfgJob::prettyName() const
{
    return tr( "Configure Plymouth theme", "@status" );
}

ClaudeInstall::JobResult
PlymouthCfgJob::exec()
{
    auto* s = ClaudeInstall::System::instance();

    // Checks existence (runnability) of plymouth in the target system.
    if ( s->targetEnvCall( { "sh", "-c", "which plymouth" } ) != 0 )
    {
        return ClaudeInstall::JobResult::ok();
    }

    if ( !m_theme.isEmpty() )
    {
        s->targetEnvCall( { "plymouth-set-default-theme", m_theme } );
    }
    return ClaudeInstall::JobResult::ok();
}

void
PlymouthCfgJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_theme = ClaudeInstall::getString( configurationMap, "plymouth_theme" );
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( PlymouthCfgJobFactory, registerPlugin< PlymouthCfgJob >(); )
