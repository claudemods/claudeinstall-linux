/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef NETINSTALLTUISTEP_H
#define NETINSTALLTUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

#include <QModelIndex>
#include <QSet>

#include <vector>

/**
 * @brief The package-group tree of netinstall in the terminal
 *
 * Shows the tree of groups and packages from the Config's model, one row per
 * line. Up and down move; right opens and left closes a group; space
 * (un)checks. Items that cannot be changed show no box.
 */
class PLUGINDLLEXPORT NetInstallTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit NetInstallTuiStep( QObject* parent = nullptr );
    ~NetInstallTuiStep() override;

    QString prettyName() const override;

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

private:
    struct Row
    {
        QModelIndex index;
        int depth;
        bool hasChildren;
    };

    /// @brief The visible rows, from the model and what is expanded
    std::vector< Row > rows() const;
    void collect( const QModelIndex& parent, int depth, std::vector< Row >& into ) const;
    /// @brief Expands (recursively) what the model says is expanded at the start
    void expandInitial( const QModelIndex& parent );
    void toggleChecked( const QModelIndex& index );
    void nextIsReady();

    Config* m_config;
    QSet< quintptr > m_expanded;  ///< internal ids of the expanded groups
    bool m_expandedInitially = false;
    int m_cursor = 0;
    bool m_nextEnabled = false;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( NetInstallTuiStepFactory )

#endif
