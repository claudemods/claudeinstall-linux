# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
# The headless part: the Config, the loader and the package tree model,
# shared by all frontends
claudeinstall_add_config_library(netinstall_config
    SOURCES
        Config.cpp
        LoaderQueue.cpp
        PackageTreeItem.cpp
        PackageModel.cpp
    LINK_LIBRARIES
        ${qtname}::Network
)

claudeinstall_add_plugin(netinstall
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        groupstreeview.cpp
        NetInstallViewStep.cpp
        NetInstallPage.cpp
    UI
        page_netinst.ui
    LINK_PRIVATE_LIBRARIES
        netinstall_config
        ${qtname}::Network
    SHARED_LIB
)

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(netinstall
    SOURCES
        NetInstallTuiStep.cpp
    LINK_LIBRARIES
        netinstall_config
)

if(TARGET ${kfname}::CoreAddons)
    claudeinstall_add_test(
        netinstalltest
        SOURCES Tests.cpp Config.cpp LoaderQueue.cpp PackageTreeItem.cpp PackageModel.cpp
        LIBRARIES ${qtname}::Gui ${qtname}::Network ${kfname}::CoreAddons
    )
endif()
