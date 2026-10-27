/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef INITRAMFSJOB_H
#define INITRAMFSJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantMap>

class PLUGINDLLEXPORT InitramfsJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit InitramfsJob( QObject* parent = nullptr );
    ~InitramfsJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    QString m_kernel;
    bool m_unsafe = false;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( InitramfsJobFactory )

#endif  // INITRAMFSJOB_H
