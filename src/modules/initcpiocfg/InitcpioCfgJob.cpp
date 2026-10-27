/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Rohan Garg <rohan@kde.org>
 *   SPDX-FileCopyrightText: 2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2016 David McKinney <mckinney@subgraph.com>
 *   SPDX-FileCopyrightText: 2016 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Bernhard Landauer <oberon@manjaro.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "InitcpioCfgJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

InitcpioCfgJob::InitcpioCfgJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

InitcpioCfgJob::~InitcpioCfgJob() {}

QString
InitcpioCfgJob::prettyName() const
{
    return tr( "Configuring mkinitcpio…", "@status" );
}

/** @brief Is the first CPU in /proc/cpuinfo an Intel one? */
static bool
cpuIsIntel()
{
    QFile f( QStringLiteral( "/proc/cpuinfo" ) );
    if ( !f.open( QIODevice::ReadOnly ) )
    {
        return false;
    }
    while ( !f.atEnd() )
    {
        const QByteArray line = f.readLine();
        if ( line.trimmed().isEmpty() )
        {
            break;  // end of the first processor
        }
        const int colon = line.indexOf( ':' );
        if ( colon > 0 && line.left( colon ).trimmed() == "vendor_id" )
        {
            return line.mid( colon + 1 ).trimmed().toLower() == "genuineintel";
        }
    }
    return false;
}

/** @brief Checks that @p program exists (is runnable) in the target */
static bool
targetHas( const char* program )
{
    return ClaudeInstall::System::instance()->targetEnvCall( { "sh", "-c", QStringLiteral( "which " ) + program } ) == 0;
}

InitcpioCfgJob::Features
InitcpioCfgJob::findFeatures( const QVariantList& partitions, const QString& rootMountPoint ) const
{
    Features f;
    QStringList& hooks = f.hooks;
    hooks = QStringList { "autodetect", "microcode", "kms", "modconf", "block", "keyboard" };

    const bool useSystemd = m_useSystemdHook && targetHas( "systemd-cat" );
    if ( useSystemd )
    {
        hooks.prepend( "systemd" );
        hooks.append( "sd-vconsole" );
    }
    else
    {
        hooks.prepend( "udev" );
        hooks.prepend( "base" );
        hooks.append( "keymap" );
        hooks.append( "consolefont" );
    }

    QString swapUuid;
    bool usesBtrfs = false;
    bool usesZfs = false;
    bool usesLvm2 = false;
    bool encryptHook = false;
    bool openswapHook = false;
    bool unencryptedSeparateBoot = false;

    // It is important that the plymouth hook comes before any encrypt hook
    if ( targetHas( "plymouth" ) )
    {
        hooks.append( "plymouth" );
    }

    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    for ( const auto& p : partitions )
    {
        const QVariantMap partition = p.toMap();
        const QString fs = partition.value( "fs" ).toString();
        const QString mountPoint = partition.value( "mountPoint" ).toString();
        const bool hasLuks = partition.contains( "luksMapperName" );

        if ( fs == "linuxswap" && !partition.value( "claimed" ).toBool() )
        {
            continue;  // foreign swap
        }
        if ( fs == "linuxswap" )
        {
            swapUuid = partition.value( "uuid" ).toString();
            if ( hasLuks )
            {
                openswapHook = true;
            }
        }
        if ( fs == "btrfs" )
        {
            usesBtrfs = true;
        }
        // In addition to checking the filesystem, check to ensure that zfs is enabled
        if ( fs == "zfs" && gs->contains( "zfsPoolInfo" ) )
        {
            usesZfs = true;
        }
        if ( fs.contains( "lvm2" ) )
        {
            usesLvm2 = true;
        }
        if ( mountPoint == "/" && hasLuks )
        {
            encryptHook = true;
        }
        if ( mountPoint == "/boot" && !hasLuks )
        {
            unencryptedSeparateBoot = true;
        }
        if ( mountPoint == "/usr" )
        {
            hooks.append( "usr" );
        }
    }

    if ( encryptHook )
    {
        hooks.append( useSystemd ? "sd-encrypt" : "encrypt" );
        const QString cryptoFile = QStringLiteral( "crypto_keyfile.bin" );
        if ( !unencryptedSeparateBoot && QFileInfo( QDir( rootMountPoint ).filePath( cryptoFile ) ).isFile() )
        {
            f.files.append( '/' + cryptoFile );
        }
    }
    if ( usesLvm2 )
    {
        hooks.append( "lvm2" );
    }
    if ( usesZfs )
    {
        hooks.append( "zfs" );
    }
    if ( !swapUuid.isEmpty() )
    {
        if ( encryptHook && openswapHook )
        {
            hooks.append( "openswap" );
        }
        hooks.append( "resume" );
    }
    hooks.append( "filesystems" );

    if ( usesBtrfs )
    {
        f.modules.append( cpuIsIntel() ? "crc32c-intel" : "crc32c" );
    }
    else
    {
        hooks.append( "fsck" );
    }

    // Modify according to the keys in the configuration
    QStringList all = m_hooksPrepend + hooks + m_hooksAppend;
    QStringList kept;
    for ( const QString& h : all )
    {
        if ( !m_hooksRemove.contains( h ) )
        {
            kept.append( h );
        }
    }
    hooks = kept;

    return f;
}

