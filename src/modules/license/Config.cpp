/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2015 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2015 Alexandre Arnt <qtgzmanager@gmail.com>
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Config.h"

#include "GlobalStorage.h"
#include "JobQueue.h"

#include "compat/Variant.h"
#include "utils/Logger.h"
#include "utils/Variant.h"

#include <utility>

const NamedEnumTable< LicenseEntry::Type >&
LicenseEntry::typeNames()
{
    static const NamedEnumTable< LicenseEntry::Type > names {
        { QStringLiteral( "software" ), LicenseEntry::Type::Software },
        { QStringLiteral( "driver" ), LicenseEntry::Type::Driver },
        { QStringLiteral( "gpudriver" ), LicenseEntry::Type::GpuDriver },
        { QStringLiteral( "browserplugin" ), LicenseEntry::Type::BrowserPlugin },
        { QStringLiteral( "codec" ), LicenseEntry::Type::Codec },
        { QStringLiteral( "package" ), LicenseEntry::Type::Package }
    };

    return names;
}

LicenseEntry::LicenseEntry( const QVariantMap& conf )
{
    if ( !conf.contains( "id" ) || !conf.contains( "name" ) || !conf.contains( "url" ) )
    {
        return;
    }

    m_id = conf[ "id" ].toString();
    m_prettyName = conf[ "name" ].toString();
    m_prettyVendor = conf.value( "vendor" ).toString();
    m_url = QUrl( conf[ "url" ].toString() );

    m_required = ClaudeInstall::getBool( conf, "required", false );
    m_expand = ClaudeInstall::getBool( conf, "expand", false );

    bool ok = false;
    QString typeString = conf.value( "type", "software" ).toString();
    m_type = typeNames().find( typeString, ok );
    if ( !ok )
    {
        cWarning() << "License entry" << m_id << "has unknown type" << typeString << "(using 'software')";
    }
}

bool
LicenseEntry::isLocal() const
{
    return m_url.isLocalFile();
}

Config::Config( QObject* parent )
    : QObject( parent )
{
    updateGlobalStorage( false );  // Have not agreed yet
}

void
Config::updateGlobalStorage( bool v )
{
    auto* queue = ClaudeInstall::JobQueue::instance();
    if ( queue && queue->globalStorage() )
    {
        queue->globalStorage()->insert( "licenseAgree", v );
    }
}

void
Config::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_entries.clear();
    if ( configurationMap.contains( "entries" )
         && ClaudeInstall::typeOf( configurationMap.value( "entries" ) ) == ClaudeInstall::ListVariantType )
    {
        const auto entries = configurationMap.value( "entries" ).toList();
        for ( const QVariant& entryV : entries )
        {
            if ( ClaudeInstall::typeOf( entryV ) != ClaudeInstall::MapVariantType )
            {
                continue;
            }

            LicenseEntry entry( entryV.toMap() );
            if ( entry.isValid() )
            {
                m_entries.append( entry );
            }
        }
    }

    m_allLicensesOptional = true;
    for ( const LicenseEntry& entry : std::as_const( m_entries ) )
    {
        m_allLicensesOptional &= !entry.isRequired();
    }
    emit entriesChanged();

    // New licenses to accept: nothing has been accepted yet
    setAccepted( false );
}

void
Config::setAccepted( bool accepted )
{
    updateGlobalStorage( accepted );

    const bool changed = accepted != m_accepted;
    m_accepted = accepted;
    m_isNextEnabled = accepted || m_allLicensesOptional;

    if ( changed )
    {
        emit acceptedChanged( m_accepted );
    }
    emit isNextEnabledChanged( m_isNextEnabled );
}
