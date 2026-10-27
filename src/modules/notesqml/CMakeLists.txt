# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
if(NOT WITH_QML)
    claudeinstall_skip_module( "notesqml (QML is not supported in this build)" )
    return()
endif()

claudeinstall_add_plugin(notesqml
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        NotesQmlViewStep.cpp
    RESOURCES
        notesqml.qrc
    SHARED_LIB
)
