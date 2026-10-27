# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#

# The headless part: the Config, the models and the job, shared by all
# frontends. (A frontend that links it also needs keyboard.qrc.)
claudeinstall_add_config_library(keyboard_config
    SOURCES
        Config.cpp
        KeyboardLayoutModel.cpp
        SetKeyboardLayoutJob.cpp
        keyboardwidget/keyboardglobal.cpp
    LINK_LIBRARIES
        ${qtname}::DBus
)

claudeinstall_add_plugin(keyboard
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        KeyboardViewStep.cpp
        KeyboardPage.cpp
        keyboardwidget/keyboardpreview.cpp
    UI
        KeyboardPage.ui
    RESOURCES
        keyboard.qrc
    SHARED_LIB
    LINK_LIBRARIES
        ${qtname}::DBus
    LINK_PRIVATE_LIBRARIES
        keyboard_config
)

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(keyboard
    SOURCES
        KeyboardTuiStep.cpp
    RESOURCES
        keyboard.qrc
    LINK_LIBRARIES
        keyboard_config
)

claudeinstall_add_test(keyboardtest SOURCES Tests.cpp SetKeyboardLayoutJob.cpp RESOURCES keyboard.qrc)
