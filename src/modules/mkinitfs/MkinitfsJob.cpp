/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "MkinitfsJob.h"

#include "utils/System.h"

MkinitfsJob::MkinitfsJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

MkinitfsJob::~MkinitfsJob() {}

QString
MkinitfsJob::prettyName() const
{
    return tr( "Creating initramfs with mkinitfs…", "@status" );
}

ClaudeInstall::JobResult
MkinitfsJob::exec()
{
    // Creates the initramfs, even when it already exists.
    auto r = ClaudeInstall::System::instance()->targetEnvCommand( { "mkinitfs" } );
    if ( r.getExitCode() != 0 )
    {
        return ClaudeInstall::JobResult::error( tr( "Failed to run mkinitfs on the target" ),
                                            tr( "The exit code was %1" ).arg( r.getExitCode() ) );
    }
    return ClaudeInstall::JobResult::ok();
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( MkinitfsJobFactory, registerPlugin< MkinitfsJob >(); )
