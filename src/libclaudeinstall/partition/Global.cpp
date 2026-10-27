/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 *
 */
#include "Global.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include <QVariantMap>

static const QString fsUse_key = QStringLiteral( "filesystem_use" );

bool
ClaudeInstall::Partition::isFilesystemUsedGS( const ClaudeInstall::GlobalStorage* gs, const QString& filesystemType )
{
    if ( !gs )
    {
        return false;
    }
    const QVariantMap fsUse = gs->value( fsUse_key ).toMap();
    QString key = filesystemType.toLower();
    if ( fsUse.contains( key ) )
    {
        const auto v = fsUse.value( key );
        return v.toBool();
    }
    return false;
}

void
ClaudeInstall::Partition::useFilesystemGS( ClaudeInstall::GlobalStorage* gs, const QString& filesystemType, bool used )
{
    if ( gs )
    {
        QVariantMap existingMap = gs->contains( fsUse_key ) ? gs->value( fsUse_key ).toMap() : QVariantMap();
        QString key = filesystemType.toLower();
        existingMap.insert( key, used );
        gs->insert( fsUse_key, existingMap );
    }
}

void
ClaudeInstall::Partition::clearFilesystemGS( ClaudeInstall::GlobalStorage* gs )
{
    if ( gs )
    {
        gs->remove( fsUse_key );
    }
}
