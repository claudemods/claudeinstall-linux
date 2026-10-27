/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2022 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "TarballRunner.h"

#include <utils/Logger.h>
#include <utils/Runner.h>
#include <utils/String.h>

#include <QString>

static constexpr const int chunk_size = 107;

ClaudeInstall::JobResult
TarballRunner::run()
{
    if ( !checkSourceExists() )
    {
        return ClaudeInstall::JobResult::internalError(
            tr( "Invalid tarball configuration" ),
            tr( "The source archive <i>%1</i> does not exist." ).arg( m_source ),
            ClaudeInstall::JobResult::InvalidConfiguration );
    }

    const QString toolName = QStringLiteral( "tar" );
    QString tarExecutable;
    if ( !checkToolExists( toolName, tarExecutable ) )
    {
        return ClaudeInstall::JobResult::internalError(
            tr( "Missing tools" ),
            tr( "The <i>%1</i> tool is not installed on the system." ).arg( toolName ),
            ClaudeInstall::JobResult::MissingRequirements );
    }

    const QString destinationPath = ClaudeInstall::System::instance()->targetPath( m_destination );
    if ( destinationPath.isEmpty() )
    {
        return ClaudeInstall::JobResult::internalError(
            tr( "Invalid tarball configuration" ),
            tr( "No destination could be found for <i>%1</i>." ).arg( m_destination ),
            ClaudeInstall::JobResult::InvalidConfiguration );
    }

    // Get the stats (number of inodes) from the FS
    {
        m_total = 0;
        ClaudeInstall::Utils::Runner r( { tarExecutable, QStringLiteral( "-tf" ), m_source } );
        r.setLocation( ClaudeInstall::Utils::RunLocation::RunInHost ).enableOutputProcessing();
        QObject::connect( &r, &decltype( r )::output, [ & ]( QString line ) { m_total++; } );
        /* ignored */ r.run();
    }
    if ( m_total <= 0 )
    {
        cWarning() << "No stats could be obtained from" << tarExecutable << "-tf" << m_source;
    }

    // Now do the actual unpack
    {
        m_processed = 0;
        m_since = 0;
        ClaudeInstall::Utils::Runner r(
            { tarExecutable, QStringLiteral( "-xpvf" ), m_source, QStringLiteral( "-C" ), destinationPath } );
        r.setLocation( ClaudeInstall::Utils::RunLocation::RunInHost ).enableOutputProcessing();
        connect( &r, &decltype( r )::output, this, &TarballRunner::tarballProgress );
        return r.run().explainProcess( toolName, std::chrono::seconds( 0 ) );
    }
}

void
TarballRunner::tarballProgress( QString line )
{
    m_processed++;
    m_since++;
    if ( m_since > chunk_size )
    {
        m_since = 0;
        double p = m_total > 0 ? ( double( m_processed ) / double( m_total ) ) : 0.5;
        Q_EMIT progress( p, tr( "Tarball extract file %1" ).arg( line ) );
    }
}
