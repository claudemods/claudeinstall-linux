/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 *
 */
#include "InstanceKey.h"

namespace ClaudeInstall
{
namespace ModuleSystem
{

InstanceKey
InstanceKey::fromString( const QString& s )
{
    QStringList moduleEntrySplit = s.split( '@' );
    if ( moduleEntrySplit.length() < 1 || moduleEntrySplit.length() > 2 )
    {
        return InstanceKey();
    }
    // For length 1, first == last
    return InstanceKey( moduleEntrySplit.first(), moduleEntrySplit.last() );
}


QDebug&
operator<<( QDebug& s, const ClaudeInstall::ModuleSystem::InstanceKey& i )
{
    return s << i.toString();
}

}  // namespace ModuleSystem
}  // namespace ClaudeInstall
