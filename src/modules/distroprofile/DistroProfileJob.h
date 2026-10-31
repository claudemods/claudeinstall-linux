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

#include <QElapsedTimer>
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSet>
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
 * Package names are checked against the synced repository databases before
 * pacstrap runs, and a name that is gone is resolved instead of skipped: through
 * the rename table (profiles/package-renames.conf), the "replaces" and "provides"
 * fields of the repositories, or a close spelling (-git/-bin suffixes). What
 * cannot be resolved stops the install early with suggestions, before anything
 * is downloaded. Package lists are refreshed between attempts.
 */
class PLUGINDLLEXPORT DistroProfileJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit DistroProfileJob( QObject* parent = nullptr );
    ~DistroProfileJob() override;

    QString prettyName() const override;
    /// What the job is doing right now (updated from every step; the queue also shows it every 10 s)
    QString prettyStatusMessage() const override;

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

    /// What the synced pacman databases know: names, groups, and who provides / replaces what
    struct RepoIndex
    {
        QSet< QString > names;
        QSet< QString > groups;
        QHash< QString, QStringList > providedBy;  ///< virtual name -> packages providing it
        QHash< QString, QStringList > replacedBy;  ///< old name -> packages that replace it
    };

    bool loadRepoIndex( RepoIndex& index ) const;
    /// Finds the package that now stands for @p name, or an empty string
    QString findReplacement( const RepoIndex& index, const QString& name, const QStringList& wanted, QString& how ) const;
    /// Replaces names that left the repositories by their successors; errors when one cannot be resolved
    ClaudeInstall::JobResult resolvePackages( QStringList& packages );
    void loadRenameTable();

    QString expand( const QString& text, const Context& ctx ) const;
    bool loadProfile( const QString& id, QVariantMap& out, QString& error ) const;

    ClaudeInstall::JobResult runSteps( const QVariantList& steps, const Context& ctx );
    ClaudeInstall::JobResult runStep( const QVariantMap& step, const Context& ctx );
    ClaudeInstall::JobResult bootstrap( const QString& root, QStringList packages );
    ClaudeInstall::JobResult writeRegdom( const Context& ctx );
    void refreshHostPackageLists();
    ClaudeInstall::JobResult ensureAssets( QString& assetsDir );

    /// Say what is going on: logged, and shown in the progress view
    void setStatus( const QString& status );
    /// The status without the network speeds
    QString statusText() const;
    /// Reads the network counters (about once a second) and updates the download / upload speed shown with the status
    void sampleNetwork();
    void stopNetworkMeter();
    /// Runs a command, handing every line it prints to the status; returns its exit code and output.
    /// With @p stallSeconds, a command that in that time neither downloaded at least stallMinKiBps
    /// on average nor printed anything but download progress is stopped (returns -2).
    int runStreaming( const QStringList& cmd, QString& output, int stallSeconds = 0 );
    /// Runs all of @p cmds at the same time and returns what each one printed on stdout
    QStringList runParallel( const QList< QStringList >& cmds, int timeoutMs );
    /// Pings the internet and checks DNS; waits up to two minutes for the connection to come back
    ClaudeInstall::JobResult checkNetwork();
    /// Tests the servers of every mirror list used by pacman.conf and rewrites each one fastest first:
    /// a ping test of all servers (dead and broken links are dropped), then a real download speed test
    /// of the closest ones. Arch's list also gets the up-to-date mirrors of archlinux.org/mirrors/status.
    void rankMirrors();
    void rankMirrorFile( const QString& path, const QString& repo, const QStringList& official, const QSet< QString >& stale );
    /// mirror list path -> the repository whose database is used to test its servers
    QMap< QString, QString > mirrorRepos() const;
    /// Fully synced https mirrors from archlinux.org (best first) and the mirrors it reports as out of date
    void archMirrorsFromStatus( QStringList& good, QSet< QString >& stale );

    mutable QMutex m_statusMutex;
    QString m_status;
    QString m_netRates;          ///< "↓ Download … ↑ Upload …" while a download runs
    qint64 m_rxBytes = -1;
    qint64 m_txBytes = -1;
    qint64 m_netSampleMs = 0;
    QSet< QString > m_badHosts;  ///< mirrors that failed or stalled during this installation
    QStringList m_topHosts;      ///< the fastest server of each mirror list, as last ranked
    QElapsedTimer m_clock;
    qint64 m_lastEmitMs = -10000;
    double m_lastProgress = 0.0;

    QString m_profilesDir;
    QString m_assetsDir;
    QString m_assetsUrl;
    QString m_assetsSha256;
    QString m_regdomKey;
    QString m_profileKey;
    QString m_phase;
    QString m_regdom;
    int m_retries = 5;
    bool m_rankMirrors = true;
    int m_mirrorKeep = 10;      ///< fastest servers kept per mirror list
    int m_speedTests = 6;       ///< closest servers per list whose download speed is measured
    bool m_archMirrorStatus = true;
    int m_stallSeconds = 300;   ///< watchdog window for pacstrap (0 = never stop it)
    int m_stallMinKiBps = 20;   ///< slower than this over a whole window (and no other work) = stuck
    bool m_unresolvedIsFatal = true;
    QMap< QString, QString > m_renames;  ///< old name -> new name
    QSet< QString > m_removed;           ///< names that are gone for good and are dropped on purpose
    QVariantMap m_configuration;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( DistroProfileJobFactory )

#endif  // DISTROPROFILEJOB_H
