/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2022 Bob van der Linden <bobvanderlinden@gmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LIBCLAUDEINSTALLUI_LOGWIDGET_H
#define LIBCLAUDEINSTALLUI_LOGWIDGET_H

#include <QPlainTextEdit>
#include <QThread>
#include <QWidget>

namespace ClaudeInstall
{

class LogThread : public QThread
{
    Q_OBJECT

    void run() override;

public:
    explicit LogThread( QObject* parent = nullptr );
    ~LogThread() override;

Q_SIGNALS:
    void onLogChunk( const QString& logChunk );
};

class LogWidget : public QWidget
{
    Q_OBJECT

    QPlainTextEdit* m_text;
    LogThread m_log_thread;

public:
    explicit LogWidget( QWidget* parent = nullptr );

public Q_SLOTS:
    /// @brief Called by the thread when there is new data
    void handleLogChunk( const QString& logChunk );

    /// @brief Stop watching for log data
    void stop();
    /// @brief Start watching for new log data
    void start();
};

}  // namespace ClaudeInstall
#endif  // LOGWIDGET_H
