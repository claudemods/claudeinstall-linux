# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
include(KPMcoreHelper)

if(KPMcore_FOUND)
    include_directories(${KPMCORE_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/src/modules/partition)

    # The PartitionIterator is a small class, and it's easiest -- but also a
    # gross hack -- to just compile it again from the partition module tree.
    claudeinstall_add_plugin(fsresizer
        TYPE job
        EXPORT_MACRO PLUGINDLLEXPORT_PRO
        SOURCES
            ResizeFSJob.cpp
        LINK_PRIVATE_LIBRARIES
            claudeinstall::kpmcore
        COMPILE_DEFINITIONS ${KPMcore_API_DEFINITIONS}
        SHARED_LIB
    )

    claudeinstall_add_test(
        fsresizertest
        SOURCES Tests.cpp
        LIBRARIES
            claudeinstall_job_fsresizer # From above
            yamlcpp::yamlcpp
        DEFINITIONS ${KPMcore_API_DEFINITIONS}
    )
else()
    if(NOT KPMcore_FOUND)
        claudeinstall_skip_module( "fsresizer (missing suitable KPMcore)" )
    else()
        claudeinstall_skip_module( "fsresizer (missing dependencies for KPMcore)" )
    endif()
endif()
