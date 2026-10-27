# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
#   SPDX-License-Identifier: BSD-2-Clause
#

# libclaudeinstalltui is the text-mode frontend: it shows the steps of
# libclaudeinstallstep in a terminal, using FTXUI. It needs libclaudeinstall,
# libclaudeinstallstep and QtCore; no Qt Gui or Widgets (see claudeinstall_add_plugin
# versus claudeinstall_add_tui_plugin).

include_directories(
    ${CMAKE_SOURCE_DIR}/src/libclaudeinstall
    ${CMAKE_BINARY_DIR}/src/libclaudeinstall
    ${CMAKE_SOURCE_DIR}/src/libclaudeinstallstep
)

add_library(
    claudeinstalltui
    SHARED
    PlaceholderStep.cpp
    TuiApplication.cpp
    TuiExecutionStep.cpp
    TuiModule.cpp
    TuiModuleLoader.cpp
    TuiStep.cpp
    TuiWidgets.cpp
    TuiWizard.cpp
)

set_target_properties(
    claudeinstalltui
    PROPERTIES
        VERSION ${CLAUDEINSTALL_VERSION_SHORT}
        SOVERSION ${CLAUDEINSTALL_SOVERSION}
        CXX_VISIBILITY_PRESET hidden
)
target_link_libraries(
    claudeinstalltui
    PUBLIC
        ClaudeInstall::claudeinstall
        ClaudeInstall::claudeinstallstep
        ${qtname}::Core
        ftxui::screen
        ftxui::dom
        ftxui::component
)
target_compile_definitions(claudeinstalltui PRIVATE TUIDLLEXPORT_PRO)
target_include_directories(
    claudeinstalltui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/libclaudeinstall>
)

claudeinstall_automoc(claudeinstalltui)
add_library(ClaudeInstall::claudeinstalltui ALIAS claudeinstalltui)

### Installation
#
install(
    TARGETS claudeinstalltui
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
    FILES
        PlaceholderStep.h
        TuiApplication.h
        TuiExecutionStep.h
        TuiModule.h
        TuiModuleLoader.h
        TuiStep.h
        TuiWidgets.h
        TuiWizard.h
    DESTINATION include/libclaudeinstall
)

claudeinstall_add_test(
    test_libclaudeinstalltui
    SOURCES Tests.cpp
    LIBRARIES claudeinstalltui
)