ClaudeInstall::JobResult
InitcpioCfgJob::exec()
{
    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    const QVariantList partitions = gs->value( "partitions" ).toList();
    const QString rootMountPoint = gs->value( "rootMountPoint" ).toString();

    if ( partitions.isEmpty() )
    {
        cWarning() << "partitions are empty";
        return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                            tr( "No partitions are defined for <pre>initcpiocfg</pre>." ) );
    }
    if ( rootMountPoint.isEmpty() )
    {
        cWarning() << "rootMountPoint is empty";
        return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                            tr( "No root mount point for <pre>initcpiocfg</pre>." ) );
    }

    const Features features = findFeatures( partitions, rootMountPoint );

    // Read the host's mkinitcpio.conf (or nothing, if there is none)
    const QString hostFile = m_source.isEmpty() ? QStringLiteral( "/etc/mkinitcpio.conf" ) : m_source;
    QStringList lines;
    QFile host( hostFile );
    if ( host.open( QIODevice::ReadOnly ) )
    {
        for ( const QByteArray& l : host.readAll().split( '\n' ) )
        {
            lines.append( QString::fromUtf8( l ).trimmed() );
        }
        if ( !lines.isEmpty() && lines.last().isEmpty() )
        {
            lines.removeLast();  // the empty piece after the final newline
        }
    }
    else
    {
        cDebug() << "Could not open host file" << hostFile;
    }

    const QString targetPath = QDir( rootMountPoint ).filePath( "etc/mkinitcpio.conf" );
    QFile target( targetPath );
    if ( !target.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
    {
        return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                            tr( "Could not write <pre>%1</pre>." ).arg( targetPath ) );
    }
    for ( QString line : lines )
    {
        // Replace HOOKS, MODULES, BINARIES and FILES lines with what we have found
        if ( line.startsWith( "HOOKS" ) )
        {
            line = QStringLiteral( "HOOKS=(%1)" ).arg( features.hooks.join( ' ' ) );
        }
        else if ( line.startsWith( "BINARIES" ) )
        {
            line = QStringLiteral( "BINARIES=(%1)" ).arg( features.binaries.join( ' ' ) );
        }
        else if ( line.startsWith( "MODULES" ) )
        {
            line = QStringLiteral( "MODULES=(%1)" ).arg( features.modules.join( ' ' ) );
        }
        else if ( line.startsWith( "FILES" ) )
        {
            line = QStringLiteral( "FILES=(%1)" ).arg( features.files.join( ' ' ) );
        }
        target.write( line.toUtf8() + '\n' );
    }

    return ClaudeInstall::JobResult::ok();
}

void
InitcpioCfgJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_source = ClaudeInstall::getString( configurationMap, "source" );
    m_useSystemdHook = ClaudeInstall::getBool( configurationMap, "useSystemdHook", false );

    const QVariantMap hooks = configurationMap.value( "hooks" ).toMap();
    m_hooksPrepend = ClaudeInstall::getStringList( hooks, "prepend" );
    m_hooksAppend = ClaudeInstall::getStringList( hooks, "append" );
    m_hooksRemove = ClaudeInstall::getStringList( hooks, "remove" );
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( InitcpioCfgJobFactory, registerPlugin< InitcpioCfgJob >(); )
