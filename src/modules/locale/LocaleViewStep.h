/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2016 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LOCALEVIEWSTEP_H
#define LOCALEVIEWSTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

#include <memory>

class LocalePage;

class PLUGINDLLEXPORT LocaleViewStep : public ClaudeInstall::ViewStep
{
    Q_OBJECT

public:
    explicit LocaleViewStep( QObject* parent = nullptr );
    ~LocaleViewStep() override;

    QString prettyName() const override;
    QString prettyStatus() const override;

    QWidget* widget() override;

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

private slots:
    void setUpPage();

private:
    QWidget* m_widget;

    LocalePage* m_actualWidget;
    bool m_nextEnabled;

    std::unique_ptr< Config > m_config;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( LocaleViewStepFactory )

#endif  // LOCALEVIEWSTEP_H
