/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef BOOTLOADER_TESTS_H
#define BOOTLOADER_TESTS_H

#include <QObject>

class BootloaderTests : public QObject
{
    Q_OBJECT
public:
    BootloaderTests();
    ~BootloaderTests() override;

private Q_SLOTS:
    void initTestCase();

    void testRenderInBase();
    void testSerialSuffix();
    void testRandomAndPhraseSuffix();
    void testInvalidSuffix_data();
    void testInvalidSuffix();
    void testChangeEfiSuffix();
    void testSanitize();
    void testFindKernels();
    void testVfatCase();
};

#endif
