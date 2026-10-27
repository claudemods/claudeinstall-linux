/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2019 Bernhard Landauer <oberon@manjaro.org>
 *   SPDX-FileCopyrightText: 2020 Camilo Higuita <milo.h@aol.com>
 *   SPDX-FileCopyrightText: 2022 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "GrubCfgJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "compat/Variant.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace
{
using ConfigItems = QList< QPair< QString, QString > >;

int
indexOfKey( const ConfigItems& items, const QString& key )
{
    for ( int i = 0; i < items.size(); ++i )
    {
        if ( items.at( i ).first == key )
        {
            return i;
        }
    }
    return -1;
}

/// Sets @p key, keeping its position if already present (like assigning to a dict)
void
setItem( ConfigItems& items, const QString& key, const QString& value )
{
    const int i = indexOfKey( items, key );
    if ( i >= 0 )
    {
        items[ i ].second = value;
    }
    else
    {
        items.append( { key, value } );
    }
}

/// Like Python's str.strip(chars)
QString
stripChars( const QString& s, const QString& chars )
{
    int start = 0;
    int end = s.length();
    while ( start < end && chars.contains( s.at( start ) ) )
    {
        ++start;
    }
    while ( end > start && chars.contains( s.at( end - 1 ) ) )
    {
        --end;
    }
    return s.mid( start, end - start );
}

QString
escapeSingleQuotes( QString s )
{
    return s.replace( '\'', QStringLiteral( "'\\''" ) );
}

bool
targetHas( const char* program )
{
    return ClaudeInstall::System::instance()->targetEnvCall( { "sh", "-c", QStringLiteral( "which " ) + program } ) == 0;
}

/// Looks in global storage to find the zfs root, or returns an empty string.
QString
getZfsRoot()
{
    const QVariantList zfs = ClaudeInstall::JobQueue::instance()->globalStorage()->value( "zfsDatasets" ).toList();
    if ( zfs.isEmpty() )
    {
        cWarning() << "Failed to locate zfs dataset list";
        return QString();
    }

    for ( const auto& d : zfs )
    {
        const QVariantMap dataset = d.toMap();
        if ( dataset.value( "mountpoint" ).toString() == "/" )
        {
            return dataset.value( "zpool" ).toString() + '/' + dataset.value( "dsName" ).toString();
        }
    }
    return QString();
}

QStringList
readStrippedLines( const QString& path )
{
    QStringList lines;
    QFile f( path );
    if ( f.open( QIODevice::ReadOnly ) )
    {
        for ( const QByteArray& l : f.readAll().split( '\n' ) )
        {
            lines.append( QString::fromUtf8( l ).trimmed() );
        }
        if ( !lines.isEmpty() && lines.last().isEmpty() )
        {
            lines.removeLast();
        }
    }
    return lines;
}
}  // namespace

GrubCfgJob::GrubCfgJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

GrubCfgJob::~GrubCfgJob() {}

QString
GrubCfgJob::prettyName() const
{
    return tr( "Configure GRUB…", "@status" );
}

QString
GrubCfgJob::grubConfigPath( const QString& rootMountPoint ) const
{
    QString defaultDir = QDir( rootMountPoint ).filePath( "etc/default" );
    QString defaultConfigFile = QStringLiteral( "grub" );

    if ( ClaudeInstall::getBool( m_configuration, "prefer_grub_d", false ) )
    {
        const QString possibleDir = QDir( rootMountPoint ).filePath( "etc/default/grub.d" );
        if ( QFileInfo( possibleDir ).isDir() )
        {
            defaultDir = possibleDir;
            defaultConfigFile = QStringLiteral( "00claudeinstall.cfg" );
        }
    }

    if ( !QFileInfo::exists( defaultDir ) && !QDir().mkdir( defaultDir ) )
    {
        cDebug() << "Failed to create" << defaultDir;
        return QString();
    }
    return QDir( defaultDir ).filePath( defaultConfigFile );
}

bool
GrubCfgJob::updateExistingConfig( const QString& defaultGrub, ConfigItems items )
{
    const QString original = defaultGrub + QStringLiteral( ".claudeinstall" );
    QFile::remove( original );
    if ( !QFile::rename( defaultGrub, original ) )
    {
        return false;
    }

    const QStringList lines = readStrippedLines( original );
    QByteArray out;
    for ( const QString& line : lines )
    {
        QString written = line;
        if ( line.contains( '=' ) )
        {
            // This may be a key, strip the leading comment if it has one
            QString key = line;
            while ( key.startsWith( '#' ) )
            {
                key.remove( 0, 1 );
            }
            key = key.section( '=', 0, 0 ).trimmed();

            // check if this is one of the keys we care about
            const int i = indexOfKey( items, key );
            if ( i >= 0 )
            {
                written = QStringLiteral( "%1=%2" ).arg( key, items.at( i ).second );
                items.removeAt( i );
            }
        }
        out += written.toUtf8() + '\n';
    }
    for ( const auto& item : items )
    {
        out += QStringLiteral( "%1=%2\n" ).arg( item.first, item.second ).toUtf8();
    }

    QFile f( defaultGrub );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) || f.write( out ) != out.size() )
    {
        return false;
    }
    f.close();
    QFile::remove( original );
    return true;
}

