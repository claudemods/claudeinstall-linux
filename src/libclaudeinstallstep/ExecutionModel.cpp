/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ExecutionModel.h"

#include "JobQueue.h"

namespace ClaudeInstall
{

ExecutionModel::ExecutionModel( QObject* parent )
    : QObject( parent )
{
    if ( JobQueue::instance() )
    {
        connect( JobQueue::instance(), &JobQueue::progress, this, &ExecutionModel::onProgress );
        // The running state changes when the queue ends, one way or the other
        connect( JobQueue::instance(), &JobQueue::finished, this, &ExecutionModel::changed );
        connect( JobQueue::instance(), &JobQueue::failed, this, &ExecutionModel::changed );
    }
}

ExecutionModel::~ExecutionModel() {}

bool
ExecutionModel::running() const
{
    return JobQueue::instance() && JobQueue::instance()->isRunning();
}

void
ExecutionModel::start( const QList< WeightedJobs >& groups )
{
    JobQueue* queue = JobQueue::instance();
    if ( !queue )
    {
        return;
    }

    for ( const auto& group : groups )
    {
        queue->enqueue( group.weight, group.jobs );
    }
    queue->start();
    emit changed();
}

void
ExecutionModel::onProgress( qreal percent, const QString& message )
{
    m_progress = percent;
    if ( !message.isEmpty() )
    {
        m_message = message;
    }
    emit updated( percent, message );
    emit changed();
}

}  // namespace ClaudeInstall
