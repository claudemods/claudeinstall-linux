/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LCLOCALEDIALOG_H
#define LCLOCALEDIALOG_H

#include <QDialog>

class QListWidget;

class LCLocaleDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LCLocaleDialog( const QString& guessedLCLocale,
                             const QStringList& localeGenLines,
                             QWidget* parent = nullptr );

    QString selectedLCLocale();

private:
    QListWidget* m_localesWidget;
};

#endif  // LCLOCALEDIALOG_H
