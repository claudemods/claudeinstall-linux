/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2014 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2014 Daniel Hillenbrand <codeworkx@bbqlinux.org>
 *   SPDX-FileCopyrightText: 2014 Benjamin Vaudour <benjamin.vaudour@yahoo.fr>
 *   SPDX-FileCopyrightText: 2014-2019 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2015-2018 Philip Mueller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2016-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2017 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2017 Ben Green <Bezzy1999@hotmail.com>
 *   SPDX-FileCopyrightText: 2021 Neal Gompa <ngompa13@gmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef BOOTLOADER_HELPERS_H
#define BOOTLOADER_HELPERS_H

#include "DllMacro.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <memory>

namespace Bootloader
{

/** @brief Thrown for bad values (configuration, system state) that stop the installation */
struct ValueError
{
    QString message;
};

/** @brief Thrown when a command that must succeed fails (like Python's CalledProcessError) */
struct CommandFailed
{
    QStringList command;
    int exitCode = 0;
    QString output;
};

/** @brief Renders @p value in base-N, where N is the number of @p baseValues
 *
 * The items of @p baseValues are used for the digits: use "0".."9" for regular
 * decimal rendering, or letters (or words) to get letters-as-numbers 'encoding'.
 * If @p length is positive, pads out to at least that long with leading "zeroes"
 * (whatever the first of @p baseValues is).
 *
 * Throws ValueError for insufficient items.
 */
PLUGINDLLEXPORT QString renderInBase( quint64 value, const QStringList& baseValues, int length = -1 );

/** @brief Generator for unique suffixes to an EFI Id */
class PLUGINDLLEXPORT EfiSuffixGenerator
{
public:
    explicit EfiSuffixGenerator( const QString& name )
        : m_name( name )
    {
    }
    virtual ~EfiSuffixGenerator();

    /// The first call returns the name unchanged, later calls add a suffix
    QString next();

protected:
    virtual QString suffix( int counter ) = 0;

    QString m_name;
    int m_counter = -1;  // So the first call to next() will bump it to 0
};

/** @brief Handles EFI bootloader Ids with `${<something>}` for suffix-processing
 *
 * The generator name is one of SERIAL (a serial number), RANDOM (a random
 * 4-digit hex number) or PHRASE (a random phrase of space-themed words).
 *
 * Throws ValueError if the @p name does not have exactly one valid generator at its end.
 */
PLUGINDLLEXPORT std::unique_ptr< EfiSuffixGenerator > efiSuffixGenerator( const QString& name );

/** @brief A label based on @p bootloaderId that is usable within @p efiDirectory
 *
 * If there is a `${<something>}` suffix marker at the end of the id, tries
 * 10 times to generate a unique label, otherwise returns the id (also if
 * all attempts are taken).
 */
PLUGINDLLEXPORT QString changeEfiSuffix( const QString& efiDirectory, const QString& bootloaderId );

/// Tidies up @p name to be used as an identifier or filename: spaces, slashes and parentheses are replaced
PLUGINDLLEXPORT QString sanitizeFileName( const QString& name );

/** @brief A kernel found in the target */
struct Kernel
{
    QString path;  // relative to the root of the installation
    QString type;
    QString version;
};

/** @brief Gets the kernels and associated values for each kernel
 *
 * Files in the tree of @p searchPath (relative to @p installationRootPath) whose
 * names match the regular expression @p pattern are kernels; the kernel version
 * is the name of the directory they are in. This works as-is for many distros.
 */
PLUGINDLLEXPORT QList< Kernel > findKernels( const QString& installationRootPath,
                                              const QString& searchPath,
                                              const QString& pattern );

/// Finds @p name in the directory @p parent, ignoring case (because of VFAT), or the path it would be at
PLUGINDLLEXPORT QString vfatCorrectCase( const QString& parent, const QString& name );

}  // namespace Bootloader

#endif
