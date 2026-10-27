/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CLAUDEINSTALLUTILSGUI_H
#define CLAUDEINSTALLUTILSGUI_H

#include "DllMacro.h"

#include <QObject>
#include <QPixmap>
#include <QSize>

class QLayout;

namespace ClaudeInstall
{

/**
 * @brief The ImageType enum lists all common ClaudeInstall icons.
 * Icons are loaded from SVGs and cached. Each icon has an enum value, through which
 * it can be accessed.
 * You can forward-declare this as:
 * enum ImageType : int;
 */
enum ImageType : int
{
    Yes,
    No,
    Information,
    Fail,
    Bugs,
    Help,
    Release,
    Donate,
    PartitionDisk,
    PartitionPartition,
    PartitionAlongside,
    PartitionEraseAuto,
    PartitionManual,
    PartitionReplaceOs,
    PartitionTable,
    BootEnvironment,
    Squid,
    StatusOk,  // Icons for the requirements checker
    StatusWarning,
    StatusError
};

/**
 * @brief The ImageMode enum contains different transformations that can be applied.
 * Most of these are currently unused.
 */
enum ImageMode
{
    Original,
    CoverInCase,
    Grid,
    DropShadow,
};

/**
 * @brief defaultPixmap returns a resized and/or transformed pixmap for a given
 * ImageType.
 * @param type the ImageType i.e. the enum value for an SVG.
 * @param mode the transformation to apply (default: no transformation).
 * @param size the target pixmap size (default: original SVG size).
 * @return the new pixmap.
 */
UIDLLEXPORT QPixmap defaultPixmap( ImageType type,
                                   ImageMode mode = ClaudeInstall::Original,
                                   const QSize& size = QSize( 0, 0 ) );

/**
 * @brief unmarginLayout recursively walks the QLayout tree and removes all margins.
 * @param layout the layout to unmargin.
 */
UIDLLEXPORT void unmarginLayout( QLayout* layout );

UIDLLEXPORT void setDefaultFontSize( int points );
UIDLLEXPORT int defaultFontSize();  // in points
UIDLLEXPORT int defaultFontHeight();  // in pixels, DPI-specific
UIDLLEXPORT QFont largeFont();
UIDLLEXPORT QSize defaultIconSize();

/**
 * @brief Size constants for the main ClaudeInstall window.
 */
constexpr int windowMinimumWidth = 800;
constexpr int windowMinimumHeight = 520;
constexpr int windowPreferredWidth = 1024;
constexpr int windowPreferredHeight = 520;

}  // namespace ClaudeInstall

#endif  // CLAUDEINSTALLUTILSGUI_H
