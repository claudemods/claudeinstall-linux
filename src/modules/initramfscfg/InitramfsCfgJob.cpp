/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Rohan Garg <rohan@kde.org>
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "InitramfsCfgJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Dirs.h"
#include "utils/Logger.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>

InitramfsCfgJob::InitramfsCfgJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

InitramfsCfgJob::~InitramfsCfgJob() {}

QString
InitramfsCfgJob::prettyName() const
{
    return tr( "Configuring initramfs…", "@status" );
}

ClaudeInstall::JobResult
InitramfsCfgJob::exec()
{
    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    const QVariantList partitions = gs->value( "partitions" ).toList();
    const QString rootMountPoint = gs->value( "rootMountPoint" ).toString();

    if ( partitions.isEmpty() )
    {
        cWarning() << "partitions is empty";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ), tr( "No partitions are defined for <pre>%1</pre> to use." ).arg( "initramfscfg" ) );
    }
    if ( rootMountPoint.isEmpty() )
    {
        cWarning() << "rootMountPoint is empty";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ),
            tr( "No root mount point is given for <pre>%1</pre> to use." ).arg( "initramfscfg" ) );
    }

    bool encryptHook = false;
    bool unencryptedSeparateBoot = false;
    for ( const auto& p : partitions )
    {
        const QVariantMap partition = p.toMap();
        const QString mountPoint = partition.value( "mountPoint" ).toString();
        const bool hasLuks = partition.contains( "luksMapperName" );
        if ( mountPoint == "/" && hasLuks )
        {
            encryptHook = true;
        }
        if ( mountPoint == "/boot" && !hasLuks )
        {
            unencryptedSeparateBoot = true;
        }
    }

    if ( !encryptHook )
    {
        return ClaudeInstall::JobResult::ok();
    }

    // The hooks are installed next to the plugin (see CMakeLists.txt)
    const QString source = ClaudeInstall::systemLibDir().absoluteFilePath(
        QStringLiteral( "claudeinstall/modules/initramfscfg/" )
        + ( unencryptedSeparateBoot ? QStringLiteral( "encrypt_hook_nokey" ) : QStringLiteral( "encrypt_hook" ) ) );
    const QString target = rootMountPoint + QStringLiteral( "/usr/share/initramfs-tools/hooks/encrypt_hook" );

    QFile::remove( target );
    if ( !QFile::copy( source, target ) )
    {
        return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                            tr( "Could not copy the encrypt hook from <pre>%1</pre> to <pre>%2</pre>." )
                                                .arg( source, target ) );
    }
    QFile::setPermissions( target,
                           QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                               | QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther
                               | QFileDevice::ExeOther );  // 0755
    return ClaudeInstall::JobResult::ok();
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( InitramfsCfgJobFactory, registerPlugin< InitramfsCfgJob >(); )
