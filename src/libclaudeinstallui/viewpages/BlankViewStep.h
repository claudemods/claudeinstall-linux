/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef BLANKVIEWSTEP_H
#define BLANKVIEWSTEP_H

#include "viewpages/ViewStep.h"

namespace ClaudeInstall
{

/** @brief A "blank" view step, used for error and status reporting
 *
 * This view step never allows navigation (forward or back); it's a trap.
 * It displays a title and explanation, and optional details.
 */
class BlankViewStep : public ClaudeInstall::ViewStep
{
    Q_OBJECT

public:
    explicit BlankViewStep( const QString& title,
                            const QString& description,
                            const QString& details = QString(),
                            QObject* parent = nullptr );
    ~BlankViewStep() override;

    QString prettyName() const override;

    QWidget* widget() override;

    void next() override;
    void back() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

private:
    QWidget* m_widget;
};

}  // namespace ClaudeInstall
#endif  // BLANKVIEWSTEP_H
