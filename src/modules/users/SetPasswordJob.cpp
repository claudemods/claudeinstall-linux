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

    // crypt() returns null or a "*0" / "*1" marker when the salt is not supported: that would
    // set an empty or locked password without any error. Try the library's best method first,
    // then SHA512, and stop with an error if neither produces a real hash.
    QString salt, encrypted;
    for ( int attempt = 0; attempt < 2 && ( encrypted.size() < 13 || encrypted.startsWith( '*' ) ); ++attempt )
    {
        salt.clear();
#ifdef HAVE_CRYPT_GENSALT
        if ( attempt == 0 )
        {
            const char* generated = crypt_gensalt( NULL, 0, NULL, 0 );
            salt = generated ? QString::fromLatin1( generated ) : QString();
        }
#endif
        if ( salt.isEmpty() )
        {
            salt = sha512Salt( 16 );
        }
        const char* hashed = crypt( m_newPassword.toUtf8(), salt.toUtf8() );
        encrypted = hashed ? QString::fromLatin1( hashed ) : QString();
    }
    if ( encrypted.size() < 13 || encrypted.startsWith( '*' ) )
    {
        return ClaudeInstall::JobResult::error( tr( "Cannot set password for user %1." ).arg( m_userName ),
                                                tr( "The password could not be hashed (salt prefix %1)." ).arg( salt.left( 3 ) ) );
    }
    cDebug() << "Password for" << m_userName << "hashed with method" << encrypted.left( encrypted.indexOf( '$', 1 ) + 1 );

    int ec = ClaudeInstall::System::instance()->targetEnvCall( { "usermod", "-p", encrypted, m_userName } );
    if ( ec )
    {
        return ClaudeInstall::JobResult::error( tr( "Cannot set password for user %1." ).arg( m_userName ),
                                            tr( "usermod terminated with error code %1." ).arg( ec ) );
    }

    return ClaudeInstall::JobResult::ok();
}
