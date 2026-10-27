# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(initramfscfg
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        InitramfsCfgJob.cpp
    NO_CONFIG
    SHARED_LIB
)

# The hook scripts are copied into the target by the job, and live
# next to the plugin (where the Python module used to find them).
install(
    FILES encrypt_hook encrypt_hook_nokey
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/claudeinstall/modules/initramfscfg
)
