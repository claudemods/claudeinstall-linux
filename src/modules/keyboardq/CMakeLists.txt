# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
if(NOT WITH_QML)
    claudeinstall_skip_module( "keyboardq (QML is not supported in this build)" )
    return()
endif()

set(_keyboard ${CMAKE_CURRENT_SOURCE_DIR}/../keyboard)

include_directories(${_keyboard})

claudeinstall_add_plugin(keyboardq
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        KeyboardQmlViewStep.cpp
        ${_keyboard}/Config.cpp
        ${_keyboard}/KeyboardLayoutModel.cpp
        ${_keyboard}/SetKeyboardLayoutJob.cpp
        ${_keyboard}/keyboardwidget/keyboardglobal.cpp
    RESOURCES
        keyboardq${QT_VERSION_SUFFIX}.qrc
    SHARED_LIB
    LINK_LIBRARIES
        ${qtname}::DBus
)
