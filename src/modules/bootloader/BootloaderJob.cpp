/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2014 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2014 Daniel Hillenbrand <codeworkx@bbqlinux.org>
 *   SPDX-FileCopyrightText: 2014 Benjamin Vaudour <benjamin.vaudour@yahoo.fr>
 *   SPDX-FileCopyrightText: 2014-2019 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2015-2018 Philip Mueller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2016-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2017 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2017 Ben Green <Bezzy1999@hotmail.com>
 *   SPDX-FileCopyrightText: 2021 Neal Gompa <ngompa13@gmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "BootloaderJob.h"

#include "BootloaderHelpers.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/System.h"
#include "utils/Variant.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <optional>
#include <tuple>

#include <sys/utsname.h>

using namespace Bootloader;
using ClaudeInstall::System;

namespace
{
/// Runs @p command in the target, throws CommandFailed if it fails
void
checkTargetCall( const QStringList& command )
{
    const auto r = System::instance()->targetEnvCommand( command );
    if ( r.getExitCode() != 0 )
    {
        throw CommandFailed { command, r.getExitCode(), r.getOutput() };
    }
}

/// Runs @p command in the target, throws CommandFailed if it fails, and returns the output
QString
checkTargetOutput( const QStringList& command )
{
    const auto r = System::instance()->targetEnvCommand( command );
    if ( r.getExitCode() != 0 )
    {
        throw CommandFailed { command, r.getExitCode(), r.getOutput() };
    }
    return r.getOutput();
}

int
targetCall( const QStringList& command )
{
    return System::instance()->targetEnvCall( command );
}

/// Runs @p command in the host system and returns the exit code
int
hostCall( const QStringList& command )
{
    return System::runCommand( System::RunLocation::RunInHost, command ).getExitCode();
}

/// Runs @p command in the host system, throws CommandFailed if it fails, and returns the output
QString
checkHostOutput( const QStringList& command )
{
    const auto r = System::runCommand( System::RunLocation::RunInHost, command );
    if ( r.getExitCode() != 0 )
    {
        throw CommandFailed { command, r.getExitCode(), r.getOutput() };
    }
    return r.getOutput();
}

bool
isBtrfsRoot( const QVariantMap& partition )
{
    return partition.value( "mountPoint" ).toString() == "/" && partition.value( "fs" ).toString() == "btrfs";
}

bool
isZfsRoot( const QVariantMap& partition )
{
    return partition.value( "mountPoint" ).toString() == "/" && partition.value( "fs" ).toString() == "zfs";
}

bool
anyZfsRoot( const QVariantList& partitions )
{
    for ( const auto& p : partitions )
    {
        if ( isZfsRoot( p.toMap() ) )
        {
            return true;
        }
    }
    return false;
}

QString
readFileText( const QString& path )
{
    QFile f( path );
    if ( !f.open( QIODevice::ReadOnly ) )
    {
        throw ValueError { QStringLiteral( "Could not read %1" ).arg( path ) };
    }
    return QString::fromUtf8( f.readAll() );
}

void
writeFileText( const QString& path, const QString& text )
{
    QFile f( path );
    const QByteArray data = text.toUtf8();
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) || f.write( data ) != data.size() )
    {
        throw ValueError { QStringLiteral( "Could not write %1" ).arg( path ) };
    }
}

/// The lines of @p text, each stripped (and without the empty piece after a final newline)
QStringList
strippedLines( const QString& text )
{
    QStringList lines;
    for ( const QString& line : text.split( '\n' ) )
    {
        lines.append( line.trimmed() );
    }
    if ( !lines.isEmpty() && lines.last().isEmpty() )
    {
        lines.removeLast();
    }
    return lines;
}

/** @brief The installation of the bootloader, for one run of the job
 *
 * Problems throw ValueError or CommandFailed.
 */
class Installer
{
public:
    explicit Installer( const QVariantMap& configuration )
        : m_cfg( configuration )
        , m_gs( ClaudeInstall::JobQueue::instance()->globalStorage() )
    {
    }

    void run();

private:
    // Utility
    QString required( const QString& key ) const;
    QString rootMountPoint() const { return m_gs->value( "rootMountPoint" ).toString(); }
    QVariantList partitions() const { return m_gs->value( "partitions" ).toList(); }
    QString machineId( const QString& installationRootPath ) const;

