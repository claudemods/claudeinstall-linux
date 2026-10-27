/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2023 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 *
 */
#ifndef CLAUDEINSTALL_COMPAT_MUTEX_H
#define CLAUDEINSTALL_COMPAT_MUTEX_H

#include <QMutexLocker>

namespace ClaudeInstall
{

/*
 * In Qt5, QMutexLocker is a class and operates implicitly on
 * QMutex but in Qt6 it is a template and needs a specialization.
 */
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
using MutexLocker = QMutexLocker;
#else
using MutexLocker = QMutexLocker< QMutex >;
#endif

}  // namespace ClaudeInstall

#endif
