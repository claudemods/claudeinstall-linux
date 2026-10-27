/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 *
 */
#ifndef CLAUDEINSTALL_COMPAT_SIZE_H
#define CLAUDEINSTALL_COMPAT_SIZE_H

#include <QVariant>

namespace ClaudeInstall
{
/* Compatibility of size types (e.g. qsizetype, STL sizes, int) between Qt5 and Qt6 */

using NumberForTr = int;

}

#endif
