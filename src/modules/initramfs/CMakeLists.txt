# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(initramfs
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        InitramfsJob.cpp
    SHARED_LIB
)

claudeinstall_add_test(
    initramfstest
    SOURCES Tests.cpp
    LIBRARIES
        claudeinstall_job_initramfs # From above
        yamlcpp::yamlcpp
)
