/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2019-2020 Collabora Ltd
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef MOUNTJOB_H
#define MOUNTJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

/** @brief Mounts the target partitions (and extra mounts) in a temporary root */
class PLUGINDLLEXPORT MountJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit MountJob( QObject* parent = nullptr );
    ~MountJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    /// Mount options for @p filesystem on @p partition, as a comma-separated string
    QString mountOptions( const QString& filesystem, const QVariantMap& partition, const QString& efiLocation = QString() ) const;

    /// The btrfs subvolume list to create, see mount.conf
    QVariantList btrfsSubvolumes( const QVariantList& partitions ) const;

    /// Mounts a zfs partition; returns an error message, or empty on success
    QString mountZfs( const QString& rootMountPoint, const QVariantMap& partition ) const;

    /// Mounts one partition; returns an error message, or empty on success
    QString mountPartition( const QString& rootMountPoint,
                            const QVariantMap& partition,
                            const QVariantList& partitions,
                            const QString& efiLocation,
                            QVariantList& mountOptionsList ) const;

    QVariantMap m_configuration;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( MountJobFactory )

#endif  // MOUNTJOB_H
