/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2014 Daniel Hillenbrand <codeworkx@bbqlinux.org>
 *   SPDX-FileCopyrightText: 2014 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef UNPACKFSJOB_H
#define UNPACKFSJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QList>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

/** @brief Unpacks filesystem images (or plain files) into the target using rsync
 *
 * This is the rsync-based *unpackfs* module; the *unpackfsc* module is a
 * different implementation (one item per job, without rsync).
 */
class PLUGINDLLEXPORT UnpackFSJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit UnpackFSJob( QObject* parent = nullptr );
    ~UnpackFSJob() override;

    QString prettyName() const override;
    QString prettyStatusMessage() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    struct UnpackEntry
    {
        QString source;  // Image file (or directory, or device)
        QString sourcefs;  // "file" is special
        QString destination;  // Already prefixed by rootMountPoint
        QStringList exclude;
        QString excludeFile;
        QString mountPoint;  // Where the source is mounted (or would be, for files)
        qint64 copied = 0;
        qint64 total = 0;
        int weight = 1;

        bool isFile() const { return sourcefs == QStringLiteral( "file" ); }
    };

    void setStatus( const QString& status );
    void reportProgress();

    qint64 doCount( UnpackEntry& entry ) const;
    /// Mounts the entry below @p base; returns an error message or empty
    QString doMount( UnpackEntry& entry, const QString& base ) const;
    /// Runs rsync from @p source for @p entry; returns an error message or empty
    QString fileCopy( const QString& source, UnpackEntry& entry );
    QString unpackImage( UnpackEntry& entry );

    QVariantList m_unpack;

    QList< UnpackEntry > m_entries;
    int m_totalWeight = 1;

    mutable QMutex m_statusMutex;
    QString m_status;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( UnpackFSJobFactory )

#endif  // UNPACKFSJOB_H
