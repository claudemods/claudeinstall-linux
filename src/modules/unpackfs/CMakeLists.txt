# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(unpackfs
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        UnpackFSJob.cpp
    REQUIRES
        mount
    WEIGHT 12
    SHARED_LIB
)
