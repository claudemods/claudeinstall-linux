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

#ifndef LICENSE_CONFIG_H
#define LICENSE_CONFIG_H

#include "utils/NamedEnum.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

/** @brief One license (or EULA) to show, from the configuration */
struct LicenseEntry
{
    enum class Type
    {
        Software = 0,
        Driver,
        GpuDriver,
        BrowserPlugin,
        Codec,
        Package
    };

    /// @brief Lookup table for the enums
    const NamedEnumTable< Type >& typeNames();

    LicenseEntry( const QVariantMap& conf );
    LicenseEntry( const LicenseEntry& ) = default;
    LicenseEntry& operator=( const LicenseEntry& ) = default;

    bool isValid() const { return !m_id.isEmpty(); }
    bool isRequired() const { return m_required; }
    bool isLocal() const;
    bool expandByDefault() const { return m_expand; }

    QString m_id;
    QString m_prettyName;
    QString m_prettyVendor;
    Type m_type = Type::Software;
    QUrl m_url;
    bool m_required = false;
    bool m_expand = false;
};

/** @brief The licenses to accept, and whether the user accepted them
 *
 * The user can continue if none of the licenses is required, or if the
 * user has accepted them all (there is one acceptance for all of the
 * licenses). The acceptance is shared with the other modules through the
 * global storage key *licenseAgree*.
 */
class Config : public QObject
{
    Q_OBJECT

    ///@brief Has the user accepted the terms?
    Q_PROPERTY( bool accepted READ accepted WRITE setAccepted NOTIFY acceptedChanged FINAL )
    ///@brief Are none of the licenses required?
    Q_PROPERTY( bool allLicensesOptional READ allLicensesOptional NOTIFY entriesChanged FINAL )
    ///@brief Can the user continue?
    Q_PROPERTY( bool isNextEnabled READ isNextEnabled NOTIFY isNextEnabledChanged FINAL )

public:
    explicit Config( QObject* parent = nullptr );

    void setConfigurationMap( const QVariantMap& configurationMap );

    /// @brief The (valid) licenses from the configuration
    QList< LicenseEntry > entries() const { return m_entries; }

    bool accepted() const { return m_accepted; }
    bool allLicensesOptional() const { return m_allLicensesOptional; }
    bool isNextEnabled() const { return m_isNextEnabled; }

public slots:
    /** @brief Records whether the user accepted the terms
     *
     * Updates the global storage *licenseAgree* key, and whether
     * the user can continue (when @p accepted is true, always; otherwise
     * only if no license is required).
     */
    void setAccepted( bool accepted );

signals:
    void acceptedChanged( bool accepted );
    void entriesChanged();
    void isNextEnabledChanged( bool enabled );

private:
    void updateGlobalStorage( bool v );

    QList< LicenseEntry > m_entries;
    bool m_accepted = false;
    bool m_allLicensesOptional = false;  ///< @brief none of the licenses are required
    bool m_isNextEnabled = false;
};

#endif
