# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
#   SPDX-License-Identifier: BSD-2-Clause
#

# libclaudeinstallstep is the frontend-neutral part of the wizard: steps (the
# non-widget half of view modules), the wizard that navigates them, and the
# model of the execution phase. It depends on libclaudeinstall and QtCore only,
# so that frontends without Widgets (e.g. a text-mode installer) can use it.
#
# Note that claudeinstall_add_library() is not used here: it always links
# Qt Gui and Widgets.

add_library(
    claudeinstallstep
    SHARED
    BrandingInfo.cpp
    CppJobModule.cpp
    ExecutionModel.cpp
    ModuleFactory.cpp
    ModuleLoadState.cpp
    ProcessJobModule.cpp
    Step.cpp
    Wizard.cpp
)

set_target_properties(
    claudeinstallstep
    PROPERTIES
        VERSION ${CLAUDEINSTALL_VERSION_SHORT}
        SOVERSION ${CLAUDEINSTALL_SOVERSION}
        CXX_VISIBILITY_PRESET hidden
)
target_link_libraries(claudeinstallstep PUBLIC ClaudeInstall::claudeinstall ${qtname}::Core)

target_compile_definitions(claudeinstallstep PRIVATE STEPDLLEXPORT_PRO)
target_include_directories(
    claudeinstallstep
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/libclaudeinstall>
)

claudeinstall_automoc(claudeinstallstep)
add_library(ClaudeInstall::claudeinstallstep ALIAS claudeinstallstep)

### Installation
#
#
install(
    TARGETS claudeinstallstep
    EXPORT ClaudeInstall
    RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
    FILES
        BrandingInfo.h
        CppJobModule.h
        ExecutionModel.h
        ModuleFactory.h
        ModuleLoadState.h
        ProcessJobModule.h
        Step.h
        Wizard.h
    DESTINATION include/libclaudeinstall
)

claudeinstall_add_test(
    test_libclaudeinstallstep
    SOURCES Tests.cpp
    LIBRARIES claudeinstallstep
)
