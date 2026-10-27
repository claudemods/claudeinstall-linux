/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
 *   SPDX-FileCopyrightText: 2015 Rohan Garg <rohan@garg.io>
 *   SPDX-FileCopyrightText: 2016-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017 2019, Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2017 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2017 Ben Green <Bezzy1999@hotmail.com>
 *   SPDX-FileCopyrightText: 2021 Anubhav Choudhary <ac.10edu@gmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PACKAGES_PACKAGEMANAGER_H
#define PACKAGES_PACKAGEMANAGER_H

#include "DllMacro.h"

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

namespace Packages
{

/** @brief Thrown by package managers when a command in the target fails
 *
 * This is the equivalent of Python's CalledProcessError: the commands
 * that must succeed are run through checked calls, which throw this.
 */
struct CommandFailed
{
    QStringList command;
    int exitCode = 0;
    QString output;
};

/** @brief The settings for the *pacman* backend, from the job configuration */
struct PLUGINDLLEXPORT PacmanConfig
{
    int numRetries = 0;
    bool disableDownloadTimeout = false;
    bool neededOnly = false;

    /** @brief Reads the (optional) *pacman* key from @p jobConfiguration
     *
     * If the key is present, but not a map, it is ignored (with a warning).
     */
    static PacmanConfig fromJobConfiguration( const QVariantMap& jobConfiguration );
};

/** @brief Package manager base class
 *
 * A subclass implements package management for a specific backend.
 * Commands that must succeed throw CommandFailed on failure.
 */
class PLUGINDLLEXPORT PackageManager
{
public:
    virtual ~PackageManager();

    /// The identifier of this backend, as it is used in the configuration
    virtual QString backend() const = 0;

    /** @brief Install packages into the system
     *
     * Although this handles lists, in practice it is called with one
     * package at a time. If @p fromLocal is true, then these are local
     * packages (on disk) and the names are paths.
     */
    virtual void install( const QStringList& pkgs, bool fromLocal = false ) = 0;
    virtual void remove( const QStringList& pkgs ) = 0;
    virtual void updateDb() = 0;
    virtual void updateSystem() = 0;

    /// Runs a pre- or post-script (not through a shell), unless empty
    virtual void run( const QString& script );

    /** @brief Install a package from a single entry in the install list
     *
     * The entry is either a package name, or a map with pre- and post-scripts.
     */
    void installPackage( const QVariant& packageData, bool fromLocal = false );
    void removePackage( const QVariant& packageData );

    /** @brief Installs the packages in @p packageList
     *
     * For "critical" packages, which are expected to succeed or fail all
     * together. If there are packages with scripts, they are installed one-by-one.
     * Throws CommandFailed.
     */
    void operationInstall( const QVariantList& packageList, bool fromLocal = false );
    /** @brief Installs "non-critical" packages, one-by-one; failures are only logged. */
    void operationTryInstall( const QVariantList& packageList );
    void operationRemove( const QVariantList& packageList );
    void operationTryRemove( const QVariantList& packageList );

    /// All the backends that can be created
    static QStringList backends();
    /// Creates the package manager for @p backend, or returns null if there is none
    static std::unique_ptr< PackageManager > create( const QString& backend, const QVariantMap& jobConfiguration );
};

/** @brief Localizes the list of packages
 *
 * Package names that contain LOCALE are localized with the BCP47 name of
 * @p locale; if the locale is 'en' (or empty) then these localized packages
 * are dropped from the list. The entries are strings or maps with a *package* key.
 */
PLUGINDLLEXPORT QVariantList substLocale( const QVariantList& packages, const QString& locale );

}  // namespace Packages

#endif
