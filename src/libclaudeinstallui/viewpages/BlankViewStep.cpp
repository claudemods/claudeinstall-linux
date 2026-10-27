/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */
#include "BlankViewStep.h"

#include "utils/Gui.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace ClaudeInstall
{

BlankViewStep::BlankViewStep( const QString& title,
                              const QString& description,
                              const QString& details,
                              QObject* parent )
    : ClaudeInstall::ViewStep( parent )
    , m_widget( new QWidget() )
{
    QBoxLayout* layout = new QVBoxLayout();

    constexpr int const marginWidth = 10;
    constexpr int const spacingHeight = 10;

    auto* label = new QLabel( title );
    label->setAlignment( Qt::AlignHCenter );
    label->setFont( ClaudeInstall::largeFont() );
    layout->addWidget( label );

    label = new QLabel( description );
    label->setWordWrap( true );
    label->setMargin( marginWidth );
    layout->addSpacing( spacingHeight );
    layout->addWidget( label );

    if ( !details.isEmpty() )
    {
        label = new QLabel( details );
        label->setMargin( marginWidth );
        layout->addSpacing( spacingHeight );
        layout->addWidget( label );
    }

    layout->addStretch( 1 );  // Push the rest to the top

    m_widget->setLayout( layout );
}

BlankViewStep::~BlankViewStep() {}

QString
BlankViewStep::prettyName() const
{
    return tr( "Blank Page" );
}

void
BlankViewStep::back()
{
}

void
BlankViewStep::next()
{
}

bool
BlankViewStep::isBackEnabled() const
{
    return false;
}

bool
BlankViewStep::isNextEnabled() const
{
    return false;
}

bool
BlankViewStep::isAtBeginning() const
{
    return true;
}

bool
BlankViewStep::isAtEnd() const
{
    return false;
}

QWidget*
BlankViewStep::widget()
{
    return m_widget;
}

ClaudeInstall::JobList
BlankViewStep::jobs() const
{
    return JobList();
}

}  // namespace ClaudeInstall
