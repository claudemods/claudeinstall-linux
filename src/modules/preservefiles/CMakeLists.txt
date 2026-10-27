# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(preservefiles
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        Item.cpp
        PreserveFiles.cpp
    # REQUIRES mount  # To set the rootMountPoint
    SHARED_LIB
    EMERGENCY
)

claudeinstall_add_test(preservefilestest SOURCES Item.cpp Tests.cpp)
