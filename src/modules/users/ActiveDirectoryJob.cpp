/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 Simon Quigley <tsimonq2@ubuntu.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ActiveDirectoryJob.h"

#include "Config.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/Permissions.h"
#include "utils/System.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>

ActiveDirectoryJob::ActiveDirectoryJob( const QString& adminLogin,
                                        const QString& adminPassword,
                                        const QString& domain,
                                        const QString& ip )
    : ClaudeInstall::Job()
    , m_adminLogin( adminLogin )
    , m_adminPassword( adminPassword )
    , m_domain( domain )
    , m_ip( ip )
{
}

QString
ActiveDirectoryJob::prettyName() const
{
    return tr( "Enroll system in Active Directory", "@label" );
}

QString
ActiveDirectoryJob::prettyStatusMessage() const
{
    return tr( "Enrolling system in Active Directory…", "@status" );
}

ClaudeInstall::JobResult
ActiveDirectoryJob::exec()
{
    if ( !m_ip.isEmpty() )
    {
        const QString hostsFilePath = ClaudeInstall::System::instance()->targetPath( QStringLiteral( "/etc/hosts" ) );
        QFile hostsFile( hostsFilePath );
        if ( hostsFile.open( QIODevice::Append | QIODevice::Text ) )
        {
            QTextStream out( &hostsFile );
            out << m_ip << " " << m_domain << "\n";
            hostsFile.close();
        }
        else
        {
            return ClaudeInstall::JobResult::error( "Failed to open /etc/hosts for writing." );
        }
    }

    const QString installPath = ClaudeInstall::System::instance()->targetPath( QStringLiteral( "/" ) );
    auto r = ClaudeInstall::System::instance()->runCommand(
        ClaudeInstall::System::RunLocation::RunInHost,
        { "realm", "join", m_domain, "-U", m_adminLogin, "--install=" + installPath, "--verbose" },
        QString(),
        m_adminPassword,
        std::chrono::seconds( 30 ) );


    if ( r.getExitCode() == 0 )
    {
        return ClaudeInstall::JobResult::ok();
    }
    else
    {
        return ClaudeInstall::JobResult::error( QString( "Failed to join realm: %1" ).arg( r.getOutput() ) );
    }
}
