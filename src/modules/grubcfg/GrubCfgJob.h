/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2019 Bernhard Landauer <oberon@manjaro.org>
 *   SPDX-FileCopyrightText: 2020 Camilo Higuita <milo.h@aol.com>
 *   SPDX-FileCopyrightText: 2022 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef GRUBCFGJOB_H
#define GRUBCFGJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QList>
#include <QObject>
#include <QPair>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

/** @brief Configures /etc/default/grub (or a grub.d snippet) in the target */
class PLUGINDLLEXPORT GrubCfgJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit GrubCfgJob( QObject* parent = nullptr );
    ~GrubCfgJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    /// Name/value pairs for the grub config file, in insertion order (like a Python dict)
    using ConfigItems = QList< QPair< QString, QString > >;

    ClaudeInstall::JobResult modifyGrubDefault( const QVariantList& partitions,
                                            const QString& rootMountPoint,
                                            QString distributor ) const;

    /** @brief Figures out where to put the grub config; creates the directory if needed.
     *  @return the full path, or an empty string if the directory could not be created.
     */
    QString grubConfigPath( const QString& rootMountPoint ) const;

    /// Updates the existing file @p defaultGrub with @p items (updating or appending)
    static bool updateExistingConfig( const QString& defaultGrub, ConfigItems items );

    QVariantMap m_configuration;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( GrubCfgJobFactory )

#endif  // GRUBCFGJOB_H
