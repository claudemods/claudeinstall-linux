/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2022 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ResultDelegate.h"

#include "modulesystem/RequirementsModel.h"
#include "utils/Gui.h"

#include <QApplication>
#include <QPainter>

static constexpr int const item_margin = 8;
static inline int
item_fontsize()
{
    return ClaudeInstall::defaultFontSize() + 4;
}

static void
paintRequirement( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index, int role )
{
    const auto fontsize = item_fontsize();

    QRect textRect = option.rect.adjusted( item_margin, item_margin, -item_margin, -item_margin );
    QFont font = qApp->font();
    font.setPointSize( fontsize );
    font.setBold( false );
    painter->setFont( font );

    ClaudeInstall::ImageType statusImage = ClaudeInstall::StatusOk;

    painter->setPen( option.palette.text().color() );
    if ( index.data( ClaudeInstall::RequirementsModel::Satisfied ).toBool() )
    {
        painter->fillRect( textRect, option.palette.window().color() );
    }
    else
    {
        if ( index.data( ClaudeInstall::RequirementsModel::Mandatory ).toBool() )
        {
            QColor bgColor = option.palette.window().color();
            bgColor.setHsv( 0, 64, bgColor.value() );
            painter->fillRect( option.rect, bgColor );
            statusImage = ClaudeInstall::StatusError;
        }
        else
        {
            QColor bgColor = option.palette.window().color();
            bgColor.setHsv( 60, 64, bgColor.value() );
            painter->fillRect( option.rect, bgColor );
            statusImage = ClaudeInstall::StatusWarning;
        }
    }

    auto image
        = ClaudeInstall::defaultPixmap( statusImage, ClaudeInstall::Original, QSize( 2 * fontsize, 2 * fontsize ) ).toImage();
    painter->drawImage( textRect.topLeft(), image );

    // Leave space for that image (already drawn)
    textRect.moveLeft( 2 * fontsize + 2 * item_margin );
    painter->drawText( textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, index.data( role ).toString() );
}

QSize
ResultDelegate::sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    if ( !index.isValid() )
    {
        return option.rect.size();
    }

    QFont font = qApp->font();

    font.setPointSize( item_fontsize() );
    QFontMetrics fm( font );

    const int height = fm.height() + 2 * item_margin;
    int textwidth = fm.boundingRect( index.data( ClaudeInstall::RequirementsModel::NegatedText ).toString() ).width();

    return QSize( qMax( option.rect.width(), textwidth ), height );
}

void
ResultDelegate::paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    QStyleOptionViewItem opt = option;

    painter->save();

    initStyleOption( &opt, index );
    opt.text.clear();

    paintRequirement( painter, opt, index, m_textRole );

    painter->restore();
}
