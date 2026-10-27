/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *  SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *  SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef PRESERVEFILES_H
#define PRESERVEFILES_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

class Item;

class PLUGINDLLEXPORT PreserveFiles : public ClaudeInstall::CppJob
{
    Q_OBJECT

    using ItemList = QList< Item >;

public:
    explicit PreserveFiles( QObject* parent = nullptr );
    ~PreserveFiles() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    ItemList m_items;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( PreserveFilesFactory )

#endif  // PRESERVEFILES_H
