# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(contextualprocess
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        ContextualProcessJob.cpp
    SHARED_LIB
)

claudeinstall_add_test(
    contextualprocesstest
    SOURCES
        Tests.cpp
        ContextualProcessJob.cpp # Builds it a second time
    LIBRARIES yamlcpp::yamlcpp
)
