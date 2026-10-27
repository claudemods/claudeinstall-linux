# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2021 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(umount
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        UmountJob.cpp
    SHARED_LIB
    EMERGENCY
)

claudeinstall_add_test(umounttest SOURCES Tests.cpp)
