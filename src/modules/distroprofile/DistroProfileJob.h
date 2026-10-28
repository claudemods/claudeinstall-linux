/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef DISTROPROFILEJOB_H
#define DISTROPROFILEJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QMap>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

/** @brief Sets a system up from a distro profile (what the claudemods scripts did)
 *
 * A profile (profilesDir/<id>.conf) names the packages to bootstrap and a list
 * of setup steps. The job has two phases, chosen with the *phase* setting so the
 * same module can be used twice in settings.conf:
 *
 *  - *install*: runs the profile's *install* steps on the host, then bootstraps
 *    the packages into the target with pacstrap. Replaces unpackfs.
 *  - *customize*: runs the profile's *customize* steps (files, services,
 *    downloads, commands, commands as the new user) inside the target. Run it
 *    after the users module.
 *
 * Package names that no longer exist are left out and logged, and the package
 * lists are refreshed between attempts, so a profile does not rot when the
 * repositories change.
 */
class PLUGINDLLEXPORT DistroProfileJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit DistroProfileJob( QObject* parent = nullptr );
    ~DistroProfileJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    struct Context
    {
        QString root;
        QString user;
        QString assets;
        QMap< QString, QString > extra;  ///< further @NAME@ placeholders
    };

    QString expand( const QString& text, const Context& ctx ) const;
    bool loadProfile( const QString& id, QVariantMap& out, QString& error ) const;

    ClaudeInstall::JobResult runSteps( const QVariantList& steps, const Context& ctx );
    ClaudeInstall::JobResult runStep( const QVariantMap& step, const Context& ctx );
    ClaudeInstall::JobResult bootstrap( const QString& root, QStringList packages );
    ClaudeInstall::JobResult writeRegdom( const Context& ctx );
    void refreshHostPackageLists() const;
    ClaudeInstall::JobResult ensureAssets( QString& assetsDir );

    QString m_profilesDir;
    QString m_assetsDir;
    QString m_assetsUrl;
    QString m_assetsSha256;
    QString m_regdomKey;
    QString m_profileKey;
    QString m_phase;
    QString m_regdom;
    int m_retries = 3;
    QVariantMap m_configuration;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( DistroProfileJobFactory )

#endif  // DISTROPROFILEJOB_H
