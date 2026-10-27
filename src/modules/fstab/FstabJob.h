/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2014 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2014 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2016 Philip Müller <philm@manjaro.org>
 *   SPDX-FileCopyrightText: 2017 Alf Gaida <agaida@siduction.org>
 *   SPDX-FileCopyrightText: 2017-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef FSTABJOB_H
#define FSTABJOB_H

#include "CppJob.h"
#include "DllMacro.h"
#include "utils/PluginFactory.h"

#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

/** @brief Writes /etc/fstab and /etc/crypttab into the target, and creates mount points */
class PLUGINDLLEXPORT FstabJob : public ClaudeInstall::CppJob
{
    Q_OBJECT

public:
    explicit FstabJob( QObject* parent = nullptr );
    ~FstabJob() override;

    QString prettyName() const override;

    ClaudeInstall::JobResult exec() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    /// The fields of one /etc/fstab line
    struct FstabLine
    {
        QString device;
        QString mountPoint;
        QString fs;
        QString options;
        int check = 0;
    };

    /// The fields of one /etc/crypttab line
    struct CrypttabLine
    {
        QString name;
        QString device;
        QString password;
        QString options;
    };

    bool findSsdDisks();
    bool generateFstab() const;
    bool generateCrypttab() const;
    bool createMountPoints() const;

    bool fstabLineInfo( const QVariantMap& partition, FstabLine& line ) const;
    bool crypttabLineInfo( const QVariantMap& partition, CrypttabLine& line ) const;
    QString mountOptions( const QString& mountPoint ) const;

    /// Creates /swapfile (or /swap/swapfile for btrfs) in the target; returns an error message or empty
    QString createSwapfile( bool rootBtrfs );

    QString m_rootMountPoint;
    QVariantList m_partitions;
    QVariantList m_mountOptionsList;

    QString m_crypttabOptions;
    QVariantMap m_tmpOptions;

    QSet< QString > m_ssdDisks;
    mutable bool m_rootIsSsd = false;
};

CLAUDEINSTALL_PLUGIN_FACTORY_DECLARATION( FstabJobFactory )

#endif  // FSTABJOB_H
