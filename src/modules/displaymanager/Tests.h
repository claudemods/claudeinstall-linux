/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef DISPLAYMANAGER_TESTS_H
#define DISPLAYMANAGER_TESTS_H

#include <QObject>

class DisplayManagerTests : public QObject
{
    Q_OBJECT
public:
    DisplayManagerTests();
    ~DisplayManagerTests() override;

private Q_SLOTS:
    void initTestCase();

    void testIniFile();
    void testIniFileErrors();
    void testTomlParse();
    void testTomlUnsupported();
    void testTomlRoundTrip();

    void testDesktopEnvironments();
    void testCreateDisplayManagers();

    void testSddm();
    void testGdm();
    void testGreetd();
    void testLightdm();
};

#endif
