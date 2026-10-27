/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CONTEXTUALPROCESSJOB_H
#define CONTEXTUALPROCESSJOB_H

#include <QObject>
#include <QVariantMap>

#include "CppJob.h"
#include "DllMacro.h"

#include "utils/PluginFactory.h"

class ContextualProcessBinding;

class PLUGINDLLEXPORT ContextualProcessJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit ContextualProcessJob( QObject* parent = nullptr );
    ~ContextualProcessJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

    /// The number of bindings
    int count();
    /// The number of value-checks for the named binding (-1 if binding doesn't exist)
    int count( const QString& variableName );

private:
    QList< ContextualProcessBinding* > m_commands;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( ContextualProcessJobFactory )

#endif  // CONTEXTUALPROCESSJOB_H