ClaudeInstall::JobResult
GrubCfgJob::modifyGrubDefault( const QVariantList& partitions, const QString& rootMountPoint, QString distributor ) const
{
    const QString defaultGrub = grubConfigPath( rootMountPoint );
    if ( defaultGrub.isEmpty() )
    {
        return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                            tr( "Could not create the GRUB configuration directory." ) );
    }

    distributor = escapeSingleQuotes( distributor );

    const bool haveDracut = targetHas( "dracut" );
    const bool havePlymouth = targetHas( "plymouth" );
    const bool usesSystemdHook = ClaudeInstall::System::instance()->targetEnvCall(
                                     { "sh", "-c", QStringLiteral( "grep -q \"^HOOKS.*systemd\" /etc/mkinitcpio.conf" ) } )
        == 0;
    const bool useSystemdNaming = haveDracut || usesSystemdHook;

    QString swapUuid;
    QString swapOuterUuid;
    QString swapOuterMapperName;
    bool noSaveDefault = false;
    bool unencryptedSeparateBoot = false;
    // If there is no dracut, and the root partition is ZFS, this gets set below
    QString zfsRootPath;

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
        const QString fs = partition.value( "fs" ).toString();
        if ( ( mountPoint == "/" || mountPoint == "/boot" ) && ( fs == "btrfs" || fs == "f2fs" || fs == "zfs" ) )
        {
            noSaveDefault = true;
            break;
        }
    }

    QStringList cryptdeviceParams;
    for ( const auto& p : partitions )
    {
        const QVariantMap partition = p.toMap();
        const QString mountPoint = partition.value( "mountPoint" ).toString();
        const QString fs = partition.value( "fs" ).toString();
        if ( fs == "linuxswap" && !partition.value( "claimed" ).toBool() )
        {
            continue;  // foreign swap
        }
        const bool hasLuks = partition.contains( "luksMapperName" );
        const QString luksUuid = partition.value( "luksUuid" ).toString();
        const QString mapperName = partition.value( "luksMapperName" ).toString();

        if ( fs == "linuxswap" && !hasLuks )
        {
            swapUuid = partition.value( "uuid" ).toString();
        }

        if ( useSystemdNaming )
        {
            if ( fs == "linuxswap" && hasLuks )
            {
                swapOuterUuid = luksUuid;
                swapOuterMapperName = mapperName;
            }
            if ( mountPoint == "/" && hasLuks )
            {
                cryptdeviceParams = QStringList { QStringLiteral( "rd.luks.uuid=%1" ).arg( luksUuid ) };
                if ( !unencryptedSeparateBoot && usesSystemdHook )
                {
                    cryptdeviceParams.append( "rd.luks.key=/crypto_keyfile.bin" );
                }
            }
        }
        else
        {
            if ( fs == "linuxswap" && hasLuks )
            {
                swapOuterMapperName = mapperName;
            }
            if ( mountPoint == "/" && hasLuks )
            {
                cryptdeviceParams = QStringList { QStringLiteral( "cryptdevice=UUID=%1:%2" ).arg( luksUuid, mapperName ),
                                                  QStringLiteral( "root=/dev/mapper/%1" ).arg( mapperName ) };
            }
            if ( fs == "zfs" && mountPoint == "/" )
            {
                zfsRootPath = getZfsRoot();
            }
        }
    }

    QStringList kernelParams
        = ClaudeInstall::getStringList( m_configuration, "kernel_params", QStringList { QStringLiteral( "quiet" ) } );

    // Currently, grub doesn't detect this properly so it must be set manually
    if ( !zfsRootPath.isEmpty() )
    {
        kernelParams.prepend( "zfs=" + zfsRootPath );
    }
    kernelParams.append( cryptdeviceParams );
    if ( havePlymouth )
    {
        kernelParams.append( "splash" );
    }
    if ( !swapUuid.isEmpty() )
    {
        kernelParams.append( "resume=UUID=" + swapUuid );
    }
    if ( useSystemdNaming && !swapOuterUuid.isEmpty() )
    {
        kernelParams.append( "rd.luks.uuid=" + swapOuterUuid );
    }
    if ( !swapOuterMapperName.isEmpty() )
    {
        kernelParams.append( "resume=/dev/mapper/" + swapOuterMapperName );
    }

    const bool overwrite = ClaudeInstall::getBool( m_configuration, "overwrite", false );
    const bool grubExists = QFileInfo::exists( defaultGrub );

    ConfigItems items;
    // read the lines we need from the existing config
    if ( grubExists && !overwrite )
    {
        static const QRegularExpression cmdlinePrefix( QStringLiteral( "^GRUB_CMDLINE_LINUX_DEFAULT\\s*=\\s*" ) );
        for ( const QString& line : readStrippedLines( defaultGrub ) )
        {
            if ( line.startsWith( "GRUB_CMDLINE_LINUX_DEFAULT" ) )
            {
                QString value = line;
                value.remove( cmdlinePrefix );
                const QStringList existingParams
                    = stripChars( value.trimmed(), QStringLiteral( "\"'" ) ).split( QRegularExpression( "\\s+" ), Qt::SkipEmptyParts );

                for ( const QString& existingParam : existingParams )
                {
                    const QString existingName = existingParam.section( '=', 0, 0 ).trimmed();

                    // Ensure we aren't adding duplicated params
                    bool paramExists = false;
                    for ( const QString& param : kernelParams )
                    {
                        if ( param.section( '=', 0, 0 ).trimmed() == existingName )
                        {
                            paramExists = true;
                            break;
                        }
                    }
                    if ( !paramExists && existingName != "quiet" && existingName != "resume" && existingName != "splash" )
                    {
                        kernelParams.append( existingParam );
                    }
                }
            }
            else if ( line.startsWith( "GRUB_DISTRIBUTOR" ) && ClaudeInstall::getBool( m_configuration, "keep_distributor", false ) )
            {
                const QStringList parts = line.split( '=' );
                if ( parts.size() > 1 )
                {
                    distributor = stripChars( parts.at( 1 ).trimmed(), QStringLiteral( "'\"" ) );
                }
            }

            // If a filesystem grub can't write to is used, disable save default
            if ( noSaveDefault && line.trimmed().startsWith( "GRUB_SAVEDEFAULT" ) )
            {
                setItem( items, "GRUB_SAVEDEFAULT", "false" );
            }
        }
    }

    const bool alwaysUseDefaults = ClaudeInstall::getBool( m_configuration, "always_use_defaults", false );

    // If applicable add the items from defaults to the list containing the grub config to write/modify
    if ( ( alwaysUseDefaults || overwrite || !grubExists ) && m_configuration.contains( "defaults" ) )
    {
        const QVariantMap defaults = m_configuration.value( "defaults" ).toMap();
        for ( auto it = defaults.constBegin(); it != defaults.constEnd(); ++it )
        {
            QString escaped;
            if ( ClaudeInstall::typeOf( it.value() ) == ClaudeInstall::BoolVariantType )
            {
                escaped = it.value().toBool() ? QStringLiteral( "true" ) : QStringLiteral( "false" );
            }
            else
            {
                escaped = escapeSingleQuotes( it.value().toString() );
            }
            setItem( items, it.key(), QStringLiteral( "'%1'" ).arg( escaped ) );
        }
    }

    setItem( items, "GRUB_CMDLINE_LINUX_DEFAULT", QStringLiteral( "'%1'" ).arg( kernelParams.join( ' ' ) ) );
    setItem( items, "GRUB_DISTRIBUTOR", QStringLiteral( "'%1'" ).arg( distributor ) );

    if ( !cryptdeviceParams.isEmpty() && !unencryptedSeparateBoot )
    {
        setItem( items, "GRUB_ENABLE_CRYPTODISK", "y" );
    }

    bool ok = true;
    if ( overwrite || !grubExists || ClaudeInstall::getBool( m_configuration, "prefer_grub_d", false ) )
    {
        QFile f( defaultGrub );
        ok = f.open( QIODevice::WriteOnly | QIODevice::Truncate );
        for ( const auto& item : items )
        {
            if ( ok )
            {
                f.write( QStringLiteral( "%1=%2\n" ).arg( item.first, item.second ).toUtf8() );
            }
        }
    }
    else
    {
        ok = updateExistingConfig( defaultGrub, items );
    }
    if ( !ok )
    {
        return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                            tr( "Could not write <pre>%1</pre>." ).arg( defaultGrub ) );
    }
    return ClaudeInstall::JobResult::ok();
}

