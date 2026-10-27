# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
#   SPDX-License-Identifier: BSD-2-Clause
#
###
#
# Support function for the headless part of a view module.
#
# A view module has a Config object (and the jobs and models it needs)
# which does not depend on how the module is shown. Frontends -- the
# Widgets or QML view step, a text-mode step -- are separate plugins
# that all use the same code. That shared code is built as an OBJECT
# library (compiled once, linked into each plugin), which is what
# this function makes.
#
# Usage:
#
# claudeinstall_add_config_library(
#   library-name
#   SOURCES source-file...
#   [LINK_LIBRARIES lib...]
#   [DEFINITIONS def...]
# )
#
# The library links against libclaudeinstall, libclaudeinstallstep and QtCore
# **only**: not libclaudeinstallui, not Qt Gui or Widgets. That is deliberate,
# and the compiler enforces it: code that needs a GUI does not belong in the
# library. Use LINK_LIBRARIES for the other things the sources need
# (QtDBus, QtNetwork, KF CoreAddons, ...).
#
# The library is conventionally named <module>_config.

include( CMakeParseArguments )
include( ClaudeInstallAutomoc )

function( claudeinstall_add_config_library )
    set( NAME ${ARGV0} )
    set( multiValueArgs SOURCES LINK_LIBRARIES DEFINITIONS )
    cmake_parse_arguments( CONFIGLIB "" "" "${multiValueArgs}" ${ARGN} )

    add_library( ${NAME} OBJECT ${CONFIGLIB_SOURCES} )
    target_link_libraries(
        ${NAME}
        PUBLIC
            ClaudeInstall::claudeinstall
            ClaudeInstall::claudeinstallstep
            ${qtname}::Core
            ${CONFIGLIB_LINK_LIBRARIES}
    )
    # The objects end up in plugins, so they must be position independent; the
    # export macro is the one the plugins use (e.g. for the jobs in the library).
    target_compile_definitions( ${NAME} PUBLIC PLUGINDLLEXPORT_PRO ${CONFIGLIB_DEFINITIONS} )
    set_target_properties( ${NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON )
    claudeinstall_automoc( ${NAME} )
endfunction()
