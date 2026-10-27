/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019-2020, Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2020, Camilo Higuita <milo.h@aol.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef SUMMARY_SUMMARYMODEL_H
#define SUMMARY_SUMMARYMODEL_H

#include "Step.h"

#include <QAbstractListModel>
#include <QObject>
#include <QVector>

class Config;

/** @brief Data for one step
 *
 * A step generally has a text description, but **may** have a
 * widget (an object that the frontend knows how to show; it is a QWidget
 * in the Widgets frontend). There is no ownership of the widget, that is
 * assumed to be handed off to some owning parent-widget.
 */
struct StepSummary
{
    QString title;
    QString message;
    QObject* widget = nullptr;
};

class SummaryModel : public QAbstractListModel
{
    Q_OBJECT
    friend class Config;

public:
    enum Roles : int
    {
        TitleRole = Qt::DisplayRole,  // Name of the step
        MessageRole = Qt::UserRole,  // String saying what it will do
        WidgetRole,  // Pointer to widget
    };

    explicit SummaryModel( QObject* parent = nullptr );
    int rowCount( const QModelIndex& = QModelIndex() ) const override;
    QVariant data( const QModelIndex& index, int role ) const override;

protected:
    QHash< int, QByteArray > roleNames() const override;

private:
    /** @brief Sets the model data from @p steps
     *
     * Replaces the list of summaries with summaries given by
     * the jobs and Step objects in @p steps. If @p withWidgets
     * is @c true, then also queries for widget summaries alongside
     * the text summaries for each step.
     */
    void setSummaryList( const ClaudeInstall::StepList& steps, bool withWidgets = false );

    QVector< StepSummary > m_summary;
};

#endif
