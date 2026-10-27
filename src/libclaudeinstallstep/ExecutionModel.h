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

#ifndef EXECUTIONMODEL_H
#define EXECUTIONMODEL_H

#include "DllMacro.h"
#include "Job.h"

#include <QList>
#include <QObject>
#include <QString>

namespace ClaudeInstall
{

/**
 * @brief The state of the *exec* phase, for any frontend to show
 *
 * The model starts the queued jobs on the JobQueue and reports their overall
 * progress and status message. A progress bar, a QML item or a text-mode
 * screen all read the same thing.
 */
class STEPDLLEXPORT ExecutionModel : public QObject
{
    Q_OBJECT
    ///@brief Overall progress, from 0 to 1
    Q_PROPERTY( qreal progress READ progress NOTIFY changed FINAL )
    ///@brief What the running job reports it is doing (the last non-empty message)
    Q_PROPERTY( QString message READ message NOTIFY changed FINAL )
    ///@brief Are jobs running?
    Q_PROPERTY( bool running READ running NOTIFY changed FINAL )

public:
    /// @brief The jobs of one module, with the module's weight in the overall progress
    struct WeightedJobs
    {
        int weight = 1;
        JobList jobs;
    };

    explicit ExecutionModel( QObject* parent = nullptr );
    ~ExecutionModel() override;

    /** @brief Queues all the jobs in @p groups, and starts the queue */
    void start( const QList< WeightedJobs >& groups );

    qreal progress() const { return m_progress; }
    QString message() const { return m_message; }
    /// @brief Are jobs running (queue not yet finished)?
    bool running() const;

signals:
    /// @brief The progress, message or running state changed
    void changed();
    /// @brief The job queue reported progress (same values as JobQueue::progress())
    void updated( qreal percent, const QString& message );

private:
    void onProgress( qreal percent, const QString& message );

    qreal m_progress = 0.0;
    QString m_message;
};

}  // namespace ClaudeInstall

#endif
