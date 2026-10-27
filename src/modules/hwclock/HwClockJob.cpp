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

#include "HwClockJob.h"

#include "utils/Logger.h"
#include "utils/System.h"

HwClockJob::HwClockJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

HwClockJob::~HwClockJob() {}

QString
HwClockJob::prettyName() const
{
    return tr( "Setting hardware clock…", "@status" );
}

ClaudeInstall::JobResult
HwClockJob::exec()
{
    auto* s = ClaudeInstall::System::instance();

    int r = s->targetEnvCall( { "hwclock", "--systohc", "--utc" } );
    if ( r == 0 )
    {
        cDebug() << "Hwclock set using RTC method.";
        return ClaudeInstall::JobResult::ok();
    }
    cDebug() << "Hwclock returned error code" << r << Logger::SubEntry << "RTC method failed, trying ISA bus method.";

    r = s->targetEnvCall( { "hwclock", "--systohc", "--utc", "--directisa" } );
    if ( r == 0 )
    {
        cDebug() << "Hwclock set using ISA bus method.";
    }
    else
    {
        cDebug() << "Hwclock returned error code" << r << Logger::SubEntry << "ISA bus method failed.";
        cDebug() << "BIOS or Kernel BUG: Setting hwclock failed.";
    }

    // Like the Python module, failure to set the clock is never fatal.
    return ClaudeInstall::JobResult::ok();
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( HwClockJobFactory, registerPlugin< HwClockJob >(); )
