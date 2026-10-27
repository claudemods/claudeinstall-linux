/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LOCALETUISTEP_H
#define LOCALETUISTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "TuiStep.h"
#include "utils/PluginFactory.h"

#include <QStringList>

#include <string>
#include <vector>

/**
 * @brief The location page in the terminal
 *
 * Four lists instead of the world map: region and zone (the time zone),
 * language, and formats (the LC_* locale). Left and right move between the lists.
 */
class PLUGINDLLEXPORT LocaleTuiStep : public ClaudeInstall::TuiStep
{
    Q_OBJECT

public:
    explicit LocaleTuiStep( QObject* parent = nullptr );
    ~LocaleTuiStep() override;

    QString prettyName() const override;
    QString prettyStatus() const override;

    ftxui::Component createComponent() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;
    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

    void onActivate() override;
    void onLeave() override;
    void onCancel() override;
    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    /// @brief Makes the lists show what the Config has; with @p force also the ones with the focus
    void sync( bool force );
    void fillZones();
    void regionChanged();
    void zoneChanged();

    Config* m_config;

    std::vector< std::string > m_regions;
    std::vector< std::string > m_zones;
    std::vector< std::string > m_languages;
    std::vector< std::string > m_formats;
    QStringList m_regionKeys;
    QStringList m_zoneKeys;
    int m_region = 0;
    int m_zone = 0;
    int m_language = 0;
    int m_format = 0;

    ftxui::Component m_regionList;
    ftxui::Component m_zoneList;
    ftxui::Component m_languageList;
    ftxui::Component m_formatList;

    bool m_syncing = false;  ///< ignore the lists' change notices while they are being set
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( LocaleTuiStepFactory )

#endif
