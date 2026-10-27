# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
include_directories(${PROJECT_BINARY_DIR}/src/libclaudeinstallui)

# The headless part: the licenses and whether the user accepted them,
# shared by all frontends
claudeinstall_add_config_library(license_config
    SOURCES
        Config.cpp
)

claudeinstall_add_plugin(license
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        LicensePage.cpp
        LicenseViewStep.cpp
        LicenseWidget.cpp
    UI
        LicensePage.ui
    LINK_PRIVATE_LIBRARIES
        license_config
    SHARED_LIB
)

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(license
    SOURCES
        LicenseTuiStep.cpp
    LINK_LIBRARIES
        license_config
)

claudeinstall_add_test(licensetest SOURCES Tests.cpp Config.cpp)

