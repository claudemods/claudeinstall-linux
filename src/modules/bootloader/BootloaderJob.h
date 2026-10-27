/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2014 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2014 Daniel Hillenbrand <codeworkx@bbqlinux.org>
 *   SPDX-FileCopyrightText: 2014 Benjamin Vaudour <benjamin.vaudour@yahoo.fr>
 *   SPDX-FileCopyrightText: 2014-2019 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2015-2018 Philip Mueller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2016-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2017 Gabriel Craciunescu <crazy@frugalware.org>
 *   SPDX-FileCopyrightText: 2017 Ben Green <Bezzy1999@hotmail.com>
 *   SPDX-FileCopyrightText: 2021 Neal Gompa <ngompa13@gmail.com>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef BOOTLOADERJOB_H
#define BOOTLOADERJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QVariantMap>

/** @brief Installs the bootloader (GRUB, systemd-boot, rEFInd, shim, clr-boot-manager) in the target */
class PLUGINDLLEXPORT BootloaderJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit BootloaderJob( QObject* parent = nullptr );
    ~BootloaderJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    QVariantMap m_configuration;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( BootloaderJobFactory )

#endif  // BOOTLOADERJOB_H
