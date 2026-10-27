/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef DISPLAYMANAGER_INIFILE_H
#define DISPLAYMANAGER_INIFILE_H

#include "DllMacro.h"

#include <QList>
#include <QPair>
#include <QString>

namespace DisplayManagers
{

/** @brief A minimal INI file, with the behavior of Python's configparser
 *
 * Used for the SDDM configuration. It reads and writes like
 * `configparser.ConfigParser(strict=False)` with case-sensitive keys, and
 * writes without spaces around the delimiter: comments are not preserved,
 * duplicate keys keep the last value, `=` and `:` both delimit keys.
 */
class PLUGINDLLEXPORT IniFile
{
public:
    /** @brief Reads @p path, adding to the current contents
     *
     * Returns false if the file can't be read, or is malformed (an option
     * before the first section, or a line that is not a section or option).
     * A missing file is an error; check for it first.
     */
    bool read( const QString& path, QString* error = nullptr );
    /// Parses @p text as the contents of an INI file
    bool parse( const QString& text, QString* error = nullptr );

    bool write( const QString& path ) const;
    QString toString() const;

    bool hasSection( const QString& section ) const;
    void addSection( const QString& section );
    bool hasOption( const QString& section, const QString& key ) const;
    QString value( const QString& section, const QString& key ) const;
    /// Sets an option; the section must already exist
    void set( const QString& section, const QString& key, const QString& value );
    void removeOption( const QString& section, const QString& key );

private:
    using Options = QList< QPair< QString, QString > >;
    using Section = QPair< QString, Options >;

    Section* findSection( const QString& name );
    const Section* findSection( const QString& name ) const;

    QList< Section > m_sections;
};

}  // namespace DisplayManagers

#endif
