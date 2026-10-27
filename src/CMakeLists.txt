# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
include(ClaudeInstallAddBrandingSubdirectory)
include(ClaudeInstallAddConfigLibrary)
include(ClaudeInstallAddLibrary)
include(ClaudeInstallAddModuleSubdirectory)
include(ClaudeInstallAddPlugin)
include(ClaudeInstallAddTest)
include(ClaudeInstallAddTuiPlugin)
include(ClaudeInstallAddTranslations)

# library
add_subdirectory(libclaudeinstall)
add_subdirectory(libclaudeinstallstep)
if(WITH_GUI)
    add_subdirectory(libclaudeinstallui)
endif()
if(WITH_TUI)
    add_subdirectory(libclaudeinstalltui)
endif()

if(WITH_GUI)
    add_subdirectory(qml)
endif()

# application (the directory also has the configuration-file test program, which every build needs)
add_subdirectory(claudeinstall)
if(WITH_TUI)
    add_subdirectory(claudeinstall-tui)
endif()

# plugins
add_subdirectory(modules)

# branding components
add_subdirectory(branding)
