# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
find_package(${qtname} COMPONENTS Core Gui REQUIRED)
if(WITH_GUI)
    find_package(${qtname} COMPONENTS Widgets REQUIRED)
endif()

### OPTIONAL AppData XML support in PackageModel
#
#
option(BUILD_APPDATA "Support appdata: items in PackageChooser (requires QtXml)" OFF)
if(BUILD_APPDATA)
    find_package(${qtname} REQUIRED COMPONENTS Xml)
endif()

### OPTIONAL AppStream support in PackageModel
#
#
include(AppStreamHelper)

# The headless part: the Config and the model of packages (screenshots are
# paths, the frontend loads the images), shared by all frontends. The
# AppStream and AppData support are part of it.
set(_packagechooser_config_sources Config.cpp PackageModel.cpp)
set(_packagechooser_config_libraries)
set(_packagechooser_config_definitions)

if(AppStreamQt_FOUND)
    list(APPEND _packagechooser_config_sources ItemAppStream.cpp)
    list(APPEND _packagechooser_config_libraries claudeinstall::appstreamqt)
endif()

if(BUILD_APPDATA AND TARGET ${qtname}::Xml)
    list(APPEND _packagechooser_config_sources ItemAppData.cpp)
    list(APPEND _packagechooser_config_libraries ${qtname}::Xml)
    list(APPEND _packagechooser_config_definitions HAVE_APPDATA)
endif()

claudeinstall_add_config_library(packagechooser_config
    SOURCES
        ${_packagechooser_config_sources}
    LINK_LIBRARIES
        ${_packagechooser_config_libraries}
    DEFINITIONS
        ${_packagechooser_config_definitions}
)

claudeinstall_add_plugin(packagechooser
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        PackageChooserPage.cpp
        PackageChooserViewStep.cpp
    RESOURCES
        packagechooser.qrc
    UI
        page_package.ui
    LINK_PRIVATE_LIBRARIES
        packagechooser_config
    SHARED_LIB
)

# The text-mode page (only with WITH_TUI), on the same Config
claudeinstall_add_tui_plugin(packagechooser
    SOURCES
        PackageChooserTuiStep.cpp
    LINK_LIBRARIES
        packagechooser_config
)

claudeinstall_add_test(
    packagechoosertest
    GUI
    SOURCES Tests.cpp
    LIBRARIES ${packagechooser_TARGET}
)
