/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2022 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "ClaudeInstallAbout.h"

#include "ClaudeInstallVersionX.h"

#include <QCoreApplication>

static const char s_header[]
    = QT_TRANSLATE_NOOP( "AboutData", "<h1>%1</h1><br/><strong>%2<br/> for %3</strong><br/><br/>" );

static const char s_footer[]
    = QT_TRANSLATE_NOOP( "AboutData",
                         "Thanks to <a href=\"https://claudeinstall.io/team/\">the ClaudeInstall team</a> "
                         "and the <a href=\"https://app.transifex.com/claudeinstall/claudeinstall/\">ClaudeInstall "
                         "translators team</a>." );

struct Maintainer
{
    unsigned int start;
    unsigned int end;
    const char* name;
    const char* email;
    QString text() const
    {
        //: Copyright year-year Name <email-address>
        return QCoreApplication::translate( "AboutData", "Copyright %1-%2 %3 &lt;%4&gt;<br/>" )
            .arg( start )
            .arg( end )
            .arg( name )
            .arg( email );
    }
};

static constexpr const Maintainer maintainers[] = {
    { 2014, 2017, "Teo Mrnjavac", "teo@kde.org" },
    { 2017, 2022, "Adriaan de Groot", "groot@kde.org" },
    { 2022, 2024, "Adriaan de Groot (community)", "groot@kde.org" },
};

static QString
aboutMaintainers()
{
    QStringList s;
    for ( const auto& m : maintainers )
    {
        s.append( m.text() );
    }
    return s.join( QString() );
}

static QString
substituteVersions( const QString& s )
{
    return s.arg( CLAUDEINSTALL_APPLICATION_NAME ).arg( CLAUDEINSTALL_VERSION );
}

const QString
ClaudeInstall::aboutString()
{
    return substituteVersions( QCoreApplication::translate( "AboutData", s_header ) ) + aboutMaintainers()
        + QCoreApplication::translate( "AboutData", s_footer );
}

const QString
ClaudeInstall::aboutStringUntranslated()
{
    return substituteVersions( QString( s_header ) ) + aboutMaintainers() + QString( s_footer );
}
