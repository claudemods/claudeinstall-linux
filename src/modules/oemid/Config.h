/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef OEMID_CONFIG_H
#define OEMID_CONFIG_H

#include "Job.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

/** @brief The OEM batch identifier
 *
 * The identifier is written to a file in the target (by a job). The
 * configuration gives the suggested identifier, which may contain
 * `${DATE}`; the user may change it. It is fine for it to be empty.
 */
class Config : public QObject
{
    Q_OBJECT

    ///@brief The batch identifier to write; initially the suggestion from the configuration
    Q_PROPERTY( QString batchIdentifier READ batchIdentifier WRITE setBatchIdentifier NOTIFY batchIdentifierChanged )
    ///@brief What this module will do, as a sentence
    Q_PROPERTY( QString prettyStatus READ prettyStatus NOTIFY batchIdentifierChanged )

public:
    explicit Config( QObject* parent = nullptr );

    void setConfigurationMap( const QVariantMap& configurationMap );

    QString batchIdentifier() const { return m_batchIdentifier; }
    void setBatchIdentifier( const QString& batchIdentifier );

    QString prettyStatus() const;

    /// @brief The job that writes the identifier
    ClaudeInstall::JobList createJobs() const;

signals:
    void batchIdentifierChanged( const QString& batchIdentifier );

private:
    QString m_batchIdentifier;
};

#endif
