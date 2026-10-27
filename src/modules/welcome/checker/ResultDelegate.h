/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2022 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef WELCOME_CHECKER_RESULTDELEGATE_HH
#define WELCOME_CHECKER_RESULTDELEGATE_HH

#include <QStyledItemDelegate>

#include "modulesystem/RequirementsModel.h"

/**
 * @brief Class for drawing (un)satisfied requirements
 */
class ResultDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    ResultDelegate( QObject* parent, ClaudeInstall::RequirementsModel::Roles text )
        : QStyledItemDelegate( parent )
        , m_textRole( text )
    {
    }

protected:
    QSize sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const override;
    void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const override;

    int m_textRole = ClaudeInstall::RequirementsModel::Name;
};

#endif  // PROGRESSTREEDELEGATE_H
