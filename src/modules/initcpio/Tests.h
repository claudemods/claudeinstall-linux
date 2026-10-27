/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TESTS_H
#define TESTS_H

#include <QObject>

class InitcpioTests : public QObject
{
    Q_OBJECT
public:
    InitcpioTests();
    ~InitcpioTests() override;

private Q_SLOTS:
    void initTestCase();
    void testFixPermissions();
};

#endif
