/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef DLLMACRO_H
#define DLLMACRO_H

#ifndef CLAUDEINSTALL_EXPORT
#define CLAUDEINSTALL_EXPORT __attribute__( ( visibility( "default" ) ) )
#endif

/*
 * Mark symbols exported from ClaudeInstall non-GUI library with DLLEXPORT.
 * These are the public API of libclaudeinstall.
 */
#ifndef DLLEXPORT
#if defined( DLLEXPORT_PRO )
#define DLLEXPORT CLAUDEINSTALL_EXPORT
#else
#define DLLEXPORT
#endif
#endif

/*
 * Mark symbols exported from the ClaudeInstall frontend-neutral wizard
 * library (steps, wizard navigation, execution model) with STEPDLLEXPORT.
 * These are the public API of libclaudeinstallstep.
 */
#ifndef STEPDLLEXPORT
#if defined( STEPDLLEXPORT_PRO )
#define STEPDLLEXPORT CLAUDEINSTALL_EXPORT
#else
#define STEPDLLEXPORT
#endif
#endif

/*
 * Mark symbols exported from ClaudeInstall GUI library with DLLEXPORT.
 * These are the public API of libclaudeinstallui.
 */
#ifndef UIDLLEXPORT
#if defined( UIDLLEXPORT_PRO )
#define UIDLLEXPORT CLAUDEINSTALL_EXPORT
#else
#define UIDLLEXPORT
#endif
#endif

/*
 * Mark symbols exported from the ClaudeInstall text-mode (TUI) library with
 * TUIDLLEXPORT. These are the public API of libclaudeinstalltui.
 */
#ifndef TUIDLLEXPORT
#if defined( TUIDLLEXPORT_PRO )
#define TUIDLLEXPORT CLAUDEINSTALL_EXPORT
#else
#define TUIDLLEXPORT
#endif
#endif

/*
 * Mark symbols exported from ClaudeInstall C++ plugins with PLUGINDLLEXPORT.
 * These are the public API of the libraries (generally, the plugin
 * entry point)
 */
#ifndef PLUGINDLLEXPORT
#if defined( PLUGINDLLEXPORT_PRO )
#define PLUGINDLLEXPORT CLAUDEINSTALL_EXPORT
#else
#define PLUGINDLLEXPORT
#endif
#endif

/*
 * For functions that should be static in production but also need to
 * be tested, use STATICTEST as linkage specifier. When built as part
 * of a test, the function will be given normal linkage.
 */
#ifndef STATICTEST
#if defined( BUILD_AS_TEST )
#define STATICTEST
#else
#define STATICTEST static
#endif
#endif

#endif
