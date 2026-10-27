/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Config.h"

#include "IDJob.h"

#include "utils/StringExpander.h"
#include "utils/Variant.h"

#include <QCoreApplication>
#include <QDate>

/// @brief Replaces ${DATE} in @p s by the current date (ISO format)
static QString
substitute( QString s )
{
    ClaudeInstall::String::DictionaryExpander d;
    d.insert( QStringLiteral( "DATE" ), QDate::currentDate().toString( Qt::ISODate ) );

    return d.expand( s );
}

Config::Config( QObject* parent )
    : QObject( parent )
{
}

void
Config::setConfigurationMap( const QVariantMap& configurationMap )
{
    setBatchIdentifier( substitute( ClaudeInstall::getString( configurationMap, "batch-identifier" ) ) );
}

void
Config::setBatchIdentifier( const QString& batchIdentifier )
{
    if ( batchIdentifier != m_batchIdentifier )
    {
        m_batchIdentifier = batchIdentifier;
        emit batchIdentifierChanged( m_batchIdentifier );
    }
}

QString
Config::prettyStatus() const
{
    // This text was translated as part of the view step; keep that context
    return QCoreApplication::translate( "OEMViewStep", "Set the OEM Batch Identifier to <code>%1</code>." )
        .arg( m_batchIdentifier );
}

ClaudeInstall::JobList
Config::createJobs() const
{
    return ClaudeInstall::JobList() << ClaudeInstall::job_ptr( new IDJob( m_batchIdentifier ) );
}
