/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2015 Rohan Garg <rohan@garg.io>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "SetTimezoneJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "Settings.h"
#include "utils/Logger.h"
#include "utils/System.h"

#include <QDir>
#include <QFileInfo>

SetTimezoneJob::SetTimezoneJob( const QString& region, const QString& zone )
    : ClaudeInstall::Job()
    , m_region( region )
    , m_zone( zone )
{
}

QString
SetTimezoneJob::prettyName() const
{
    return tr( "Setting timezone to %1/%2…", "@status" ).arg( m_region ).arg( m_zone );
}

ClaudeInstall::JobResult
SetTimezoneJob::exec()
{
    // do not call timedatectl in a chroot, it is not safe (timedatectl talks
    // to a running timedated over D-Bus), and we have code that works
    if ( !ClaudeInstall::Settings::instance()->doChroot() )
    {
        int ec = ClaudeInstall::System::instance()->targetEnvCall(
            { "timedatectl", "set-timezone", m_region + '/' + m_zone } );

        if ( !ec )
        {
            return ClaudeInstall::JobResult::ok();
        }
    }

    QString localtimeSlink( "/etc/localtime" );
    QString zoneinfoPath( "/usr/share/zoneinfo" );
    zoneinfoPath.append( QDir::separator() + m_region );
    zoneinfoPath.append( QDir::separator() + m_zone );

    ClaudeInstall::GlobalStorage* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    QFileInfo zoneFile( gs->value( "rootMountPoint" ).toString() + zoneinfoPath );
    if ( !zoneFile.exists() || !zoneFile.isReadable() )
    {
        return ClaudeInstall::JobResult::error( tr( "Cannot access selected timezone path.", "@error" ),
                                            tr( "Bad path: %1", "@error" ).arg( zoneFile.absolutePath() ) );
    }

    // Make sure /etc/localtime doesn't exist, otherwise symlinking will fail
    ClaudeInstall::System::instance()->targetEnvCall( { "rm", "-f", localtimeSlink } );

    int ec = ClaudeInstall::System::instance()->targetEnvCall( { "ln", "-s", zoneinfoPath, localtimeSlink } );
    if ( ec )
    {
        return ClaudeInstall::JobResult::error( tr( "Cannot set timezone.", "@error" ),
                                            tr( "Link creation failed, target: %1; link name: %2", "@info" )
                                                .arg( zoneinfoPath )
                                                .arg( "/etc/localtime" ) );
    }

    QFile timezoneFile( gs->value( "rootMountPoint" ).toString() + "/etc/timezone" );

    if ( !timezoneFile.open( QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate ) )
    {
        return ClaudeInstall::JobResult::error( tr( "Cannot set timezone.", "@error" ),
                                            tr( "Cannot open /etc/timezone for writing", "@info" ) );
    }

    QTextStream out( &timezoneFile );
    out << m_region << '/' << m_zone << "\n";
    timezoneFile.close();

    return ClaudeInstall::JobResult::ok();
}
