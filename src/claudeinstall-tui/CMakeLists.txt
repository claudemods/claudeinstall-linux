# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
#   SPDX-License-Identifier: BSD-2-Clause
#

### EXECUTABLE
#
# "claudeinstall_tui_bin" is the text-mode installer; the program is
# called claudeinstall-tui. It uses QtCore only (and what libclaudeinstall
# itself pulls in), no Widgets or QML.

include_directories(
    ${CMAKE_SOURCE_DIR}/src/libclaudeinstall
    ${CMAKE_SOURCE_DIR}/src/libclaudeinstallstep
    ${CMAKE_SOURCE_DIR}/src/libclaudeinstalltui
    ${CMAKE_BINARY_DIR}/src/libclaudeinstall
)

add_executable(claudeinstall_tui_bin main.cpp)
target_include_directories(claudeinstall_tui_bin PRIVATE ${CMAKE_SOURCE_DIR})
set_target_properties(claudeinstall_tui_bin PROPERTIES ENABLE_EXPORTS TRUE RUNTIME_OUTPUT_NAME claudeinstall-tui)

target_link_libraries(
    claudeinstall_tui_bin
    PRIVATE
        claudeinstall
        claudeinstallstep
        claudeinstalltui
        claudeinstall-i18n
        kdsingleapplication
        ${qtname}::Core
        ${kfname}::CoreAddons
)

install(TARGETS claudeinstall_tui_bin RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
