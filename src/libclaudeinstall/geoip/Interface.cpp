/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Interface.h"

#include "utils/Logger.h"
#include "utils/String.h"

namespace ClaudeInstall
{
namespace GeoIP
{

Interface::Interface( const QString& e )
    : m_element( e )
{
}

Interface::~Interface() {}

RegionZonePair
splitTZString( const QString& tz )
{
    QString timezoneString( tz );
    timezoneString.remove( '\\' );
    timezoneString.replace( ' ', '_' );

    QStringList tzParts = timezoneString.split( '/', SplitSkipEmptyParts );
    if ( tzParts.size() >= 2 )
    {
        QString region = tzParts.takeFirst();
        QString zone = tzParts.join( '/' );
        return RegionZonePair( region, zone );
    }

    return RegionZonePair( QString(), QString() );
}

}  // namespace GeoIP
}  // namespace ClaudeInstall
