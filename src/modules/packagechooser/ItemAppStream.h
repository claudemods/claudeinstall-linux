/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef ITEMAPPSTREAM_H
#define ITEMAPPSTREAM_H

#include "PackageModel.h"

/*
 * This weird include mechanism is because an #include line is allowed
 * to consist of preprocessor-tokens, which are expanded, and then
 * the #include is *re*processed. But if it starts with < or ", then
 * preprocessor tokens are not expanded. So we build up a #include <>
 * style line with a suitable path -- if we are given a value for
 * HAVE_APPSTREAM_HEADERS, that is the directory that the AppStreamQt
 * headers live in.
 */
#define CLAUDEINSTALL_LT <
#define CLAUDEINSTALL_GT >

#ifndef HAVE_APPSTREAM_HEADERS
#define HAVE_APPSTREAM_HEADERS AppStreamQt
#endif

// clang-format off
#include CLAUDEINSTALL_LT HAVE_APPSTREAM_HEADERS/pool.h CLAUDEINSTALL_GT
#include CLAUDEINSTALL_LT HAVE_APPSTREAM_HEADERS/image.h CLAUDEINSTALL_GT
#include CLAUDEINSTALL_LT HAVE_APPSTREAM_HEADERS/screenshot.h CLAUDEINSTALL_GT
// clang-format on

#undef CLAUDEINSTALL_LT
#undef CLAUDEINSTALL_GT

/** @brief Loads an item from AppStream data.
 *
 * The @p map must have a key *appstream*. That is used as the
 * primary source of information from the AppStream cache, but
 * keys *id* and *screenshotPath* may be used to override parts
 * of the AppStream data -- so that the ID is under the control
 * of ClaudeInstall, and the screenshot can be forced to a local path
 * available on the installation medium.
 *
 * Requires AppStreamQt, if not present will return invalid
 * PackageItems.
 */
PackageItem fromAppStream( AppStream::Pool& pool, const QVariantMap& map );

#endif
