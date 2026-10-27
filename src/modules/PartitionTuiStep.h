/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PARTITIONTUISTEP_H
#define PARTITIONTUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

#include <QList>

#include <string>
#include <vector>

class Device;
class PartitionCoreModule;

template < typename T >
class QFutureWatcher;

/**
 * @brief The partitioning page in the terminal: automatic partitioning only
 *
 * The page offers what the GUI calls *erase disk*: choose a disk, choose
 * the swap and the filesystem, and optionally encrypt; the whole disk is
 * then partitioned automatically. The other choices of the GUI (install
 * alongside another system, replace a partition, partition manually) are
 * not available in the terminal.
 *
 * The page works on the same PartitionCoreModule and Config as the GUI,
 * and applies its choice in the same way (PartitionActions::doAutopartition())
 * when the step is left, which is also when the summary and the jobs get to
 * see the result.
 */
class PLUGINDLLEXPORT PartitionTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit PartitionTuiStep( QObject* parent = nullptr );
    ~PartitionTuiStep() override;

    QString prettyName() const override;
    QString prettyStatus() const override;

    ftxui::Component createComponent() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;
    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

    void onActivate() override;
    void onLeave() override;
    void setConfigurationMap( const QVariantMap& configurationMap ) override;

    ClaudeInstall::RequirementsList checkRequirements() override;

private:
    void continueLoading();
    Device* selectedDevice() const;
    /// @brief Why the chosen disk cannot be erased (empty if it can)
    QString eraseProblem() const;
    /// @brief The passphrase is wanted, and is (not) confirmed
    bool encryptionWanted() const;
    bool encryptionConfirmed() const;
    bool calculateNextEnabled() const;
    void updateNext();
    /// @brief Sets up the partitioning actions for the choices made
    void apply();

    Config* m_config;
    PartitionCoreModule* m_core;
    QFutureWatcher< void >* m_future = nullptr;
    bool m_ready = false;  ///< the devices have been scanned
    bool m_nextEnabled = false;

    // The lists to choose from, and what is chosen
    std::vector< std::string > m_devices;
    std::vector< std::string > m_swapNames;
    QList< Config::SwapChoice > m_swapChoices;
    std::vector< std::string > m_filesystems;
    int m_device = 0;
    int m_swap = 0;
    int m_filesystem = 0;

    bool m_encrypt = false;
    std::string m_passphrase;
    std::string m_passphraseAgain;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( PartitionTuiStepFactory )

#endif
