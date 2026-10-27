/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 *   SPDX-FileCopyrightText: 2015 Rohan Garg <rohan@garg.io>
 *   SPDX-FileCopyrightText: 2016-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017 2019, Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PACKAGESJOB_H
#define PACKAGESJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "PackageManager.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantMap>

#include <atomic>

/** @brief Installs and removes packages in the target through its package manager */
class PLUGINDLLEXPORT PackagesJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit PackagesJob( QObject* parent = nullptr );
    ~PackagesJob() override;

    QString prettyName() const override;
    QString prettyStatusMessage() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    enum Mode
    {
        NoMode,
        InstallMode,
        RemoveMode
    };

    void changeMode( Mode mode );
    /// Runs the package-actions in @p entry (see packages.conf); may throw Packages::CommandFailed
    void runOperations( Packages::PackageManager& pkgman, const QVariantMap& entry );

    QVariantMap m_configuration;

    // Progress reporting; status messages are asked for from other threads
    std::atomic< int > m_totalPackages { 0 };  // For the entire job
    std::atomic< int > m_completedPackages { 0 };  // Done so far for this job
    std::atomic< int > m_groupPackages { 0 };  // One group of packages from an -install or -remove entry
    std::atomic< int > m_mode { NoMode };
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( PackagesJobFactory )

#endif  // PACKAGESJOB_H
