/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef PACKAGES_TESTS_H
#define PACKAGES_TESTS_H

#include <QObject>

class PackagesTests : public QObject
{
    Q_OBJECT
public:
    PackagesTests();
    ~PackagesTests() override;

private Q_SLOTS:
    void initTestCase();

    void testBackends();
    void testPacmanDefaults();
    void testPacmanConfig_data();
    void testPacmanConfig();
    void testPacmanBadConfig();
    void testSubstLocale();
};

#endif