    // Kernel parameters
    QString getUuid() const;
    QString zfsRoot() const;
    static bool haveProgramInTarget( const QString& program ) { return targetCall( { "/usr/bin/which", program } ) == 0; }
    QStringList kernelParams( const QString& uuid ) const;

    // EFI
    QString efiLabel( const QString& efiDirectory ) const;
    static QString efiWordSize();
    static std::optional< std::tuple< QString, QString, QString > > grubEfiParameters();
    void efiBootNext() const;

    // The bootloaders
    void installClrBootManager();
    void createSystemdBootConf( const QString& installationRootPath,
                                const QString& efiDirectory,
                                const QString& uuid,
                                const Kernel& kernel );
    void createLoader( const QString& loaderPath, const QString& installationRootPath );
    void installSystemdBoot( const QString& efiDirectory );
    void runGrubMkconfig( const QVariantList& partitions, const QString& outputFile );
    void runGrubInstall( const QString& fwType, const QVariantList& partitions, const QString& efiDirectory, bool installHybridGrub );
    void installGrub( const QString& efiDirectory, const QString& fwType, bool installHybridGrub );
    void installSecureboot( const QString& efiDirectory );
    void updateRefindConfig( const QString& efiDirectory, const QString& installationRootPath );
    void installRefind( const QString& efiDirectory );
    void prepareBootloader( const QString& fwType, bool installHybridGrub );

    const QVariantMap& m_cfg;
    ClaudeInstall::GlobalStorage* m_gs;
};

QString
Installer::required( const QString& key ) const
{
    const QString value = ClaudeInstall::getString( m_cfg, key );
    if ( value.isEmpty() )
    {
        throw ValueError { QStringLiteral( "The configuration key '%1' is missing" ).arg( key ) };
    }
    return value;
}

QString
Installer::machineId( const QString& installationRootPath ) const
{
    QString id = readFileText( QDir( installationRootPath ).filePath( "etc/machine-id" ) );
    while ( id.endsWith( '\n' ) )
    {
        id.chop( 1 );
    }
    return id;
}

QString
Installer::getUuid() const
{
    for ( const auto& p : partitions() )
    {
        const QVariantMap partition = p.toMap();
        if ( partition.value( "mountPoint" ).toString() == "/" )
        {
            cDebug() << "Root partition uuid:" << partition.value( "uuid" ).toString();
            return partition.value( "uuid" ).toString();
        }
    }
    return QString();
}

