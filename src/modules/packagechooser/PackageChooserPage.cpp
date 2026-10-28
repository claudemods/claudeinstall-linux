/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "PackageChooserPage.h"

#include "ui_page_package.h"

#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"

#include <QHash>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyledItemDelegate>

namespace
{
/** @brief Draws each choice as a tile: picture on top, name below
 *
 * The selected tile is filled blue, the one under the mouse is lightened, so the
 * grid reads at a glance instead of being a plain list squeezed to one side.
 */
class TileDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint( const QStyleOptionViewItem&, const QModelIndex& ) const override { return QSize( 200, 176 ); }

    void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const override
    {
        painter->save();
        painter->setRenderHint( QPainter::Antialiasing, true );
        painter->setRenderHint( QPainter::SmoothPixmapTransform, true );

        const QRectF card = QRectF( option.rect ).adjusted( 6, 6, -6, -6 );
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const QColor accent( 0x1f, 0x7a, 0xe0 );

        QPainterPath path;
        path.addRoundedRect( card, 10, 10 );
        QColor fill = option.palette.color( QPalette::Base );
        if ( selected )
        {
            fill = accent;
        }
        else if ( hovered )
        {
            fill = accent.lighter( 190 );
        }
        painter->fillPath( path, fill );
        painter->setPen( QPen( selected ? accent.darker( 130 ) : option.palette.color( QPalette::Mid ), selected ? 2 : 1 ) );
        painter->drawPath( path );

        // The picture, kept in proportion
        const QString file = index.data( PackageListModel::ScreenshotRole ).toString();
        QPixmap picture = m_cache.value( file );
        if ( picture.isNull() && !file.isEmpty() )
        {
            picture = QPixmap( file );
            m_cache.insert( file, picture );
        }
        const QRectF pictureArea = card.adjusted( 12, 10, -12, -50 );
        if ( !picture.isNull() )
        {
            const QPixmap scaled = picture.scaled( pictureArea.size().toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation );
            painter->drawPixmap( QPointF( pictureArea.center().x() - scaled.width() / 2.0,
                                          pictureArea.center().y() - scaled.height() / 2.0 ),
                                 scaled );
        }

        // The name, up to two lines
        QFont font = option.font;
        font.setBold( true );
        painter->setFont( font );
        painter->setPen( selected ? Qt::white : option.palette.color( QPalette::Text ) );
        const QRectF textArea( card.left() + 8, card.bottom() - 46, card.width() - 16, 40 );
        painter->drawText( textArea, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap, index.data( Qt::DisplayRole ).toString() );
        painter->restore();
    }

private:
    mutable QHash< QString, QPixmap > m_cache;
};
}  // namespace

PackageChooserPage::PackageChooserPage( PackageChooserMode mode, QWidget* parent )
    : QWidget( parent )
    , ui( new Ui::PackageChooserPage )
    , m_introduction( QString(),
                      QString(),
                      tr( "Package Selection" ),
                      tr( "Please pick a product from the list. The selected product will be installed." ) )
{
    m_introduction.screenshot = QStringLiteral( ":/images/no-selection.png" );

    ui->setupUi( this );
    CLAUDEINSTALL_RETRANSLATE( updateLabels(); );

    switch ( mode )
    {
    case PackageChooserMode::Optional:
        [[fallthrough]];
    case PackageChooserMode::Required:
        ui->products->setSelectionMode( QAbstractItemView::SingleSelection );
        break;
    case PackageChooserMode::OptionalMultiple:
        [[fallthrough]];
    case PackageChooserMode::RequiredMultiple:
        ui->products->setSelectionMode( QAbstractItemView::ExtendedSelection );
    }

    // A wrapping grid of tiles filling the page, not a list on one side
    auto* products = ui->products;
    products->setViewMode( QListView::IconMode );
    products->setFlow( QListView::LeftToRight );
    products->setWrapping( true );
    products->setResizeMode( QListView::Adjust );
    products->setMovement( QListView::Static );
    products->setUniformItemSizes( true );
    products->setSpacing( 4 );
    products->setMouseTracking( true );
    products->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    products->setFrameShape( QFrame::NoFrame );
    products->setItemDelegate( new TileDelegate( products ) );
    products->setMinimumHeight( 8 * ClaudeInstall::defaultFontHeight() );
}

void
PackageChooserPage::currentChanged( const QModelIndex& index )
{
    if ( !index.isValid() || !ui->products->selectionModel()->hasSelection() )
    {
        ui->productName->setText( m_introduction.name.get() );
        ui->productScreenshot->setPixmap( QPixmap( m_introduction.screenshot ) );
        ui->productDescription->setText( m_introduction.description.get() );
    }
    else
    {
        const auto* model = ui->products->model();

        ui->productName->setText( model->data( index, PackageListModel::NameRole ).toString() );
        ui->productDescription->setText( model->data( index, PackageListModel::DescriptionRole ).toString() );

        QPixmap currentScreenshot( model->data( index, PackageListModel::ScreenshotRole ).toString() );
        if ( currentScreenshot.isNull() )
        {
            ui->productScreenshot->setPixmap( QPixmap( m_introduction.screenshot ) );
        }
        else
        {
            ui->productScreenshot->setPixmap( currentScreenshot );
        }
    }
}

void
PackageChooserPage::updateLabels()
{
    if ( ui && ui->products && ui->products->selectionModel() )
    {
        currentChanged( ui->products->selectionModel()->currentIndex() );
    }
    else
    {
        currentChanged( QModelIndex() );
    }
    emit selectionChanged();
}

void
PackageChooserPage::setModel( QAbstractItemModel* model )
{
    ui->products->setModel( model );
    currentChanged( QModelIndex() );
    connect( ui->products->selectionModel(),
             &QItemSelectionModel::selectionChanged,
             this,
             &PackageChooserPage::updateLabels );
}

void
PackageChooserPage::setSelection( const QModelIndex& index )
{
    if ( index.isValid() )
    {
        ui->products->selectionModel()->select( index, QItemSelectionModel::Select );
    }
    currentChanged( index );
}

bool
PackageChooserPage::hasSelection() const
{
    return ui && ui->products && ui->products->selectionModel() && ui->products->selectionModel()->hasSelection();
}

QStringList
PackageChooserPage::selectedPackageIds() const
{
    if ( !( ui && ui->products && ui->products->selectionModel() ) )
    {
        return QStringList();
    }

    const auto* model = ui->products->model();
    QStringList ids;
    for ( const auto& index : ui->products->selectionModel()->selectedIndexes() )
    {
        QString pid = model->data( index, PackageListModel::IdRole ).toString();
        if ( !pid.isEmpty() )
        {
            ids.append( pid );
        }
    }
    return ids;
}

void
PackageChooserPage::setIntroduction( const PackageItem& item )
{
    m_introduction.name = item.name;
    m_introduction.description = item.description;
    m_introduction.screenshot = item.screenshot;
}
