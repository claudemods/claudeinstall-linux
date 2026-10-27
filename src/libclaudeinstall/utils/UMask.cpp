/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 *
 */

#include "UMask.h"

#include <sys/stat.h>
#include <sys/types.h>

namespace ClaudeInstall
{
mode_t
setUMask( mode_t u )
{
    return umask( u );
}

UMask::UMask( mode_t u )
    : m_mode( setUMask( u ) )
{
}

UMask::~UMask()
{
    setUMask( m_mode );
}

static_assert( UMask::Safe == ( S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH ), "Bad permissions." );

}  // namespace ClaudeInstall
