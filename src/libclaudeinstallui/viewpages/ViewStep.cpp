/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ViewStep.h"

#include <QApplication>
#include <QStyle>

namespace ClaudeInstall
{

ViewStep::ViewStep( QObject* parent )
    : Step( parent )
{
}

ViewStep::~ViewStep() {}

QWidget*
ViewStep::createSummaryWidget() const
{
    return nullptr;
}

QSize
ViewStep::widgetMargins( Qt::Orientations panelSides )
{
    Q_UNUSED( panelSides )

    // Application's default style
    const auto* s = QApplication::style();
    return QSize( s->pixelMetric( QStyle::PM_LayoutLeftMargin ), s->pixelMetric( QStyle::PM_LayoutTopMargin ) );
}

}  // namespace ClaudeInstall
