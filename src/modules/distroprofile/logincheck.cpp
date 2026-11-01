/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

/* claudeinstall-logincheck SERVICE USER < password
 *
 * Logs USER in through the PAM service SERVICE (the display manager's, e.g. sddm) the way
 * the login screen does: authentication with the password from the first line of stdin,
 * then the account checks (locked, expired, password change forced, faillock ...).
 * The distroprofile job runs it inside the new system with chroot, so the new system's
 * own PAM configuration, modules and /etc/shadow are what is tested.
 *
 * Prints "ok" or the stage that failed with PAM's reason; exit code 0 when the login works.
 */

#include <security/pam_appl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace
{
std::string g_password;

int
conversation( int count, const struct pam_message** messages, struct pam_response** responses, void* )
{
    if ( count <= 0 || count > PAM_MAX_NUM_MSG )
    {
        return PAM_CONV_ERR;
    }
    auto* replies = static_cast< pam_response* >( calloc( size_t( count ), sizeof( pam_response ) ) );
    if ( !replies )
    {
        return PAM_BUF_ERR;
    }
    for ( int i = 0; i < count; ++i )
    {
        switch ( messages[ i ]->msg_style )
        {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON:
            replies[ i ].resp = strdup( g_password.c_str() );
            break;
        case PAM_ERROR_MSG:
        case PAM_TEXT_INFO:
            std::printf( "pam says: %s\n", messages[ i ]->msg ? messages[ i ]->msg : "" );
            break;
        default:
            for ( int j = 0; j < i; ++j )
            {
                free( replies[ j ].resp );
            }
            free( replies );
            return PAM_CONV_ERR;
        }
    }
    *responses = replies;
    return PAM_SUCCESS;
}
}  // namespace

int
main( int argc, char** argv )
{
    // --auth-only: test only the "auth" lines (used to try one PAM module at a time)
    const bool authOnly = argc == 4 && std::strcmp( argv[ 3 ], "--auth-only" ) == 0;
    if ( argc != 3 && !authOnly )
    {
        std::fprintf( stderr, "usage: %s SERVICE USER [--auth-only] < password\n", argv[ 0 ] );
        return 2;
    }
    std::getline( std::cin, g_password );

    const pam_conv conv { conversation, nullptr };
    pam_handle_t* handle = nullptr;
    int rc = pam_start( argv[ 1 ], argv[ 2 ], &conv, &handle );
    if ( rc != PAM_SUCCESS )
    {
        std::printf( "start: %s\n", pam_strerror( handle, rc ) );
        return 2;
    }
    pam_set_item( handle, PAM_TTY, "tty7" );

    const char* stage = "authentication";
    rc = pam_authenticate( handle, 0 );
    if ( rc == PAM_SUCCESS && !authOnly )
    {
        stage = "account";
        rc = pam_acct_mgmt( handle, 0 );
    }
    if ( rc == PAM_SUCCESS )
    {
        std::printf( "ok\n" );
    }
    else
    {
        std::printf( "%s: %s\n", stage, pam_strerror( handle, rc ) );
    }
    pam_end( handle, rc );
    return rc == PAM_SUCCESS ? 0 : 1;
}
