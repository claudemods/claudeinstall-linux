/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LIBCLAUDEINSTALLUI_WIDGETS_TRANSLATIONFIX_H
#define LIBCLAUDEINSTALLUI_WIDGETS_TRANSLATIONFIX_H

#include "DllMacro.h"

class QMessageBox;
class QDialogButtonBox;

namespace ClaudeInstall
{

/** @brief Fixes the labels on the standard buttons of the message box
 *
 * Updates OK / Cancel / Yes / No because there does not
 * seem to be a way to do so in the Retranslator code
 * (in libclaudeinstall) since the translated strings may come
 * from a variety of platform-plugin sources and we can't
 * guess the context.
 */
void UIDLLEXPORT fixButtonLabels( QMessageBox* );

void UIDLLEXPORT fixButtonLabels( QDialogButtonBox* );
}  // namespace ClaudeInstall

#endif
