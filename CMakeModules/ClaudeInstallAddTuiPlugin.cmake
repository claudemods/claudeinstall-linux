# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
#   SPDX-License-Identifier: BSD-2-Clause
#
###
#
# Support function for the text-mode (TUI) part of a view module.
#
# A view module that can be used in the terminal installer ships a second
# plugin next to the one for the GUI: its factory creates a ClaudeInstall::TuiStep.
# The two share the module's Config (see claudeinstall_add_config_library()).
# This function builds that second plugin and adds the *tuiLoad* key to
# the module's descriptor, so that the text-mode loader finds it.
#
# Usage:
#
# claudeinstall_add_tui_plugin(
#   module-name
#   SOURCES source-file...
#   [LINK_LIBRARIES lib...]
#   [COMPILE_DEFINITIONS def...]
#   [RESOURCES resource-file]
# )
#
# RESOURCES is one (single) RCC file for the plugin, for modules whose
# Config reads data from the resources (e.g. keyboard).
#
# Call it *after* claudeinstall_add_plugin() of the same module (in the
# same directory): it extends that module's module.desc. It does nothing
# if WITH_TUI is off or if the module itself is not being built. It works
# with WITH_GUI off, when claudeinstall_add_plugin() writes only the descriptor
# of a view module.
#
# The target is called "claudeinstall_tuimodule_<module-name>"; the plugin
# is installed next to the module's other files.

include( CMakeParseArguments )

function( claudeinstall_add_tui_plugin )
    set( NAME ${ARGV0} )
    set( oneValueArgs RESOURCES )
    set( multiValueArgs SOURCES LINK_LIBRARIES COMPILE_DEFINITIONS )
    cmake_parse_arguments( TUIPLUGIN "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

    if( NOT WITH_TUI )
        return()
    endif()
    if( NOT CLAUDEINSTALL_MODULE_${NAME}_ENABLED )
        # The module is skipped (claudeinstall_add_plugin() did not set this)
        return()
    endif()

    set( target "claudeinstall_tuimodule_${NAME}" )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" )

    message( "-- Found ${CLAUDEINSTALL_APPLICATION_NAME} text-mode module: ${NAME}" )

    add_library( ${target} MODULE ${TUIPLUGIN_SOURCES} )
    claudeinstall_automoc( ${target} )
    if( TUIPLUGIN_RESOURCES )
        claudeinstall_autorcc( ${target} ${TUIPLUGIN_RESOURCES} )
    endif()
    target_compile_definitions( ${target} PRIVATE PLUGINDLLEXPORT_PRO ${TUIPLUGIN_COMPILE_DEFINITIONS} )
    target_link_libraries(
        ${target}
        PRIVATE
            ClaudeInstall::claudeinstalltui
            ${TUIPLUGIN_LINK_LIBRARIES}
    )
    set_target_properties( ${target} PROPERTIES CXX_VISIBILITY_PRESET hidden PREFIX "lib" )

    # The descriptor was written (or copied) by claudeinstall_add_plugin()
    set( _file ${CMAKE_CURRENT_BINARY_DIR}/module.desc )
    if( EXISTS ${_file} )
        file( READ ${_file} _desc )
        if( NOT _desc MATCHES "tuiLoad" )
            file( APPEND ${_file} "tuiLoad: \"lib${target}.so\"\n" )
        endif()
    else()
        message( FATAL_ERROR "claudeinstall_add_tui_plugin(${NAME}) needs claudeinstall_add_plugin(${NAME}) first" )
    endif()

    install(
        TARGETS ${target}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/claudeinstall/modules/${NAME}
    )
endfunction()
