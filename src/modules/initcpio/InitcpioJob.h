/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef INITCPIOJOB_H
#define INITCPIOJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantMap>

class PLUGINDLLEXPORT InitcpioJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit InitcpioJob( QObject* parent = nullptr );
    ~InitcpioJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    QString m_kernel;
    bool m_unsafe = false;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( InitcpioJobFactory )

#endif  // INITCPIOJOB_H
