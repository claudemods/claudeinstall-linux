/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 2020, Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef WELCOME_SCREENCHECK_H
#define WELCOME_SCREENCHECK_H

#include "utils/Gui.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSize>

/** @brief Can the biggest single screen show the whole installer window?
 *
 * This is the GUI frontends' answer to the *screen* requirement; pass it to
 * GeneralRequirements::setScreenCheck(). It lives in a header, apart from
 * the (headless) requirements checker, because it needs QtGui.
 */
inline bool
screenIsLargeEnough()
{
    QSize biggest;
    for ( const auto* screen : QGuiApplication::screens() )
    {
        QSize thisScreen = screen->availableSize();
        if ( !biggest.isValid() || ( biggest.width() * biggest.height() < thisScreen.width() * thisScreen.height() ) )
        {
            biggest = thisScreen;
        }
    }
    return biggest.isValid() && ( biggest.width() >= ClaudeInstall::windowMinimumWidth )
        && ( biggest.height() >= ClaudeInstall::windowMinimumHeight );
}

#endif
