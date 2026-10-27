/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef BRANDINGINFO_H
#define BRANDINGINFO_H

#include "DllMacro.h"

#include <QHash>
#include <QString>

#include <functional>

namespace ClaudeInstall
{

class GlobalStorage;

/**
 * @brief The branding information that does not need a GUI
 *
 * The product name, version, URLs and a few paths. Frontend-neutral
 * code (e.g. the Config objects of view modules) reads these through
 * BrandingInfo; each frontend fills it in from its own branding: the
 * Widgets / QML frontend's Branding object publishes its strings here
 * when it is created.
 *
 * Reading from an empty BrandingInfo is fine, it gives empty strings.
 */
class STEPDLLEXPORT BrandingInfo
{
public:
    /// The same entries (in the same order) as Branding::StringEntry
    enum StringEntry
    {
        ProductName,
        Version,
        ShortVersion,
        VersionedName,
        ShortVersionedName,
        ShortProductName,
        BootloaderEntryName,
        ProductUrl,
        SupportUrl,
        KnownIssuesUrl,
        ReleaseNotesUrl,
        DonateUrl
    };

    /// @brief The (single) branding information of this application
    static BrandingInfo& instance();

    /// @brief The name of the entry as used in branding.desc (e.g. "productName")
    static QString entryKey( StringEntry entry );

    /** @brief Fills in the information from a branding descriptor
     *
     * This is the headless reader of branding.desc, for frontends that
     * have no Branding object: it reads the component name, the *strings*
     * (with the os-release macro expansion that the GUI's Branding does) and
     * the welcome style. Images, styles and slideshows are ignored.
     *
     * Unlike Branding, a problem is not fatal: the function logs it and returns
     * @c false, leaving the information as it was.
     */
    bool loadFromDescriptor( const QString& descriptorPath );

    /// @brief The name of the loaded branding component (empty if there is none)
    QString componentName() const { return m_componentName; }

    /// @brief Stores the strings in the *branding* key of @p globalStorage (as Branding::setGlobals() does)
    void setGlobals( GlobalStorage* globalStorage ) const;

    QString string( StringEntry entry ) const { return m_strings.value( entry ); }
    void setString( StringEntry entry, const QString& value ) { m_strings.insert( entry, value ); }

    QString productName() const { return string( ProductName ); }
    QString versionedName() const { return string( VersionedName ); }
    QString shortProductName() const { return string( ShortProductName ); }
    QString shortVersionedName() const { return string( ShortVersionedName ); }

    /// @brief The directory holding all of the branding assets
    QString componentDirectory() const { return m_componentDirectory; }
    void setComponentDirectory( const QString& directory ) { m_componentDirectory = directory; }

    /// @brief The path prefix of the branding translations (see Branding::translationsDirectory())
    QString translationsDirectory() const { return m_translationsDirectory; }
    void setTranslationsDirectory( const QString& directory ) { m_translationsDirectory = directory; }

    /// @brief Does the welcome page use the ClaudeInstall-style message?
    bool welcomeStyleClaudeInstall() const { return m_welcomeStyleClaudeInstall; }
    void setWelcomeStyleClaudeInstall( bool b ) { m_welcomeStyleClaudeInstall = b; }

    using ImageProbe = std::function< bool( const QString& ) >;
    /** @brief Does the branding have an image, or the theme an icon, called @p name?
     *
     * Only frontends that can show images set a probe; without one,
     * nothing has an image.
     */
    bool hasImage( const QString& name ) const { return m_imageProbe ? m_imageProbe( name ) : false; }
    void setImageProbe( const ImageProbe& probe ) { m_imageProbe = probe; }

private:
    BrandingInfo() = default;

    QHash< int, QString > m_strings;
    QString m_componentName;
    QString m_componentDirectory;
    QString m_translationsDirectory;
    bool m_welcomeStyleClaudeInstall = false;
    ImageProbe m_imageProbe;
};

}  // namespace ClaudeInstall

#endif
