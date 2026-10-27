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

#include "LocaleCfgJob.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Logger.h"
#include "utils/System.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <utility>

static const QString enUsLocale = QStringLiteral( "en_US.UTF-8" );

LocaleCfgJob::LocaleCfgJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

LocaleCfgJob::~LocaleCfgJob() {}

QString
LocaleCfgJob::prettyName() const
{
    return tr( "Configuring locales…", "@status" );
}

bool
LocaleCfgJob::isComment( const QString& line )
{
    static const QRegularExpression re( QStringLiteral( "^ *#" ) );
    return re.match( line ).hasMatch();
}

QPair< QString, QString >
LocaleCfgJob::extractLocale( const QString& line )
{
    static const QRegularExpression leadingComment( QStringLiteral( "^ *#" ) );
    static const QRegularExpression trailingComment( QStringLiteral( "#.*$" ) );
    static const QRegularExpression whitespace( QStringLiteral( "\\s+" ) );

    // Remove leading spaces and comment sign
    QString s = line;
    s.remove( leadingComment );
    const QString uncommented = s.trimmed();

    QString withoutTrailing = uncommented;
    withoutTrailing.remove( trailingComment );
    const QStringList fields = withoutTrailing.trimmed().split( whitespace, Qt::SkipEmptyParts );
    if ( fields.length() != 2 )
    {
        // Not exactly two fields, can't be a proper locale line
        return { QString(), uncommented };
    }
    return { fields.first(), uncommented };
}

QString
LocaleCfgJob::rewriteLocaleGen( const QStringList& lines, const QStringList& localeValues )
{
    // Ordered, because the order of enabling is the order in the source file
    QList< QPair< QString, QString > > enabledLocales;
    QSet< QString > seenLocales;

    QString out;
    for ( const QString& line : lines )
    {
        const bool c = isComment( line );
        const auto extracted = extractLocale( line );
        const QString& locale = extracted.first;
        const QString& uncommented = extracted.second;

        // Non-comment lines are preserved, and comment lines
        // may be enabled if they match a desired locale
        if ( !c )
        {
            seenLocales.insert( locale );
        }
        else
        {
            for ( const QString& value : localeValues )
            {
                if ( locale.startsWith( value ) )
                {
                    bool found = false;
                    for ( auto& e : enabledLocales )
                    {
                        if ( e.first == locale )
                        {
                            e.second = uncommented;
                            found = true;
                        }
                    }
                    if ( !found )
                    {
                        enabledLocales.append( { locale, uncommented } );
                    }
                }
            }
        }
        out += line;
    }

    out += QStringLiteral( "\n###\n#\n# Locales enabled by ClaudeInstall\n" );
    for ( const auto& e : enabledLocales )
    {
        if ( !seenLocales.contains( e.first ) )
        {
            out += e.second + '\n';
            seenLocales.insert( e.first );
        }
    }
    for ( const QString& locale : localeValues )
    {
        if ( !seenLocales.contains( locale ) )
        {
            out += QStringLiteral( "# Missing: %1\n" ).arg( locale );
        }
    }
    return out;
}

/** @brief Splits @p text into lines, keeping the line-endings. */
static QStringList
splitLinesKeepEnds( const QString& text )
{
    QStringList lines;
    int start = 0;
    while ( start < text.length() )
    {
        int nl = text.indexOf( '\n', start );
        if ( nl < 0 )
        {
            lines << text.mid( start );
            break;
        }
        lines << text.mid( start, nl - start + 1 );
        start = nl + 1;
    }
    return lines;
}

static bool
writeFile( const QString& path, const QByteArray& contents )
{
    QFile f( path );
    return f.open( QIODevice::WriteOnly | QIODevice::Truncate ) && f.write( contents ) == contents.size();
}

