# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(packages
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        PackageManager.cpp
        PackagesJob.cpp
    SHARED_LIB
)

# The configuration code of the package managers (and the locale
# substitution) is tested directly; this replaces the Python tests
# that used to live in tests/.
claudeinstall_add_test(
    packagestest
    SOURCES Tests.cpp
    LIBRARIES
        claudeinstall_job_packages # From above
        yamlcpp::yamlcpp
)
