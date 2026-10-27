/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LIBCLAUDEINSTALL_NETWORK_TESTS_H
#define LIBCLAUDEINSTALL_NETWORK_TESTS_H

#include <QObject>

class NetworkTests : public QObject
{
    Q_OBJECT
public:
    NetworkTests();
    ~NetworkTests() override;

private Q_SLOTS:
    void initTestCase();

    void testInstance();
    void testPing();

    void testCheckUrl();
    void testCheckMultiUrl();
};

#endif
