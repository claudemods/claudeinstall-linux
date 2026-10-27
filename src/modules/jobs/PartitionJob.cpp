/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "PartitionJob.h"

PartitionJob::PartitionJob( Partition* partition )
    : m_partition( partition )
{
}

void
PartitionJob::iprogress( int percent )
{
    if ( percent < 0 )
    {
        percent = 0;
    }
    if ( percent > 100 )
    {
        percent = 100;
    }
    Q_EMIT progress( qreal( percent / 100.0 ) );
}
