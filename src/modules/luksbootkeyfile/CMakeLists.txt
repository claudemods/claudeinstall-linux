# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(luksbootkeyfile
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        LuksBootKeyFileJob.cpp
    SHARED_LIB
)

claudeinstall_add_test(luksbootkeyfiletest SOURCES Tests.cpp LuksBootKeyFileJob.cpp)
