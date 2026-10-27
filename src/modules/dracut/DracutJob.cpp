/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaid@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2022 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "DracutJob.h"

#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

DracutJob::DracutJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

DracutJob::~DracutJob() {}

QString
DracutJob::prettyName() const
{
    return tr( "Creating initramfs with dracut…", "@status" );
}

ClaudeInstall::JobResult
DracutJob::exec()
{
    QStringList command { QStringLiteral( "dracut" ) };
    command << m_options;
    if ( !m_initramfsName.isEmpty() )
    {
        command << m_initramfsName;
    }

    // Creates the initramfs, even when it already exists.
    auto r = ClaudeInstall::System::instance()->targetEnvCommand( command );
    if ( r.getExitCode() != 0 )
    {
        cWarning() << "Dracut failed with output:" << r.getOutput();
        return ClaudeInstall::JobResult::error(
            tr( "Failed to run dracut" ),
            tr( "Dracut failed to run on the target with return code: %1" ).arg( r.getExitCode() ) );
    }
    return ClaudeInstall::JobResult::ok();
}

void
DracutJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_initramfsName = ClaudeInstall::getString( configurationMap, "initramfsName" );
    m_options = ClaudeInstall::getStringList( configurationMap, "options", QStringList { QStringLiteral( "-f" ) } );
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( DracutJobFactory, registerPlugin< DracutJob >(); )
