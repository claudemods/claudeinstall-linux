# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2021 Anke Boersma <demm@kaosx.us>
#   SPDX-License-Identifier: BSD-2-Clause
#
if(NOT WITH_QML)
    claudeinstall_skip_module( "summaryq (QML is not supported in this build)" )
    return()
endif()

set(_summary ${CMAKE_CURRENT_SOURCE_DIR}/../summary)
include_directories(${_summary})

claudeinstall_add_plugin(summaryq
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        SummaryQmlViewStep.cpp
        ${_summary}/Config.cpp
        ${_summary}/SummaryModel.cpp
    UI
    RESOURCES
        summaryq${QT_VERSION_SUFFIX}.qrc
    LINK_PRIVATE_LIBRARIES
        claudeinstallui
    SHARED_LIB
    NO_CONFIG
)
