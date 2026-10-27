/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TRACKINGVIEWSTEP_H
#define TRACKINGVIEWSTEP_H

#include "TrackingType.h"

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

#include <QObject>
#include <QUrl>
#include <QVariantMap>

class Config;
class TrackingPage;

class PLUGINDLLEXPORT TrackingViewStep : public ClaudeInstall::ViewStep
{
    Q_OBJECT

public:
    explicit TrackingViewStep( QObject* parent = nullptr );
    ~TrackingViewStep() override;

    QString prettyName() const override;

    QWidget* widget() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    void onLeave() override;

    ClaudeInstall::JobList jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    Config* m_config;
    TrackingPage* m_widget;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( TrackingViewStepFactory )

#endif  // TRACKINGVIEWSTEP_H
