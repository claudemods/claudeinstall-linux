/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "BrandingInfo.h"

#include "GlobalStorage.h"
#include "compat/Variant.h"
#include "utils/Logger.h"
#include "utils/Variant.h"
#include "utils/Yaml.h"

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>

#include <KMacroExpander>
#include <KOSRelease>

namespace ClaudeInstall
{

// The order is that of BrandingInfo::StringEntry
static const char* const s_entryKeys[] = { "productName",
                                           "version",
                                           "shortVersion",
                                           "versionedName",
                                           "shortVersionedName",
                                           "shortProductName",
                                           "bootloaderEntryName",
                                           "productUrl",
                                           "supportUrl",
                                           "knownIssuesUrl",
                                           "releaseNotesUrl",
                                           "donateUrl" };
static constexpr int s_entryCount = int( sizeof( s_entryKeys ) / sizeof( s_entryKeys[ 0 ] ) );
static_assert( s_entryCount == int( BrandingInfo::DonateUrl ) + 1, "Branding entries out of sync" );

BrandingInfo&
BrandingInfo::instance()
{
    static BrandingInfo s_instance;
    return s_instance;
}

QString
BrandingInfo::entryKey( StringEntry entry )
{
    const int i = int( entry );
    return ( 0 <= i && i < s_entryCount ) ? QString::fromLatin1( s_entryKeys[ i ] ) : QString();
}

bool
BrandingInfo::loadFromDescriptor( const QString& descriptorPath )
{
    const QFileInfo descriptor( descriptorPath );
    if ( !descriptor.exists() || !descriptor.isReadable() )
    {
        cWarning() << "Cannot read branding file" << descriptorPath;
        return false;
    }

    bool ok = false;
    const QVariantMap doc = ClaudeInstall::YAML::load( descriptor, &ok );
    if ( !ok )
    {
        cWarning() << "Branding file" << descriptorPath << "is not valid.";
        return false;
    }

    const QDir componentDir = descriptor.absoluteDir();
    const QString componentName = doc.value( "componentName" ).toString();
    if ( componentName.isEmpty() || componentName != componentDir.dirName() )
    {
        cWarning() << "The branding component name" << componentName
                   << "should match the name of the component directory" << componentDir.dirName();
        return false;
    }
    if ( ClaudeInstall::typeOf( doc.value( "strings" ) ) != ClaudeInstall::MapVariantType )
    {
        cWarning() << "Branding configuration has no *strings* map:" << descriptorPath;
        return false;
    }

    // Copy the os-release information into a QHash for use by KMacroExpander.
    KOSRelease relInfo;
    const QHash< QString, QString > relMap { { QStringLiteral( "NAME" ), relInfo.name() },
                                             { QStringLiteral( "VERSION" ), relInfo.version() },
                                             { QStringLiteral( "ID" ), relInfo.id() },
                                             { QStringLiteral( "ID_LIKE" ), relInfo.idLike().join( ' ' ) },
                                             { QStringLiteral( "VERSION_CODENAME" ), relInfo.versionCodename() },
                                             { QStringLiteral( "VERSION_ID" ), relInfo.versionId() },
                                             { QStringLiteral( "PRETTY_NAME" ), relInfo.prettyName() },
                                             { QStringLiteral( "HOME_URL" ), relInfo.homeUrl() },
                                             { QStringLiteral( "DOCUMENTATION_URL" ), relInfo.documentationUrl() },
                                             { QStringLiteral( "SUPPORT_URL" ), relInfo.supportUrl() },
                                             { QStringLiteral( "BUG_REPORT_URL" ), relInfo.bugReportUrl() },
                                             { QStringLiteral( "PRIVACY_POLICY_URL" ), relInfo.privacyPolicyUrl() },
                                             { QStringLiteral( "BUILD_ID" ), relInfo.buildId() },
                                             { QStringLiteral( "VARIANT" ), relInfo.variant() },
                                             { QStringLiteral( "VARIANT_ID" ), relInfo.variantId() },
                                             { QStringLiteral( "LOGO" ), relInfo.logo() } };

    const QVariantMap strings = doc.value( "strings" ).toMap();
    for ( int i = 0; i < s_entryCount; ++i )
    {
        const QString raw = strings.value( entryKey( StringEntry( i ) ) ).toString();
        setString( StringEntry( i ), KMacroExpander::expandMacros( raw, relMap, QLatin1Char( '$' ) ) );
    }

    m_componentName = componentName;
    m_componentDirectory = componentDir.absolutePath();
    m_welcomeStyleClaudeInstall = doc.value( "welcomeStyleClaudeInstall" ).toBool();

    // Same convention as Branding: <component>/lang/claudeinstall-<component>
    m_translationsDirectory = QDir( componentDir.filePath( "lang" ) ).absolutePath()
        + QStringLiteral( "/claudeinstall-" ) + componentName;

    cDebug() << "Loaded branding component" << componentName << "(headless)";
    return true;
}

void
BrandingInfo::setGlobals( GlobalStorage* globalStorage ) const
{
    QVariantMap brandingMap;
    for ( int i = 0; i < s_entryCount; ++i )
    {
        brandingMap.insert( entryKey( StringEntry( i ) ), string( StringEntry( i ) ) );
    }
    globalStorage->insert( "branding", brandingMap );
}

}  // namespace ClaudeInstall