QString
Installer::zfsRoot() const
{
    const QVariantList zfs = m_gs->value( "zfsDatasets" ).toList();
    if ( zfs.isEmpty() )
    {
        cWarning() << "Failed to locate zfs dataset list";
        return QString();
    }

    // Find the root dataset
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
Installer::kernelParams( const QString& uuid ) const
{
    // Configured kernel parameters (default "quiet"), if plymouth installed, add splash
    // screen parameter and then "rw".
    QStringList params = ClaudeInstall::getStringList( m_cfg, "kernelParams", QStringList { QStringLiteral( "quiet" ) } );
    if ( haveProgramInTarget( "plymouth" ) )
    {
        params.append( "splash" );
    }
    params.append( "rw" );

    const bool useSystemdNaming = haveProgramInTarget( "dracut" )
        || targetCall( { "/usr/bin/grep", "-q", "^HOOKS.*systemd", "/etc/mkinitcpio.conf" } ) == 0;

    const QVariantList parts = partitions();

    QStringList cryptdeviceParams;
    QString swapUuid;
    QString swapOuterMapperName;
    QString swapOuterUuid;

    // Take over swap settings:
    //  - unencrypted swap partition sets swap_uuid
    //  - encrypted root sets cryptdevice_params
    for ( const auto& p : parts )
    {
        const QVariantMap partition = p.toMap();
        const QString fs = partition.value( "fs" ).toString();
        if ( fs == "linuxswap" && !partition.value( "claimed" ).toBool() )
        {
            continue;  // foreign swap
        }
        const bool hasLuks = partition.contains( "luksMapperName" );
        const QString mapperName = partition.value( "luksMapperName" ).toString();
        const QString luksUuid = partition.value( "luksUuid" ).toString();

        if ( fs == "linuxswap" && !hasLuks )
        {
            swapUuid = partition.value( "uuid" ).toString();
        }
        if ( fs == "linuxswap" && hasLuks )
        {
            swapOuterMapperName = mapperName;
            swapOuterUuid = luksUuid;
        }
        if ( partition.value( "mountPoint" ).toString() == "/" && hasLuks )
        {
            cryptdeviceParams = useSystemdNaming
                ? QStringList { QStringLiteral( "rd.luks.uuid=%1" ).arg( luksUuid ) }
                : QStringList { QStringLiteral( "cryptdevice=UUID=%1:%2" ).arg( luksUuid, mapperName ) };
            cryptdeviceParams.append( QStringLiteral( "root=/dev/mapper/%1" ).arg( mapperName ) );
        }
    }

    // btrfs and zfs handling
    for ( const auto& p : parts )
    {
        const QVariantMap partition = p.toMap();
        // If a btrfs root subvolume wasn't set, it means the root is directly on the partition
        // and this option isn't needed
        if ( isBtrfsRoot( partition ) )
        {
            const QString subvolume = m_gs->value( "btrfsRootSubvolume" ).toString();
            if ( !subvolume.isEmpty() )
            {
                params.append( "rootflags=subvol=" + subvolume );
            }
        }

        // zfs needs to be told the location of the root dataset
        if ( isZfsRoot( partition ) )
        {
            const QString zfsRootPath = zfsRoot();
            if ( zfsRootPath.isEmpty() )
            {
                // Something is really broken if we get to this point
                cWarning() << "Internal error handling zfs dataset";
                throw ValueError { QStringLiteral( "Internal zfs data missing, please contact your distribution" ) };
            }
            params.append( "root=ZFS=" + zfsRootPath );
        }
    }

    if ( !cryptdeviceParams.isEmpty() )
    {
        params.append( cryptdeviceParams );
    }
    else
    {
        params.append( "root=UUID=" + uuid );
    }

    if ( !swapUuid.isEmpty() )
    {
        params.append( "resume=UUID=" + swapUuid );
    }
    if ( useSystemdNaming && !swapOuterUuid.isEmpty() )
    {
        params.append( "rd.luks.uuid=" + swapOuterUuid );
    }
    if ( !swapOuterMapperName.isEmpty() )
    {
        params.append( "resume=/dev/mapper/" + swapOuterMapperName );
    }

    return params;
}

QString
Installer::efiLabel( const QString& efiDirectory ) const
{
    QString efiBootloaderId;
    if ( m_cfg.contains( "efiBootloaderId" ) )
    {
        efiBootloaderId = changeEfiSuffix( efiDirectory, ClaudeInstall::getString( m_cfg, "efiBootloaderId" ) );
    }
    else
    {
        const QVariantMap branding = m_gs->value( "branding" ).toMap();
        if ( branding.isEmpty() )
        {
            throw ValueError { QStringLiteral( "There is no efiBootloaderId, and no branding to take a name from" ) };
        }
        efiBootloaderId = branding.value( "bootloaderEntryName" ).toString();
    }
    return sanitizeFileName( efiBootloaderId );
}

QString
Installer::efiWordSize()
{
    // get bitness of the underlying UEFI
    QFile f( QStringLiteral( "/sys/firmware/efi/fw_platform_size" ) );
    if ( f.open( QIODevice::ReadOnly ) )
    {
        const QString bitness = QString::fromLatin1( f.read( 2 ) );
        if ( !bitness.isEmpty() )
        {
            return bitness;
        }
    }
    // if the kernel is older than 4.0, the UEFI bitness likely isn't
    // exposed to the userspace so we assume a 64 bit UEFI here
    return QStringLiteral( "64" );
}

std::optional< std::tuple< QString, QString, QString > >
Installer::grubEfiParameters()
{
    // The parameters are: the target name, grub.efi name and boot.efi name,
    // depending on the host machine architecture.
    const QString efiBitness = efiWordSize();
    struct utsname name;
    const QString cpuType = ::uname( &name ) == 0 ? QString::fromLocal8Bit( name.machine ) : QString();

    if ( efiBitness == "32" )
    {
        // Assume all 32-bitters are legacy x86
        return std::make_tuple( QStringLiteral( "i386-efi" ), QStringLiteral( "grubia32.efi" ), QStringLiteral( "bootia32.efi" ) );
    }
    if ( efiBitness == "64" && cpuType == "aarch64" )
    {
        return std::make_tuple( QStringLiteral( "arm64-efi" ), QStringLiteral( "grubaa64.efi" ), QStringLiteral( "bootaa64.efi" ) );
    }
    if ( efiBitness == "64" && cpuType == "loongarch64" )
    {
        return std::make_tuple( QStringLiteral( "loongarch64-efi" ),
                                QStringLiteral( "grubloongarch64.efi" ),
                                QStringLiteral( "bootloongarch64.efi" ) );
    }
    if ( efiBitness == "64" )
    {
        // If it's not ARM, must by AMD64
        return std::make_tuple( QStringLiteral( "x86_64-efi" ), QStringLiteral( "grubx64.efi" ), QStringLiteral( "bootx64.efi" ) );
    }
    cWarning() << "Could not find GRUB parameters for bits" << efiBitness << "and cpu" << cpuType;
    return std::nullopt;
}

void
Installer::efiBootNext() const
{
    // Tell EFI to definitely boot into the just-installed system next time.
    const QString bootMgr = required( "efiBootMgr" );
    const QString efiBootvars = checkHostOutput( { bootMgr } );

    QString bootEntry;
    for ( const QString& line : efiBootvars.split( '\n' ) )
    {
        const QStringList words = line.split( QRegularExpression( "\\s+" ), Qt::SkipEmptyParts );
        if ( words.size() >= 2 && words.first() == "BootOrder:" )
        {
            bootEntry = words.at( 1 ).split( ',' ).first();
            break;
        }
    }
    if ( !bootEntry.isEmpty() )
    {
        hostCall( { bootMgr, "-n", bootEntry } );
    }
}

void
Installer::installClrBootManager()
{
    cDebug() << "Bootloader: clr-boot-manager";

    const QString installationRootPath = rootMountPoint();
    const QString kernelConfigPath = QDir( installationRootPath ).filePath( "etc/kernel" );
    QDir().mkpath( kernelConfigPath );

    // Write out the cmdline file for clr-boot-manager
    writeFileText( QDir( kernelConfigPath ).filePath( "cmdline" ), kernelParams( getUuid() ).join( ' ' ) );

    checkTargetCall( { "clr-boot-manager", "update" } );
}

void
Installer::createSystemdBootConf( const QString& installationRootPath,
                                  const QString& efiDirectory,
                                  const QString& uuid,
                                  const Kernel& kernel )
{
    // Get the kernel params and write them to /etc/kernel/cmdline
    // This file is used by kernel-install
    const QString kernelCmdlinePath = QDir( installationRootPath ).filePath( "etc/kernel" );
    QDir().mkpath( kernelCmdlinePath );
    writeFileText( QDir( kernelCmdlinePath ).filePath( "cmdline" ), kernelParams( uuid ).join( ' ' ) );

    cDebug() << "Configuring kernel version" << kernel.version;

    // Ensure the directory exists
    QDir().mkpath( installationRootPath + efiDirectory + '/' + machineId( installationRootPath ) );

    // Call kernel-install for each kernel
    checkTargetCall( { "kernel-install", "add", kernel.version, '/' + kernel.path } );
}

void
Installer::createLoader( const QString& loaderPath, const QString& installationRootPath )
{
    const QString id = machineId( installationRootPath );

    if ( !m_cfg.contains( "loaderEntries" ) )
    {
        cDebug() << "No additional loader entries found in config";
    }
    QStringList lines { QStringLiteral( "default %1*" ).arg( id ) };
    lines.append( ClaudeInstall::getStringList( m_cfg, "loaderEntries" ) );

    writeFileText( loaderPath, lines.join( '\n' ) + '\n' );
}

void
Installer::installSystemdBoot( const QString& efiDirectory )
{
    cDebug() << "Bootloader: systemd-boot";
    const QString installationRootPath = rootMountPoint();
    const QString installEfiDirectory = installationRootPath + efiDirectory;
    const QString uuid = getUuid();
    const QString loaderPath = QDir( installEfiDirectory ).filePath( "loader/loader.conf" );

    // This is run in the host, with the target's ESP: the result is not checked
    hostCall( { "bootctl", "--path=" + installEfiDirectory, "install" } );

    for ( const Kernel& kernel : findKernels( installationRootPath,
                                              ClaudeInstall::getString( m_cfg, "kernelSearchPath", QStringLiteral( "/usr/lib/modules" ) ),
                                              ClaudeInstall::getString( m_cfg, "kernelPattern", QStringLiteral( "vmlinuz" ) ) ) )
    {
        createSystemdBootConf( installationRootPath, efiDirectory, uuid, kernel );
    }

    createLoader( loaderPath, installationRootPath );
}

void
Installer::runGrubMkconfig( const QVariantList& parts, const QString& outputFile )
{
    const QString grubMkconfig = required( "grubMkconfig" );
    // zfs needs an environment variable set for grub-mkconfig
    if ( anyZfsRoot( parts ) )
    {
        checkTargetCall( { "sh", "-c", "ZPOOL_VDEV_NAME_PATH=1 " + grubMkconfig + " -o " + outputFile } );
    }
    else
    {
        // The input file /etc/default/grub should already be filled out by the grubcfg job module.
        checkTargetCall( { grubMkconfig, "-o", outputFile } );
    }
}

void
Installer::runGrubInstall( const QString& fwType, const QVariantList& parts, const QString& efiDirectory, bool installHybridGrub )
{
    const bool isZfs = anyZfsRoot( parts );
    const QString grubInstall = required( "grubInstall" );

    // zfs needs an environment variable set for grub
    if ( isZfs )
    {
        checkTargetCall( { "sh", "-c", "echo ZPOOL_VDEV_NAME_PATH=1 >> /etc/environment" } );
    }

    if ( fwType == "efi" )
    {
        if ( efiDirectory.isEmpty() )
        {
            throw ValueError { QStringLiteral( "There is no EFI directory" ) };
        }
        const QString efiBootloaderId = efiLabel( efiDirectory );
        const auto parameters = grubEfiParameters();
        if ( !parameters )
        {
            throw ValueError { QStringLiteral( "There are no GRUB parameters for this firmware and machine" ) };
        }
        const QString efiTarget = std::get< 0 >( *parameters );

        if ( isZfs )
        {
            checkTargetCall( { "sh",
                               "-c",
                               "ZPOOL_VDEV_NAME_PATH=1 " + grubInstall + " --target=" + efiTarget + " --efi-directory=" + efiDirectory
                                   + " --bootloader-id=" + efiBootloaderId + " --force" } );
        }
        else
        {
            checkTargetCall( { grubInstall,
                               "--target=" + efiTarget,
                               "--efi-directory=" + efiDirectory,
                               "--bootloader-id=" + efiBootloaderId,
                               "--force" } );
        }
        return;
    }

    if ( !efiDirectory.isEmpty() && !installHybridGrub )
    {
        cWarning() << "Cannot install BIOS bootloader on UEFI installation when install_hybrid_grub is 'False'!";
        return;
    }

    QString bootLoaderInstallPath;
    const QVariant bootLoader = m_gs->value( "bootLoader" );
    if ( bootLoader.isNull() )
    {
        QString efiInstallPath = m_gs->value( "efiSystemPartition" ).toString();
        if ( efiInstallPath.isEmpty() )
        {
            efiInstallPath = QStringLiteral( "/boot/efi" );
        }
        const QString findEspDiskCommand
            = QStringLiteral( "lsblk -o PKNAME \"$(df --output=source '%1' | tail -n1)\"" ).arg( efiInstallPath );
        const QString output = checkTargetOutput( { "sh", "-c", findEspDiskCommand } ).trimmed();
        if ( !output.contains( '\n' ) )
        {
            cWarning() << "Cannot find the drive containing the EFI system partition!";
            return;
        }
        bootLoaderInstallPath = "/dev/" + output.split( '\n' ).at( 1 );
    }
    else
    {
        bootLoaderInstallPath = bootLoader.toMap().value( "installPath" ).toString();
        if ( bootLoaderInstallPath.isEmpty() )
        {
            return;
        }
    }

    // bootLoaderInstallPath points to the physical disk to install GRUB to.
    // It should start with "/dev/", and be at least as long as the string "/dev/sda".
    if ( !bootLoaderInstallPath.startsWith( "/dev/" ) || bootLoaderInstallPath.length() < 8 )
    {
        throw ValueError { QStringLiteral( "boot_loader_install_path contains unexpected value '%1'" ).arg( bootLoaderInstallPath ) };
    }

    if ( isZfs )
    {
        checkTargetCall(
            { "sh", "-c", "ZPOOL_VDEV_NAME_PATH=1 " + grubInstall + " --target=i386-pc --recheck --force " + bootLoaderInstallPath } );
    }
    else
    {
        checkTargetCall( { grubInstall, "--target=i386-pc", "--recheck", "--force", bootLoaderInstallPath } );
    }
}

void
Installer::installGrub( const QString& efiDirectory, const QString& fwType, bool installHybridGrub )
{
    // get the partitions from global storage
    const QVariantList parts = partitions();
    if ( parts.isEmpty() )
    {
        cWarning() << "Failed to install grub, no partitions defined in global storage";
        return;
    }

    if ( fwType != "bios" && fwType != "efi" )
    {
        throw ValueError { QStringLiteral( "fw_type must be 'bios' or 'efi'" ) };
    }

    if ( fwType == "efi" || installHybridGrub )
    {
        cDebug() << "Bootloader: grub (efi)";
        cDebug() << "install_hybrid_grub:" << installHybridGrub;
        const QString installEfiDirectory = rootMountPoint() + efiDirectory;

        if ( !QFileInfo( installEfiDirectory ).isDir() )
        {
            QDir().mkpath( installEfiDirectory );
        }

        const QString efiBootloaderId = efiLabel( efiDirectory );

        const auto parameters = grubEfiParameters();
        if ( !parameters )
        {
            throw ValueError { QStringLiteral( "There are no GRUB parameters for this firmware and machine" ) };
        }
        const QString efiGrubFile = std::get< 1 >( *parameters );
        const QString efiBootFile = std::get< 2 >( *parameters );

        runGrubInstall( "efi", parts, efiDirectory, installHybridGrub );

        // VFAT is weird, see issue CAL-385
        const QString installEfiDirectoryFirmware = vfatCorrectCase( installEfiDirectory, "EFI" );
        if ( !QFileInfo::exists( installEfiDirectoryFirmware ) )
        {
            QDir().mkpath( installEfiDirectoryFirmware );
        }

        // there might be several values for the boot directory
        // most usual they are boot, Boot, BOOT
        const QString installEfiBootDirectory = vfatCorrectCase( installEfiDirectoryFirmware, "boot" );
        if ( !QFileInfo::exists( installEfiBootDirectory ) )
        {
            QDir().mkpath( installEfiBootDirectory );
        }

        // Workaround for some UEFI firmwares
        cDebug() << "UEFI Fallback:"
                 << ( m_cfg.contains( "installEFIFallback" ) ? m_cfg.value( "installEFIFallback" ).toString() : QStringLiteral( "<unset>" ) );
        if ( ClaudeInstall::getBool( m_cfg, "installEFIFallback", true ) )
        {
            cDebug() << "  .. installing" << efiBootFile << "fallback firmware";
            const QString efiFileSource = QDir( installEfiDirectoryFirmware ).filePath( efiBootloaderId + '/' + efiGrubFile );
            const QString efiFileTarget = QDir( installEfiBootDirectory ).filePath( efiBootFile );

            QFile::remove( efiFileTarget );  // the copy overwrites
            if ( !QFile::copy( efiFileSource, efiFileTarget ) )
            {
                throw ValueError { QStringLiteral( "Could not copy %1 to %2" ).arg( efiFileSource, efiFileTarget ) };
            }
        }
    }
    if ( fwType == "bios" || installHybridGrub )
    {
        cDebug() << "Bootloader: grub (bios)";
        runGrubInstall( "bios", parts, efiDirectory, installHybridGrub );
    }

    runGrubMkconfig( parts, required( "grubCfg" ) );
}

void
Installer::installSecureboot( const QString& efiDirectory )
{
    // Installs the secureboot shim in the system by calling efibootmgr.
    const QString efiBootloaderId = efiLabel( efiDirectory );

    const QString installEfiDirectory = rootMountPoint() + efiDirectory;

    QString installEfiBin;
    if ( efiWordSize() == "64" )
    {
        installEfiBin = QStringLiteral( "shimx64.efi" );
    }
    else if ( efiWordSize() == "32" )
    {
        installEfiBin = QStringLiteral( "shimia32.efi" );
    }
    else
    {
        cWarning() << "Unknown efi word size of" << efiWordSize() << "found";
        return;
    }

    // Copied, roughly, from openSUSE's install script. *disk* is something like /dev/sda,
    // while *drive* may return "(disk/dev/sda,gpt1)" .. we're interested in the numbers
    // in the second part of that tuple. (The output ends in a newline, which is stripped.)
    const QString grubProbe = required( "grubProbe" );
    const QString efiDrive = checkHostOutput( { grubProbe, "-t", "drive", "--device-map=", installEfiDirectory } ).trimmed();
    const QString efiDisk = checkHostOutput( { grubProbe, "-t", "disk", "--device-map=", installEfiDirectory } ).trimmed();

    QString cleanedDrive = efiDrive;
    cleanedDrive.remove( '(' ).remove( ')' );
    const QStringList driveParts = cleanedDrive.split( ',' );
    if ( driveParts.size() < 2 )
    {
        throw ValueError { QStringLiteral( "No partition found in drive '%1'" ).arg( efiDrive ) };
    }
    // Get the first run of digits from the partition
    const auto m = QRegularExpression( QStringLiteral( "\\d+" ) ).match( driveParts.at( 1 ) );
    if ( !m.hasMatch() )
    {
        throw ValueError { QStringLiteral( "No partition number found for %1" ).arg( installEfiDirectory ) };
    }

    hostCall( { required( "efiBootMgr" ),
                "-c",
                "-w",
                "-L",
                efiBootloaderId,
                "-d",
                efiDisk,
                "-p",
                m.captured( 0 ),
                "-l",
                installEfiDirectory + '/' + installEfiBin } );

    efiBootNext();

    // The input file /etc/default/grub should already be filled out by the grubcfg job module.
    checkTargetCall( { required( "grubMkconfig" ), "-o", QDir( efiDirectory ).filePath( "EFI/" + efiBootloaderId + "/grub.cfg" ) } );
}

void
Installer::updateRefindConfig( const QString& efiDirectory, const QString& installationRootPath )
{
    if ( !m_cfg.contains( "refindKernelList" ) )
    {
        cWarning() << "refindKernelList not set.  Skipping updating refind.conf";
        return;
    }
    const QStringList kernelList = ClaudeInstall::getStringList( m_cfg, "refindKernelList" );

    // Update the config in the file
    const QString path = installationRootPath + efiDirectory + "/EFI/refind/refind.conf";
    QStringList lines = strippedLines( readFileText( path ) );
    for ( QString& line : lines )
    {
        if ( line.startsWith( "#extra_kernel_version_strings" ) || line.startsWith( "extra_kernel_version_strings" ) )
        {
            while ( line.startsWith( '#' ) )
            {
                line.remove( 0, 1 );
            }
            for ( const QString& kernel : kernelList )
            {
                if ( !line.contains( kernel ) )
                {
                    line += ',' + kernel;
                }
            }
        }
    }
    writeFileText( path, lines.join( '\n' ) + '\n' );
}

void
Installer::installRefind( const QString& efiDirectory )
{
    const QString installationRootPath = rootMountPoint();
    if ( installationRootPath.isEmpty() )
    {
        cWarning() << "Global storage value \"rootMountPoint\" missing";
    }

    const QString uuid = getUuid();
    const QString params = kernelParams( uuid ).join( ' ' );
    const QString confPath = QDir( installationRootPath ).filePath( "boot/refind_linux.conf" );

    checkTargetCall( { "refind-install" } );

    QStringList lines = strippedLines( readFileText( confPath ) );
    for ( QString& line : lines )
    {
        if ( line.startsWith( "\"Boot with standard options\"" ) )
        {
            line = QStringLiteral( "\"Boot with standard options\"    \"%1\"" ).arg( params );
        }
        else if ( line.startsWith( "\"Boot to single-user mode\"" ) )
        {
            line = QStringLiteral( "\"Boot to single-user mode\"    \"%1\" single" ).arg( params );
        }
    }
    writeFileText( confPath, lines.join( '\n' ) + '\n' );

    updateRefindConfig( efiDirectory, installationRootPath );
}

void
Installer::prepareBootloader( const QString& fwType, bool installHybridGrub )
{
    // Get the boot loader selection from global storage if it is set in the config file
    QString efiBootLoader;
    if ( m_cfg.contains( "efiBootLoaderVar" ) )
    {
        const QString gsName = ClaudeInstall::getString( m_cfg, "efiBootLoaderVar" );
        if ( !m_gs->contains( gsName ) )
        {
            cWarning() << "Specified global storage value not found in global storage";
            return;
        }
        efiBootLoader = m_gs->value( gsName ).toString();
    }
    else if ( m_cfg.contains( "efiBootLoader" ) )
    {
        // If the conf value for using global storage is not set, use the setting from the config file.
        efiBootLoader = ClaudeInstall::getString( m_cfg, "efiBootLoader" );
    }
    else if ( fwType == "efi" || installHybridGrub )
    {
        cWarning() << "Configuration missing both efiBootLoader and efiBootLoaderVar on an EFI-enabled "
                      "system, bootloader not installed";
        return;
    }
    // (otherwise no bootloader is named and GRUB is used for BIOS; the Python version failed here)

    // If the user has selected not to install bootloader, bail out here
    if ( efiBootLoader.compare( "none", Qt::CaseInsensitive ) == 0 )
    {
        cDebug() << "Skipping bootloader installation since no bootloader was selected";
        return;
    }

    const QString efiDirectory = m_gs->value( "efiSystemPartition" ).toString();

    if ( efiBootLoader == "clr-boot-manager" )
    {
        if ( fwType != "efi" )
        {
            // Grub has to be installed first on non-EFI systems
            // (the Python version forgot to pass the hybrid setting here)
            installGrub( efiDirectory, fwType, installHybridGrub );
        }
        installClrBootManager();
    }
    else if ( efiBootLoader == "systemd-boot" && fwType == "efi" )
    {
        installSystemdBoot( efiDirectory );
    }
    else if ( efiBootLoader == "sb-shim" && fwType == "efi" )
    {
        installSecureboot( efiDirectory );
    }
    else if ( efiBootLoader == "refind" && fwType == "efi" )
    {
        installRefind( efiDirectory );
    }
    else if ( efiBootLoader == "grub" || fwType != "efi" )
    {
        installGrub( efiDirectory, fwType, installHybridGrub );
    }
    else
    {
        cDebug() << "WARNING: the combination of boot-loader" << efiBootLoader << "and firmware" << fwType
                 << "is not supported.";
    }
}

void
Installer::run()
{
    const QString fwType = m_gs->value( "firmwareType" ).toString();
    const QVariant bootLoader = m_gs->value( "bootLoader" );

    const bool installHybridGrub = ClaudeInstall::getBool( m_cfg, "installHybridGRUB", false );
    const QString efiBootLoader = ClaudeInstall::getString( m_cfg, "efiBootLoader", QString() );

    if ( installHybridGrub && efiBootLoader != "grub" )
    {
        throw ValueError { QStringLiteral( "efi_boot_loader '%1' is illegal when install_hybrid_grub is 'true'!" ).arg( efiBootLoader ) };
    }

    if ( bootLoader.isNull() && fwType != "efi" )
    {
        cWarning() << "Non-EFI system, and no bootloader is set.";
        return;
    }

    if ( fwType == "efi" )
    {
        const QString efiSystemPartition = m_gs->value( "efiSystemPartition" ).toString();
        bool espFound = false;
        for ( const auto& p : partitions() )
        {
            espFound = espFound || p.toMap().value( "mountPoint" ).toString() == efiSystemPartition;
        }
        if ( !espFound )
        {
            cWarning() << "EFI system, but nothing mounted on" << efiSystemPartition;
            return;
        }
    }

    prepareBootloader( fwType, installHybridGrub );
}
}  // namespace

BootloaderJob::BootloaderJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

BootloaderJob::~BootloaderJob() {}

QString
BootloaderJob::prettyName() const
{
    return tr( "Install bootloader.", "@status" );
}

ClaudeInstall::JobResult
BootloaderJob::exec()
{
    try
    {
        Installer( m_configuration ).run();
    }
    catch ( const CommandFailed& e )
    {
        cWarning() << "Command" << e.command.join( ' ' ) << "failed with exit code" << e.exitCode;
        cDebug() << "output:" << e.output;
        return ClaudeInstall::JobResult::error(
            tr( "Bootloader installation error" ),
            tr( "The bootloader could not be installed. The installation command <pre>%1</pre> returned error code %2." )
                .arg( e.command.join( ' ' ) )
                .arg( e.exitCode ) );
    }
    catch ( const ValueError& e )
    {
        cWarning() << "Bootloader installation failed:" << e.message;
        return ClaudeInstall::JobResult::error( tr( "Bootloader installation error" ), e.message );
    }
    return ClaudeInstall::JobResult::ok();
}

void
BootloaderJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_configuration = configurationMap;
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( BootloaderJobFactory, registerPlugin< BootloaderJob >(); )
