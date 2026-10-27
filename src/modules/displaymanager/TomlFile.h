/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef DISPLAYMANAGER_TOMLFILE_H
#define DISPLAYMANAGER_TOMLFILE_H

#include "DllMacro.h"

#include <QString>
#include <QVariantMap>

namespace DisplayManagers
{

/** @brief Reading and writing of (simple) TOML files
 *
 * This is used for the greetd configuration, which uses only a handful of
 * tables with string, number and boolean values. The supported TOML is:
 * tables (`[a.b]`), (dotted) bare and quoted keys, basic and literal strings,
 * integers, floats, booleans, arrays and inline tables, and comments.
 *
 * Multi-line strings, arrays of tables (`[[a]]`) and dates are **not**
 * supported: parsing a file that uses them fails, rather than silently
 * dropping data, so that a caller will not overwrite the file.
 *
 * Tables are QVariantMaps (so written in sorted order), arrays are QVariantLists.
 */
namespace Toml
{

/** @brief Parses @p text into @p result
 *
 * Returns false and sets @p error (if given) when the text is not valid or
 * not supported.
 */
PLUGINDLLEXPORT bool parse( const QString& text, QVariantMap& result, QString* error = nullptr );

/// Reads and parses the file at @p path
PLUGINDLLEXPORT bool load( const QString& path, QVariantMap& result, QString* error = nullptr );

/// Formats @p table as TOML: top-level values first, then the sub-tables
PLUGINDLLEXPORT QString dump( const QVariantMap& table );

/// Writes @p table to the file at @p path
PLUGINDLLEXPORT bool save( const QString& path, const QVariantMap& table );

}  // namespace Toml
}  // namespace DisplayManagers

#endif
