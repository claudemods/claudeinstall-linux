/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CLAUDEINSTALLWINDOW_H
#define CLAUDEINSTALLWINDOW_H

#include <QWidget>

#include <memory>

namespace ClaudeInstall
{
class DebugWindowManager;
class ViewManager;
}  // namespace ClaudeInstall

/**
 * @brief The ClaudeInstallWindow class represents the main window of the ClaudeInstall UI.
 */
class ClaudeInstallWindow : public QWidget
{
    Q_OBJECT
public:
    ClaudeInstallWindow( QWidget* parent = nullptr );
    ~ClaudeInstallWindow() override {}

public Q_SLOTS:
    /**
     * This asks the main window to grow to accomodate @p size pixels, to accomodate
     * larger-than-expected window contents. The enlargement may be silently
     * ignored.
     */
    void ensureSize( QSize size );

protected:
    virtual void closeEvent( QCloseEvent* e ) override;

private:
    ClaudeInstall::DebugWindowManager* m_debugManager = nullptr;
    ClaudeInstall::ViewManager* m_viewManager = nullptr;
};

#endif  // CLAUDEINSTALLWINDOW_H
