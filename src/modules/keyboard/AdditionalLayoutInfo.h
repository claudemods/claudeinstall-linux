/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2020 Artem Grinev <agrinev@manjaro.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef KEYBOARD_ADDITIONAL_LAYOUT_INFO_H
#define KEYBOARD_ADDITIONAL_LAYOUT_INFO_H

#include <QString>

struct BasicLayoutInfo
{
    QString selectedLayout;
    QString selectedModel;
    QString selectedVariant;
    QString selectedGroup;
};

struct AdditionalLayoutInfo
{
    QString additionalLayout;
    QString additionalVariant;

    QString groupSwitcher;

    QString vconsoleKeymap;
};

#endif
