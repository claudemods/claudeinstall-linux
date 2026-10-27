# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(initcpio
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        InitcpioJob.cpp
    SHARED_LIB
)

claudeinstall_add_test(
    initcpiotest
    SOURCES Tests.cpp
    LIBRARIES
        claudeinstall_job_initcpio # From above
        yamlcpp::yamlcpp
)