ClaudeInstall::JobResult
LocaleCfgJob::exec()
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();

    QMap< QString, QString > localeConf;
    const QVariantMap stored = gs->value( "localeConf" ).toMap();
    for ( auto it = stored.constBegin(); it != stored.constEnd(); ++it )
    {
        localeConf.insert( it.key(), it.value().toString() );
    }
    if ( localeConf.isEmpty() )
    {
        for ( const char* key : { "LANG",
                                  "LC_NUMERIC",
                                  "LC_TIME",
                                  "LC_MONETARY",
                                  "LC_PAPER",
                                  "LC_NAME",
                                  "LC_ADDRESS",
                                  "LC_TELEPHONE",
                                  "LC_MEASUREMENT",
                                  "LC_IDENTIFICATION" } )
        {
            localeConf.insert( key, enUsLocale );
        }
    }

    const QString installPath = gs->value( "rootMountPoint" ).toString();
    if ( installPath.isEmpty() )
    {
        cWarning() << "rootMountPoint is empty";
        return ClaudeInstall::JobResult::error(
            tr( "Configuration Error" ), tr( "No root mount point is given for <pre>%1</pre> to use." ).arg( "localecfg" ) );
    }

    const QString targetLocaleGen = installPath + QStringLiteral( "/etc/locale.gen" );
    const QString targetLocaleGenBak = targetLocaleGen + QStringLiteral( ".bak" );
    const QString targetLocaleConfPath = installPath + QStringLiteral( "/etc/locale.conf" );
    const QString targetEtcDefaultPath = installPath + QStringLiteral( "/etc/default" );

    // restore backup if available
    if ( QFileInfo::exists( targetLocaleGenBak ) )
    {
        QFile::remove( targetLocaleGen );
        if ( QFile::copy( targetLocaleGenBak, targetLocaleGen ) )
        {
            cDebug() << "Restored backup" << targetLocaleGenBak << "->" << targetLocaleGen;
        }
    }

    // run locale-gen if detected in the host system
    if ( QFileInfo::exists( QStringLiteral( "/etc/locale.gen" ) ) )
    {
        QStringList lines;
        {
            // A missing source file is fine: assume it is empty
            QFile gen( targetLocaleGen );
            if ( gen.open( QIODevice::ReadOnly ) )
            {
                lines = splitLinesKeepEnds( QString::fromUtf8( gen.readAll() ) );
            }
        }

        // We want unique values, so there should be 1 or 2 of them; always enable en_US as well
        QSet< QString > uniqueValues;
        for ( const QString& v : std::as_const( localeConf ) )
        {
            uniqueValues.insert( v );
        }
        uniqueValues.insert( enUsLocale );
        QStringList localeValues = uniqueValues.values();
        localeValues.sort();

        if ( !writeFile( targetLocaleGen, rewriteLocaleGen( lines, localeValues ).toUtf8() ) )
        {
            return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                                tr( "Could not write <pre>%1</pre>." ).arg( targetLocaleGen ) );
        }
        ClaudeInstall::System::instance()->targetEnvCall( { "locale-gen" } );
        cDebug() << targetLocaleGen << "done";
    }

    QByteArray localeConfText;
    for ( auto it = localeConf.constBegin(); it != localeConf.constEnd(); ++it )
    {
        localeConfText += it.key().toUtf8() + '=' + it.value().toUtf8() + '\n';
    }

    // write /etc/locale.conf
    if ( !writeFile( targetLocaleConfPath, localeConfText ) )
    {
        return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                            tr( "Could not write <pre>%1</pre>." ).arg( targetLocaleConfPath ) );
    }
    cDebug() << targetLocaleConfPath << "done";

    // write /etc/default/locale if /etc/default exists and is a dir
    if ( QFileInfo( targetEtcDefaultPath ).isDir() )
    {
        const QString path = QDir( targetEtcDefaultPath ).filePath( "locale" );
        if ( !writeFile( path, localeConfText ) )
        {
            return ClaudeInstall::JobResult::error( tr( "Configuration Error" ),
                                                tr( "Could not write <pre>%1</pre>." ).arg( path ) );
        }
        cDebug() << targetEtcDefaultPath << "done";
    }

    return ClaudeInstall::JobResult::ok();
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( LocaleCfgJobFactory, registerPlugin< LocaleCfgJob >(); )
