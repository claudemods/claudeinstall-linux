# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-License-Identifier: BSD-2-Clause
#
claudeinstall_add_plugin(distroprofile
    TYPE job
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        DistroProfileJob.cpp
    LINK_PRIVATE_LIBRARIES
        crypt
    REQUIRES
        mount
    WEIGHT 20
    SHARED_LIB
)
