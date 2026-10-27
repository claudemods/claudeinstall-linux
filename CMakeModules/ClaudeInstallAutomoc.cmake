# === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
#
#   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
#   SPDX-License-Identifier: BSD-2-Clause
#
#   ClaudeInstall is Free Software: see the License-Identifier above.
#
#
###
#
# Helper function for doing automoc, autouic, autorcc on targets,
# and on the corresponding .ui or .rcc files.
#
# claudeinstall_automoc(target)
#   Sets AUTOMOC TRUE for a target.
#
#   If the global variable CLAUDEINSTALL_AUTOMOC_OPTIONS is set, uses that
#   as well to set options passed to MOC. This can be used to add
#   libclaudeinstall/utils/moc-warnings.h file to the moc, which in turn
#   reduces compiler warnings in generated MOC code.
#
# claudeinstall_autouic(target [uifile ..])
#   Sets AUTOUIC TRUE for a target.
#
#   If the global variable CLAUDEINSTALL_AUTOUIC_OPTIONS is set, adds that
#   to the options passed to uic for each of the named uifiles.
#
# claudeinstall_autorcc(target [rcfile ..])
#   Sets AUTOUIC TRUE for a target.
#
#   If the global variable CLAUDEINSTALL_AUTORCC_OPTIONS is set, adds that
#   to the options passed to rcc for each of the named rcfiles.

function(claudeinstall_automoc TARGET)
    set_target_properties( ${TARGET} PROPERTIES AUTOMOC TRUE )
    if ( CLAUDEINSTALL_AUTOMOC_OPTIONS )
        set_target_properties( ${TARGET} PROPERTIES AUTOMOC_MOC_OPTIONS "${CLAUDEINSTALL_AUTOMOC_OPTIONS}" )
    endif()
endfunction()

function(claudeinstall_autouic TARGET)
    set_target_properties( ${TARGET} PROPERTIES AUTOUIC TRUE )
    if ( CLAUDEINSTALL_AUTOUIC_OPTIONS )
        foreach(S ${ARGN})
            set_property(SOURCE ${S} PROPERTY AUTOUIC_OPTIONS "${CLAUDEINSTALL_AUTOUIC_OPTIONS}")
        endforeach()
    endif()
endfunction()

function(claudeinstall_autorcc TARGET)
    set_target_properties( ${TARGET} PROPERTIES AUTORCC TRUE )
    if ( CLAUDEINSTALL_AUTORCC_OPTIONS )
        foreach(S ${ARGN})
            set_property(SOURCE ${S} PROPERTY AUTORCC_OPTIONS "${CLAUDEINSTALL_AUTORCC_OPTIONS}")
        endforeach()
    endif()
endfunction()
