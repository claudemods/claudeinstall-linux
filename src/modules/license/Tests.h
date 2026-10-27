/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LICENSE_TESTS_H
#define LICENSE_TESTS_H

#include <QObject>

class LicenseTests : public QObject
{
    Q_OBJECT
public:
    LicenseTests();
    ~LicenseTests() override;

private Q_SLOTS:
    void initTestCase();

    void testEntries();
    void testAcceptance();
    void testOptional();
};

#endif
