/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef OEMID_TESTS_H
#define OEMID_TESTS_H

#include <QObject>

class OemIdTests : public QObject
{
    Q_OBJECT
public:
    OemIdTests();
    ~OemIdTests() override;

private Q_SLOTS:
    void initTestCase();

    void testSubstitution();
    void testChange();
    void testJobs();
};

#endif
