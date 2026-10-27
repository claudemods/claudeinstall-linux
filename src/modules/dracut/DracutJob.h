/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaid@siduction.org>
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2022 Anke Boersma <demm@kaosx.us>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef DRACUTJOB_H
#define DRACUTJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>

class PLUGINDLLEXPORT DracutJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit DracutJob( QObject* parent = nullptr );
    ~DracutJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    QString m_initramfsName;
    QStringList m_options;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( DracutJobFactory )

#endif  // DRACUTJOB_H
