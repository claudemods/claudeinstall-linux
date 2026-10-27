# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(shellprocess
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        ShellProcessJob.cpp
    SHARED_LIB
)

claudeinstall_add_test(shellprocesstest SOURCES Tests.cpp LIBRARIES yamlcpp::yamlcpp)
