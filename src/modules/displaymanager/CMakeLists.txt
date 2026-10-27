# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(displaymanager
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        DisplayManager.cpp
        DisplayManagerJob.cpp
        IniFile.cpp
        TomlFile.cpp
    SHARED_LIB
)

# The configuration code of the display managers is tested directly,
# which replaces the Python tests that used to live in tests/.
claudeinstall_add_test(
    displaymanagertest
    SOURCES Tests.cpp
    LIBRARIES
        claudeinstall_job_displaymanager # From above
)
