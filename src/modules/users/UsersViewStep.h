/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef USERSPAGEPLUGIN_H
#define USERSPAGEPLUGIN_H

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

#include <QObject>
#include <QVariant>

class Config;
class UsersPage;

class PLUGINDLLEXPORT UsersViewStep : public ClaudeInstall::ViewStep
{
    Q_OBJECT

public:
    explicit UsersViewStep( QObject* parent = nullptr );
    ~UsersViewStep() override;

    QString prettyName() const override;

    QWidget* widget() override;

    QObject* config() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    ClaudeInstall::JobList jobs() const override;

    void onActivate() override;
    void onLeave() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    UsersPage* m_widget;
    Config* m_config;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( UsersViewStepFactory )

#endif  // USERSPAGEPLUGIN_H
