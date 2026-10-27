/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2016 Artoo <artoo@manjaro.org>
 *   SPDX-FileCopyrightText: 2017 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef SERVICESOPENRCJOB_H
#define SERVICESOPENRCJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class PLUGINDLLEXPORT ServicesOpenrcJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit ServicesOpenrcJob( QObject* parent = nullptr );
    ~ServicesOpenrcJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    /// Runs rc-update with @p state ("add" or "del") for each of @p services
    ClaudeInstall::JobResult update( const QString& state, const QVariantList& services ) const;

    QString m_root;
    QVariantList m_add;
    QVariantList m_del;
    QString m_initdDir;
    QString m_runlevelsDir;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( ServicesOpenrcJobFactory )

#endif  // SERVICESOPENRCJOB_H
