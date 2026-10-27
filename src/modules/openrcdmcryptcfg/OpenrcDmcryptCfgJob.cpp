/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "OpenrcDmcryptCfgJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

#include <QFile>

OpenrcDmcryptCfgJob::OpenrcDmcryptCfgJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

OpenrcDmcryptCfgJob::~OpenrcDmcryptCfgJob() {}

QString
OpenrcDmcryptCfgJob::prettyName() const
{
    return tr( "Configuring OpenRC dmcrypt service…", "@status" );
}

ClaudeInstall::JobResult
OpenrcDmcryptCfgJob::exec()
{
    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    const QString rootMountPoint = gs->value( "rootMountPoint" ).toString();
    const QVariantList partitions = gs->value( "partitions" ).toList();

    if ( partitions.isEmpty() )
    {
        cWarning() << "partitions is empty";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ),
            tr( "No partitions are defined for <pre>%1</pre> to use." ).arg( "openrcdmcryptcfg" ) );
    }
    if ( rootMountPoint.isEmpty() )
    {
        cWarning() << "rootMountPoint is empty";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ),
            tr( "No root mount point is given for <pre>%1</pre> to use." ).arg( "openrcdmcryptcfg" ) );
    }

    QString relativePath = m_configFilePath;
    while ( relativePath.startsWith( '/' ) )
    {
        relativePath.remove( 0, 1 );
    }
    const QString confPath = rootMountPoint + '/' + relativePath;

    bool unencryptedSeparateBoot = false;
    for ( const auto& p : partitions )
    {
        const QVariantMap partition = p.toMap();
        if ( partition.value( "mountPoint" ).toString() == "/boot" && !partition.contains( "luksMapperName" ) )
        {
            unencryptedSeparateBoot = true;
        }
    }

    for ( const auto& p : partitions )
    {
        const QVariantMap partition = p.toMap();
        const QString mountPoint = partition.value( "mountPoint" ).toString();
        const bool hasLuks = partition.contains( "luksMapperName" );
        // root and swap partitions are handled by the initramfs generators
        const bool skip = mountPoint == "/" || partition.value( "fs" ).toString() == "linuxswap";

        if ( skip )
        {
            continue;
        }
        if ( !hasLuks )
        {
            cDebug() << "Skip writing OpenRC LUKS configuration for partition" << mountPoint;
            continue;
        }

        cDebug() << "Writing OpenRC LUKS configuration for partition" << mountPoint;
        QFile f( confPath );
        if ( !f.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) )
        {
            return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                                tr( "Could not open <pre>%1</pre> for writing." ).arg( confPath ) );
        }
        QByteArray contents;
        contents += "\ntarget=" + partition.value( "luksMapperName" ).toString().toUtf8();
        contents += "\nsource=/dev/disk/by-uuid/" + partition.value( "uuid" ).toString().toUtf8();
        // Keys must not be stored on unencrypted partitions
        if ( !unencryptedSeparateBoot )
        {
            contents += "\nkey=/crypto_keyfile.bin";
        }
        contents += "\n";
        f.write( contents );
    }

    return ClaudeInstall::JobResult::ok();
}

void
OpenrcDmcryptCfgJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_configFilePath = ClaudeInstall::getString( configurationMap, "configFilePath" );
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( OpenrcDmcryptCfgJobFactory, registerPlugin< OpenrcDmcryptCfgJob >(); )
