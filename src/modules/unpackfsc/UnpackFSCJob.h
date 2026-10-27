/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef UNPACKFSC_UNPACKFSCJOB_H
#define UNPACKFSC_UNPACKFSCJOB_H

#include <CppJob.h>
#include <DllMacro.h>
#include <utils/PluginFactory.h>

class PLUGINDLLEXPORT UnpackFSCJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    enum class Type
    {
        None,  /// << Invalid
        FSArchive,
        FSArchiveFS,
        Squashfs,
        Tarball,
    };

    explicit UnpackFSCJob( QObject* parent = nullptr );
    ~UnpackFSCJob() override;

    QString prettyName() const override;
    QString prettyStatusMessage() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    QString m_source;
    QString m_destination;
    Type m_type = Type::None;
    QString m_progressMessage;
    QString m_condition;  ///< May be empty to express condition "true"
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( UnpackFSCFactory )

#endif
