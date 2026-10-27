# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
# The headless part: the Config and the jobs, shared by all frontends
claudeinstall_add_config_library(tracking_config
    SOURCES
        Config.cpp
        TrackingJobs.cpp
    LINK_LIBRARIES
        ${kfname}::CoreAddons
)

claudeinstall_add_plugin(tracking
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        TrackingPage.cpp
        TrackingViewStep.cpp
    UI
        page_trackingstep.ui
    RESOURCES
        page_trackingstep.qrc
    SHARED_LIB
    LINK_LIBRARIES
        ${kfname}::CoreAddons
    LINK_PRIVATE_LIBRARIES
        tracking_config
)

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(tracking
    SOURCES
        TrackingTuiStep.cpp
    LINK_LIBRARIES
        tracking_config
)

claudeinstall_add_test(trackingtest SOURCES Tests.cpp Config.cpp)
