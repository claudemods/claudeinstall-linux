# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
find_package(${qtname} ${QT_VERSION} CONFIG REQUIRED DBus Network)

include_directories(${PROJECT_BINARY_DIR}/src/libclaudeinstallui)

# The headless part: the Config, shared by all frontends
claudeinstall_add_config_library(finished_config
    SOURCES
        Config.cpp
    LINK_LIBRARIES
        ${qtname}::DBus
)

claudeinstall_add_plugin(finished
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        FinishedViewStep.cpp
        FinishedPage.cpp
    UI
        FinishedPage.ui
    LINK_PRIVATE_LIBRARIES
        finished_config
        ${qtname}::DBus
    SHARED_LIB
)

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(finished
    SOURCES
        FinishedTuiStep.cpp
    LINK_LIBRARIES
        finished_config
)
