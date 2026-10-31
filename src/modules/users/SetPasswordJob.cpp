/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "SetPasswordJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Entropy.h"
#include "utils/Logger.h"
#include "utils/System.h"

#include <QDir>
#include <QRandomGenerator>

#include <random>

#ifndef NO_CRYPT_H
#include <crypt.h>
#endif
#include <unistd.h>

SetPasswordJob::SetPasswordJob( const QString& userName, const QString& newPassword )
    : ClaudeInstall::Job()
    , m_userName( userName )
    , m_newPassword( newPassword )
{
}

QString
SetPasswordJob::prettyName() const
{
    return tr( "Set password for user %1" ).arg( m_userName );
}

QString
SetPasswordJob::prettyStatusMessage() const
{
    return tr( "Setting password for user %1…", "@status" ).arg( m_userName );
}

namespace
{
/// A salt every crypt() understands: "$6$" (SHA512) and random characters from the crypt alphabet.
/// (The old "$y$" + random text was not a valid yescrypt setting: crypt() answered "*0" and the
/// account got a locked password, so nothing typed at the login screen could work.)
QString
sha512Salt( int length )
{
    static const char alphabet[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    QString salt = QStringLiteral( "$6$" );
    for ( int i = 0; i < length; ++i )
    {
        salt.append( QLatin1Char( alphabet[ QRandomGenerator::system()->bounded( 64 ) ] ) );
    }
    salt.append( QLatin1Char( '$' ) );
    return salt;
}
}  // namespace

#ifndef HAVE_CRYPT_GENSALT
/// Returns a modular hashing salt for method 6 (SHA512) with a random salt of @p length characters.
QString
SetPasswordJob::make_salt( int length )
{
    Q_ASSERT( length >= 8 );
    Q_ASSERT( length <= 16 );
    return sha512Salt( length );
}
#endif

ClaudeInstall::JobResult
SetPasswordJob::exec()
{
    ClaudeInstall::GlobalStorage* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    QDir destDir( gs->value( "rootMountPoint" ).toString() );
    if ( !destDir.exists() )
    {
        return ClaudeInstall::JobResult::error( tr( "Bad destination system path." ),
                                            tr( "rootMountPoint is %1" ).arg( destDir.absolutePath() ) );
    }

    if ( m_userName == "root" && m_newPassword.isEmpty() )  //special case for disabling root account
    {
        int ec = ClaudeInstall::System::instance()->targetEnvCall( { "usermod", "-p", "!", m_userName } );
        if ( ec )
        {
            return ClaudeInstall::JobResult::error( tr( "Cannot disable root account." ),
                                                tr( "usermod terminated with error code %1." ).arg( ec ) );
        }
        return ClaudeInstall::JobResult::ok();
    }

    // Let the target system hash the password itself: chpasswd inside the target uses the target's
    // own libxcrypt / login.defs / PAM settings, so the hash is always one its login stack accepts.
    // (Hashing with the live environment's crypt() and passing it to "usermod -p" is what the
    // old code did.) The password goes in via stdin only, so it is never on a command line or in the log.
    const QString line = m_userName + QLatin1Char( ':' ) + m_newPassword + QLatin1Char( '\n' );
    auto result = ClaudeInstall::System::instance()->targetEnvCommand( { "chpasswd" }, QString(), line );
    if ( result.getExitCode() )
    {
        return ClaudeInstall::JobResult::error( tr( "Cannot set password for user %1." ).arg( m_userName ),
                                                tr( "chpasswd terminated with error code %1." ).arg( result.getExitCode() ) );
    }

    // Read the hash back: an empty or locked ("!" / "*") field means the login would be refused.
    QString shadowLine;
    if ( ClaudeInstall::System::instance()->targetEnvOutput( { "getent", "shadow", m_userName }, shadowLine ) == 0 )
    {
        const QString hash = shadowLine.section( ':', 1, 1 );
        if ( hash.isEmpty() || hash.startsWith( '!' ) || hash.startsWith( '*' ) )
        {
            return ClaudeInstall::JobResult::error( tr( "Cannot set password for user %1." ).arg( m_userName ),
                                                    tr( "The account is still locked or empty after chpasswd." ) );
        }
        cDebug() << "Password for" << m_userName << "set with hash method"
                 << hash.left( hash.indexOf( '$', 1 ) + 1 );
    }

    return ClaudeInstall::JobResult::ok();
}
