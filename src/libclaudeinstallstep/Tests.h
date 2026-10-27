/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LIBCLAUDEINSTALLSTEP_TESTS_H
#define LIBCLAUDEINSTALLSTEP_TESTS_H

#include <QObject>

class StepTests : public QObject
{
    Q_OBJECT
public:
    StepTests();
    ~StepTests() override;

private Q_SLOTS:
    void initTestCase();

    void testStepDefaults();
    void testNavigation();
    void testModel();
    void testNextStatus();
    void testExecutionStep();
    void testCancel();
    void testExecutionModel();

    void testBrandingFromDescriptor();
    void testBrandingFromBadDescriptor();
};

#endif