ClaudeInstall::JobResult
GrubCfgJob::exec()
{
    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    const QString fwType = gs->value( "firmwareType" ).toString();
    const QVariantList partitions = gs->value( "partitions" ).toList();
    const QString rootMountPoint = gs->value( "rootMountPoint" ).toString();
    const QVariantMap branding = gs->value( "branding" ).toMap();

    if ( gs->value( "bootLoader" ).isNull() && fwType != "efi" )
    {
        return ClaudeInstall::JobResult::ok();
    }

    if ( fwType == "efi" )
    {
        const QString esp = gs->value( "efiSystemPartition" ).toString();
        bool espFound = false;
        for ( const auto& p : partitions )
        {
            if ( p.toMap().value( "mountPoint" ).toString() == esp )
            {
                espFound = true;
            }
        }
        if ( !espFound )
        {
            return ClaudeInstall::JobResult::ok();
        }
    }

    if ( branding.isEmpty() )
    {
        return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                            tr( "No branding information is available for <pre>%1</pre>." ).arg( "grubcfg" ) );
    }
    return modifyGrubDefault( partitions, rootMountPoint, branding.value( "bootloaderEntryName" ).toString() );
}

void
GrubCfgJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_configuration = configurationMap;
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( GrubCfgJobFactory, registerPlugin< GrubCfgJob >(); )
