/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ProgressTreeView.h"

#include "ProgressTreeDelegate.h"

#include "Branding.h"
#include "ViewManager.h"

ProgressTreeView::ProgressTreeView( QWidget* parent )
    : QListView( parent )
{
    this->setObjectName( "sidebarMenuApp" );
    setFrameShape( QFrame::NoFrame );
    setContentsMargins( 0, 0, 0, 0 );

    setSelectionMode( QAbstractItemView::NoSelection );
    setDragDropMode( QAbstractItemView::NoDragDrop );
    setAcceptDrops( false );

    setItemDelegate( new ProgressTreeDelegate( this ) );

    QPalette plt = palette();
    plt.setColor( QPalette::Base,
                  ClaudeInstall::Branding::instance()->styleString( ClaudeInstall::Branding::SidebarBackground ) );
    setPalette( plt );
}


ProgressTreeView::~ProgressTreeView() {}


void
ProgressTreeView::setModel( QAbstractItemModel* model )
{
    if ( ProgressTreeView::model() )
    {
        return;
    }

    QListView::setModel( model );

    connect( ClaudeInstall::ViewManager::instance(),
             &ClaudeInstall::ViewManager::currentStepChanged,
             this,
             &ProgressTreeView::update,
             Qt::UniqueConnection );
}

void
ProgressTreeView::update()
{
    viewport()->update();
}
