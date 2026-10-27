/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2016 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019-2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Workers.h"

#include "MachineIdJob.h"

#include "utils/Entropy.h"
#include "utils/Logger.h"
#include "utils/System.h"

#include <QFile>

/// @brief Returns a recommended size for the entropy pool (in bytes)
STATICTEST int
getUrandomPoolSize()
{
    QFile f( "/proc/sys/kernel/random/poolsize" );
    constexpr const int minimumPoolSize = 512;
    int poolSize = minimumPoolSize;

    if ( f.exists() && f.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        QByteArray v = f.read( 16 );
        if ( v.length() > 2 )
        {
            if ( v.endsWith( '\n' ) )
            {
                v.chop( 1 );
            }
            bool ok = false;
            poolSize = v.toInt( &ok );
            if ( !ok )
            {
                poolSize = minimumPoolSize;
            }
        }
    }
    return ( poolSize >= minimumPoolSize ) ? poolSize : minimumPoolSize;
}

static inline bool
isAbsolutePath( const QString& fileName )
{
    return fileName.startsWith( '/' );
}

ClaudeInstall::JobResult
copyFile( const QString& rootMountPoint, const QString& fileName )
{
    if ( !isAbsolutePath( fileName ) )
    {
        return ClaudeInstall::JobResult::internalError(
            MachineIdJob::tr( "File not found" ),
            MachineIdJob::tr( "Path <pre>%1</pre> must be an absolute path." ).arg( fileName ),
            0 );
    }

    QFile f( fileName );
    if ( !f.exists() )
    {
        return ClaudeInstall::JobResult::error( MachineIdJob::tr( "File not found" ), fileName );
    }
    if ( !f.copy( rootMountPoint + fileName ) )
    {
        return ClaudeInstall::JobResult::error( MachineIdJob::tr( "File not found" ), rootMountPoint + fileName );
    }
    return ClaudeInstall::JobResult::ok();
}

ClaudeInstall::JobResult
createNewEntropy( int poolSize, const QString& rootMountPoint, const QString& fileName )
{
    QFile entropyFile( rootMountPoint + fileName );
    if ( entropyFile.exists() )
    {
        cWarning() << "Entropy file" << ( rootMountPoint + fileName ) << "already exists.";
        return ClaudeInstall::JobResult::ok();  // .. anyway
    }
    if ( !entropyFile.open( QIODevice::WriteOnly ) )
    {
        return ClaudeInstall::JobResult::error(
            MachineIdJob::tr( "File not found" ),
            MachineIdJob::tr( "Could not create new random file <pre>%1</pre>." ).arg( fileName ) );
    }

    QByteArray data;
    ClaudeInstall::EntropySource source = ClaudeInstall::getEntropy( poolSize, data );
    entropyFile.write( data );
    entropyFile.close();
    if ( entropyFile.size() < data.length() )
    {
        cWarning() << "Entropy file is" << entropyFile.size() << "bytes, random data was" << data.length();
    }
    if ( data.length() < poolSize )
    {
        cWarning() << "Entropy data is" << data.length() << "bytes, rather than poolSize" << poolSize;
    }
    if ( source != ClaudeInstall::EntropySource::URandom )
    {
        cWarning() << "Entropy data for pool is low-quality.";
    }
    return ClaudeInstall::JobResult::ok();
}

ClaudeInstall::JobResult
createEntropy( const EntropyGeneration kind, const QString& rootMountPoint, const QString& fileName )
{
    if ( kind == EntropyGeneration::CopyFromHost )
    {
        if ( QFile::exists( fileName ) )
        {
            auto r = copyFile( rootMountPoint, fileName );
            if ( r )
            {
                return r;
            }
            else
            {
                cWarning() << "Could not copy" << fileName << "for entropy, generating new.";
            }
        }
        else
        {
            cWarning() << "Host system entropy does not exist at" << fileName;
        }
    }

    int poolSize = getUrandomPoolSize();
    return createNewEntropy( poolSize, rootMountPoint, fileName );
}

static ClaudeInstall::JobResult
runCmd( const QStringList& cmd, bool inTarget )
{
    auto r = inTarget ? ClaudeInstall::System::instance()->targetEnvCommand( cmd )
                      : ClaudeInstall::System::instance()->runCommand( cmd, std::chrono::seconds( 0 ) );
    if ( r.getExitCode() )
    {
        return r.explainProcess( cmd, std::chrono::seconds( 0 ) );
    }

    return ClaudeInstall::JobResult::ok();
}

ClaudeInstall::JobResult
createSystemdMachineId( SystemdMachineIdStyle style, const QString& rootMountPoint, const QString& machineIdFile )
{
    switch ( style )
    {
    case SystemdMachineIdStyle::Uuid:
        return runCmd(
            QStringList { QStringLiteral( "systemd-machine-id-setup" ), QStringLiteral( "--root=" ) + rootMountPoint },
            false );
    case SystemdMachineIdStyle::Blank:
        ClaudeInstall::System::instance()->createTargetFile(
            machineIdFile, QByteArray(), ClaudeInstall::System::WriteMode::Overwrite );
        return ClaudeInstall::JobResult::ok();
    case SystemdMachineIdStyle::Uninitialized:
        ClaudeInstall::System::instance()->createTargetFile(
            machineIdFile, "uninitialized\n", ClaudeInstall::System::WriteMode::Overwrite );
        return ClaudeInstall::JobResult::ok();
    }
    return ClaudeInstall::JobResult::internalError( QStringLiteral( "Invalid systemd-style" ),
                                                QStringLiteral( "Invalid value %1" ).arg( int( style ) ),
                                                ClaudeInstall::JobResult::InvalidConfiguration );
}

ClaudeInstall::JobResult
createDBusMachineId( const QString& rootMountPoint, const QString& fileName )
{
    Q_UNUSED( rootMountPoint )
    Q_UNUSED( fileName )
    return runCmd( QStringList { QStringLiteral( "dbus-uuidgen" ), QStringLiteral( "--ensure" ) }, true );
}

ClaudeInstall::JobResult
createDBusLink( const QString& rootMountPoint, const QString& fileName, const QString& systemdFileName )
{
    Q_UNUSED( rootMountPoint )
    return runCmd( QStringList { QStringLiteral( "ln" ), QStringLiteral( "-sf" ), systemdFileName, fileName }, true );
}
