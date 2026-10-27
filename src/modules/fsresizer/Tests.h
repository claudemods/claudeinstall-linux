/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TESTS_H
#define TESTS_H

#include <QObject>

class FSResizerTests : public QObject
{
    Q_OBJECT
public:
    FSResizerTests();
    ~FSResizerTests() override;

private Q_SLOTS:
    void initTestCase();
    // Can handle missing values
    void testConfigurationRobust();
    // Can parse % and MiB values
    void testConfigurationValues();
};

#endif
