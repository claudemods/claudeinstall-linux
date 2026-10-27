/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TUIAPPLICATION_H
#define TUIAPPLICATION_H

#include "DllMacro.h"

#include <QObject>
#include <QString>
#include <QTimer>

#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <functional>
#include <memory>

namespace ClaudeInstall
{

class TuiModuleLoader;
class TuiWizard;

/**
 * @brief The terminal user interface of ClaudeInstall
 *
 * Owns the module loader and the FTXUI screen, and draws the wizard: the
 * list of steps at the side, the page of the current step, and the
 * keys for back / next / cancel at the bottom.
 *
 * ## One thread
 *
 * The Qt event loop of the application runs on the main thread, and so does
 * FTXUI. FTXUI's own loop is not used: a timer calls ftxui::Loop::RunOnce()
 * a few dozen times a second, which handles the keys that arrived and draws
 * the screen. That way Qt signals (from the job queue, from the requirements
 * checker, from network lookups) and key presses are handled in turn,
 * and the Config objects never need to be thread-safe on account of the UI.
 * Since signals do not tell FTXUI to redraw, the timer also asks for a
 * redraw a few times a second.
 *
 * ## Keys
 *
 * F1 is *back*, F2 is *next* and F10 is *cancel* (or *done* at the end);
 * everything else goes to the page of the current step.
 */
class TUIDLLEXPORT TuiApplication : public QObject
{
    Q_OBJECT
public:
    explicit TuiApplication( QObject* parent = nullptr );
    ~TuiApplication() override;

    /** @brief Loads the modules and starts drawing
     *
     * Needs Settings, the JobQueue and the branding to exist. Returns @c false (with
     * the reason in errorString()) if the modules could not be loaded; then nothing has
     * been drawn.
     *
     * If @p redirectStderr is set, standard error is pointed at /dev/null while the
     * screen is up, since the logger would otherwise write over the pages.
     * (Everything is still logged to the log file.)
     */
    bool start( bool redirectStderr = true );

    QString errorString() const { return m_error; }

private:
    void tick();
    ftxui::Element render();
    bool handleEvent( const ftxui::Event& event );
    void askQuestion( const QString& question, const std::function< void() >& onYes );

    TuiWizard* m_wizard = nullptr;
    TuiModuleLoader* m_loader = nullptr;

    // FTXUI wants a prvalue to initialize the screen
    ftxui::ScreenInteractive m_screen;
    ftxui::Component m_pages;
    ftxui::Component m_root;
    std::unique_ptr< ftxui::Loop > m_loop;
    int m_pageIndex = 0;
    int m_stepCount = 0;
    int m_ticks = 0;

    QTimer m_timer;
    QString m_error;

    // A yes/no question being asked, or the failure message of the installation
    QString m_question;
    std::function< void() > m_questionYes;
    QString m_failure;
};

}  // namespace ClaudeInstall

#endif
