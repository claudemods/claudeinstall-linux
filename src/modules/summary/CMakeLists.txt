# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
include_directories(${PROJECT_BINARY_DIR}/src/libclaudeinstallui)

# The headless part: the Config and the model of summaries, shared by all frontends
claudeinstall_add_config_library(summary_config
    SOURCES
        Config.cpp
        SummaryModel.cpp
)

claudeinstall_add_plugin(summary
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        SummaryPage.cpp
        SummaryViewStep.cpp
    UI
    SHARED_LIB
    NO_CONFIG
    LINK_PRIVATE_LIBRARIES
        summary_config
)

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(summary
    SOURCES
        SummaryTuiStep.cpp
    LINK_LIBRARIES
        summary_config
)
