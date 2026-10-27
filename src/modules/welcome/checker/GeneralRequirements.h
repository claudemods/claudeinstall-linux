/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2017 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef GENERALREQUIREMENTS_H
#define GENERALREQUIREMENTS_H

#include <QObject>
#include <QStringList>

#include "modulesystem/Requirement.h"

#include <functional>

class GeneralRequirements : public QObject
{
    Q_OBJECT
public:
    explicit GeneralRequirements( QObject* parent = nullptr );

    void setConfigurationMap( const QVariantMap& configurationMap );

    /** @brief Sets how to tell if the screen is large enough
     *
     * Only a frontend with a screen can say. The function returns @c true
     * if the biggest screen can show the whole installer. Without a check
     * (the default) the *screen* requirement is always satisfied, as it
     * is for a text-mode frontend.
     */
    static void setScreenCheck( const std::function< bool() >& check );

    ClaudeInstall::RequirementsList checkRequirements();

private:
    QStringList m_entriesToCheck;
    QStringList m_entriesToRequire;

    bool checkEnoughStorage( qint64 requiredSpace );
    bool checkEnoughRam( qint64 requiredRam );
    bool checkBatteryExists();
    bool checkHasPower();
    bool checkHasInternet();
    bool checkIsRoot();

    qreal m_requiredStorageGiB;
    qreal m_requiredRamGiB;
};

#endif  // REQUIREMENTSCHECKER_H
