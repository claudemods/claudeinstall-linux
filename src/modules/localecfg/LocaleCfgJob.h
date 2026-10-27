/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2016 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2018-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LOCALECFGJOB_H
#define LOCALECFGJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QMap>
#include <QObject>
#include <QStringList>

class PLUGINDLLEXPORT LocaleCfgJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit LocaleCfgJob( QObject* parent = nullptr );
    ~LocaleCfgJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    /** @brief Does @p line look like a comment (whitespace, then a #)? */
    static bool isComment( const QString& line );

    /** @brief Extracts a locale from @p line
     *
     * The locale is the first word of the line after uncommenting, but only
     * if the line has exactly two fields (the human-readable text at the top of
     * most locale.gen files would otherwise give bogus locales).
     * @return the locale (or empty) and the uncommented, stripped line.
     */
    static QPair< QString, QString > extractLocale( const QString& line );

    /** @brief Rewrites the lines of a locale.gen, enabling locales from @p localeValues
     *
     * @p lines are the lines of the original file, including line-endings.
     * Returns the new contents of the file.
     */
    static QString rewriteLocaleGen( const QStringList& lines, const QStringList& localeValues );
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( LocaleCfgJobFactory )

#endif  // LOCALECFGJOB_H
