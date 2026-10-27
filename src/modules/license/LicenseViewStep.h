/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2015 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LICENSEPAGEPLUGIN_H
#define LICENSEPAGEPLUGIN_H

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

#include <QObject>
#include <QUrl>
#include <QVariantMap>

class Config;
class LicensePage;

class PLUGINDLLEXPORT LicenseViewStep : public ClaudeInstall::ViewStep
{
    Q_OBJECT

public:
    explicit LicenseViewStep( QObject* parent = nullptr );
    ~LicenseViewStep() override;

    QString prettyName() const override;

    QWidget* widget() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    QList< ClaudeInstall::job_ptr > jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    Config* m_config;
    LicensePage* m_widget;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( LicenseViewStepFactory )

#endif  // LICENSEPAGEPLUGIN_H
