/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef CLEARMOUNTSJOBTESTS_H
#define CLEARMOUNTSJOBTESTS_H

#include <QObject>

class ClearMountsJobTests : public QObject
{
    Q_OBJECT
public:
    ClearMountsJobTests();

private Q_SLOTS:
    void testFindPartitions();
};

#endif
