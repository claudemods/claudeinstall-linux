/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PARTITIONJOBTESTS_H
#define PARTITIONJOBTESTS_H

#include "JobQueue.h"

#include "partition/KPMHelper.h"

// Qt
#include <QObject>
#include <QScopedPointer>

class QueueRunner : public QObject
{
public:
    QueueRunner( ClaudeInstall::JobQueue* queue );
    ~QueueRunner() override;

    /**
     * Synchronously runs the queue. Returns true on success
     */
    bool run();

private:
    void onFailed( const QString& message, const QString& details );
    void onFinished();
    ClaudeInstall::JobQueue* m_queue;
    bool m_finished;
    bool m_success;
};

class PartitionJobTests : public QObject
{
    Q_OBJECT
public:
    PartitionJobTests();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testPartitionTable();
    void testCreatePartition();
    void testCreatePartitionExtended();
    void testResizePartition_data();
    void testResizePartition();

private:
    QScopedPointer< Device > m_device;
    ClaudeInstall::JobQueue m_queue;
    QueueRunner m_runner;

    void queuePartitionTableCreation( PartitionTable::TableType type );
    CreatePartitionJob*
    newCreatePartitionJob( Partition* freeSpacePartition, PartitionRole, FileSystem::Type type, qint64 size );
    void refreshDevice();
};

#endif /* PARTITIONJOBTESTS_H */
