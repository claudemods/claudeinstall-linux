/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2022 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef ZFSHOSTIDJOB_H
#define ZFSHOSTIDJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>

class PLUGINDLLEXPORT ZfsHostIdJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit ZfsHostIdJob( QObject* parent = nullptr );
    ~ZfsHostIdJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( ZfsHostIdJobFactory )

#endif  // ZFSHOSTIDJOB_H
