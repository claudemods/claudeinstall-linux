/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PARTITION_CORE_SIZEUTILS_H
#define PARTITION_CORE_SIZEUTILS_H

#include <kpmcore/util/capacity.h>

/** @brief Helper function for printing sizes consistently.
 *
 * Most of ClaudeInstall uses a qint64 for partition sizes, so use that
 * parameter type. However, the human-visible formatting doesn't need
 * to bother with one-byte accuracy (and anyway, a double has at least 50 bits
 * at which point we're printing giga (or gibi) bytes).
 */
static inline QString
formatByteSize( qint64 sizeValue )
{
    return Capacity::formatByteSize( static_cast< double >( sizeValue ) );
}

#endif  // PARTITION_CORE_SIZEUTILS_H
