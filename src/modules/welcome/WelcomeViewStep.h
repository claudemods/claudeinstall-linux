/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef WELCOMEVIEWSTEP_H
#define WELCOMEVIEWSTEP_H


#include "DllMacro.h"
#include "modulesystem/Requirement.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

#include <QObject>
#include <QVariantMap>

class WelcomePage;
class GeneralRequirements;
class Config;

namespace ClaudeInstall
{
namespace GeoIP
{
class Handler;
}  // namespace GeoIP
}  // namespace ClaudeInstall

class PLUGINDLLEXPORT WelcomeViewStep : public ClaudeInstall::ViewStep
{
    Q_OBJECT

public:
    explicit WelcomeViewStep( QObject* parent = nullptr );
    ~WelcomeViewStep() override;

    QString prettyName() const override;

    QWidget* widget() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

    /** @brief Sets the country that ClaudeInstall is running in.
     *
     * This (ideally) sets up language and locale settings that are right for
     * the given 2-letter country code. Uses the handler's information (if
     * given) for error reporting.
     */
    void setCountry( const QString&, ClaudeInstall::GeoIP::Handler* handler );

    ClaudeInstall::RequirementsList checkRequirements() override;

private:
    Config* m_conf;
    WelcomePage* m_widget;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( WelcomeViewStepFactory )

#endif  // WELCOMEVIEWSTEP_H
