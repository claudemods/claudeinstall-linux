/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2022 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CLAUDEINSTALL_CLAUDEINSTALLABOUT_H
#define CLAUDEINSTALL_CLAUDEINSTALLABOUT_H

#include "DllMacro.h"

#include <QString>

namespace ClaudeInstall
{
/** @brief Returns an about string for the application
 *
 * The about string includes a header-statement, a list of maintainer
 * addresses, and a thank-you to Blue Systems. There is on %-substitution
 * left, where you can fill in the name of the product (e.g. to say
 * "ClaudeInstall for Netrunner" or ".. for Manjaro").
 */
DLLEXPORT const QString aboutStringUntranslated();
/// @brief As above, but translated in the current ClaudeInstall language
DLLEXPORT const QString aboutString();
}  // namespace ClaudeInstall

#endif
