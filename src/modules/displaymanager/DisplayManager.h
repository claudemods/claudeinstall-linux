/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Benjamin Vaudour <benjamin.vaudour@yahoo.fr>
 *   SPDX-FileCopyrightText: 2014-2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2014, 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2018 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2019 Dominic Hayes <ferenosdev@outlook.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef DISPLAYMANAGER_DISPLAYMANAGER_H
#define DISPLAYMANAGER_DISPLAYMANAGER_H

#include "DllMacro.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <memory>
#include <optional>

namespace DisplayManagers
{

/** @brief A problem found while configuring a display manager
 *
 * Display managers don't fail the job on the first problem, they
 * report messages which are collected in the end.
 */
struct Message
{
    QString title;
    QString details;
};
using Result = std::optional< Message >;

/** @brief Utility functions for a desktop environment
 *
 * This is independent of the *display manager*, which is what we're configuring
 * in this module, and mostly about finding out if a DE is installed.
 */
class PLUGINDLLEXPORT DesktopEnvironment
{
public:
    DesktopEnvironment( const QString& executable, const QString& desktopFile );

    /// The (sample) executable to look for; a full path inside the target
    QString executable;
    /// The name of the .desktop file (without extension)
    QString desktopFile;

    /** @brief Full path of the configured executable within @p rootMountPoint
     *
     * Searches in a semi-sensible $PATH if the executable is not absolute.
     * Returns an empty string if it isn't found.
     */
    QString findExecutable( const QString& rootMountPoint ) const;
    /// Full path of the .desktop file within @p rootMountPoint (X11 or Wayland), or empty
    QString findDesktopFile( const QString& rootMountPoint ) const;

    /// Is this environment installed in the target?
    bool isInstalled( const QString& rootMountPoint ) const;

    /** @brief Looks for this DE in the target at @p rootMountPoint
     *
     * This can update the *executable* if the configured executable isn't
     * found but the TryExec line from the .desktop file is. The .desktop
     * file is mandatory for a DE.
     *
     * @return true if the DE is installed.
     */
    bool updateFromDesktopFile( const QString& rootMountPoint );

private:
    QString searchExecutable( const QString& rootMountPoint, const QString& pathname ) const;
    QString searchTryExec( const QString& rootMountPoint, const QString& absoluteDesktopFile ) const;
};

/** @brief The desktop environments that are looked for, in order of preference */
PLUGINDLLEXPORT QList< DesktopEnvironment > knownDesktopEnvironments();

/** @brief The first of the known desktop environments that is installed in the target */
PLUGINDLLEXPORT std::optional< DesktopEnvironment > findDesktopEnvironment( const QString& rootMountPoint );

/** @brief Display Manager -- a base class for DM configuration */
class PLUGINDLLEXPORT DisplayManager
{
public:
    explicit DisplayManager( const QString& rootMountPoint );
    virtual ~DisplayManager();

    virtual QString name() const = 0;
    QString executable() const { return m_executable; }

    /** @brief Is this DM installed in the target system?
     *
     * The default implementation checks for the executable in the target.
     */
    virtual bool haveDm();

    /** @brief Applies the DM-specific settings from the job configuration
     *
     * Only keys that the DM knows about are used.
     */
    virtual void configure( const QVariantMap& settings );

    // The four methods below are called in the order listed here. Not all of
    // them actually do something for all DMs.

    /// Do basic setup (e.g. users, groups, directory creation)
    virtual Result basicSetup() = 0;
    /// Configure @p desktopEnvironment as the default one
    virtual Result desktopEnvironmentSetup( const DesktopEnvironment& desktopEnvironment ) = 0;
    /// Additional setup for the greeter
    virtual Result greeterSetup() = 0;
    /** @brief Configure autologin (if @p doAutologin) for @p username
     *
     * If the DM supports it, set the default DE to @p defaultDesktopEnvironment
     * as well (which may be null, if none was found).
     */
    virtual Result setAutologin( const QString& username,
                                 bool doAutologin,
                                 const DesktopEnvironment* defaultDesktopEnvironment )
        = 0;

    /// The names of all supported display managers
    static QStringList names();
    /// Creates the DM called @p name for the target at @p rootMountPoint, or null if there is none
    static std::unique_ptr< DisplayManager > create( const QString& name, const QString& rootMountPoint );

protected:
    QString m_rootMountPoint;
    QString m_executable;
};

}  // namespace DisplayManagers

#endif
