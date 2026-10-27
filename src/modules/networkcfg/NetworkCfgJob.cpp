/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "NetworkCfgJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <filesystem>

#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

NetworkCfgJob::NetworkCfgJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

NetworkCfgJob::~NetworkCfgJob() {}

QString
NetworkCfgJob::prettyName() const
{
    return tr( "Saving network configuration…", "@status" );
}

/** @brief Gets the "live user" login.
 *
 * This might be "live", or "nitrux" or something similar: it is the login name
 * used *right now*, and network configurations saved for that user should be
 * applied also for the installed user (which probably has a different name).
 * Returns an empty string if it can't be determined.
 */
static QString
getLiveUser()
{
    // getlogin() depends on utmp, which isn't always set up right
    char buffer[ 256 ];
    if ( getlogin_r( buffer, sizeof( buffer ) ) == 0 && buffer[ 0 ] )
    {
        return QString::fromLocal8Bit( buffer );
    }

    // Otherwise the *current* user, which is generally root. That isn't useful,
    // since the network settings have been made outside of ClaudeInstall-running-as-root,
    // but if ClaudeInstall is running as non-root then this is fine.
    QString name;
    for ( const char* var : { "LOGNAME", "USER", "LNAME", "USERNAME" } )
    {
        name = qEnvironmentVariable( var );
        if ( !name.isEmpty() )
        {
            break;
        }
    }
    if ( name.isEmpty() )
    {
        if ( const struct passwd* pw = getpwuid( getuid() ) )
        {
            name = QString::fromLocal8Bit( pw->pw_name );
        }
    }
    return name == "root" ? QString() : name;
}

/** @brief Replaces @p liveUser by @p targetUser in the permissions lines of the file */
static void
replaceUsername( const QString& nmConfigFilename, const QString& liveUser, const QString& targetUser )
{
    if ( liveUser.isEmpty() || !QFileInfo::exists( nmConfigFilename ) )
    {
        return;
    }

    QFile f( nmConfigFilename );
    if ( !f.open( QIODevice::ReadOnly ) )
    {
        return;
    }
    const QList< QByteArray > lines = f.readAll().split( '\n' );
    f.close();

    const QByteArray livePermissions = QStringLiteral( "permissions=user:%1:;" ).arg( liveUser ).toUtf8();
    const QByteArray targetPermissions = QStringLiteral( "permissions=user:%1:;" ).arg( targetUser ).toUtf8();

    QByteArray out;
    for ( int i = 0; i < lines.size(); ++i )
    {
        // split() leaves an empty last element if the text ended in a newline
        if ( i == lines.size() - 1 && lines[ i ].isEmpty() )
        {
            break;
        }
        out += lines[ i ].contains( livePermissions ) ? targetPermissions : lines[ i ];
        out += '\n';
    }
    if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
    {
        f.write( out );
    }
}

/** @brief Copies @p from to @p to without following symlinks; returns success. */
static bool
copyNoFollow( const QString& from, const QString& to )
{
    std::error_code ec;
    fs::copy( from.toStdString(), to.toStdString(), fs::copy_options::copy_symlinks, ec );
    return !ec;
}

ClaudeInstall::JobResult
NetworkCfgJob::exec()
{
    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    const QString root = gs->value( "rootMountPoint" ).toString();
    const QString user = gs->value( "username" ).toString();
    const QString liveUser = getLiveUser();

    if ( root.isEmpty() )
    {
        cWarning() << "rootMountPoint is empty";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ), tr( "No root mount point is given for <pre>%1</pre> to use." ).arg( "networkcfg" ) );
    }

    // NetworkManager connections. We don't want to do anything if a
    // network configuration already exists on the target.
    const QString sourceNm = QStringLiteral( "/etc/NetworkManager/system-connections/" );
    const QString targetNm = root + sourceNm;
    if ( QFileInfo::exists( sourceNm ) && QFileInfo::exists( targetNm ) )
    {
        const QDir sourceDir( sourceNm );
        for ( const QString& network : sourceDir.entryList( QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System ) )
        {
            // Skip LTSP live
            if ( network == "LTSP" )
            {
                continue;
            }

            const QString sourceNetwork = sourceDir.filePath( network );
            const QString targetNetwork = QDir( targetNm ).filePath( network );
            if ( QFileInfo::exists( targetNetwork ) )
            {
                continue;
            }

            if ( copyNoFollow( sourceNetwork, targetNetwork ) )
            {
                // A symlink would lead the rewrite to the file it points to, which isn't ours
                if ( !QFileInfo( targetNetwork ).isSymLink() )
                {
                    replaceUsername( targetNetwork, liveUser, user );
                }
            }
            else
            {
                cDebug() << "Can't copy network configuration files in" << sourceNetwork;
            }
        }
    }

    // Also install netplan files
    const QString sourceNetplan = QStringLiteral( "/etc/netplan" );
    const QString targetNetplan = root + sourceNetplan;
    if ( QFileInfo::exists( sourceNetplan ) && QFileInfo::exists( targetNetplan ) )
    {
        // Set NetworkManager to be the default renderer if Netplan is installed
        const QString defaultRenderer = root + QStringLiteral( "/usr/lib/netplan/00-network-manager-all.yaml" );
        if ( !QFileInfo::exists( defaultRenderer ) )
        {
            QFile f( QDir( targetNetplan ).filePath( "01-network-manager-all.yaml" ) );
            if ( f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
            {
                f.write( "# This file was written by claudeinstall.\n"
                         "# Let NetworkManager manage all devices on this system.\n"
                         "# For more information, see netplan(5).\n"
                         "network:\n"
                         "  version: 2\n"
                         "  renderer: NetworkManager\n" );
                f.setPermissions( QFileDevice::ReadOwner | QFileDevice::WriteOwner );  // 0600
            }
        }

        // Copy existing Netplan configuration
        const QDir sourceDir( sourceNetplan );
        for ( const QString& cfg : sourceDir.entryList( { "*.yaml" }, QDir::Files ) )
        {
            const QString targetCfg = QDir( targetNetplan ).filePath( cfg );
            if ( !QFileInfo::exists( targetCfg ) )
            {
                QFile::copy( sourceDir.filePath( cfg ), targetCfg );
            }
        }
    }

    // We need to overwrite the default resolv.conf in the chroot.
    const QString sourceResolv = QStringLiteral( "/etc/resolv.conf" );
    const QString targetResolv = root + sourceResolv;
    if ( sourceResolv != targetResolv && QFileInfo::exists( sourceResolv ) )
    {
        std::error_code ec;
        fs::remove( targetResolv.toStdString(), ec );
        if ( ec )
        {
            cDebug() << "Couldn't remove" << targetResolv << ":" << QString::fromStdString( ec.message() );
        }
        if ( !copyNoFollow( sourceResolv, targetResolv ) )
        {
            cDebug() << "Can't copy resolv.conf from" << sourceResolv;
        }
    }

    return ClaudeInstall::JobResult::ok();
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( NetworkCfgJobFactory, registerPlugin< NetworkCfgJob >(); )
