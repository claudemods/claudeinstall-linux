/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ServicesSystemdJob.h"

#include "compat/Variant.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

ServicesSystemdJob::ServicesSystemdJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

ServicesSystemdJob::~ServicesSystemdJob() {}

QString
ServicesSystemdJob::prettyName() const
{
    return tr( "Configure systemd units", "@status" );
}

ClaudeInstall::JobResult
ServicesSystemdJob::exec()
{
    // Each unit is either a plain name (enabled, not mandatory) or a mapping
    // of name, action and mandatory flag. Failures of non-mandatory units
    // are logged only.
    for ( const auto& entry : m_units )
    {
        QString name;
        QString action = QStringLiteral( "enable" );
        bool mandatory = false;

        if ( ClaudeInstall::typeOf( entry ) == ClaudeInstall::StringVariantType )
        {
            name = entry.toString();
        }
        else
        {
            const QVariantMap unit = entry.toMap();
            if ( !unit.contains( "name" ) )
            {
                cError() << "The key 'name' is missing from the mapping" << unit << ". Continuing to the next unit.";
                continue;
            }
            name = ClaudeInstall::getString( unit, "name" );
            action = ClaudeInstall::getString( unit, "action", action );
            mandatory = ClaudeInstall::getBool( unit, "mandatory", false );
        }

        const int exitCode = ClaudeInstall::System::instance()->targetEnvCall( { "systemctl", action, name } );
        if ( exitCode != 0 )
        {
            cWarning() << "Cannot" << action << "systemd unit" << name;
            cWarning() << "systemctl" << action << "call in chroot returned error code" << exitCode;
            if ( mandatory )
            {
                return ClaudeInstall::JobResult::error(
                    tr( "Cannot modify unit" ),
                    tr( "Cannot %1 systemd unit <code>%2</code>." ).arg( action, name ) + QChar( ' ' )
                        + tr( "<code>systemctl %1</code> call in chroot returned error code %2." )
                              .arg( action )
                              .arg( exitCode ) );
            }
        }
    }
    return ClaudeInstall::JobResult::ok();
}

void
ServicesSystemdJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_units = configurationMap.value( "units" ).toList();
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( ServicesSystemdJobFactory, registerPlugin< ServicesSystemdJob >(); )
