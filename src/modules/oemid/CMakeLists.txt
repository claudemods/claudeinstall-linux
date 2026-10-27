# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
# The headless part: the batch identifier and the job that writes it,
# shared by all frontends
claudeinstall_add_config_library(oemid_config
    SOURCES
        Config.cpp
        IDJob.cpp
)

claudeinstall_add_plugin(oemid
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        OEMViewStep.cpp
    UI
        OEMPage.ui
    LINK_PRIVATE_LIBRARIES
        oemid_config
        ${qtname}::Widgets
    SHARED_LIB
)

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(oemid
    SOURCES
        OemTuiStep.cpp
    LINK_LIBRARIES
        oemid_config
)

claudeinstall_add_test(oemidtest SOURCES Tests.cpp Config.cpp IDJob.cpp)

