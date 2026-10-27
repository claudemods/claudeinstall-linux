# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(machineid
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        MachineIdJob.cpp
        Workers.cpp
    SHARED_LIB
)

claudeinstall_add_test(machineidtest SOURCES Tests.cpp MachineIdJob.cpp Workers.cpp)
