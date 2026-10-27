/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LIBCLAUDEINSTALLTUI_TESTS_H
#define LIBCLAUDEINSTALLTUI_TESTS_H

#include <QObject>

/// These tests need no terminal: they exercise the parts around the screen
class TuiTests : public QObject
{
    Q_OBJECT
public:
    TuiTests();
    ~TuiTests() override;

private Q_SLOTS:
    void initTestCase();

    void testPlaceholderStep();
    void testComponentIsBuiltOnce();
    void testCancelIsAskedTwice();
    void testExecutionIsAskedTwice();
    void testLoaderScan();

    void testFieldEditsValue();
    void testCheckToggles();
    void testScrolledRenders();
    void testStripMarkup();
    void testPickerMoves();
    void testFillEntries();
};

#endif
