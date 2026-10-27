/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Rohan Garg <rohan@kde.org>
 *   SPDX-FileCopyrightText: 2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2016 David McKinney <mckinney@subgraph.com>
 *   SPDX-FileCopyrightText: 2016 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Bernhard Landauer <oberon@manjaro.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef INITCPIOCFGJOB_H
#define INITCPIOCFGJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

/** @brief Writes /etc/mkinitcpio.conf for the target, based on the host one. */
class PLUGINDLLEXPORT InitcpioCfgJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    /// The lists written into mkinitcpio.conf
    struct Features
    {
        QStringList hooks;
        QStringList modules;
        QStringList files;
        QStringList binaries;
    };

    explicit InitcpioCfgJob( QObject* parent = nullptr );
    ~InitcpioCfgJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

    /** @brief Works out which features the target needs for @p partitions
     *
     * Runs commands in the target to detect systemd and plymouth.
     */
    Features findFeatures( const QVariantList& partitions, const QString& rootMountPoint ) const;

private:
    QString m_source;  // host file to base the target on
    bool m_useSystemdHook = false;
    QStringList m_hooksPrepend;
    QStringList m_hooksAppend;
    QStringList m_hooksRemove;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( InitcpioCfgJobFactory )

#endif  // INITCPIOCFGJOB_H
