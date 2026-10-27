# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(bootloader
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        BootloaderHelpers.cpp
        BootloaderJob.cpp
    REQUIRES
        partition
    SHARED_LIB
)

# The EFI Id suffix generators (and some other helpers) are tested
# directly, which replaces the Python test that used to live in tests/.
claudeinstall_add_test(
    bootloadertest
    SOURCES Tests.cpp
    LIBRARIES
        claudeinstall_job_bootloader # From above
)
