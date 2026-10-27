/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CLAUDEINSTALLAPPLICATION_H
#define CLAUDEINSTALLAPPLICATION_H

#include <QApplication>

class ClaudeInstallWindow;

namespace ClaudeInstall
{
class ModuleManager;
}  // namespace ClaudeInstall


/**
 * @brief The ClaudeInstallApplication class extends QApplication to handle
 * ClaudeInstall startup and lifetime of main components.
 */
class ClaudeInstallApplication : public QApplication
{
    Q_OBJECT
public:
    ClaudeInstallApplication( int& argc, char* argv[] );
    ~ClaudeInstallApplication() override;

    /**
     * @brief init handles the first part of ClaudeInstall application startup.
     * After the main window shows up, the latter part of the startup sequence
     * (including modules loading) happens asynchronously.
     */
    void init();
    static ClaudeInstallApplication* instance();

    /**
     * @brief mainWindow returns the ClaudeInstall application main window.
     */
    ClaudeInstallWindow* mainWindow();

private slots:
    void initView();
    void initViewSteps();
    void initFailed( const QStringList& l );

private:
    // Initialization steps happen in this order
    void initQmlPath();
    void initBranding();
    void initModuleManager();
    void initJobQueue();

    ClaudeInstallWindow* m_mainwindow;
    ClaudeInstall::ModuleManager* m_moduleManager;
};

#endif  // CLAUDEINSTALLAPPLICATION_H
