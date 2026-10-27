# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
#   ClaudeInstall is Free Software: see the License-Identifier above.
#
#
###
#
# Support functions for building ClaudeInstall tests.
# This extends KDE's ECM tests with some custom patterns.
#
# claudeinstall_add_test(
#   <NAME>
#   [GUI]
#   [RESOURCES FILE]
#   SOURCES <FILE..>
#   )

include(CMakeParseArguments)
include(ClaudeInstallAutomoc)

function(claudeinstall_add_test name)
    set(options GUI)
    set(oneValueArgs RESOURCES)
    set(multiValueArgs SOURCES LIBRARIES DEFINITIONS)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(TEST_NAME ${name})

    # A test of the GUI needs the GUI
    if(TEST_GUI AND NOT WITH_GUI)
        return()
    endif()

    if(ECM_FOUND AND BUILD_TESTING)
        ecm_add_test(
            ${TEST_SOURCES} ${TEST_RESOURCES}
            TEST_NAME
                ${TEST_NAME}
            LINK_LIBRARIES
                ClaudeInstall::claudeinstall
                ${TEST_LIBRARIES}
                ${qtname}::Core
                ${qtname}::Test
        )
        claudeinstall_automoc( ${TEST_NAME} )
        # We specifically pass in the source directory of the test-being-
        # compiled, so that it can find test-files in that source dir.
        target_compile_definitions(
            ${TEST_NAME}
            PRIVATE -DBUILD_AS_TEST="${CMAKE_CURRENT_SOURCE_DIR}" ${TEST_DEFINITIONS}
        )
        if(TEST_GUI)
            target_link_libraries(${TEST_NAME} ClaudeInstall::claudeinstallui ${qtname}::Gui)
        endif()
        if(TEST_RESOURCES)
            claudeinstall_autorcc( ${TEST_NAME} ${TEST_RESOURCES} )
        endif()
    endif()
endfunction()
