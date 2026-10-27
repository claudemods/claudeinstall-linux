/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017-2018 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef HWCLOCKJOB_H
#define HWCLOCKJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>

class PLUGINDLLEXPORT HwClockJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit HwClockJob( QObject* parent = nullptr );
    ~HwClockJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( HwClockJobFactory )

#endif  // HWCLOCKJOB_H
